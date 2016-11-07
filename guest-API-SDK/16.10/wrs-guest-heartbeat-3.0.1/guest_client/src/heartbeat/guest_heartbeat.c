/*
 * Copyright (c) 2013-2016, Wind River Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * 3) Neither the name of Wind River Systems nor the names of its contributors may be
 * used to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "guest_heartbeat.h"

#include <stdlib.h>
#include <string.h>

#include "guest_types.h"
#include "guest_debug.h"
#include "guest_timer.h"

#include "guest_heartbeat_config.h"
#include "guest_heartbeat_msg.h"
#include "guest_heartbeat_fsm.h"
#include "guest_heartbeat_health_script.h"
#include "guest_heartbeat_event_script.h"
#include "guest_heartbeat_mgmt_api.h"

static GuestTimerIdT _release_timer_id = GUEST_TIMER_ID_INVALID;

// ****************************************************************************
// Guest Heartbeat - Release
// =========================
static bool guest_heartbeat_release(GuestTimerIdT timer_id)
{
    GuestErrorT error;

    error = guest_heartbeat_fsm_event_handler(GUEST_HEARTBEAT_FSM_RELEASE,
                                              NULL);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to handle release event, error=%s.",
                 guest_error_str(error));
        return true;
    }

    return false; // don't rearm
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat - Channel State Change
// ======================================
static void guest_heartbeat_channel_state_change( bool state )
{
    GuestErrorT error;

    if (state)
    {
        error = guest_heartbeat_fsm_event_handler(
                    GUEST_HEARTBEAT_FSM_CHANNEL_UP, NULL);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to handle channel up event, error=%s.",
                     guest_error_str(error));
            return;
        }
    } else {
        error = guest_heartbeat_fsm_event_handler(
                    GUEST_HEARTBEAT_FSM_CHANNEL_DOWN, NULL);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to  handle channel down event, error=%s.",
                     guest_error_str(error));
            return;
        }
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat - Receive Init Ack Message
// ==========================================
static void guest_heartbeat_recv_init_ack_msg( int invocation_id )
{
    void* event_data[] = {&invocation_id};
    GuestErrorT error;

    error = guest_heartbeat_fsm_event_handler(GUEST_HEARTBEAT_FSM_INIT_ACK,
                                              event_data);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to handle heartbeat-init-ack event, error=%s.",
                 guest_error_str(error));
        return;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat - Receive Challenge Message
// ===========================================
static void guest_heartbeat_recv_challenge_msg( void )
{
    GuestErrorT error;

    error = guest_heartbeat_fsm_event_handler(GUEST_HEARTBEAT_FSM_CHALLENGE,
                                              NULL);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to handle heartbeat-challenge event, error=%s.",
                 guest_error_str(error));
        return;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat - Receive Action Notify Message
// ===============================================
static void guest_heartbeat_recv_action_notify_msg(
        int invocation_id, GuestHeartbeatEventT event,
        GuestHeartbeatNotifyT notify, int timeout_ms)
{
    void* event_data[] = {&invocation_id, &event, &notify, &timeout_ms};
    GuestHeartbeatFsmStateT state = guest_heartbeat_fsm_get_state();
    GuestErrorT error;

    if (GUEST_HEARTBEAT_FSM_ENABLED_STATE == state)
    {
        error = guest_heartbeat_fsm_event_handler(GUEST_HEARTBEAT_FSM_ACTION,
                                                  event_data);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to handle action notify for event %s, error=%s.",
                     guest_heartbeat_event_str(event), guest_error_str(error));
            return;
        }

    } else {
        error = guest_heartbeat_msg_send_action_response(
                    invocation_id, event, notify,
                    GUEST_HEARTBEAT_VOTE_RESULT_COMPLETE, "");
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to send action response for event %s, error=%s.",
                     guest_heartbeat_event_str(event), guest_error_str(error));
            return;
        }
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat - Initialize
// ============================
GuestErrorT guest_heartbeat_initialize( char* comm_device )
{
    GuestHeartbeatMsgCallbacksT callbacks;
    GuestErrorT error;

    _release_timer_id = GUEST_TIMER_ID_INVALID;

    error = guest_heartbeat_config_initialize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to initialize heartbeat configuration, error=%s.",
                 guest_error_str(error));
        return error;
    }

    memset(&callbacks, 0, sizeof(callbacks));

    callbacks.channel_state_change = guest_heartbeat_channel_state_change;
    callbacks.recv_init_ack = guest_heartbeat_recv_init_ack_msg;
    callbacks.recv_challenge = guest_heartbeat_recv_challenge_msg;
    callbacks.recv_action_notify = guest_heartbeat_recv_action_notify_msg;

    error = guest_heartbeat_msg_initialize(comm_device, &callbacks);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to initialize heartbeat messaging, error=%s.",
                 guest_error_str(error));
        return error;
    }

    error = guest_heartbeat_fsm_initialize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to initialize heartbeat fsm, error=%s.",
                 guest_error_str(error));
        return error;
    }

    error = guest_heartbeat_mgmt_api_initialize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to initialize heartbeat management api, error=%s.",
                 guest_error_str(error));
        return error;
    }

    error = guest_heartbeat_health_script_initialize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to initialize heartbeat health script handling, "
                         "error=%s.", guest_error_str(error));
        return error;
    }

    error = guest_heartbeat_event_script_initialize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to initialize heartbeat event script handling, "
                 "error=%s.", guest_error_str(error));
        return error;
    }

    error = guest_timer_register(1000, guest_heartbeat_release,
                                 &_release_timer_id);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to start release timer, error=%s.",
                 guest_error_str(error));
        return error;
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat - Finalize
// ==========================
GuestErrorT guest_heartbeat_finalize( void )
{
    GuestErrorT error;

    error = guest_heartbeat_fsm_event_handler(GUEST_HEARTBEAT_FSM_SHUTDOWN,
                                              NULL);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to handle shutdown event, error=%s.",
                 guest_error_str(error));
    }

    if (GUEST_TIMER_ID_INVALID != _release_timer_id)
    {
        error = guest_timer_deregister(_release_timer_id);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to cancel release timer, error=%s.",
                     guest_error_str(error));
        }
        _release_timer_id = GUEST_TIMER_ID_INVALID;
    }

    error = guest_heartbeat_event_script_finalize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to finalize heartbeat event script handling, "
                 "error=%s.", guest_error_str(error));
    }

    error = guest_heartbeat_health_script_finalize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to finalize heartbeat health script handling, "
                         "error=%s.", guest_error_str(error));
    }

    error = guest_heartbeat_mgmt_api_finalize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to finalize heartbeat management api, error=%s.",
                 guest_error_str(error));
    }

    error = guest_heartbeat_fsm_finalize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to finalize heartbeat fsm, error=%s.",
                 guest_error_str(error));
    }

    error = guest_heartbeat_msg_finalize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to finalize heartbeat messaging, error=%s.",
                 guest_error_str(error));
    }

    error = guest_heartbeat_config_finalize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to finalize heartbeat configuration, error=%s.",
                 guest_error_str(error));
    }

    return GUEST_OKAY;
}
// ****************************************************************************

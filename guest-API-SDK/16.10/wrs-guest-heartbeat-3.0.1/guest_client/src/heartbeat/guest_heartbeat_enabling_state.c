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
#include "guest_heartbeat_enabling_state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "guest_types.h"
#include "guest_debug.h"
#include "guest_timer.h"
#include "guest_config.h"

#include "guest_heartbeat_config.h"
#include "guest_heartbeat_msg.h"
#include "guest_heartbeat_fsm.h"

static int _prev_invocation_id;
static int _invocation_id;
static GuestTimerIdT _connect_timer_id = GUEST_TIMER_ID_INVALID;

// ****************************************************************************
// Guest Heartbeat Enabling State - Attempt Connect
// ================================================
static bool guest_heartbeat_enabling_state_connect( GuestTimerIdT timer_id )
{
    GuestConfigT* cfg = guest_config_get();
    GuestHeartbeatMsgInitDataT data;
    GuestHeartbeatConfigT* config = guest_heartbeat_config_get();
    GuestErrorT error;

    _prev_invocation_id = _invocation_id;
    _invocation_id = rand();

    memset(&data, 0, sizeof(data));
    snprintf(data.name, GUEST_NAME_MAX_CHAR, "%s", cfg->name);
    data.heartbeat_interval_ms = config->heartbeat_interval_ms;
    data.vote_ms = config->vote_ms;
    data.shutdown_notice_ms = config->shutdown_notice_ms;
    data.suspend_notice_ms = config->suspend_notice_ms;
    data.resume_notice_ms = config->resume_notice_ms;
    data.restart_ms = config->restart_ms;
    data.corrective_action = config->corrective_action;

    error = guest_heartbeat_msg_send_init(_invocation_id, &data);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to send heartbeat init message, error=%s.",
                 guest_error_str(error));
        return true;
    }

    return true; // rearm
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Enabling State - Enter
// ======================================
GuestErrorT guest_heartbeat_enabling_state_enter( void )
{
    GuestHeartbeatConfigT* config = guest_heartbeat_config_get();
    GuestErrorT error;

    error = guest_timer_register(config->heartbeat_init_retry_ms,
                                 guest_heartbeat_enabling_state_connect,
                                 &_connect_timer_id);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to start connect timer, error=%s.",
                 guest_error_str(error));
        return error;
    }

    guest_heartbeat_enabling_state_connect(_connect_timer_id);

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Enabling State - Exit
// =====================================
GuestErrorT guest_heartbeat_enabling_state_exit( void )
{
    GuestErrorT error;

    if (GUEST_TIMER_ID_INVALID != _connect_timer_id)
    {
        error = guest_timer_deregister(_connect_timer_id);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to cancel connect timer, error=%s.",
                     guest_error_str(error));
        }
        _connect_timer_id = GUEST_TIMER_ID_INVALID;
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Enabling State - Transition
// ===========================================
GuestErrorT guest_heartbeat_enabling_state_transition(
        GuestHeartbeatFsmStateT from_state )
{
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Enabling State - Event Handler
// ==============================================
GuestErrorT guest_heartbeat_enabling_state_event_handler(
        GuestHeartbeatFsmEventT event, void* event_data[] )
{
    int invocation_id;

    switch (event)
    {
        case GUEST_HEARTBEAT_FSM_RELEASE:
        case GUEST_HEARTBEAT_FSM_CHALLENGE:
        case GUEST_HEARTBEAT_FSM_CHALLENGE_TIMEOUT:
        case GUEST_HEARTBEAT_FSM_ACTION:
        case GUEST_HEARTBEAT_FSM_CHANNEL_UP:
            // Ignore.
            break;

        case GUEST_HEARTBEAT_FSM_CHANNEL_DOWN:
            guest_heartbeat_fsm_set_state(GUEST_HEARTBEAT_FSM_DISABLED_STATE);
            break;

        case GUEST_HEARTBEAT_FSM_INIT_ACK:
            invocation_id = *(int*) event_data[0];

            if ((invocation_id == _invocation_id) ||
                (invocation_id == _prev_invocation_id))
                guest_heartbeat_fsm_set_state(GUEST_HEARTBEAT_FSM_ENABLED_STATE);
            break;

        case GUEST_HEARTBEAT_FSM_SHUTDOWN:
            guest_heartbeat_fsm_set_state(GUEST_HEARTBEAT_FSM_INITIAL_STATE);
            break;

        default:
            DPRINTFE("Ignoring event %s.",
                     guest_heartbeat_fsm_event_str(event));
    }
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Enabling State - Initialize
// ===========================================
GuestErrorT guest_heartbeat_enabling_state_initialize( void )
{
    _connect_timer_id = GUEST_TIMER_ID_INVALID;
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Enabling State - Finalize
// =========================================
GuestErrorT guest_heartbeat_enabling_state_finalize( void )
{
    GuestErrorT error;

    if (GUEST_TIMER_ID_INVALID != _connect_timer_id)
    {
        error = guest_timer_deregister(_connect_timer_id);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to cancel connect timer, error=%s.",
                     guest_error_str(error));
        }
        _connect_timer_id = GUEST_TIMER_ID_INVALID;
    }

    return GUEST_OKAY;
}
// ****************************************************************************

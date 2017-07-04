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
#include "guest_heartbeat_initial_state.h"

#include "guest_types.h"
#include "guest_debug.h"
#include "guest_heartbeat_fsm.h"

// ****************************************************************************
// Guest Heartbeat Initial State - Enter
// =====================================
GuestErrorT guest_heartbeat_initial_state_enter( void )
{
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Initial State - Exit
// ====================================
GuestErrorT guest_heartbeat_initial_state_exit( void )
{
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Initial State - Transition
// ==========================================
GuestErrorT guest_heartbeat_initial_state_transition(
        GuestHeartbeatFsmStateT from_state )
{
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Initial State - Event Handler
// =============================================
GuestErrorT guest_heartbeat_initial_state_event_handler(
        GuestHeartbeatFsmEventT event, void* event_data[] )
{
    switch (event)
    {
        case GUEST_HEARTBEAT_FSM_INIT_ACK:
        case GUEST_HEARTBEAT_FSM_CHALLENGE:
        case GUEST_HEARTBEAT_FSM_CHALLENGE_TIMEOUT:
        case GUEST_HEARTBEAT_FSM_ACTION:
        case GUEST_HEARTBEAT_FSM_SHUTDOWN:
            // Ignore
            break;

        case GUEST_HEARTBEAT_FSM_RELEASE:
        case GUEST_HEARTBEAT_FSM_CHANNEL_UP:
            guest_heartbeat_fsm_set_state(GUEST_HEARTBEAT_FSM_ENABLING_STATE);
            break;

        case GUEST_HEARTBEAT_FSM_CHANNEL_DOWN:
            guest_heartbeat_fsm_set_state(GUEST_HEARTBEAT_FSM_DISABLED_STATE);
            break;

        default:
            DPRINTFV("Ignoring event %s.",
                     guest_heartbeat_fsm_event_str(event));
    }
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Initial State - Initialize
// ==========================================
GuestErrorT guest_heartbeat_initial_state_initialize( void )
{
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Initial State - Finalize
// ========================================
GuestErrorT guest_heartbeat_initial_state_finalize( void )
{
    return GUEST_OKAY;
}
// ****************************************************************************

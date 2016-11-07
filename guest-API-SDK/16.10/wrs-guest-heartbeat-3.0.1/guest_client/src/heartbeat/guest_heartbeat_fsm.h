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
#ifndef __GUEST_HEARTBEAT_FSM_H__
#define __GUEST_HEARTBEAT_FSM_H__

#include "guest_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GUEST_HEARTBEAT_FSM_INITIAL_STATE,
    GUEST_HEARTBEAT_FSM_ENABLING_STATE,
    GUEST_HEARTBEAT_FSM_ENABLED_STATE,
    GUEST_HEARTBEAT_FSM_DISABLED_STATE,
    GUEST_HEARTBEAT_FSM_MAX_STATES
} GuestHeartbeatFsmStateT;

typedef enum {
    GUEST_HEARTBEAT_FSM_RELEASE,
    GUEST_HEARTBEAT_FSM_INIT_ACK,
    GUEST_HEARTBEAT_FSM_CHALLENGE,
    GUEST_HEARTBEAT_FSM_CHALLENGE_TIMEOUT,
    GUEST_HEARTBEAT_FSM_ACTION,
    GUEST_HEARTBEAT_FSM_CHANNEL_UP,
    GUEST_HEARTBEAT_FSM_CHANNEL_DOWN,
    GUEST_HEARTBEAT_FSM_SHUTDOWN,
    GUEST_HEARTBEAT_FSM_MAX_EVENTS
} GuestHeartbeatFsmEventT;

// ****************************************************************************
// Guest Heartbeat FSM - State String
// ==================================
extern const char* guest_heartbeat_fsm_state_str( GuestHeartbeatFsmStateT state );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat FSM - Event String
// ==================================
extern const char* guest_heartbeat_fsm_event_str( GuestHeartbeatFsmEventT event );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat FSM - Get State
// ===============================
extern GuestHeartbeatFsmStateT guest_heartbeat_fsm_get_state( void );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat FSM - Set State
// ===============================
extern GuestErrorT guest_heartbeat_fsm_set_state( GuestHeartbeatFsmStateT state );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat FSM - Event Handler
// ===================================
extern GuestErrorT guest_heartbeat_fsm_event_handler(
        GuestHeartbeatFsmEventT event, void* event_data[] );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat FSM - Initialize
// ================================
extern GuestErrorT guest_heartbeat_fsm_initialize( void );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat FSM - Finalize
// ==============================
extern GuestErrorT guest_heartbeat_fsm_finalize( void );
// ****************************************************************************

#ifdef __cplusplus
}
#endif

#endif /* __GUEST_HEARTBEAT_FSM_H__ */

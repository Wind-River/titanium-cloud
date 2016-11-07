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
#include "guest_heartbeat_fsm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "guest_types.h"
#include "guest_debug.h"
#include "guest_heartbeat_initial_state.h"
#include "guest_heartbeat_enabling_state.h"
#include "guest_heartbeat_enabled_state.h"
#include "guest_heartbeat_disabled_state.h"

typedef GuestErrorT (*GuestHeartbeatFsmStateEnterT) (void);
typedef GuestErrorT (*GuestHeartbeatFsmStateExitT) (void);
typedef GuestErrorT (*GuestHeartbeatFsmStateTransitionT)
        (GuestHeartbeatFsmStateT from_state);
typedef GuestErrorT (*GuestHeartbeatFsmStateEventHandlerT)
        (GuestHeartbeatFsmEventT event, void* event_data[]);
typedef GuestErrorT (*GuestHeartbeatFsmStateInitializeT) (void);
typedef GuestErrorT (*GuestHeartbeatFsmStateFinalizeT) (void);

typedef struct {
    char name[40];
    GuestHeartbeatFsmStateEnterT enter;
    GuestHeartbeatFsmStateExitT exit;
    GuestHeartbeatFsmStateTransitionT transition;
    GuestHeartbeatFsmStateEventHandlerT event_handler;
    GuestHeartbeatFsmStateInitializeT initialize;
    GuestHeartbeatFsmStateFinalizeT finalize;
} GuestHeartbeatFsmStateEntryT;

static GuestHeartbeatFsmStateEntryT _states[GUEST_HEARTBEAT_FSM_MAX_STATES];
static GuestHeartbeatFsmStateT _current_state = GUEST_HEARTBEAT_FSM_INITIAL_STATE;

// ****************************************************************************
// Guest Heartbeat FSM - State String
// ==================================
const char* guest_heartbeat_fsm_state_str( GuestHeartbeatFsmStateT state )
{
    if ((0 > state) || (GUEST_HEARTBEAT_FSM_MAX_STATES <= state))
    {
        return "state-???";
    }

    return _states[state].name;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat FSM - Event String
// ==================================
const char* guest_heartbeat_fsm_event_str( GuestHeartbeatFsmEventT event )
{
    switch (event)
    {
        case GUEST_HEARTBEAT_FSM_RELEASE:           return "release";
        case GUEST_HEARTBEAT_FSM_INIT_ACK:          return "init-ack";
        case GUEST_HEARTBEAT_FSM_CHALLENGE:         return "challenge";
        case GUEST_HEARTBEAT_FSM_CHALLENGE_TIMEOUT: return "challenge-timeout";
        case GUEST_HEARTBEAT_FSM_ACTION:            return "action";
        case GUEST_HEARTBEAT_FSM_CHANNEL_UP:        return "channel-up";
        case GUEST_HEARTBEAT_FSM_CHANNEL_DOWN:      return "channel-down";
        case GUEST_HEARTBEAT_FSM_SHUTDOWN:          return "shutdown";
        default:
            return "event-???";
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat FSM - Get State
// ===============================
GuestHeartbeatFsmStateT guest_heartbeat_fsm_get_state( void )
{
    return _current_state;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat FSM - Enter State
// =================================
static GuestErrorT guest_heartbeat_fsm_enter_state( GuestHeartbeatFsmStateT state )
{
    GuestHeartbeatFsmStateEntryT* entry = &(_states[state]);
    GuestErrorT error;

    if (NULL != entry->enter)
    {
        error = entry->enter();
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Unable to enter state %s, error=%s.", entry->name,
                     guest_error_str(error));
            return error;
        }
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat FSM - Exit State
// ================================
static GuestErrorT guest_heartbeat_fsm_exit_state( GuestHeartbeatFsmStateT state )
{
    GuestHeartbeatFsmStateEntryT* entry = &(_states[state]);
    GuestErrorT error;

    if (NULL != entry->exit)
    {
        error = entry->exit();
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Unable to exit state %s, error=%s.", entry->name,
                     guest_error_str(error));
            return error;
        }
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat FSM - Transition State
// ======================================
static GuestErrorT guest_heartbeat_fsm_transition_state(
        GuestHeartbeatFsmStateT from_state )
{
    GuestHeartbeatFsmStateEntryT* entry = &(_states[from_state]);
    GuestErrorT error;

    if (NULL != entry->transition)
    {
        error = entry->transition(from_state);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Unable to transition from state %s, error=%s.",
                     entry->name, guest_error_str(error));
            return error;
        }
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat FSM - Set State
// ===============================
GuestErrorT guest_heartbeat_fsm_set_state( GuestHeartbeatFsmStateT state )
{
    GuestHeartbeatFsmStateT prev_state = _current_state;
    GuestHeartbeatFsmStateEntryT* prev_entry;
    GuestHeartbeatFsmStateEntryT* entry;
    GuestErrorT error, error2;

    if ((0 > state) || (GUEST_HEARTBEAT_FSM_MAX_STATES <= state))
    {
        DPRINTFE("Invalid state %i given.", state);
        return GUEST_FAILED;
    }

    prev_entry = &(_states[prev_state]);
    entry = &(_states[state]);

    error = guest_heartbeat_fsm_exit_state(prev_state);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to exit state %s, error=%s.", prev_entry->name,
                 guest_error_str(error));
        return( error );
    }

    _current_state = state;

    error = guest_heartbeat_fsm_transition_state(prev_state);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to transition to state %s, error=%s.",
                 prev_entry->name, guest_error_str(error));
        goto STATE_CHANGE_TRANSITION_ERROR;
    }

    error = guest_heartbeat_fsm_enter_state(state);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to enter state %s, error=%s.", entry->name,
                 guest_error_str(error));
        goto STATE_CHANGE_ENTER_ERROR;
    }

    return( GUEST_OKAY );

STATE_CHANGE_ENTER_ERROR:
    error2 = guest_heartbeat_fsm_transition_state(state);
    if (GUEST_OKAY != error2)
    {
        DPRINTFE("Failed to transition from state %s, error=%s.",
                 entry->name, guest_error_str(error2));
        abort();
    }

STATE_CHANGE_TRANSITION_ERROR:
    _current_state = prev_state;

    error2 = guest_heartbeat_fsm_enter_state(prev_state);
    if (GUEST_OKAY != error2)
    {
        DPRINTFE("Failed to enter state (%s), error=%s.", prev_entry->name,
                 guest_error_str(error2));
        abort();
    }

    return error;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat FSM - Event Handler
// ===================================
GuestErrorT guest_heartbeat_fsm_event_handler(
        GuestHeartbeatFsmEventT event, void* event_data[] )
{
    GuestHeartbeatFsmStateT prev_state = _current_state;
    GuestHeartbeatFsmStateEntryT* entry = &(_states[_current_state]);
    GuestErrorT error;

    if (NULL != entry->event_handler)
    {
        error = entry->event_handler(event, event_data);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Unable to handle event %s in state %s, error=%s.",
                     guest_heartbeat_fsm_event_str(event), entry->name,
                     guest_error_str(error));
            return error;
        }

        if (prev_state != _current_state)
        {
            DPRINTFI("Guest-Client heartbeat state change from %s to %s, "
                     "event=%s.", guest_heartbeat_fsm_state_str(prev_state),
                     guest_heartbeat_fsm_state_str(_current_state),
                     guest_heartbeat_fsm_event_str(event));
        } else {
            DPRINTFV("Guest-Client heartbeat no state change from %s, "
                     "event=%s.", guest_heartbeat_fsm_state_str(prev_state),
                     guest_heartbeat_fsm_event_str(event));
        }
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat FSM - Initialize
// ================================
GuestErrorT guest_heartbeat_fsm_initialize( void )
{
    GuestHeartbeatFsmStateEntryT* entry;

    memset(_states, 0, sizeof(_states));
    _current_state = GUEST_HEARTBEAT_FSM_INITIAL_STATE;

    // Initial State
    entry = &(_states[GUEST_HEARTBEAT_FSM_INITIAL_STATE]);
    snprintf(entry->name, sizeof(entry->name), "initial");
    entry->enter = guest_heartbeat_initial_state_enter;
    entry->exit = guest_heartbeat_initial_state_exit;
    entry->transition = guest_heartbeat_initial_state_transition;
    entry->event_handler = guest_heartbeat_initial_state_event_handler;
    entry->initialize = guest_heartbeat_initial_state_initialize;
    entry->finalize = guest_heartbeat_initial_state_finalize;

    // Enabling State
    entry = &(_states[GUEST_HEARTBEAT_FSM_ENABLING_STATE]);
    snprintf(entry->name, sizeof(entry->name), "enabling");
    entry->enter = guest_heartbeat_enabling_state_enter;
    entry->exit = guest_heartbeat_enabling_state_exit;
    entry->transition = guest_heartbeat_enabling_state_transition;
    entry->event_handler = guest_heartbeat_enabling_state_event_handler;
    entry->initialize = guest_heartbeat_enabling_state_initialize;
    entry->finalize = guest_heartbeat_enabling_state_finalize;

    // Enabled State
    entry = &(_states[GUEST_HEARTBEAT_FSM_ENABLED_STATE]);
    snprintf(entry->name, sizeof(entry->name), "enabled");
    entry->enter = guest_heartbeat_enabled_state_enter;
    entry->exit = guest_heartbeat_enabled_state_exit;
    entry->transition = guest_heartbeat_enabled_state_transition;
    entry->event_handler = guest_heartbeat_enabled_state_event_handler;
    entry->initialize = guest_heartbeat_enabled_state_initialize;
    entry->finalize = guest_heartbeat_enabled_state_finalize;

    // Disabled State
    entry = &(_states[GUEST_HEARTBEAT_FSM_DISABLED_STATE]);
    snprintf(entry->name, sizeof(entry->name), "disabled");
    entry->enter = guest_heartbeat_disabled_state_enter;
    entry->exit = guest_heartbeat_disabled_state_exit;
    entry->transition = guest_heartbeat_disabled_state_transition;
    entry->event_handler = guest_heartbeat_disabled_state_event_handler;
    entry->initialize = guest_heartbeat_disabled_state_initialize;
    entry->finalize = guest_heartbeat_disabled_state_finalize;

    unsigned int state_i;
    for (state_i=0; GUEST_HEARTBEAT_FSM_MAX_STATES > state_i; ++state_i)
    {
        entry = &(_states[state_i]);
        if (NULL != entry->initialize)
            entry->initialize();
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat FSM - Finalize
// ==============================
GuestErrorT guest_heartbeat_fsm_finalize( void )
{
    GuestHeartbeatFsmStateEntryT* entry;

    unsigned int state_i;
    for (state_i=0; GUEST_HEARTBEAT_FSM_MAX_STATES > state_i; ++state_i)
    {
        entry = &(_states[state_i]);
        if (NULL != entry->finalize)
            entry->finalize();
    }

    memset(_states, 0, sizeof(_states));
    _current_state = GUEST_HEARTBEAT_FSM_INITIAL_STATE;

    return GUEST_OKAY;
}
// ****************************************************************************

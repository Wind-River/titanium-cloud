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
#include "guest_heartbeat_enabled_state.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "guest_types.h"
#include "guest_debug.h"
#include "guest_time.h"
#include "guest_timer.h"

#include "guest_heartbeat_config.h"
#include "guest_heartbeat_msg.h"
#include "guest_heartbeat_fsm.h"
#include "guest_heartbeat_health_script.h"
#include "guest_heartbeat_event_script.h"
#include "guest_heartbeat_mgmt_api.h"

static bool _wait_application;
static bool _wait_script;
static int _action_invocation_id;
static GuestHeartbeatEventT _action_event;
static GuestHeartbeatNotifyT _action_notify;
GuestHeartbeatVoteResultT _vote_result;
static int _action_timeout_ms;
static GuestTimerIdT _health_check_timer_id = GUEST_TIMER_ID_INVALID;
static GuestTimerIdT _challenge_timeout_timer_id = GUEST_TIMER_ID_INVALID;
static GuestTimerIdT _action_timeout_timer_id = GUEST_TIMER_ID_INVALID;
static bool _health = true;
static char _health_log_msg[GUEST_HEARTBEAT_MAX_LOG_MSG_SIZE];
static char _action_log_msg[GUEST_HEARTBEAT_MAX_LOG_MSG_SIZE];
static GuestTimeT _last_time_reported;
static bool _last_health_reported = true;
static GuestHeartbeatActionT _last_corrective_action_reported;

// ****************************************************************************
// Guest Heartbeat Enabled State - Health Callback
// ===============================================
static void guest_heartbeat_enabled_state_health_callback(
        bool health, char* log_msg )
{
    if (_health && !health)
    {
        DPRINTFI("Transition from healthy to unhealthy, msg=%s.", log_msg);

    } else if (!_health && health) {
        DPRINTFI("Transition from unhealthy to healthy, msg=%s.", log_msg);
    }

    _health = health;
    snprintf(_health_log_msg, sizeof(_health_log_msg), "%s", log_msg);
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Enabled State - Health Check
// ============================================
static bool guest_heartbeat_enabled_state_health_check(
        GuestTimerIdT timer_id )
{
    GuestHeartbeatConfigT* config = guest_heartbeat_config_get();
    GuestErrorT error;

    guest_heartbeat_health_script_abort();

    if ('\0' != config->health_check_script[0])
    {
        error = guest_heartbeat_health_script_invoke(
                config->health_check_script,
                guest_heartbeat_enabled_state_health_callback);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to invoke health script %s.",
                     config->event_handling_script);
            return true; // rearm
        }
    }

    return true; // rearm
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Enabled State - Challenge Timeout
// =================================================
static bool guest_heartbeat_enabled_state_challenge_timeout(
        GuestTimerIdT timer_id )
{
    GuestHeartbeatConfigT* config = guest_heartbeat_config_get();
    GuestErrorT error;
    int max_heartbeat_delay;

    max_heartbeat_delay = config->heartbeat_interval_ms*2;
    if (max_heartbeat_delay < config->heartbeat_min_timeout_ms)
        max_heartbeat_delay = config->heartbeat_min_timeout_ms;

    if (!guest_timer_scheduling_on_time_within(max_heartbeat_delay))
    {
        DPRINTFE("Failed to receive a challenge in %i ms, but we are not "
                 "scheduling on time.", max_heartbeat_delay);
        return true; // rearm
    }

    DPRINTFE("Failed to receive a challenge in %i ms.", max_heartbeat_delay);

    error = guest_heartbeat_fsm_event_handler(GUEST_HEARTBEAT_FSM_CHALLENGE_TIMEOUT,
                                              NULL);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to handle heartbeat-challenge-timeout event, "
                 "error=%s.", guest_error_str(error));
        return true; // rearm
    }

    return true; // rearm
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Enabled State - Action Timeout
// ==============================================
static bool guest_heartbeat_enabled_state_action_timeout(
        GuestTimerIdT timer_id )
{
    char log_msg[GUEST_HEARTBEAT_MAX_LOG_MSG_SIZE];
    GuestHeartbeatVoteResultT vote_result;
    GuestErrorT error;

    if (!guest_timer_scheduling_on_time_within(_action_timeout_ms))
    {
        DPRINTFE("Failed to receive action script response in %i ms, but we "
                 "are not scheduling on time.", _action_timeout_ms);
        return true;
    }

    DPRINTFE("Failed to receive action responses in %i ms.",
             _action_timeout_ms);

    guest_heartbeat_mgmt_api_action_abort();
    guest_heartbeat_event_script_abort();

    if (((!_wait_application) || (!_wait_script)) &&
        (GUEST_HEARTBEAT_VOTE_RESULT_REJECT == _vote_result))
    {
        vote_result = _vote_result;
        snprintf(log_msg, sizeof(log_msg), "%s", _action_log_msg);
    } else {
        vote_result = GUEST_HEARTBEAT_VOTE_RESULT_TIMEOUT;
        snprintf(log_msg, sizeof(log_msg), "Timeout on application and/or "
                 "script action responses.");
    }

    error = guest_heartbeat_msg_send_action_response(
                _action_invocation_id, _action_event, _action_notify,
                vote_result, log_msg);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to send action script response for event %s, "
                 "notification=%s, error=%s.",
                 guest_heartbeat_event_str(_action_event),
                 guest_heartbeat_notify_str(_action_notify),
                 guest_error_str(error));
    }

    _action_timeout_timer_id = GUEST_TIMER_ID_INVALID;
    return false; // don't rearm
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Enabled State - Action Application Callback
// ===========================================================
static void guest_heartbeat_enabled_state_action_app_callback(
        GuestHeartbeatEventT event, GuestHeartbeatNotifyT notify,
        GuestHeartbeatVoteResultT vote_result, char* log_msg )
{
    bool update;
    GuestErrorT error;

    DPRINTFI("Received action application response, event=%s, notify=%s, "
             "vote-result=%s, msg=%s.", guest_heartbeat_event_str(event),
             guest_heartbeat_notify_str(notify),
             guest_heartbeat_vote_result_str(vote_result), log_msg);

    _wait_application = false;
    update = false;

    switch (vote_result)
    {
        case GUEST_HEARTBEAT_VOTE_RESULT_REJECT:
            update = true;
            break;

        case GUEST_HEARTBEAT_VOTE_RESULT_ACCEPT:
        case GUEST_HEARTBEAT_VOTE_RESULT_COMPLETE:
            if (GUEST_HEARTBEAT_VOTE_RESULT_REJECT != _vote_result)
                update = true;
            break;

        default:
            update = false;
            break;
    }

    if (update)
    {
        _action_event = event;
        _action_notify = notify;
        _vote_result = vote_result;
        snprintf(_action_log_msg, sizeof(_action_log_msg), "%s", log_msg);
    }

    if (!_wait_script)
    {
        error = guest_heartbeat_msg_send_action_response(
                    _action_invocation_id, _action_event, _action_notify,
                    _vote_result, _action_log_msg);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to send action response for event %s, "
                     "notification=%s, error=%s.",
                     guest_heartbeat_event_str(_action_event),
                     guest_heartbeat_notify_str(_action_notify),
                     guest_error_str(error));
        }

        if (GUEST_TIMER_ID_INVALID != _action_timeout_timer_id)
        {
            error = guest_timer_deregister(_action_timeout_timer_id);
            if (GUEST_OKAY != error)
            {
                DPRINTFE("Failed to cancel action script timeout timer, "
                         "error=%s.", guest_error_str(error));
            }
            _action_timeout_timer_id = GUEST_TIMER_ID_INVALID;
        }
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Enabled State - Action Script Callback
// ======================================================
static void guest_heartbeat_enabled_state_action_script_callback(
        GuestHeartbeatEventT event, GuestHeartbeatNotifyT notify,
        GuestHeartbeatVoteResultT vote_result, char* log_msg )
{
    bool update;
    GuestErrorT error;

    DPRINTFI("Received event script response, event=%s, notify=%s, "
             "vote-result=%s, msg=%s.", guest_heartbeat_event_str(event),
             guest_heartbeat_notify_str(notify),
             guest_heartbeat_vote_result_str(vote_result), log_msg);

    _wait_script = false;
    update = false;

    switch (vote_result)
    {
        case GUEST_HEARTBEAT_VOTE_RESULT_REJECT:
            update = true;
            break;

        case GUEST_HEARTBEAT_VOTE_RESULT_ACCEPT:
        case GUEST_HEARTBEAT_VOTE_RESULT_COMPLETE:
            if (GUEST_HEARTBEAT_VOTE_RESULT_REJECT != _vote_result)
                update = true;
            break;

        default:
            update = false;
            break;
    }

    if (update)
    {
        _action_event = event;
        _action_notify = notify;
        _vote_result = vote_result;
        snprintf(_action_log_msg, sizeof(_action_log_msg), "%s", log_msg);
    }

    if (!_wait_application)
    {
        error = guest_heartbeat_msg_send_action_response(
                    _action_invocation_id, _action_event, _action_notify,
                    _vote_result, _action_log_msg);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to send action response for event %s, "
                     "notification=%s, error=%s.",
                     guest_heartbeat_event_str(_action_event),
                     guest_heartbeat_notify_str(_action_notify),
                     guest_error_str(error));
        }

        if (GUEST_TIMER_ID_INVALID != _action_timeout_timer_id)
        {
            error = guest_timer_deregister(_action_timeout_timer_id);
            if (GUEST_OKAY != error)
            {
                DPRINTFE("Failed to cancel action script timeout timer, "
                         "error=%s.", guest_error_str(error));
            }
            _action_timeout_timer_id = GUEST_TIMER_ID_INVALID;
        }
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Enabled State - Enter
// =====================================
GuestErrorT guest_heartbeat_enabled_state_enter( void )
{
    int heartbeat_timeout;
    GuestHeartbeatConfigT* config = guest_heartbeat_config_get();
    GuestErrorT error;

    _health = true;
    _last_health_reported = true;

    heartbeat_timeout = config->heartbeat_interval_ms*2;
    if (heartbeat_timeout < config->heartbeat_min_timeout_ms)
        heartbeat_timeout = config->heartbeat_min_timeout_ms;

    error = guest_timer_register(heartbeat_timeout,
                                 guest_heartbeat_enabled_state_challenge_timeout,
                                 &_challenge_timeout_timer_id);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to start challenge timeout timer, error=%s.",
                 guest_error_str(error));
        return error;
    }

    if ((0 != config->health_check_interval_ms) &&
        ('\0' != config->health_check_script[0]))
    {
        error = guest_timer_register(config->health_check_interval_ms,
                                     guest_heartbeat_enabled_state_health_check,
                                     &_health_check_timer_id);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to start health check timer, error=%s.",
                     guest_error_str(error));
            return error;
        }
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Enabled State - Exit
// ====================================
GuestErrorT guest_heartbeat_enabled_state_exit( void )
{
    GuestErrorT error;

    if (GUEST_TIMER_ID_INVALID != _challenge_timeout_timer_id)
    {
        error = guest_timer_deregister(_challenge_timeout_timer_id);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to cancel challenge timeout timer, error=%s.",
                     guest_error_str(error));
        }
        _challenge_timeout_timer_id = GUEST_TIMER_ID_INVALID;
    }

    if (GUEST_TIMER_ID_INVALID != _health_check_timer_id)
    {
        error = guest_timer_deregister(_health_check_timer_id);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to cancel health check timer, error=%s.",
                     guest_error_str(error));
        }
        _health_check_timer_id = GUEST_TIMER_ID_INVALID;
    }

    if (GUEST_TIMER_ID_INVALID != _action_timeout_timer_id)
    {
        error = guest_timer_deregister(_action_timeout_timer_id);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to cancel action timeout timer, error=%s.",
                     guest_error_str(error));
        }
        _action_timeout_timer_id = GUEST_TIMER_ID_INVALID;
    }

    guest_heartbeat_health_script_abort();
    guest_heartbeat_event_script_abort();
    guest_heartbeat_mgmt_api_action_abort();
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Enabled State - Transition
// ==========================================
GuestErrorT guest_heartbeat_enabled_state_transition(
        GuestHeartbeatFsmStateT from_state )
{
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Enabled State - Event Handler
// =============================================
GuestErrorT guest_heartbeat_enabled_state_event_handler(
        GuestHeartbeatFsmEventT event, void* event_data[] )
{
    bool health = true;
    GuestHeartbeatActionT corrective_action = GUEST_HEARTBEAT_ACTION_NONE;
    char log_msg[GUEST_HEARTBEAT_MAX_LOG_MSG_SIZE];
    GuestHeartbeatConfigT* config = guest_heartbeat_config_get();
    GuestErrorT error;

    switch (event) {
        case GUEST_HEARTBEAT_FSM_RELEASE:
        case GUEST_HEARTBEAT_FSM_INIT_ACK:
        case GUEST_HEARTBEAT_FSM_CHANNEL_UP:
            // Ignore.
            break;

        case GUEST_HEARTBEAT_FSM_CHANNEL_DOWN:
            guest_heartbeat_fsm_set_state(GUEST_HEARTBEAT_FSM_DISABLED_STATE);
            break;

        case GUEST_HEARTBEAT_FSM_CHALLENGE:
            guest_timer_reset(_challenge_timeout_timer_id);

            error = guest_heartbeat_mgmt_api_get_health(&health,
                        &corrective_action, log_msg,
                        GUEST_HEARTBEAT_MAX_LOG_MSG_SIZE);
            if (GUEST_OKAY != error)
            {
                DPRINTFE("Failed to get application health, error=%s.",
                         guest_error_str(error));
            }

            if (health)
            {
                // Applications are healthy, use the last health script status.
                health = _health;
                corrective_action = config->corrective_action;
                snprintf(log_msg, GUEST_HEARTBEAT_MAX_LOG_MSG_SIZE, "%s",
                         _health_log_msg);

            } else if (!_health) {
                // Applications are not healthy and the health script status
                // is not healthy, need to merge the corrective action taken.
                corrective_action = guest_heartbeat_merge_action(
                                        corrective_action,
                                        config->corrective_action);

                if (corrective_action == config->corrective_action)
                {
                    snprintf(log_msg, GUEST_HEARTBEAT_MAX_LOG_MSG_SIZE, "%s",
                             _health_log_msg);
                }
            }

            if (!health)
            {
                if ((health == _last_health_reported) &&
                    (corrective_action == _last_corrective_action_reported) &&
                    (60000 > guest_time_get_elapsed_ms(&_last_time_reported)))
                {
                    DPRINTFD("Unhealthy, already reported corrective action "
                             "%s, setting corrective action to none.",
                             guest_heartbeat_action_str(corrective_action));

                    // Don't keep asking for a corrective action to be taken
                    // over and over again at the heartbeat interval if it has
                    // already been reported.
                    corrective_action = GUEST_HEARTBEAT_ACTION_NONE;

                } else {
                    _last_health_reported = health;
                    _last_corrective_action_reported = corrective_action;
                    memset(&_last_time_reported, 0, sizeof(_last_time_reported));

                    DPRINTFI("Unhealthy, reporting corrective action %s.",
                             guest_heartbeat_action_str(corrective_action));
                }
            } else {
                _last_health_reported = true;
                _last_corrective_action_reported = GUEST_HEARTBEAT_ACTION_NONE;
                memset(&_last_time_reported, 0, sizeof(_last_time_reported));
            }

            error = guest_heartbeat_msg_send_challenge_response(health,
                        corrective_action, log_msg);
            if (GUEST_OKAY == error)
            {
                if (GUEST_HEARTBEAT_ACTION_NONE != corrective_action)
                {
                    guest_time_get(&_last_time_reported);
                }
            } else {
                DPRINTFE("Failed to send challenge response, error=%s.",
                         guest_error_str(error));
                return GUEST_OKAY;
            }
            break;

        case GUEST_HEARTBEAT_FSM_CHALLENGE_TIMEOUT:
            guest_heartbeat_fsm_set_state(GUEST_HEARTBEAT_FSM_ENABLING_STATE);
            break;

        case GUEST_HEARTBEAT_FSM_ACTION:
            guest_heartbeat_mgmt_api_action_abort();
            guest_heartbeat_event_script_abort();

            _wait_application = false;
            _wait_script = false;
            _action_invocation_id = *(int*) event_data[0];
            _action_event = *(GuestHeartbeatEventT*) event_data[1];
            _action_notify = *(GuestHeartbeatNotifyT*) event_data[2];
            _action_timeout_ms = *(int*) event_data[3];
            _vote_result = GUEST_HEARTBEAT_VOTE_RESULT_UNKNOWN;

            error = guest_heartbeat_mgmt_api_action_notify(
                    _action_event, _action_notify, &_wait_application,
                    guest_heartbeat_enabled_state_action_app_callback);
            if (GUEST_OKAY != error)
            {
                DPRINTFE("Failed to notify applications for event %s, "
                         "notification=%s.",
                         guest_heartbeat_event_str(_action_event),
                         guest_heartbeat_notify_str(_action_notify));
            }

            if ('\0' != config->event_handling_script[0])
            {
                DPRINTFI("Invoke event script %s for event %s, "
                         "notification=%s.", config->event_handling_script,
                         guest_heartbeat_event_str(_action_event),
                         guest_heartbeat_notify_str(_action_notify));

                error = guest_heartbeat_event_script_invoke(
                            config->event_handling_script,
                            _action_event, _action_notify,
                            guest_heartbeat_enabled_state_action_script_callback);
                if (GUEST_OKAY == error)
                {
                    _wait_script = true;

                } else {
                    DPRINTFE("Failed to invoke event script %s for event %s, "
                             "notification=%s.", config->event_handling_script,
                             guest_heartbeat_event_str(_action_event),
                             guest_heartbeat_notify_str(_action_notify));
                }
            }

            if (_wait_application || _wait_script)
            {
                error = guest_timer_register(_action_timeout_ms,
                            guest_heartbeat_enabled_state_action_timeout,
                            &_action_timeout_timer_id);
                if (GUEST_OKAY != error)
                {
                    DPRINTFE("Failed to start action timeout timer, error=%s.",
                             guest_error_str(error));
                    guest_heartbeat_mgmt_api_action_abort();
                    guest_heartbeat_event_script_abort();
                    return GUEST_OKAY;
                }
            } else {
                error = guest_heartbeat_msg_send_action_response(
                            _action_invocation_id, _action_event,
                            _action_notify, GUEST_HEARTBEAT_VOTE_RESULT_COMPLETE,
                            "");
                if (GUEST_OKAY != error)
                {
                    DPRINTFE("Failed to send action response for event %s, "
                             "notification=%s, error=%s.",
                             guest_heartbeat_event_str(_action_event),
                             guest_heartbeat_notify_str(_action_notify),
                             guest_error_str(error));
                }
            }
            break;

        case GUEST_HEARTBEAT_FSM_SHUTDOWN:
            error = guest_heartbeat_msg_send_exit("Exiting...");
            if (GUEST_OKAY != error)
            {
                DPRINTFE("Failed to send exit, error=%s.",
                         guest_error_str(error));
            }

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
// Guest Heartbeat Enabled State - Initialize
// ==========================================
GuestErrorT guest_heartbeat_enabled_state_initialize( void )
{
    _health = true;
    _health_check_timer_id = GUEST_TIMER_ID_INVALID;
    _challenge_timeout_timer_id = GUEST_TIMER_ID_INVALID;
    _action_timeout_timer_id = GUEST_TIMER_ID_INVALID;
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Enabled State - Finalize
// ========================================
GuestErrorT guest_heartbeat_enabled_state_finalize( void )
{
    GuestErrorT error;

    if (GUEST_TIMER_ID_INVALID != _challenge_timeout_timer_id)
    {
        error = guest_timer_deregister(_challenge_timeout_timer_id);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to cancel challenge timeout timer, error=%s.",
                     guest_error_str(error));
        }
        _challenge_timeout_timer_id = GUEST_TIMER_ID_INVALID;
    }

    if (GUEST_TIMER_ID_INVALID != _health_check_timer_id)
    {
        error = guest_timer_deregister(_health_check_timer_id);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to cancel health check timer, error=%s.",
                     guest_error_str(error));
        }
        _health_check_timer_id = GUEST_TIMER_ID_INVALID;
    }

    if (GUEST_TIMER_ID_INVALID != _action_timeout_timer_id)
    {
        error = guest_timer_deregister(_action_timeout_timer_id);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to cancel action timeout timer, error=%s.",
                     guest_error_str(error));
        }
        _action_timeout_timer_id = GUEST_TIMER_ID_INVALID;
    }

    _health = false;

    return GUEST_OKAY;
}
// ****************************************************************************

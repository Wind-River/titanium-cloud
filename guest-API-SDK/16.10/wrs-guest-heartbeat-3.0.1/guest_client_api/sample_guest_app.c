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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include "guest_api_types.h"
#include "guest_api_debug.h"
#include "guest_heartbeat_api.h"

static int _heartbeat_selobj = -1;
static bool _heartbeat_registered = false;
static bool _heartbeat_registering = false;
static char _application_name[40] = "sample-guest-app";
static GuestHeartbeatApiActionT _corrective_action;

static sig_atomic_t _stay_on = 1;

// ****************************************************************************
// Guest Application - Register State Callback
// ===========================================
static void guest_app_register_state_callback( bool state )
{
    if (state)
        _heartbeat_registering = false;

    _heartbeat_registered = state;
}
// ****************************************************************************

// ****************************************************************************
// Guest Application - Health Check Callback
// =========================================
static void guest_app_health_check_callback(
        bool* healthy, GuestHeartbeatApiActionT* corrective_action,
        char log_msg[GUEST_HEARTBEAT_API_LOG_MAX] )
{
    char filename[80];
    int result;

    snprintf(filename, sizeof(filename), "/tmp/%s_unhealthy",
             _application_name);

    result = access(filename, F_OK);
    if (0 == result)
    {
        *healthy = false;
        *corrective_action = _corrective_action;
        snprintf(log_msg, GUEST_HEARTBEAT_API_LOG_MAX, "File %s exists.",
                 filename);
    } else {
        *healthy = true;
        *corrective_action = GUEST_HEARTBEAT_API_ACTION_NONE;
        log_msg[0] = '\0';
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Application - Action Notify Callback
// ==========================================
static void guest_app_action_notify_callback(
        int invocation_id, GuestHeartbeatApiEventT event,
        GuestHeartbeatApiNotifyTypeT notify_type )
{
    char filename[80];
    GuestHeartbeatApiVoteResultT vote_result;
    char log_msg[GUEST_HEARTBEAT_API_LOG_MAX];
    GuestApiErrorT error;
    int result;

    snprintf(filename, sizeof(filename), "/tmp/%s_event_timeout",
             _application_name);

    result = access(filename, F_OK);
    if (0 == result)
        return;

    if (GUEST_HEARTBEAT_API_NOTIFY_TYPE_REVOCABLE == notify_type)
    {
        switch (event)
        {
            case GUEST_HEARTBEAT_API_EVENT_STOP:
                snprintf(filename, sizeof(filename),
                         "/tmp/%s_vote_no_to_stop", _application_name);
                break;

            case GUEST_HEARTBEAT_API_EVENT_REBOOT:
                snprintf(filename, sizeof(filename),
                         "/tmp/%s_vote_no_to_reboot", _application_name);
                break;
            case GUEST_HEARTBEAT_API_EVENT_SUSPEND:
            case GUEST_HEARTBEAT_API_EVENT_PAUSE:
                snprintf(filename, sizeof(filename),
                         "/tmp/%s_vote_no_to_suspend",  _application_name);
                break;
            case GUEST_HEARTBEAT_API_EVENT_RESIZE_BEGIN:
                snprintf(filename, sizeof(filename),
                         "/tmp/%s_vote_no_to_resize", _application_name);
                break;
            case GUEST_HEARTBEAT_API_EVENT_LIVE_MIGRATE_BEGIN:
            case GUEST_HEARTBEAT_API_EVENT_COLD_MIGRATE_BEGIN:
                snprintf(filename, sizeof(filename),
                         "/tmp/%s_vote_no_to_migrate", _application_name);
                break;
            default:
                DPRINTFE("Should never be asked to vote on event %s.",
                         guest_heartbeat_api_event_str(event));
                return;
        }

        result = access(filename, F_OK);
        if (0 == result)
        {
            vote_result = GUEST_HEARTBEAT_API_VOTE_RESULT_REJECT;
            snprintf(log_msg, GUEST_HEARTBEAT_API_LOG_MAX, "File %s exists.",
                     filename);
        } else {
            vote_result = GUEST_HEARTBEAT_API_VOTE_RESULT_ACCEPT;
            log_msg[0] = '\0';
        }
    } else {
        vote_result = GUEST_HEARTBEAT_API_VOTE_RESULT_COMPLETE;
    }

    error = guest_heartbeat_api_send_action_response(invocation_id, event,
                notify_type, vote_result, log_msg);
    if (GUEST_API_OKAY != error)
    {
        DPRINTFE("Failed to send action response, error=%s.",
                 guest_api_error_str(error));
        return;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Application - Signal Handler
// ==================================
static void guest_app_signal_handler( int signum )
{
    switch (signum)
    {
        case SIGINT:
        case SIGTERM:
        case SIGQUIT:
            _stay_on = 0;
            break;

        case SIGCONT:
            DPRINTFD("Ignoring signal SIGCONT (%i).", signum);
            break;

        case SIGPIPE:
            DPRINTFD("Ignoring signal SIGPIPE (%i).", signum);
            break;

        default:
            DPRINTFD("Signal (%i) ignored.", signum);
            break;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Application - Main
// ========================
int main( int argc, char *argv[], char *envp[] )
{
    GuestHeartbeatApiCallbacksT callbacks;
    GuestApiErrorT error;

    error = guest_api_debug_initialize("Guest-Application");
    if (GUEST_API_OKAY != error)
    {
        printf("Debug initialization failed, error=%s.\n",
               guest_api_error_str(error));
        return EXIT_FAILURE;
    }

    DPRINTFI("Starting.");

    signal(SIGINT,  guest_app_signal_handler);
    signal(SIGTERM, guest_app_signal_handler);
    signal(SIGQUIT, guest_app_signal_handler);
    signal(SIGCONT, guest_app_signal_handler);
    signal(SIGPIPE, guest_app_signal_handler);

    _corrective_action = GUEST_HEARTBEAT_API_ACTION_REBOOT;

    unsigned int arg_i;
    for (arg_i=1; arg_i < argc; ++arg_i)
    {
        if (0 == strcmp("--name", argv[arg_i]))
        {
            arg_i++;
            if (arg_i < argc)
                snprintf(_application_name, sizeof(_application_name), "%s",
                         argv[arg_i]);

        } else if (0 == strcmp("--corrective-action", argv[arg_i])) {
            arg_i++;
            if (arg_i < argc)
            {
                if (0 == strcmp("reboot", argv[arg_i]))
                {
                    _corrective_action = GUEST_HEARTBEAT_API_ACTION_REBOOT;

                } else if (0 == strcmp("stop", argv[arg_i])) {
                    _corrective_action = GUEST_HEARTBEAT_API_ACTION_STOP;

                } else if (0 == strcmp("log", argv[arg_i])) {
                    _corrective_action = GUEST_HEARTBEAT_API_ACTION_LOG;
                }
            }
        }
    }

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.register_state = guest_app_register_state_callback;
    callbacks.health_check = guest_app_health_check_callback;
    callbacks.action_notify = guest_app_action_notify_callback;

    error = guest_heartbeat_api_initialize(&callbacks);
    if (GUEST_API_OKAY != error)
    {
        DPRINTFE("Failed to initialize guest heartbeat api, error=%s.",
                 guest_api_error_str(error));
        return EXIT_FAILURE;
    }

    DPRINTFI("Started.");

    while (_stay_on)
    {
        int num_fds;
        fd_set fds;
        struct timeval tv;
        int result;

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        num_fds = 0;
        FD_ZERO(&fds);

        if (!_heartbeat_registered)
        {
            GuestHeartbeatApiInitDataT init_data;

            snprintf(init_data.application_name,
                     sizeof(init_data.application_name), "%s",
                     _application_name);
            init_data.heartbeat_interval_ms = 1000;
            init_data.vote_secs = 8;
            init_data.shutdown_notice_secs = 5;
            init_data.suspend_notice_secs = 5;
            init_data.resume_notice_secs = 5;
            init_data.corrective_action = _corrective_action;

            error = guest_heartbeat_api_register(&init_data);
            if (GUEST_API_OKAY == error)
            {
                _heartbeat_registering = true;
            } else {
                _heartbeat_registering = false;

                if (GUEST_API_TRY_AGAIN != error)
                {
                    DPRINTFE("Failed to register for guest heartbeating, "
                             "error=%s.", guest_api_error_str(error));
                    return EXIT_FAILURE;
                }
            }
        }

        if (_heartbeat_registering || _heartbeat_registered)
        {
            _heartbeat_selobj = guest_heartbeat_api_get_selobj();
            FD_SET(_heartbeat_selobj, &fds);
            num_fds = _heartbeat_selobj;
        }

        result = select(num_fds+1, &fds, NULL, NULL, &tv);
        if (0 > result)
        {
            if (errno == EINTR)
            {
                DPRINTFD("Interrupted by a signal.");
            } else {
                DPRINTFE("Select failed, error=%s.", strerror(errno));
            }
        } else if (0 == result) {
            DPRINTFD("Nothing selected.");
        } else {
            if (FD_ISSET(_heartbeat_selobj, &fds))
            {
                guest_heartbeat_api_dispatch(_heartbeat_selobj);
            }
        }
    }

    DPRINTFI("Shutting down.");

    error = guest_heartbeat_api_deregister("Exiting");
    if (GUEST_API_OKAY != error)
    {
        DPRINTFE("Failed to deregister from guest heartbeat api, error=%s.",
                 guest_api_error_str(error));
    }

    error = guest_heartbeat_api_finalize();
    if (GUEST_API_OKAY != error)
    {
        DPRINTFE("Failed to finalize guest heartbeat api, error=%s.",
                 guest_api_error_str(error));
    }

    DPRINTFI("Shutdown complete.");

    error = guest_api_debug_finalize();
    if (GUEST_API_OKAY != error)
    {
        printf("Debug finalization failed, error=%s.\n",
               guest_api_error_str(error));
    }

    return EXIT_SUCCESS;
}
// ****************************************************************************

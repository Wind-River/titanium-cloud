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
#include "guest_process.h"

#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "guest_limits.h"
#include "guest_types.h"
#include "guest_debug.h"
#include "guest_signal.h"
#include "guest_config.h"
#include "guest_selobj.h"
#include "guest_timer.h"
#include "guest_channel.h"
#include "guest_stream.h"
#include "guest_unix.h"
#include "guest_script.h"
#include "guest_heartbeat.h"
#include "guest_child_death.h"

static sig_atomic_t _stay_on = 1;
static sig_atomic_t _reload = 0;
static sig_atomic_t _reap_children = 0;

// ****************************************************************************
// Guest Process - Reload
// ======================
static void guest_process_reload( void )
{
    int result;

    DPRINTFI("Reload signal handled.");
    _reload = 0;

    result = access("/tmp/guest_debug_debug", F_OK);
    if (0 == result)
    {
        DPRINTFI("Debug log level set to debug.");
        guest_debug_set_log_level(GUEST_DEBUG_LOG_LEVEL_DEBUG);
        return;
    }

    result = access("/tmp/guest_debug_verbose", F_OK);
    if (0 == result)
    {
        DPRINTFI("Debug log level set to verbose.");
        guest_debug_set_log_level(GUEST_DEBUG_LOG_LEVEL_VERBOSE);
        return;
    }

    DPRINTFI("Debug log level set to info.");
    guest_debug_set_log_level(GUEST_DEBUG_LOG_LEVEL_INFO);
}
// ****************************************************************************

// ****************************************************************************
// Guest Process - Reap Children
// =============================
static void guest_process_reap_children( void )
{
    pid_t pid;
    int status;

    if (_reap_children)
    {
        _reap_children = 0;

        while (0 < (pid = waitpid(-1, &status, WNOHANG | WUNTRACED)))
        {
            if (WIFEXITED(status))
                guest_child_death_save(pid, WEXITSTATUS(status));
            else
                guest_child_death_save(pid, GUEST_CHILD_FAILED);
        }
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Process - Signal Handler
// ==============================
static void guest_process_signal_handler( int signum )
{
    switch (signum)
    {
        case SIGINT:
        case SIGTERM:
        case SIGQUIT:
            _stay_on = 0;
            break;

        case SIGHUP:
            _reload = 1;
            break;

        case SIGCHLD:
            _reap_children = 1;
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
// Guest Process - Initialize
// ==========================
static GuestErrorT guest_process_initialize(
        int argc, char *argv[], char *envp[] )
{
    GuestConfigT* config = NULL;
    GuestErrorT error;

    error = guest_config_initialize(argc, argv, envp);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to initialize configuration module, error=%s.",
                 guest_error_str(error));
        guest_config_show_usage();
        return error;
    }

    error = guest_selobj_initialize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to initialize selection object module, error=%s.",
                 guest_error_str(error));
        return GUEST_FAILED;
    }

    error = guest_timer_initialize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to initialize timer module, error=%s.",
                 guest_error_str(error));
        return GUEST_FAILED;
    }

    error = guest_child_death_initialize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to initialize child death module, error=%s.",
                 guest_error_str(error));
        return GUEST_FAILED;
    }

    error = guest_unix_initialize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to initialize unix module, error=%s.",
                 guest_error_str(error));
        return GUEST_FAILED;
    }

    error = guest_channel_initialize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to initialize channel module, error=%s.",
                 guest_error_str(error));
        return GUEST_FAILED;
    }

    error = guest_stream_initialize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to initialize stream module, error=%s.",
                 guest_error_str(error));
        return GUEST_FAILED;
    }

    error = guest_script_initialize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to initialize script module, error=%s.",
                 guest_error_str(error));
        return GUEST_FAILED;
    }

    config = guest_config_get();

    error = guest_heartbeat_initialize(config->comm_device);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to initialize heartbeat module, error=%s.",
                 guest_error_str(error));
        return error;
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Process - Finalize
// ========================
static GuestErrorT guest_process_finalize( void )
{
    GuestErrorT error;

    error = guest_heartbeat_finalize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to finalize heartbeat module, error=%s.",
                 guest_error_str(error));
    }

    error = guest_script_finalize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to finalize script module, error=%s.",
                 guest_error_str(error));
    }

    error = guest_stream_finalize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to finalize stream module, error=%s.",
                 guest_error_str(error));
    }

    error = guest_channel_finalize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to finalize channel module, error=%s.",
                 guest_error_str(error));
    }

    error = guest_unix_finalize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to finalize unix module, error=%s.",
                 guest_error_str(error));
    }

    error = guest_child_death_finalize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to finalize child death module, error=%s.",
                 guest_error_str(error));
    }

    error = guest_timer_finalize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to finalize timer module, error=%s.",
                 guest_error_str(error));
    }

    error = guest_selobj_finalize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to finialize selection object module, error=%s.",
                 guest_error_str(error));
    }

    error = guest_config_finalize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to finialize configuration module, error=%s.",
                 guest_error_str(error));
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Process - Main
// ====================
GuestErrorT guest_process_main( int argc, char *argv[], char *envp[] )
{
    unsigned int next_interval_in_ms;
    GuestErrorT error;

    DPRINTFI("Starting.");

    guest_signal_register_handler(SIGINT,  guest_process_signal_handler);
    guest_signal_register_handler(SIGTERM, guest_process_signal_handler);
    guest_signal_register_handler(SIGQUIT, guest_process_signal_handler);
    guest_signal_register_handler(SIGHUP,  guest_process_signal_handler);
    guest_signal_register_handler(SIGCHLD, guest_process_signal_handler);
    guest_signal_register_handler(SIGCONT, guest_process_signal_handler);
    guest_signal_register_handler(SIGPIPE, guest_process_signal_handler);
    guest_signal_ignore(SIGIO);

    error = guest_process_initialize(argc, argv, envp);
    if (GUEST_OKAY != error)
    {
        if (error != GUEST_NOT_CONFIGURED)
        {
           DPRINTFE("Failed initialize process restarting in 20 seconds,"
                 "error=%s.", guest_error_str(error));
           sleep(20);
        }
        else {
           DPRINTFI("Application is not configured, will be not restarted,"
                 " exit code=%s", guest_error_str(error));
        }
        return error;
    }

    DPRINTFI("Started.");

    while (_stay_on)
    {
        next_interval_in_ms = guest_timer_schedule();

        error = guest_selobj_dispatch(next_interval_in_ms);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Selection object dispatch failed, error=%s.",
                     guest_error_str(error));
            break;
        }

        guest_process_reap_children();

        if (_reload)
            guest_process_reload();
    }

    DPRINTFI("Shutting down.");

    error = guest_process_finalize();
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed finalize process, error=%s.",
                 guest_error_str(error) );
    }

    DPRINTFI("Shutdown complete.");

    return GUEST_OKAY;
}
// ****************************************************************************

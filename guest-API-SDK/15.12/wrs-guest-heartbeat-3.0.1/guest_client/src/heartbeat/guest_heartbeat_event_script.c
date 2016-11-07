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
#include "guest_heartbeat_event_script.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "guest_types.h"
#include "guest_debug.h"
#include "guest_script.h"

#include "guest_heartbeat_types.h"

static GuestScriptIdT _script_id = GUEST_SCRIPT_ID_INVALID;
static GuestHeartbeatEventT _event;
static GuestHeartbeatNotifyT _notify;
static GuestHeartbeatEventScriptCallbackT _callback = NULL;

// ****************************************************************************
// Guest Heartbeat Event Script - Event Argument
// =============================================
const char* guest_heartbeat_event_script_event_arg( GuestHeartbeatEventT event )
{
    switch (event)
    {
        case GUEST_HEARTBEAT_EVENT_STOP:               return "stop";
        case GUEST_HEARTBEAT_EVENT_REBOOT:             return "reboot";
        case GUEST_HEARTBEAT_EVENT_SUSPEND:            return "suspend";
        case GUEST_HEARTBEAT_EVENT_PAUSE:              return "pause";
        case GUEST_HEARTBEAT_EVENT_UNPAUSE:            return "unpause";
        case GUEST_HEARTBEAT_EVENT_RESUME:             return "resume";
        case GUEST_HEARTBEAT_EVENT_RESIZE_BEGIN:       return "resize_begin";
        case GUEST_HEARTBEAT_EVENT_RESIZE_END:         return "resize_end";
        case GUEST_HEARTBEAT_EVENT_LIVE_MIGRATE_BEGIN: return "live_migrate_begin";
        case GUEST_HEARTBEAT_EVENT_LIVE_MIGRATE_END:   return "live_migrate_end";
        case GUEST_HEARTBEAT_EVENT_COLD_MIGRATE_BEGIN: return "cold_migrate_begin";
        case GUEST_HEARTBEAT_EVENT_COLD_MIGRATE_END:   return "cold_migrate_end";
        default:
            return NULL;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Event Script - Notify Argument
// ==============================================
const char* guest_heartbeat_event_script_notify_arg( GuestHeartbeatNotifyT notify )
{
    switch (notify)
    {
        case GUEST_HEARTBEAT_NOTIFY_REVOCABLE:   return "revocable";
        case GUEST_HEARTBEAT_NOTIFY_IRREVOCABLE: return "irrevocable";
        default:
            return NULL;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Event Script - Abort
// ====================================
void guest_heartbeat_event_script_abort( void )
{
    if (GUEST_SCRIPT_ID_INVALID != _script_id)
    {
        DPRINTFI("Aborting event script for event %s, notification=%s, "
                 "script_id=%i.", guest_heartbeat_event_str(_event),
                 guest_heartbeat_notify_str(_notify), _script_id);
        guest_script_abort(_script_id);
        _script_id = GUEST_SCRIPT_ID_INVALID;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Event Script - Callback
// =======================================
static void guest_heartbeat_event_script_callback(
        GuestScriptIdT script_id, int exit_code, char* log_msg )
{
    GuestHeartbeatVoteResultT vote_result;

    if (script_id == _script_id)
    {
        switch (exit_code)
        {
            case 0:
                vote_result = GUEST_HEARTBEAT_VOTE_RESULT_ACCEPT;
                break;
            case 1:
                vote_result = GUEST_HEARTBEAT_VOTE_RESULT_REJECT;
                break;
            default:
                vote_result = GUEST_HEARTBEAT_VOTE_RESULT_ERROR;
                break;
        }

        if (NULL != _callback)
            _callback(_event, _notify, vote_result, log_msg);

        _script_id = GUEST_SCRIPT_ID_INVALID;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Event Script - Invoke
// =====================================
GuestErrorT guest_heartbeat_event_script_invoke(
        char script[], GuestHeartbeatEventT event, GuestHeartbeatNotifyT notify,
        GuestHeartbeatEventScriptCallbackT callback)
{
    const char* event_arg = guest_heartbeat_event_script_event_arg(event);
    const char* notify_arg = guest_heartbeat_event_script_notify_arg(notify);
    const char* script_argv[] = {script, notify_arg, event_arg, NULL};
    GuestErrorT error;

    _event = event;
    _notify = notify;
    _callback = callback;

    if (NULL == event_arg)
    {
        DPRINTFE("Event argument invalid, event=%s.",
                 guest_heartbeat_event_str(event));
        return GUEST_FAILED;
    }

    if (NULL == notify_arg)
    {
        DPRINTFE("Notify argument invalid, event=%s.",
                 guest_heartbeat_notify_str(notify));
        return GUEST_FAILED;
    }

    error = guest_script_invoke(script, (char**) script_argv,
                                guest_heartbeat_event_script_callback,
                                &_script_id);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to invoke script %s, error=%s.", script,
                 guest_error_str(error));
        return error;
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Event Script - Initialize
// =========================================
GuestErrorT guest_heartbeat_event_script_initialize( void )
{
    _script_id = GUEST_SCRIPT_ID_INVALID;
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Event Script - Finalize
// =======================================
GuestErrorT guest_heartbeat_event_script_finalize( void )
{
    guest_heartbeat_event_script_abort();
    _script_id = GUEST_SCRIPT_ID_INVALID;
    return GUEST_OKAY;
}
// ****************************************************************************

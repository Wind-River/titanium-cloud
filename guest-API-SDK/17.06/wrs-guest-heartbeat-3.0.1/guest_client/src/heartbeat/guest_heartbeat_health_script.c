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
#include "guest_heartbeat_health_script.h"

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

static GuestScriptIdT _script_id = GUEST_SCRIPT_ID_INVALID;
static GuestHeartbeatHealthScriptCallbackT _callback = NULL;

// ****************************************************************************
// Guest Heartbeat Health Script - Abort
// =====================================
void guest_heartbeat_health_script_abort( void )
{
    if (GUEST_SCRIPT_ID_INVALID != _script_id)
    {
        DPRINTFI("Aborting health script, script_id=%i.", _script_id);
        guest_script_abort(_script_id);
        _script_id = GUEST_SCRIPT_ID_INVALID;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Health Script - Callback
// ========================================
static void guest_heartbeat_health_script_callback(
        GuestScriptIdT script_id, int exit_code, char* log_msg )
{
    if (script_id == _script_id)
    {
        if (NULL != _callback)
            _callback((1 != exit_code), log_msg);

        _script_id = GUEST_SCRIPT_ID_INVALID;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Health Script - Invoke
// ======================================
GuestErrorT guest_heartbeat_health_script_invoke(
        char script[], GuestHeartbeatHealthScriptCallbackT callback)
{
    const char* script_argv[] = {script, NULL};
    GuestErrorT error;

    _callback = callback;

    error = guest_script_invoke(script, (char**) script_argv,
                                guest_heartbeat_health_script_callback,
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
// Guest Heartbeat Health Script - Initialize
// ==========================================
GuestErrorT guest_heartbeat_health_script_initialize( void )
{
    _script_id = GUEST_SCRIPT_ID_INVALID;
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Health Script - Finalize
// ========================================
GuestErrorT guest_heartbeat_health_script_finalize( void )
{
    guest_heartbeat_health_script_abort();
    _script_id = GUEST_SCRIPT_ID_INVALID;
    return GUEST_OKAY;
}
// ****************************************************************************

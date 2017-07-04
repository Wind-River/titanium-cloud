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
#include "guest_script.h"

#include <stdbool.h>
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

#include "guest_limits.h"
#include "guest_types.h"
#include "guest_debug.h"
#include "guest_selobj.h"
#include "guest_utils.h"
#include "guest_child_death.h"

#define GUEST_SCRIPT_SETUP_FAILURE      -65535

typedef struct {
    bool inuse;
    int pid;
    int fd;
    int log_end_ptr;
    char log_msg[256];
    GuestScriptIdT script_id;
    GuestScriptCallbackT callback;
} GuestScriptDataT;

static GuestScriptDataT _scripts[GUEST_CHILD_PROCESS_MAX];

// ****************************************************************************
// Guest Script - Abort
// ====================
void guest_script_abort( GuestScriptIdT script_id )
{
    int result;
    GuestScriptDataT* entry;
    GuestErrorT error;

    if (GUEST_SCRIPT_ID_INVALID == script_id)
        return;

    if (GUEST_CHILD_PROCESS_MAX <= script_id)
        return;

    entry = &(_scripts[script_id]);

    if (entry->inuse)
    {
        if (-1 != entry->pid)
        {
            error = guest_child_death_deregister(entry->pid);
            if (GUEST_OKAY != error)
            {
                DPRINTFE("Failed to deregister for child death %i, error=%s.",
                         entry->pid, guest_error_str(error));
            }

            result = kill(entry->pid, SIGKILL);
            if (0 > result)
            {
                if (ESRCH == errno)
                {
                    DPRINTFV("Script pid (%i) not running.", entry->pid);
                } else {
                    DPRINTFE("Failed to send kill signal to script pid %i, "
                             "error=%s.", entry->pid, strerror(errno));
                }
            } else {
                DPRINTFD("Script pid (%i) killed.", entry->pid);
            }
        }

        if (-1 != entry->fd)
        {
            error = guest_selobj_deregister(entry->fd);
            if (GUEST_OKAY != error)
            {
                DPRINTFE("Failed to deregister selection object %i, error=%s.",
                         entry->fd, guest_error_str(error));
            }

            close(entry->fd);
        }

        memset(entry, 0, sizeof(GuestScriptDataT));
        entry->pid = -1;
        entry->fd = -1;
        entry->script_id = GUEST_SCRIPT_ID_INVALID;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Script - Dispatch
// =======================
static void guest_script_dispatch( int selobj )
{
    int bytes_avail;
    int result;
    GuestScriptDataT* entry;

    unsigned int script_i;
    for (script_i=0; GUEST_CHILD_PROCESS_MAX > script_i; ++script_i)
    {
        entry = &(_scripts[script_i]);
        if (entry->inuse)
            if (selobj == entry->fd)
                break;
     }

    if (GUEST_CHILD_PROCESS_MAX <= script_i)
        return;

    bytes_avail = sizeof(entry->log_msg) - entry->log_end_ptr;

    result = read(selobj, &(entry->log_msg[entry->log_end_ptr]), bytes_avail);
    if (0 > result)
    {
        if (EINTR == errno) {
            DPRINTFD("Interrupted on read, error=%s.", strerror(errno));
            return;

        } else {
            DPRINTFE("Failed to read, error=%s.", strerror(errno));
            return;
        }
    } else if (0 == result) {
        DPRINTFD("No message received.");
        return;

    } else {
        DPRINTFD("Received message, msg_size=%i.", result);
        entry->log_end_ptr += result;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Script - Callback
// =======================
static void guest_script_callback( pid_t pid, int exit_code )
{
    GuestScriptDataT* entry;

    unsigned int script_i;
    for (script_i=0; GUEST_CHILD_PROCESS_MAX > script_i; ++script_i)
    {
        entry = &(_scripts[script_i]);
        if (entry->inuse)
            if ((int) pid == entry->pid)
                break;
    }

    if (GUEST_CHILD_PROCESS_MAX <= script_i)
        return;

    DPRINTFD("PID %i exited with %i", (int) pid, exit_code);

    if (NULL != entry->callback)
        entry->callback(entry->script_id, exit_code, entry->log_msg);

    guest_script_abort(entry->script_id);
}
// ****************************************************************************

// ****************************************************************************
// Guest Script - Invoke
// =====================
GuestErrorT guest_script_invoke(
        char script[], char* script_argv[], GuestScriptCallbackT callback,
        GuestScriptIdT* script_id )
{
    int fd[2];
    pid_t pid;
    struct stat stat_data;
    char* script_name = guest_utils_basename(script);
    char* script_exec = script;
    int result;
    GuestScriptDataT* entry;
    GuestSelObjCallbacksT callbacks;
    GuestErrorT error;

    *script_id = GUEST_SCRIPT_ID_INVALID;

    unsigned int script_i;
    for (script_i=1; GUEST_CHILD_PROCESS_MAX > script_i; ++script_i)
    {
        entry = &(_scripts[script_i]);
        if (!entry->inuse)
            break;
    }

    if (GUEST_CHILD_PROCESS_MAX <= script_i)
    {
        DPRINTFE("Failed to allocate script data.");
        return GUEST_FAILED;
    }

    memset(entry, 0, sizeof(GuestScriptDataT));
    entry->script_id = script_i;
    entry->callback = callback;
    entry->pid = -1;
    entry->fd = -1;

    result = access(script_exec,  F_OK | X_OK);
    if (0 > result)
    {
        DPRINTFE("Script %s access failed, error=%s.", script_exec,
                 strerror(errno));
        return GUEST_FAILED;
    }

    result = stat(script_exec, &stat_data);
    if (0 > result)
    {
        DPRINTFE("Script %s stat failed, error=%s.", script_exec,
                 strerror( errno ) );
        return GUEST_FAILED;
    }

    if (0 >= stat_data.st_size)
    {
        DPRINTFE("Script %s has zero size.", script_exec);
        return GUEST_FAILED;
    }

    result = pipe(fd);
    if (0 > result)
    {
        DPRINTFE("Script %s pipe creation failed, error=%s.", script_exec,
                 strerror(errno));
        return GUEST_FAILED;
    }

    result = fcntl(fd[0], F_SETFL, O_NONBLOCK);
    if (0 > result)
    {
        DPRINTFE("Script %s pipe failed to make read end non-blocking, "
                 "error=%s.", script_exec, strerror(errno));
        close(fd[0]);
        close(fd[1]);
        return GUEST_FAILED;
    }

    pid = fork();
    if (0 > pid)
    {
        DPRINTFE("Failed to fork process for script %s, error=%s.",
                 script_exec, strerror(errno));
        close(fd[0]);
        close(fd[1]);
        return GUEST_FAILED;

    } else if (0 == pid) {
        // Child process.
        struct rlimit file_limits;

        close(fd[0]); // close read end of pipe

        result = setpgid(0, 0);
        if (0 > result)
        {
            DPRINTFE("Failed to set process group id for script %s, "
                     "error=%s.", script_exec, strerror( errno ) );
            exit(GUEST_SCRIPT_SETUP_FAILURE);
        }

        result = getrlimit(RLIMIT_NOFILE, &file_limits);
        if (0 > result)
        {
            DPRINTFE("Failed to get file limits for script %s, error=%s.",
                     script_exec, strerror(errno));
            exit(GUEST_SCRIPT_SETUP_FAILURE);
        }

        unsigned int fd_i;
        for (fd_i=0; fd_i < file_limits.rlim_cur; ++fd_i)
            if (fd_i != fd[1])
                close(fd_i);

        result = dup2(fd[1], 1); // make stdout into writable end of pipe
        if (0 > result)
        {
            DPRINTFE("Failed to make stdout into writable end of pipe for "
                     "script %s, error=%s.", script_exec, strerror(errno));
            exit(GUEST_SCRIPT_SETUP_FAILURE);
        }

        result = execv(script_exec, (char**) script_argv);
        if (0 > result)
            DPRINTFE("Failed to exec command for script %s, error=%s.",
                     script_exec, strerror(errno));

        exit(GUEST_SCRIPT_SETUP_FAILURE);

    } else {
        // Parent process.
        close(fd[1]); // close write end of pipe
        entry->pid = (int) pid;
        entry->fd = fd[0];
        entry->inuse = true;

        DPRINTFD("Child process %i created for script %s, script_id=%i.",
                 entry->pid, script_name, entry->script_id);

        error = guest_child_death_register(pid, guest_script_callback);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to register for child death %i, error=%s.",
                     entry->pid, guest_error_str(error));
            guest_script_abort(entry->script_id);
            return error;
        }

        memset(&callbacks, 0, sizeof(callbacks));
        callbacks.read_callback = guest_script_dispatch;

        error = guest_selobj_register(entry->fd, &callbacks);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to register selection object %i, error=%s.",
                     entry->fd, guest_error_str(error));
            guest_script_abort(entry->script_id);
            return error;
        }

        *script_id = entry->script_id;
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Script - Initialize
// =========================
GuestErrorT guest_script_initialize( void )
{
    GuestScriptDataT* entry;

    memset(_scripts, 0, sizeof(_scripts));

    unsigned int script_i;
    for (script_i=0; GUEST_CHILD_PROCESS_MAX > script_i; ++script_i)
    {
        entry = &(_scripts[script_i]);

        entry->pid = -1;
        entry->fd = -1;
        entry->script_id = GUEST_SCRIPT_ID_INVALID;
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Script - Finalize
// =======================
GuestErrorT guest_script_finalize( void )
{
    GuestScriptDataT* entry;

    unsigned int script_i;
    for (script_i=0; GUEST_CHILD_PROCESS_MAX > script_i; ++script_i)
    {
        entry = &(_scripts[script_i]);
        if (entry->inuse)
            guest_script_abort(entry->script_id);
    }

    memset(_scripts, 0, sizeof(_scripts));
    return GUEST_OKAY;
}
// ****************************************************************************

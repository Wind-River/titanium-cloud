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
#include "guest_child_death.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/eventfd.h>

#include "guest_limits.h"
#include "guest_types.h"
#include "guest_debug.h"
#include "guest_selobj.h"

#define GUEST_CHILD_DEATH_MAX_DISPATCH                  32

typedef struct {
    bool valid;
    pid_t pid;
    int exit_code;
} GuestChildDeathInfoT;

typedef struct {
    bool valid;
    pid_t pid;
    GuestChildDeathCallbackT death_callback;
} GuestChildDeathCallbackInfoT;

static int _child_death_fd = -1;
static GuestChildDeathCallbackInfoT _callbacks[GUEST_CHILD_PROCESS_MAX];
static GuestChildDeathInfoT _child_deaths[GUEST_CHILD_PROCESS_MAX];
static uint64_t _child_death_count = 0;

// ****************************************************************************
// Guest Child Death - Register
// ============================
GuestErrorT guest_child_death_register(
        pid_t pid, GuestChildDeathCallbackT callback )
{
    GuestChildDeathCallbackInfoT* callback_info = NULL;

    unsigned int callbacks_i;
    for (callbacks_i=0; GUEST_CHILD_PROCESS_MAX > callbacks_i; ++callbacks_i)
    {
        callback_info = &(_callbacks[callbacks_i]);

        if (callback_info->valid)
        {
            if (pid == callback_info->pid)
            {
                callback_info->death_callback = callback;
                break;
            }
        } else {
            callback_info->valid = true;
            callback_info->pid = pid;
            callback_info->death_callback = callback;
            break;
        }
    }

    if (GUEST_CHILD_PROCESS_MAX <= callbacks_i)
    {
        DPRINTFE("Failed to register child death callback for pid (%i).",
                 (int) pid);
        return GUEST_FAILED;
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Child Death - Deregister
// ==============================
GuestErrorT guest_child_death_deregister( pid_t pid )
{
    GuestChildDeathCallbackInfoT* callback_info = NULL;

    unsigned int callbacks_i;
    for (callbacks_i=0; GUEST_CHILD_PROCESS_MAX > callbacks_i; ++callbacks_i)
    {
        callback_info = &(_callbacks[callbacks_i]);

        if (!callback_info->valid)
            continue;

        if (pid != callback_info->pid)
            continue;

        callback_info->valid = 0;
        callback_info->pid = 0;
        callback_info->death_callback = NULL;
        break;
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Child Death - Save
// ========================
GuestErrorT guest_child_death_save( pid_t pid, int exit_code )
{
    uint64_t child_death_count = ++_child_death_count;
    GuestChildDeathInfoT* info = NULL;
    int result;

    result = write(_child_death_fd, &child_death_count,
                   sizeof(child_death_count));
    if (0 > result)
        DPRINTFE("Failed to signal child death, error=%s", strerror(errno));

    DPRINTFD("Child process (%i) died.", (int) pid);

    unsigned int death_i;
    for (death_i=0; GUEST_CHILD_PROCESS_MAX > death_i; ++death_i)
    {
        info = &(_child_deaths[death_i]);

        if (info->valid)
        {
            if (pid == info->pid)
            {
                info->exit_code = exit_code;
                break;
            }
        } else {
            info->valid = true;
            info->pid = pid;
            info->exit_code = exit_code;
            break;
        }
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Child Death - Dispatch
// ============================
static void guest_child_death_dispatch( int selobj )
{
    static unsigned int _last_entry = 0;

    uint64_t child_death_count;
    GuestChildDeathInfoT* info = NULL;
    GuestChildDeathCallbackInfoT* callback_info = NULL;
    unsigned int num_child_death_dispatched = 0;
    int result;

    result = read(_child_death_fd, &child_death_count, sizeof(child_death_count));
    if (0 > result)
    {
        if (EINTR == errno)
        {
            DPRINTFD("Interrupted on read, error=%s.", strerror(errno));
        } else {
            DPRINTFE("Failed to dispatch, error=%s.", strerror(errno));
        }
    }

    unsigned int death_i;
    for( death_i=_last_entry; GUEST_CHILD_PROCESS_MAX > death_i; ++death_i )
    {
        info = &(_child_deaths[death_i]);

        if (!info->valid)
            continue;

        if (0 == info->pid)
            continue;

        DPRINTFD("Child process (%i) exited with %i.", (int) info->pid,
                 info->exit_code);

        unsigned int callbacks_i;
        for (callbacks_i=0; GUEST_CHILD_PROCESS_MAX > callbacks_i; ++callbacks_i)
        {
            callback_info = &(_callbacks[callbacks_i]);

            if (callback_info->valid)
            {
                if (info->pid == callback_info->pid)
                {
                    if (NULL != callback_info->death_callback)
                    {
                        callback_info->death_callback(info->pid, info->exit_code);
                        callback_info->valid = false;
                    }
                }
            }
        }

        info->valid = false;

        if (GUEST_CHILD_DEATH_MAX_DISPATCH <= ++num_child_death_dispatched)
            DPRINTFD("Maximum child process death dispatches (%i) reached.",
                     GUEST_CHILD_DEATH_MAX_DISPATCH);
    }

    if (GUEST_CHILD_PROCESS_MAX <= death_i)
        _last_entry = 0;
    else
        _last_entry = death_i;

    // Check for outstanding child process deaths to handle.
    for (death_i=0; GUEST_CHILD_PROCESS_MAX > death_i; ++death_i)
    {
        info = &(_child_deaths[death_i]);

        if (!info->valid)
            continue;

        if (0 == info->pid)
            continue;

        result = write(_child_death_fd, &child_death_count,
                       sizeof(child_death_count));
        if (0 > result)
            DPRINTFE("Failed to signal child process death, error=%s",
                     strerror(errno));
        break;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Child Death - Initialize
// ==============================
GuestErrorT guest_child_death_initialize( void )
{
    GuestSelObjCallbacksT callbacks;
    GuestErrorT error;

    _child_death_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (0 > _child_death_fd)
    {
        DPRINTFE("Failed to open child death file descriptor,error=%s.",
                 strerror(errno));
        return GUEST_FAILED;
    }

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.read_callback = guest_child_death_dispatch;

    error = guest_selobj_register(_child_death_fd, &callbacks);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to register selection object, error=%s.",
                 guest_error_str(error));
        close(_child_death_fd);
        _child_death_fd = -1;
        return error;
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Child Death - Finalize
// ============================
GuestErrorT guest_child_death_finalize( void )
{
    GuestErrorT error;

    if (0 <= _child_death_fd)
    {
        error = guest_selobj_deregister(_child_death_fd);
        if (GUEST_OKAY != error)
            DPRINTFE("Failed to deregister selection object, error=%s.",
                     guest_error_str(error));

        close(_child_death_fd);
        _child_death_fd = -1;
    }

    return GUEST_OKAY;
}
// ****************************************************************************

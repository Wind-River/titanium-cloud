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
#include "guest_selobj.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

#include "guest_limits.h"
#include "guest_types.h"
#include "guest_debug.h"

typedef struct {
    bool inuse;
    int selobj;
    GuestSelObjCallbacksT callbacks;
} GuestSelObjEntryT;

typedef GuestSelObjEntryT GuestSelObjTableT[GUEST_SELECT_OBJS_MAX];

static int _num_poll_fds = 0;
static struct pollfd _poll_fds[GUEST_SELECT_OBJS_MAX];
static GuestSelObjTableT _select_objs;

// ****************************************************************************
// Guest Selection Object - Find Selection Object
// ==============================================
static GuestSelObjEntryT* guest_selobj_find( int selobj )
{
    GuestSelObjEntryT* entry;

    unsigned int entry_i;
    for (entry_i=0; GUEST_SELECT_OBJS_MAX > entry_i; ++entry_i)
    {
        entry = &(_select_objs[entry_i]);
        if (entry->inuse)
            if (selobj == entry->selobj)
                return entry;
    }
    return NULL;
}
// ****************************************************************************

// ****************************************************************************
// Guest Selection Object - Register
// =================================
GuestErrorT guest_selobj_register (
        int selobj, GuestSelObjCallbacksT* callbacks )
{
    GuestSelObjEntryT* entry;

    entry = guest_selobj_find(selobj);
    if (NULL == entry)
    {
        unsigned int entry_i;
        for (entry_i=0; GUEST_SELECT_OBJS_MAX > entry_i; ++entry_i)
        {
            entry = &(_select_objs[entry_i]);
            if (!entry->inuse )
            {
                entry->inuse = true;
                entry->selobj = selobj;
                memcpy(&(entry->callbacks), callbacks,
                       sizeof(GuestSelObjCallbacksT));
                break;
            }
        }

        // Rebuild polling file descriptors.
        _num_poll_fds =0;

        for (entry_i=0; GUEST_SELECT_OBJS_MAX > entry_i; ++entry_i)
        {
            entry = &(_select_objs[entry_i]);
            if (entry->inuse)
            {
                memset(&_poll_fds[_num_poll_fds], 0, sizeof(struct pollfd));

                _poll_fds[_num_poll_fds].fd = entry->selobj;

                if (NULL != entry->callbacks.read_callback)
                    _poll_fds[_num_poll_fds].events |= POLLIN;

                if (NULL != entry->callbacks.write_callback)
                    _poll_fds[_num_poll_fds].events |= POLLOUT;

                if (NULL != entry->callbacks.hangup_callback)
                    _poll_fds[_num_poll_fds].events |= POLLHUP;

                ++_num_poll_fds;
            }
        }
    } else {
        memcpy(&(entry->callbacks), callbacks, sizeof(GuestSelObjCallbacksT));
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Selection Object - Deregister
// ===================================
GuestErrorT guest_selobj_deregister( int selobj )
{
    GuestSelObjEntryT* entry;

    entry = guest_selobj_find(selobj);
    if (NULL != entry)
        memset(entry, 0, sizeof(GuestSelObjEntryT));

    // Rebuild polling file descriptors.
    _num_poll_fds =0;

    unsigned int entry_i;
    for (entry_i=0; GUEST_SELECT_OBJS_MAX > entry_i; ++entry_i)
    {
        entry = &(_select_objs[entry_i]);
        if (entry->inuse)
        {
            memset(&_poll_fds[_num_poll_fds], 0, sizeof(struct pollfd));

            _poll_fds[_num_poll_fds].fd = entry->selobj;

            if (NULL != entry->callbacks.read_callback)
                _poll_fds[_num_poll_fds].events |= POLLIN;

            if (NULL != entry->callbacks.write_callback)
                _poll_fds[_num_poll_fds].events |= POLLOUT;

            if (NULL != entry->callbacks.hangup_callback)
                _poll_fds[_num_poll_fds].events |= POLLHUP;

            ++_num_poll_fds;
        }
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Selection Object - Dispatch
// =================================
GuestErrorT guest_selobj_dispatch( unsigned int timeout_in_ms )
{
    struct pollfd* poll_entry;
    GuestSelObjEntryT* entry;
    int result;

    result = poll(_poll_fds, _num_poll_fds, timeout_in_ms);
    if (0 > result)
    {
        if (errno == EINTR)
        {
            DPRINTFD("Interrupted by a signal.");
            return GUEST_OKAY;
        } else {
            DPRINTFE("Select failed, error=%s.", strerror(errno));
            return GUEST_FAILED;
        }
    } else if (0 == result) {
        DPRINTFV("Nothing selected.");
        return GUEST_OKAY;
    }

    unsigned int entry_i;
    for (entry_i=0; _num_poll_fds > entry_i; ++entry_i)
    {
        poll_entry = &(_poll_fds[entry_i]);

        entry = guest_selobj_find(poll_entry->fd);
        if (NULL != entry)
        {
            if (0 != (poll_entry->revents & POLLIN))
                if (NULL != entry->callbacks.read_callback)
                {
                    DPRINTFD("Read on selection object %i", poll_entry->fd);
                    entry->callbacks.read_callback(entry->selobj);
                }

            if (0 != (poll_entry->revents & POLLOUT))
                if (NULL != entry->callbacks.write_callback)
                {
                    DPRINTFD("Write on selection object %i", poll_entry->fd);
                    entry->callbacks.write_callback(entry->selobj);
                }

            if (0 != (poll_entry->revents & POLLHUP))
                if (NULL != entry->callbacks.hangup_callback)
                {
                    DPRINTFD("Hangup on selection object %i", poll_entry->fd);
                    entry->callbacks.hangup_callback(entry->selobj);
                }
        }
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Selection Object - Initialize
// ===================================
GuestErrorT guest_selobj_initialize( void )
{
    _num_poll_fds = 0;
    memset(_poll_fds, 0, sizeof(_poll_fds));
    memset(_select_objs, 0, sizeof(GuestSelObjTableT));
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Selection Object - Finalize
// =================================
GuestErrorT guest_selobj_finalize( void )
{
    _num_poll_fds = 0;
    memset(_poll_fds, 0, sizeof(_poll_fds));
    memset(_select_objs, 0, sizeof(GuestSelObjTableT));
    return GUEST_OKAY;
}
// ****************************************************************************

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
#include "guest_signal.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "guest_limits.h"
#include "guest_types.h"
#include "guest_debug.h"

typedef struct {
    bool inuse;
    int signum;
    GuestSignalHandlerT handler;
} GuestSignalT;

static GuestSignalT _signal[GUEST_MAX_SIGNALS];

// ****************************************************************************
// Guest Signal - Map
// ==================
static GuestSignalT* guest_signal_map( int signum )
{
    switch (signum)
    {
        case SIGINT:  return &(_signal[0]);
        case SIGTERM: return &(_signal[1]);
        case SIGQUIT: return &(_signal[2]);
        case SIGHUP:  return &(_signal[3]);
        case SIGCHLD: return &(_signal[4]);
        case SIGCONT: return &(_signal[5]);
        case SIGPIPE: return &(_signal[6]);
        case SIGIO:   return &(_signal[7]);
        default:
            DPRINTFE("Mapping for signal %i missing.", signum);
    }

    return NULL;
}
// ****************************************************************************

// ****************************************************************************
// Guest Signal - Handler
// ======================
static void guest_signal_handler( int signum )
{
    GuestSignalT* entry;

    DPRINTFD("Signal %i received.", signum);

    entry = guest_signal_map(signum);
    if (NULL != entry)
        if (entry->inuse)
            if (NULL != entry->handler)
                entry->handler(signum);
}
// ****************************************************************************

// ****************************************************************************
// Guest Signal - Register Handler
// ===============================
void guest_signal_register_handler( int signum, GuestSignalHandlerT handler )
{
    GuestSignalT* entry;

    entry = guest_signal_map(signum);
    if (NULL != entry)
    {
        entry->inuse = true;
        entry->signum = signum;
        entry->handler = handler;
        signal(signum, guest_signal_handler);
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Signal - Deregister Handler
// =================================
void guest_signal_deregister_handler( int signum )
{
    GuestSignalT* entry;

    entry = guest_signal_map(signum);
    if (NULL != entry)
    {
        memset(entry, 0, sizeof(GuestSignalT));
        signal(signum, SIG_DFL);
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Signal - Ignore
// =====================
void guest_signal_ignore( int signum )
{
    guest_signal_deregister_handler(signum);
    signal(signum, SIG_IGN);
}
// ****************************************************************************

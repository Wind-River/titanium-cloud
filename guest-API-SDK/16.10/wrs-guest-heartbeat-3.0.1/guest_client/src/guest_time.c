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
#include "guest_time.h"

#include <time.h>

// ****************************************************************************
// Guest Time - Get
// ================
void guest_time_get( GuestTimeT* time )
{
#ifdef CLOCK_MONOTONIC_RAW
     clock_gettime(CLOCK_MONOTONIC_RAW, time);
#else
     clock_gettime(CLOCK_MONOTONIC, time);
#endif
}
// ****************************************************************************

// ****************************************************************************
// Guest Time - Get Elapsed Milliseconds
// =====================================
long guest_time_get_elapsed_ms( GuestTimeT* time )
{
    GuestTimeT now;

    guest_time_get(&now);

    if (NULL == time)
        return ((now.tv_sec*1000) + (now.tv_nsec/1000000));
    else
        return (guest_time_delta_in_ms(&now, time));
}
// ****************************************************************************

// ****************************************************************************
// Guest Time - Delta in Milliseconds
// ==================================
long guest_time_delta_in_ms( GuestTimeT* end, GuestTimeT* start )
{
    long start_in_ms = (start->tv_sec*1000) + (start->tv_nsec/1000000);
    long end_in_ms = (end->tv_sec*1000) + (end->tv_nsec/1000000);

    return (end_in_ms - start_in_ms);
}
// ****************************************************************************

// ****************************************************************************
// Guest Time - Convert Milliseconds
// =================================
void guest_time_convert_ms( long ms, GuestTimeT* time )
{
    time->tv_sec = ms / 1000;
    time->tv_nsec = (ms % 1000) * 1000000;
}
// ****************************************************************************

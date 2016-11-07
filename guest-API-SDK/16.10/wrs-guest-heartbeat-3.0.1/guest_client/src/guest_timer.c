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
#include "guest_timer.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "guest_limits.h"
#include "guest_types.h"
#include "guest_debug.h"
#include "guest_time.h"

typedef uint64_t GuestTimerInstanceT;

typedef struct {
    bool inuse;
    GuestTimerInstanceT timer_instance;
    GuestTimerIdT timer_id;
    unsigned int ms_interval;
    GuestTimeT arm_timestamp;
    GuestTimerCallbackT callback;
} GuestTimerEntryT;

typedef GuestTimerEntryT GuestTimerTableT[GUEST_TIMERS_MAX];

static bool _scheduling_on_time = true;
static GuestTimerInstanceT _timer_instance = 0;
static GuestTimerIdT _last_timer_dispatched = 0;
static GuestTimerTableT _timers;
static GuestTimeT _delay_timestamp;
static GuestTimeT _schedule_timestamp;

// ****************************************************************************
// Guest Timer - Scheduling On Time
// ================================
bool guest_timer_scheduling_on_time( void )
{
    return _scheduling_on_time;
}
// ****************************************************************************

// ****************************************************************************
// Guest Timer - Scheduling On Time Within
// =======================================
bool guest_timer_scheduling_on_time_within( unsigned int period_in_ms )
{
    long ms_expired;

    ms_expired = guest_time_get_elapsed_ms(&_delay_timestamp);
    return (period_in_ms < ms_expired);
}
// ****************************************************************************

// ****************************************************************************
// Guest Timer - Reset
// ===================
GuestErrorT guest_timer_reset( GuestTimerIdT timer_id )
{
    GuestTimerEntryT* timer_entry = NULL;

    if ((GUEST_TIMER_ID_INVALID == timer_id)||(GUEST_TIMERS_MAX <= timer_id))
        return GUEST_FAILED;

    timer_entry = &(_timers[timer_id]);
    guest_time_get(&timer_entry->arm_timestamp);

    DPRINTFD("Timer (%i) reset.", timer_entry->timer_id);
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Timer - Register
// ======================
GuestErrorT guest_timer_register(
        unsigned int ms, GuestTimerCallbackT callback, GuestTimerIdT* timer_id )
{
    GuestTimerEntryT* timer_entry;

    *timer_id = GUEST_TIMER_ID_INVALID;

    unsigned int timer_i;
    for (timer_i=1; GUEST_TIMERS_MAX > timer_i; ++timer_i)
    {
        timer_entry = &(_timers[timer_i]);

        if (timer_entry->inuse)
            continue;

        memset(timer_entry, 0, sizeof(GuestTimerEntryT));

        timer_entry->inuse = true;
        timer_entry->timer_instance = ++_timer_instance;
        timer_entry->timer_id = timer_i;
        timer_entry->ms_interval = ms;
        guest_time_get(&timer_entry->arm_timestamp);
        timer_entry->callback = callback;
        break;
    }

    if (GUEST_TIMERS_MAX <= timer_i)
    {
        DPRINTFE("No space available to create timer, exiting...");
        abort();
    }

    *timer_id = timer_i;

    DPRINTFD("Created timer, id=%i.", timer_entry->timer_id);
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Timer - Deregister
// ========================
GuestErrorT guest_timer_deregister( GuestTimerIdT timer_id )
{
    GuestTimerEntryT* timer_entry = NULL;

    if ((GUEST_TIMER_ID_INVALID == timer_id)||(GUEST_TIMERS_MAX <= timer_id))
        return GUEST_OKAY;

    timer_entry = &(_timers[timer_id]);
    timer_entry->inuse = false;
    timer_entry->timer_instance = 0;

    DPRINTFD("Cancelled timer, id=%i.", timer_entry->timer_id);
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Timer - Schedule Next
// ===========================
static unsigned int guest_timer_schedule_next( void )
{
    GuestTimerEntryT* timer_entry;
    long ms_expired, ms_remaining;
    unsigned int interval_in_ms = GUEST_TICK_INTERVAL_IN_MS;

    unsigned int timer_i;
    for (timer_i=0; GUEST_TIMERS_MAX > timer_i; ++timer_i)
    {
        timer_entry = &(_timers[timer_i]);

        if (timer_entry->inuse)
        {
            ms_expired = guest_time_get_elapsed_ms(&timer_entry->arm_timestamp);
            if (ms_expired < timer_entry->ms_interval)
            {
                ms_remaining = timer_entry->ms_interval - ms_expired;
                if (ms_remaining < interval_in_ms)
                    interval_in_ms = ms_remaining;
            } else {
                interval_in_ms = GUEST_MIN_TICK_INTERVAL_IN_MS;
                break;
            }
        }
    }

    if (GUEST_MIN_TICK_INTERVAL_IN_MS > interval_in_ms)
        interval_in_ms = GUEST_MIN_TICK_INTERVAL_IN_MS;

    else if (GUEST_TICK_INTERVAL_IN_MS < interval_in_ms)
        interval_in_ms = GUEST_TICK_INTERVAL_IN_MS;

    DPRINTFV("Scheduling timers in %d ms.", interval_in_ms);
    return interval_in_ms;
}
// ****************************************************************************

// ****************************************************************************
// Guest Timer - Schedule
// ======================
unsigned int guest_timer_schedule( void )
{
    long ms_expired;
    GuestTimeT time_prev;
    GuestTimerEntryT* timer_entry;
    unsigned int total_timers_fired =0;

    ms_expired = guest_time_get_elapsed_ms(&_schedule_timestamp);
    if (ms_expired >= GUEST_SCHEDULING_MAX_DELAY_IN_MS)
    {
        if (_scheduling_on_time)
        {
            _scheduling_on_time = false;
            DPRINTFI("Not scheduling on time, elapsed=%li ms.", ms_expired);
        }
        guest_time_get(&_delay_timestamp);

    } else if (!_scheduling_on_time) {
        ms_expired = guest_time_get_elapsed_ms(&_delay_timestamp);
        if (GUEST_SCHEDULING_DELAY_DEBOUNCE_IN_MS < ms_expired)
        {
            _scheduling_on_time = true;
            DPRINTFI("Now scheduling on time.");
        }
    }

    guest_time_get(&time_prev);

    unsigned int timer_i;
    for (timer_i=_last_timer_dispatched; GUEST_TIMERS_MAX > timer_i; ++timer_i)
    {
        timer_entry = &(_timers[timer_i]);

        if (timer_entry->inuse)
        {
            ms_expired = guest_time_get_elapsed_ms(&timer_entry->arm_timestamp);

            if (ms_expired >= timer_entry->ms_interval)
            {
                bool rearm;
                GuestTimerInstanceT timer_instance;

                DPRINTFD("Timer %i fire, ms_interval=%d, ms_expired=%li.",
                         timer_entry->timer_id, timer_entry->ms_interval,
                         ms_expired);

                timer_instance = timer_entry->timer_instance;

                rearm = timer_entry->callback(timer_entry->timer_id);

                if (timer_instance == timer_entry->timer_instance)
                {
                    if (rearm)
                    {
                        guest_time_get(&timer_entry->arm_timestamp);
                        DPRINTFD("Timer (%i) rearmed.", timer_entry->timer_id);
                    } else {
                        timer_entry->inuse = 0;
                        DPRINTFD("Timer (%i) removed.", timer_entry->timer_id);
                    }
                } else {
                    DPRINTFD("Timer (%i) instance changed since callback, "
                             "rearm=%d.", timer_entry->timer_id, (int) rearm);
                }

                if (GUEST_MAX_TIMERS_PER_TICK <= ++total_timers_fired)
                {
                    DPRINTFD("Maximum timers per tick (%d) reached.",
                             GUEST_MAX_TIMERS_PER_TICK);
                    break;
                }
            }
        }
    }

    if (GUEST_TIMERS_MAX <= timer_i)
        _last_timer_dispatched = 0;
    else
        _last_timer_dispatched = timer_i;

    ms_expired = guest_time_get_elapsed_ms(&time_prev);
    if (ms_expired >= GUEST_SCHEDULING_MAX_DELAY_IN_MS)
    {
        _scheduling_on_time = false;
        guest_time_get(&_delay_timestamp);

        DPRINTFI("Not scheduling on time, timer callbacks are taking too "
                 "long to execute, elapsed_time=%li ms.", ms_expired);
    } else {
        DPRINTFV("Timer callbacks took %li ms.", ms_expired);
    }

    guest_time_get(&_schedule_timestamp);

    return guest_timer_schedule_next();
}
// ****************************************************************************

// ****************************************************************************
// Guest Timer - Initialize
// ========================
GuestErrorT guest_timer_initialize( void )
{
    _scheduling_on_time = true;
    _last_timer_dispatched = 0;
    memset(_timers, 0, sizeof(GuestTimerTableT));
    guest_time_get(&_schedule_timestamp);
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Timer - Finalize
// ======================
GuestErrorT guest_timer_finalize( void )
{
    memset(_timers, 0, sizeof(GuestTimerTableT));
    return GUEST_OKAY;
}
// ****************************************************************************

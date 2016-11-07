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
#ifndef __GUEST_TIMER_H__
#define __GUEST_TIMER_H__

#include <stdbool.h>

#include "guest_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GUEST_TIMER_ID_INVALID -1

typedef int GuestTimerIdT;

typedef bool (*GuestTimerCallbackT) (GuestTimerIdT timer_id);

// ****************************************************************************
// Guest Timer - Scheduling On Time
// ================================
extern bool guest_timer_scheduling_on_time( void );
// ****************************************************************************

// ****************************************************************************
// Guest Timer - Scheduling On Time Within
// =======================================
extern bool guest_timer_scheduling_on_time_within( unsigned int period_in_ms );
// ****************************************************************************

// ****************************************************************************
// Guest Timer - Reset
// ===================
extern GuestErrorT guest_timer_reset( GuestTimerIdT timer_id );
// ****************************************************************************

// ****************************************************************************
// Guest Timer - Register
// ======================
extern GuestErrorT guest_timer_register(
        unsigned int ms, GuestTimerCallbackT callback, GuestTimerIdT* timer_id );
// ****************************************************************************

// ****************************************************************************
// Guest Timer - Deregister
// ========================
extern GuestErrorT guest_timer_deregister( GuestTimerIdT timer_id );
// ****************************************************************************

// ****************************************************************************
// Guest Timer - Schedule
// ======================
extern unsigned int guest_timer_schedule( void );
// ****************************************************************************

// ****************************************************************************
// Guest Timer - Initialize
// ========================
extern GuestErrorT guest_timer_initialize( void );
// ****************************************************************************

// ****************************************************************************
// Guest Timer - Finalize
// ======================
extern GuestErrorT guest_timer_finalize( void );
// ****************************************************************************

#ifdef __cplusplus
}
#endif

#endif /* __GUEST_TIMER_H__ */

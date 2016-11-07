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
#ifndef __GUEST_STREAM_H__
#define __GUEST_STREAM_H__

#include <stdbool.h>

#include "guest_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char* delimiter;
    int delimiter_size;
    char* end_ptr;
    char* bytes;
    int avail;
    int size;
    int max_size;
} GuestStreamT;

// ****************************************************************************
// Guest Stream - Get
// ==================
extern int guest_stream_get( GuestStreamT* stream );
// ****************************************************************************

// ****************************************************************************
// Guest Stream - Get Next
// =======================
extern bool guest_stream_get_next( GuestStreamT* stream );
// ****************************************************************************

// ****************************************************************************
// Guest Stream - Advance
// ======================
extern void guest_stream_advance( int adv, GuestStreamT* stream );
// ****************************************************************************

// ****************************************************************************
// Guest Stream - Reset
// ====================
extern void guest_stream_reset( GuestStreamT* stream );
// ****************************************************************************

// ****************************************************************************
// Guest Stream - Setup
// ====================
extern GuestErrorT guest_stream_setup(
        const char* delimiter, int delimiter_size, int stream_size,
        GuestStreamT* stream );
// ****************************************************************************

// ****************************************************************************
// Guest Stream - Release
// ======================
extern GuestErrorT guest_stream_release( GuestStreamT* stream );
// ****************************************************************************

// ****************************************************************************
// Guest Stream - Initialize
// =========================
extern GuestErrorT guest_stream_initialize( void );
// ****************************************************************************

// ****************************************************************************
// Guest Stream - Finalize
// =======================
extern GuestErrorT guest_stream_finalize( void );
// ****************************************************************************

#ifdef __cplusplus
}
#endif

#endif /* __GUEST_STREAM_H__ */

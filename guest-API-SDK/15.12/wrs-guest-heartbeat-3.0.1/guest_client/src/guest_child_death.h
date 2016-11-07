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
#ifndef __GUEST_CHILD_DEATH_H__
#define __GUEST_CHILD_DEATH_H__

#include <sys/types.h>

#include "guest_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GUEST_CHILD_FAILED      -65536

typedef void (*GuestChildDeathCallbackT) (pid_t pid, int exit_code);

// ****************************************************************************
// Guest Child Death - Register
// ============================
extern GuestErrorT guest_child_death_register(
        pid_t pid, GuestChildDeathCallbackT callback );
// ****************************************************************************

// ****************************************************************************
// Guest Child Death - Deregister
// ==============================
extern GuestErrorT guest_child_death_deregister( pid_t pid );
// ****************************************************************************

// ****************************************************************************
// Guest Child Death - Save
// ========================
extern GuestErrorT guest_child_death_save( pid_t pid, int exit_code );
// ****************************************************************************

// ****************************************************************************
// Guest Child Death - Initialize
// ==============================
extern GuestErrorT guest_child_death_initialize( void );
// ****************************************************************************

// ****************************************************************************
// Guest Child Death - Finalize
// ============================
extern GuestErrorT guest_child_death_finalize( void );
// ****************************************************************************

#ifdef __cplusplus
}
#endif

#endif /* __GUEST_CHILD_DEATH_H__ */

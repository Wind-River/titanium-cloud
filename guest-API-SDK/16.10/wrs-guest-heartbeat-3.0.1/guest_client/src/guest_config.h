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
#ifndef __GUEST_CONFIGURATION_H__
#define __GUEST_CONFIGURATION_H__

#include "guest_limits.h"
#include "guest_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char name[GUEST_NAME_MAX_CHAR];
    char comm_device[GUEST_DEVICE_NAME_MAX_CHAR];
} GuestConfigT;

// ****************************************************************************
// Guest Configuration - Get
// =========================
extern GuestConfigT* guest_config_get( void );
// ****************************************************************************

// ****************************************************************************
// Guest Configuration - Show Usage
// ================================
extern void guest_config_show_usage( void );
// ****************************************************************************

// ****************************************************************************
// Guest Configuration - Initialize
// ================================
extern GuestErrorT guest_config_initialize(
        int argc, char *argv[], char *envp[] );
// ****************************************************************************

// ****************************************************************************
// Guest Configuration - Finalize
// ==============================
extern GuestErrorT guest_config_finalize( void );
// ****************************************************************************

#ifdef __cplusplus
}
#endif

#endif /* __GUEST_CONFIGURATION_H__ */

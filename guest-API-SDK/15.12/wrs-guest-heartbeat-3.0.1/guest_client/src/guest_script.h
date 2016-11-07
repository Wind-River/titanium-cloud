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
#ifndef __GUEST_SCRIPT_H__
#define __GUEST_SCRIPT_H__

#include "guest_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GUEST_SCRIPT_ID_INVALID -1

typedef int GuestScriptIdT;

typedef void (*GuestScriptCallbackT)
        (GuestScriptIdT script_id,int exit_code, char* log_msg);

// ****************************************************************************
// Guest Script - Abort
// ====================
extern void guest_script_abort( GuestScriptIdT script_id );
// ****************************************************************************

// ****************************************************************************
// Guest Script - Invoke
// =====================
extern GuestErrorT guest_script_invoke(
        char script[], char* script_argv[], GuestScriptCallbackT callback,
        GuestScriptIdT* script_id );
// ****************************************************************************

// ****************************************************************************
// Guest Script - Initialize
// =========================
extern GuestErrorT guest_script_initialize( void );
// ****************************************************************************

// ****************************************************************************
// Guest Script - Finalize
// =======================
extern GuestErrorT guest_script_finalize( void );
// ****************************************************************************

#ifdef __cplusplus
}
#endif

#endif /* __GUEST_SCRIPT_H__ */

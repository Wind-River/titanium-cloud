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
#ifndef __GUEST_DEBUG_H__
#define __GUEST_DEBUG_H__

#include <stdbool.h>

#include "guest_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GUEST_DEBUG_LOG_LEVEL_ERROR,
    GUEST_DEBUG_LOG_LEVEL_INFO,
    GUEST_DEBUG_LOG_LEVEL_DEBUG,
    GUEST_DEBUG_LOG_LEVEL_VERBOSE,
} GuestDebugLogLevelT;

#define DPRINTF(level, format, args...) \
    if (guest_debug_want_log(level)) \
        guest_debug_log("%s: %s(%i): " format, \
                        guest_debug_log_level_str(level), \
                        __FILE__, __LINE__, ##args)

#define DPRINTFE(format, args...) \
    DPRINTF(GUEST_DEBUG_LOG_LEVEL_ERROR, format, ##args)
#define DPRINTFI(format, args...) \
    DPRINTF(GUEST_DEBUG_LOG_LEVEL_INFO, format, ##args)
#define DPRINTFD(format, args...) \
    DPRINTF(GUEST_DEBUG_LOG_LEVEL_DEBUG, format, ##args)
#define DPRINTFV(format, args... ) \
    DPRINTF(GUEST_DEBUG_LOG_LEVEL_VERBOSE, format, ##args)

// ****************************************************************************
// Guest Debug - Log Level String
// ==============================
extern const char* guest_debug_log_level_str( GuestDebugLogLevelT level );
// ****************************************************************************

// ****************************************************************************
// Guest Debug - Set Log Level
// ===========================
extern void guest_debug_set_log_level( GuestDebugLogLevelT level );
// ****************************************************************************

// ****************************************************************************
// Guest Debug - Want Log
// ======================
extern bool guest_debug_want_log( GuestDebugLogLevelT level );
// ****************************************************************************

// ****************************************************************************
// Guest Debug - Log
// =================
extern void guest_debug_log( const char* format, ... );
// ****************************************************************************

// ****************************************************************************
// Guest Debug - Initialize
// ========================
extern GuestErrorT guest_debug_initialize( char process_name[] );
// ****************************************************************************

// ****************************************************************************
// Guest Debug - Finalize
// ======================
extern GuestErrorT guest_debug_finalize( void );
// ****************************************************************************

#ifdef __cplusplus
}
#endif

#endif /* __GUEST_DEBUG_H__ */

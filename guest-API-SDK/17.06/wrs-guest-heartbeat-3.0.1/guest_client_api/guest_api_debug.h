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
#ifndef __GUEST_API_DEBUG_H__
#define __GUEST_API_DEBUG_H__

#include <stdbool.h>

#include "guest_api_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GUEST_API_DEBUG_LOG_LEVEL_ERROR,
    GUEST_API_DEBUG_LOG_LEVEL_INFO,
    GUEST_API_DEBUG_LOG_LEVEL_DEBUG,
    GUEST_API_DEBUG_LOG_LEVEL_VERBOSE,
} GuestApiDebugLogLevelT;

#define DPRINTF(level, format, args...) \
    if (guest_api_debug_want_log(level)) \
        guest_api_debug_log("%s: %s(%i): " format, \
                            guest_api_debug_log_level_str(level), \
                            __FILE__, __LINE__, ##args)
#define DPRINTFE(format, args...) \
    DPRINTF(GUEST_API_DEBUG_LOG_LEVEL_ERROR, format, ##args)
#define DPRINTFI(format, args...) \
    DPRINTF(GUEST_API_DEBUG_LOG_LEVEL_INFO, format, ##args)
#define DPRINTFD(format, args...) \
    DPRINTF(GUEST_API_DEBUG_LOG_LEVEL_DEBUG, format, ##args)
#define DPRINTFV(format, args... ) \
    DPRINTF(GUEST_API_DEBUG_LOG_LEVEL_VERBOSE, format, ##args)

// ****************************************************************************
// Guest API Debug - Log Level String
// ==================================
extern const char* guest_api_debug_log_level_str( GuestApiDebugLogLevelT level );
// ****************************************************************************

// ****************************************************************************
// Guest API Debug - Set Log Level
// ===============================
extern void guest_api_debug_set_log_level( GuestApiDebugLogLevelT level );
// ****************************************************************************

// ****************************************************************************
// Guest API Debug - Want Log
// ==========================
extern bool guest_api_debug_want_log( GuestApiDebugLogLevelT level );
// ****************************************************************************

// ****************************************************************************
// Guest API Debug - Log
// =====================
extern void guest_api_debug_log( const char* format, ... );
// ****************************************************************************

// ****************************************************************************
// Guest API Debug - Initialize
// ============================
extern GuestApiErrorT guest_api_debug_initialize( char process_name[] );
// ****************************************************************************

// ****************************************************************************
// Guest API Debug - Finalize
// ==========================
extern GuestApiErrorT guest_api_debug_finalize( void );
// ****************************************************************************

#ifdef __cplusplus
}
#endif

#endif /* __GUEST_API_DEBUG_H__ */

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
#include "guest_debug.h"

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "guest_types.h"

#define GUEST_DEBUG_WANT_SYSLOG
#ifdef GUEST_DEBUG_WANT_SYSLOG
#include <syslog.h>
#endif

static char _process_name[30];
static GuestDebugLogLevelT _log_level = GUEST_DEBUG_LOG_LEVEL_INFO;

// ****************************************************************************
// Guest Debug - Log Level String
// ==============================
const char* guest_debug_log_level_str( GuestDebugLogLevelT level )
{
    switch (level) {
        case GUEST_DEBUG_LOG_LEVEL_ERROR:   return "error";
        case GUEST_DEBUG_LOG_LEVEL_INFO:    return " info";
        case GUEST_DEBUG_LOG_LEVEL_DEBUG:   return "debug";
        case GUEST_DEBUG_LOG_LEVEL_VERBOSE: return " verb";
        default:
            return "???";
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Debug - Set Log Level
// ===========================
void guest_debug_set_log_level( GuestDebugLogLevelT level )
{
    _log_level = level;
}
// ****************************************************************************

// ****************************************************************************
// Guest Debug - Want Log
// ======================
bool guest_debug_want_log( GuestDebugLogLevelT level )
{
    return (level <= _log_level);
}
// ****************************************************************************

// ****************************************************************************
// Guest Debug - Log
// =================
void guest_debug_log( const char* format, ... )
{
    char time_str[80];
    char date_str[32];
    struct tm t_real;
    struct timespec ts_real;
    va_list arguments;
    char log_data[512];

    va_start(arguments, format);
    vsnprintf(log_data, sizeof(log_data), format, arguments);
    va_end(arguments);

    clock_gettime(CLOCK_REALTIME, &ts_real);

    if (NULL == localtime_r(&(ts_real.tv_sec), &t_real))
    {
        snprintf( time_str, sizeof(time_str),
                  "YYYY:MM:DD HH:MM:SS.xxx" );
    } else {
        strftime( date_str, sizeof(date_str), "%b %e %H:%M:%S",
                  &t_real );
        snprintf( time_str, sizeof(time_str), "%s.%03ld", date_str,
                  ts_real.tv_nsec/1000000 );
    }

#ifdef GUEST_DEBUG_WANT_SYSLOG
    syslog(LOG_DEBUG, "%s", log_data);
#else
    printf("%s %s: %s\n", time_str, _process_name, log_data);
#endif
}
// ****************************************************************************

// ****************************************************************************
// Guest Debug - Initialize
// ========================
GuestErrorT guest_debug_initialize( char process_name[] )
{
    _log_level = GUEST_DEBUG_LOG_LEVEL_INFO;
    snprintf(_process_name, sizeof(_process_name), "%s", process_name);

#ifdef GUEST_DEBUG_WANT_SYSLOG
    openlog(_process_name, LOG_PID | LOG_NDELAY, LOG_DAEMON);
#endif

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Debug - Finalize
// ======================
GuestErrorT guest_debug_finalize( void )
{
    _log_level = GUEST_DEBUG_LOG_LEVEL_INFO;
    _process_name[0] = '\0';

#ifdef GUEST_DEBUG_WANT_SYSLOG
    closelog();
#endif

    return GUEST_OKAY;
}
// ****************************************************************************

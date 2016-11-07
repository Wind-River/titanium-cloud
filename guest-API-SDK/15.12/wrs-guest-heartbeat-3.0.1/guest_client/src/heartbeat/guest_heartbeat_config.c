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
#include "guest_heartbeat_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "guest_types.h"
#include "guest_debug.h"
#include "guest_utils.h"

#include "guest_heartbeat_types.h"

#ifndef SYSCONFDIR
#define GUEST_HEARTBEAT_DEFAULT_CONFIG_FILE  \
    "/etc/guest-client/heartbeat/guest_heartbeat.conf"
#else
#define GUEST_HEARTBEAT_DEFAULT_CONFIG_FILE  \
    MAKE_STRING(SYSCONFDIR) "/guest-client/heartbeat/guest_heartbeat.conf"
#endif

#define GUEST_HEARTBEAT_DEFAULT_HEARTBEAT_INIT_RETRY_MS          5000
#define GUEST_HEARTBEAT_DEFAULT_HEARTBEAT_MIN_TIMEOUT_MS         5000
#define GUEST_HEARTBEAT_DEFAULT_HEARTBEAT_INTERVAL_MS            1000
#define GUEST_HEARTBEAT_DEFAULT_VOTE_MS                         10000
#define GUEST_HEARTBEAT_DEFAULT_SHUTDOWN_MS                     10000
#define GUEST_HEARTBEAT_DEFAULT_SUSPEND_MS                      10000
#define GUEST_HEARTBEAT_DEFAULT_RESUME_MS                       10000
#define GUEST_HEARTBEAT_DEFAULT_RESTART_MS                     120000

static GuestHeartbeatConfigT _config;

// ****************************************************************************
// Guest Heartbeat Configuration - Read
// ====================================
static GuestErrorT guest_heartbeat_config_read( char filename[] )
{
    char* s;
    char* key;
    char* value;
    char delimiter[] = "=";
    char discard[] = "\'\"";
    char buf[1024];
    FILE* fp;

    fp = fopen(filename, "r");
    if (NULL == fp)
    {
        DPRINTFE("Failed to open file %s.", filename);
        return GUEST_FAILED;
    }

    while (NULL != (s = fgets(buf, sizeof(buf), fp)))
    {
        s = guest_utils_trim(s, NULL);

        // Skip empty string and comments
        if (('\0' == *s) || ('#' == *s))
            continue;

        key = strtok(s, delimiter);
        value = strtok(NULL, delimiter);

        key = guest_utils_trim(key, discard);
        value = guest_utils_trim(value, discard);

        if ((NULL != key) && (NULL != value))
        {
            if (0 == strcmp("HB_INIT_RETRY", key))
            {
                _config.heartbeat_init_retry_ms = atoi(value);

            } else if (0 == strcmp("HB_MIN_TIMEOUT", key)) {
                _config.heartbeat_min_timeout_ms = atoi(value);

            } else if (0 == strcmp("HB_INTERVAL", key)) {
                _config.heartbeat_interval_ms = atoi(value);

            } else if (0 == strcmp("VOTE", key)) {
                _config.vote_ms = atoi(value) * 1000;

            } else if (0 == strcmp("SHUTDOWN_NOTICE", key)) {
                _config.shutdown_notice_ms = atoi(value) * 1000;

            } else if (0 == strcmp("SUSPEND_NOTICE", key)) {
                _config.suspend_notice_ms = atoi(value) * 1000;

            } else if (0 == strcmp("RESUME_NOTICE", key)) {
                _config.resume_notice_ms = atoi(value) * 1000;

            } else if (0 == strcmp("RESTART", key)) {
                _config.restart_ms = atoi(value) * 1000;

            } else if (0 == strcmp("CORRECTIVE_ACTION", key)) {
                if (0 == strcmp("reboot", value))
                {
                    _config.corrective_action = GUEST_HEARTBEAT_ACTION_REBOOT;

                } else if (0 == strcmp("stop", value)) {
                    _config.corrective_action = GUEST_HEARTBEAT_ACTION_STOP;

                } else if (0 == strcmp("log", value)) {
                    _config.corrective_action = GUEST_HEARTBEAT_ACTION_LOG;
                }

            } else if (0 == strcmp("HEALTH_CHECK_INTERVAL", key)) {
                _config.health_check_interval_ms = atoi(value) * 1000;

            } else if (0 == strcmp("HEALTH_CHECK_SCRIPT", key)) {
                snprintf(_config.health_check_script,
                         sizeof(_config.health_check_script), "%s", value);

            } else if (0 == strcmp("EVENT_NOTIFICATION_SCRIPT", key)) {
                snprintf(_config.event_handling_script,
                         sizeof(_config.event_handling_script), "%s", value);

            } else {
                DPRINTFE("Unknown key %s in configuration file %s.", key,
                         filename);
            }
        }
    }

    fclose(fp);
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Configuration - Dump
// ====================================
static void guest_heartbeat_config_dump( void )
{
    DPRINTFI("Guest-Client Heartbeat Configuration:");
    DPRINTFI("  heartbeat-init-retry:  %i ms", _config.heartbeat_init_retry_ms);
    DPRINTFI("  heartbeat-interval:    %i ms", _config.heartbeat_interval_ms);
    DPRINTFI("  heartbeat-min-timeout: %i ms", _config.heartbeat_min_timeout_ms);
    DPRINTFI("  vote:                  %i ms", _config.vote_ms);
    DPRINTFI("  shutdown-notice:       %i ms", _config.shutdown_notice_ms);
    DPRINTFI("  suspend-notice:        %i ms", _config.suspend_notice_ms);
    DPRINTFI("  resume-notice:         %i ms", _config.resume_notice_ms);
    DPRINTFI("  restart:               %i ms", _config.restart_ms);
    DPRINTFI("  health-check-interval: %i ms", _config.health_check_interval_ms);
    DPRINTFI("  health-check-script:   %s", _config.health_check_script);
    DPRINTFI("  event-handling-script: %s", _config.event_handling_script);
    DPRINTFI("  corrective-action:     %s",
             guest_heartbeat_action_str(_config.corrective_action));
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Configuration - Get
// ===================================
GuestHeartbeatConfigT* guest_heartbeat_config_get( void )
{
    return &_config;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Configuration - Initialize
// ==========================================
GuestErrorT guest_heartbeat_config_initialize( void )
{
    GuestErrorT error;

    memset(&_config, 0, sizeof(GuestHeartbeatConfigT));

    _config.heartbeat_init_retry_ms = GUEST_HEARTBEAT_DEFAULT_HEARTBEAT_INIT_RETRY_MS;
    _config.heartbeat_min_timeout_ms = GUEST_HEARTBEAT_DEFAULT_HEARTBEAT_MIN_TIMEOUT_MS;
    _config.heartbeat_interval_ms = GUEST_HEARTBEAT_DEFAULT_HEARTBEAT_INTERVAL_MS;
    _config.vote_ms = GUEST_HEARTBEAT_DEFAULT_VOTE_MS;
    _config.shutdown_notice_ms = GUEST_HEARTBEAT_DEFAULT_SHUTDOWN_MS;
    _config.suspend_notice_ms = GUEST_HEARTBEAT_DEFAULT_SUSPEND_MS;
    _config.resume_notice_ms = GUEST_HEARTBEAT_DEFAULT_RESUME_MS;
    _config.restart_ms = GUEST_HEARTBEAT_DEFAULT_RESTART_MS;
    _config.corrective_action = GUEST_HEARTBEAT_ACTION_REBOOT;

    error = guest_heartbeat_config_read(GUEST_HEARTBEAT_DEFAULT_CONFIG_FILE);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to read guest heartbeat configuration, error=%s.",
                 guest_error_str(error));
        return error;
    }

    guest_heartbeat_config_dump();

    if (GUEST_HEARTBEAT_MIN_INTERVAL_MS > _config.heartbeat_interval_ms)
    {
        DPRINTFE("Guest heartbeat interval configuration is less than %i ms.",
                 GUEST_HEARTBEAT_MIN_INTERVAL_MS);
        return GUEST_FAILED;
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Configuration - Finalize
// ========================================
GuestErrorT guest_heartbeat_config_finalize( void )
{
    memset(&_config, 0, sizeof(GuestHeartbeatConfigT));
    return GUEST_OKAY;
}
// ****************************************************************************

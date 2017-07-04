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
#ifndef __GUEST_HEARTBEAT_MESSAGE_DEFINITIONS_H__
#define __GUEST_HEARTBEAT_MESSAGE_DEFINITIONS_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GUEST_HEARTBEAT_MSG_VERSION_CURRENT                                3
#define GUEST_HEARTBEAT_MSG_REVISION_CURRENT                               1

#define GUEST_HEARTBEAT_MSG_HEALTHY                                "healthy"
#define GUEST_HEARTBEAT_MSG_UNHEALTHY                            "unhealthy"

// maximum size of a string value except instance name and log msg
#define GUEST_HEARTBEAT_MSG_MAX_VALUE_SIZE                                32
// maximum size of instance name
#define GUEST_HEARTBEAT_MSG_MAX_NAME_SIZE                                 64
#define GUEST_HEARTBEAT_MSG_MAX_LOG_SIZE                                 224
#define GUEST_HEARTBEAT_MSG_MAX_MSG_SIZE                                1056
#define GUEST_HEARTBEAT_MSG_MIN_MSG_SIZE                                  32

// *IMPORTANT NOTE* The keys defined here should match those defined in
// NFV-VIM in order to properly encode/decode REST-API messages.

// Keys for Repair Action
#define    GUEST_HEARTBEAT_MSG_ACTION_UNKNOWN                      "unknown"
#define    GUEST_HEARTBEAT_MSG_ACTION_NONE                            "none"
#define    GUEST_HEARTBEAT_MSG_ACTION_REBOOT                        "reboot"
#define    GUEST_HEARTBEAT_MSG_ACTION_STOP                            "stop"
#define    GUEST_HEARTBEAT_MSG_ACTION_LOG                              "log"

// Keys for Event Type
#define    GUEST_HEARTBEAT_MSG_EVENT_UNKNOWN                       "unknown"
#define    GUEST_HEARTBEAT_MSG_EVENT_STOP                             "stop"
#define    GUEST_HEARTBEAT_MSG_EVENT_REBOOT                         "reboot"
#define    GUEST_HEARTBEAT_MSG_EVENT_SUSPEND                       "suspend"
#define    GUEST_HEARTBEAT_MSG_EVENT_PAUSE                           "pause"
#define    GUEST_HEARTBEAT_MSG_EVENT_UNPAUSE                       "unpause"
#define    GUEST_HEARTBEAT_MSG_EVENT_RESUME                         "resume"
#define    GUEST_HEARTBEAT_MSG_EVENT_RESIZE_BEGIN             "resize_begin"
#define    GUEST_HEARTBEAT_MSG_EVENT_RESIZE_END                 "resize_end"
#define    GUEST_HEARTBEAT_MSG_EVENT_LIVE_MIGRATE_BEGIN "live_migrate_begin"
#define    GUEST_HEARTBEAT_MSG_EVENT_LIVE_MIGRATE_END     "live_migrate_end"
#define    GUEST_HEARTBEAT_MSG_EVENT_COLD_MIGRATE_BEGIN "cold_migrate_begin"
#define    GUEST_HEARTBEAT_MSG_EVENT_COLD_MIGRATE_END     "cold_migrate_end"

// Keys for Notification Type
#define    GUEST_HEARTBEAT_MSG_NOTIFY_UNKNOWN                      "unknown"
#define    GUEST_HEARTBEAT_MSG_NOTIFY_REVOCABLE                  "revocable"
#define    GUEST_HEARTBEAT_MSG_NOTIFY_IRREVOCABLE              "irrevocable"

// Keys for Vote Result
#define    GUEST_HEARTBEAT_MSG_VOTE_RESULT_UNKNOWN                 "unknown"
#define    GUEST_HEARTBEAT_MSG_VOTE_RESULT_ACCEPT                   "accept"
#define    GUEST_HEARTBEAT_MSG_VOTE_RESULT_REJECT                   "reject"
#define    GUEST_HEARTBEAT_MSG_VOTE_RESULT_COMPLETE               "complete"
#define    GUEST_HEARTBEAT_MSG_VOTE_RESULT_TIMEOUT                 "timeout"
#define    GUEST_HEARTBEAT_MSG_VOTE_RESULT_ERROR                     "error"

// client registers for heartbeat service
#define    GUEST_HEARTBEAT_MSG_INIT                                   "init"
// server accepts new client
#define    GUEST_HEARTBEAT_MSG_INIT_ACK                           "init_ack"
// client intends to exit
#define    GUEST_HEARTBEAT_MSG_EXIT                                   "exit"
// server challenges client, are you healthy
#define    GUEST_HEARTBEAT_MSG_CHALLENGE                         "challenge"
// client response to challenge
#define    GUEST_HEARTBEAT_MSG_CHALLENGE_RESPONSE       "challenge_response"
// server proposes/demands action
#define    GUEST_HEARTBEAT_MSG_ACTION_NOTIFY                 "action_notify"
// client votes on action, or indicates action complete
#define    GUEST_HEARTBEAT_MSG_ACTION_RESPONSE             "action_response"
// server notify client of failure in processing client message
#define    GUEST_HEARTBEAT_MSG_NACK                                   "nack"

// Keys for messages between Host and Guest
#define GUEST_HEARTBEAT_MSG_VERSION                                "version"
#define GUEST_HEARTBEAT_MSG_REVISION                              "revision"
#define GUEST_HEARTBEAT_MSG_MSG_TYPE                              "msg_type"
#define GUEST_HEARTBEAT_MSG_SEQUENCE                              "sequence"
#define GUEST_HEARTBEAT_MSG_INVOCATION_ID                    "invocation_id"
#define GUEST_HEARTBEAT_MSG_NAME                                      "name"
#define GUEST_HEARTBEAT_MSG_HEARTBEAT_INTERVAL_MS    "heartbeat_interval_ms"
#define GUEST_HEARTBEAT_MSG_VOTE_SECS                            "vote_secs"
#define GUEST_HEARTBEAT_MSG_SHUTDOWN_NOTICE_SECS      "shutdown_notice_secs"
#define GUEST_HEARTBEAT_MSG_SUSPEND_NOTICE_SECS        "suspend_notice_secs"
#define GUEST_HEARTBEAT_MSG_RESUME_NOTICE_SECS          "resume_notice_secs"
#define GUEST_HEARTBEAT_MSG_RESTART_SECS                      "restart_secs"
#define GUEST_HEARTBEAT_MSG_CORRECTIVE_ACTION            "corrective_action"
#define GUEST_HEARTBEAT_MSG_LOG_MSG                                "log_msg"
#define GUEST_HEARTBEAT_MSG_HEARTBEAT_CHALLENGE        "heartbeat_challenge"
#define GUEST_HEARTBEAT_MSG_HEARTBEAT_RESPONSE          "heartbeat_response"
#define GUEST_HEARTBEAT_MSG_HEARTBEAT_HEALTH              "heartbeat_health"
#define GUEST_HEARTBEAT_MSG_EVENT_TYPE                          "event_type"
#define GUEST_HEARTBEAT_MSG_NOTIFICATION_TYPE            "notification_type"
#define GUEST_HEARTBEAT_MSG_TIMEOUT_MS                          "timeout_ms"
#define GUEST_HEARTBEAT_MSG_VOTE_RESULT                        "vote_result"

#ifdef __cplusplus
}
#endif

#endif /* __GUEST_HEARTBEAT_MESSAGE_DEFINITIONS_H__ */

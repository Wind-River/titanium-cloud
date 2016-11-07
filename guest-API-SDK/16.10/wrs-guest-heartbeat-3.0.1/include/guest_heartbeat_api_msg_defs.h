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
#ifndef __GUEST_HEARTBEAT_API_MESSAGE_DEFINITIONS_H__
#define __GUEST_HEARTBEAT_API_MESSAGE_DEFINITIONS_H__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GUEST_HEARTBEAT_API_MSG_ADDRESS     "/var/run/.guest_heartbeat_api"

#define GUEST_HEARTBEAT_API_MSG_MAGIC_VALUE                     "FDFDA5A5"
#define GUEST_HEARTBEAT_API_MSG_MAGIC_SIZE                              8
#define GUEST_HEARTBEAT_API_MSG_VERSION_CURRENT                         2
#define GUEST_HEARTBEAT_API_MSG_REVISION_CURRENT                        1

#define GUEST_HEARTBEAT_API_MSG_MAX_APPLICATION_NAME_SIZE              40
#define GUEST_HEARTBEAT_API_MSG_MAX_LOG_SIZE                          192

typedef enum {
    GUEST_HEARTBEAT_API_MSG_ACTION_UNKNOWN,
    GUEST_HEARTBEAT_API_MSG_ACTION_NONE,
    GUEST_HEARTBEAT_API_MSG_ACTION_REBOOT,
    GUEST_HEARTBEAT_API_MSG_ACTION_STOP,
    GUEST_HEARTBEAT_API_MSG_ACTION_LOG,
    GUEST_HEARTBEAT_API_MSG_ACTION_MAX,
} GuestHeartbeatApiMsgActionT;

typedef enum {
    GUEST_HEARTBEAT_API_MSG_EVENT_UNKNOWN,
    GUEST_HEARTBEAT_API_MSG_EVENT_STOP,
    GUEST_HEARTBEAT_API_MSG_EVENT_REBOOT,
    GUEST_HEARTBEAT_API_MSG_EVENT_SUSPEND,
    GUEST_HEARTBEAT_API_MSG_EVENT_PAUSE,
    GUEST_HEARTBEAT_API_MSG_EVENT_UNPAUSE,
    GUEST_HEARTBEAT_API_MSG_EVENT_RESUME,
    GUEST_HEARTBEAT_API_MSG_EVENT_RESIZE_BEGIN,
    GUEST_HEARTBEAT_API_MSG_EVENT_RESIZE_END,
    GUEST_HEARTBEAT_API_MSG_EVENT_LIVE_MIGRATE_BEGIN,
    GUEST_HEARTBEAT_API_MSG_EVENT_LIVE_MIGRATE_END,
    GUEST_HEARTBEAT_API_MSG_EVENT_COLD_MIGRATE_BEGIN,
    GUEST_HEARTBEAT_API_MSG_EVENT_COLD_MIGRATE_END,
    GUEST_HEARTBEAT_API_MSG_EVENT_MAX,
} GuestHeartbeatApiMsgEventT;

typedef enum {
    GUEST_HEARTBEAT_API_MSG_NOTIFY_UNKNOWN,
    GUEST_HEARTBEAT_API_MSG_NOTIFY_REVOCABLE,
    GUEST_HEARTBEAT_API_MSG_NOTIFY_IRREVOCABLE,
    GUEST_HEARTBEAT_API_MSG_NOTIFY_MAX,
} GuestHeartbeatApiMsgNotifyT;

typedef enum {
    GUEST_HEARTBEAT_API_MSG_VOTE_RESULT_UNKNOWN,
    GUEST_HEARTBEAT_API_MSG_VOTE_RESULT_ACCEPT,
    GUEST_HEARTBEAT_API_MSG_VOTE_RESULT_REJECT,
    GUEST_HEARTBEAT_API_MSG_VOTE_RESULT_COMPLETE,
    GUEST_HEARTBEAT_API_MSG_VOTE_RESULT_MAX,
} GuestHeartbeatApiMsgVoteResultT;

typedef enum {
    GUEST_HEARTBEAT_API_MSG_INIT,
    GUEST_HEARTBEAT_API_MSG_INIT_ACK,
    GUEST_HEARTBEAT_API_MSG_FINAL,
    GUEST_HEARTBEAT_API_MSG_CHALLENGE,
    GUEST_HEARTBEAT_API_MSG_CHALLENGE_RESPONSE,
    GUEST_HEARTBEAT_API_MSG_ACTION_NOTIFY,
    GUEST_HEARTBEAT_API_MSG_ACTION_RESPONSE,
    GUEST_HEARTBEAT_API_MSG_TYPE_MAX,
} GuestHeartbeatApiMsgTypeT;

typedef struct {
    char magic[GUEST_HEARTBEAT_API_MSG_MAGIC_SIZE];
    uint8_t version;
    uint8_t revision;
    uint16_t msg_type;
    uint32_t sequence;
    uint32_t size;
} GuestHeartbeatApiMsgHeaderT;

typedef struct {
    char application_name[GUEST_HEARTBEAT_API_MSG_MAX_APPLICATION_NAME_SIZE];
    uint32_t heartbeat_interval_ms;
    uint32_t vote_secs;
    uint32_t shutdown_notice_secs;
    uint32_t suspend_notice_secs;
    uint32_t resume_notice_secs;
    uint32_t corrective_action;
} GuestHeartbeatApiMsgInitT;

typedef struct {
    uint32_t accepted;
} GuestHeartbeatApiMsgInitAckT;

typedef struct {
    char log_msg[GUEST_HEARTBEAT_API_MSG_MAX_LOG_SIZE];
} GuestHeartbeatApiMsgFinalT;

typedef struct {
    uint32_t heartbeat_challenge;
} GuestHeartbeatApiMsgChallengeT;

typedef struct {
    uint32_t heartbeat_response;
    uint32_t health;
    uint32_t corrective_action;
    char log_msg[GUEST_HEARTBEAT_API_MSG_MAX_LOG_SIZE];
} GuestHeartbeatApiMsgChallengeResponseT;

typedef struct {
    uint32_t invocation_id;
    uint32_t event_type;
    uint32_t notification_type;
} GuestHeartbeatApiMsgActionNotifyT;

typedef struct {
    uint32_t invocation_id;
    uint32_t event_type;
    uint32_t notification_type;
    uint32_t vote_result;
    char log_msg[GUEST_HEARTBEAT_API_MSG_MAX_LOG_SIZE];
} GuestHeartbeatApiMsgActionResponseT;

typedef union {
    GuestHeartbeatApiMsgInitT  init;
    GuestHeartbeatApiMsgInitAckT init_ack;
    GuestHeartbeatApiMsgFinalT final;
    GuestHeartbeatApiMsgChallengeT challenge;
    GuestHeartbeatApiMsgChallengeResponseT challenge_response;
    GuestHeartbeatApiMsgActionNotifyT action_notify;
    GuestHeartbeatApiMsgActionResponseT action_response;
} GuestHeartbeatApiMsgBodyT;

typedef struct {
    GuestHeartbeatApiMsgHeaderT header;
    GuestHeartbeatApiMsgBodyT body;
} GuestHeartbeatApiMsgT;

#ifdef __cplusplus
}
#endif

#endif /* __GUEST_HEARTBEAT_API_MESSAGE_DEFINITIONS_H__ */

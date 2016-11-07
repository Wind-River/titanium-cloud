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
#include "guest_heartbeat_msg_defs.h"
#include "guest_heartbeat_msg.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/eventfd.h>
#include <json-c/json.h>

#include "guest_limits.h"
#include "guest_types.h"
#include "guest_debug.h"
#include "guest_selobj.h"
#include "guest_channel.h"
#include "guest_signal.h"
#include "guest_utils.h"

#include "guest_heartbeat_types.h"

#define GUEST_HEARTBEAT_PROPAGATION_DELAY_IN_SECS           1
#define GUEST_HEARTBEAT_CHALLENGE_DEPTH                     6

static int _signal_fd = -1;
static int _challenge_depth;
static uint32_t _last_tx_challenge[GUEST_HEARTBEAT_CHALLENGE_DEPTH];
static uint32_t _last_rx_challenge;
static uint32_t _msg_sequence;
static GuestChannelIdT _channel_id = GUEST_CHANNEL_ID_INVALID;
static GuestHeartbeatMsgCallbacksT _callbacks;
// Tokener serves as reassembly buffer for host connection.
static struct json_tokener* tok;

// ****************************************************************************
// Guest Heartbeat Message - Action (Host to Network)
// ==================================================
static const char *guest_heartbeat_msg_action_hton(
        GuestHeartbeatActionT action )
{
    switch (action)
    {
        case GUEST_HEARTBEAT_ACTION_NONE:
            return GUEST_HEARTBEAT_MSG_ACTION_NONE;
        case GUEST_HEARTBEAT_ACTION_REBOOT:
            return GUEST_HEARTBEAT_MSG_ACTION_REBOOT;
        case GUEST_HEARTBEAT_ACTION_STOP:
            return GUEST_HEARTBEAT_MSG_ACTION_STOP;
        case GUEST_HEARTBEAT_ACTION_LOG:
            return GUEST_HEARTBEAT_MSG_ACTION_LOG;
        default:
            DPRINTFE("Unknown action %i.", action);
            return GUEST_HEARTBEAT_MSG_ACTION_UNKNOWN;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Action (Network to Host)
// ==================================================
static GuestHeartbeatActionT guest_heartbeat_msg_action_ntoh(
        const char *action )
{
    if (!strcmp(action, GUEST_HEARTBEAT_MSG_ACTION_REBOOT))
    {
        return GUEST_HEARTBEAT_ACTION_REBOOT;
    }
    else if (!strcmp(action, GUEST_HEARTBEAT_MSG_ACTION_STOP)) {
        return GUEST_HEARTBEAT_ACTION_STOP;
    }
    else if (!strcmp(action, GUEST_HEARTBEAT_MSG_ACTION_LOG)) {
        return GUEST_HEARTBEAT_ACTION_LOG;
    }
    else {
        DPRINTFE("Unknown action %i.", action);
        return GUEST_HEARTBEAT_ACTION_UNKNOWN;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Event (Host to Network)
// ==================================================
static const char *guest_heartbeat_msg_event_hton(
        GuestHeartbeatEventT event )
{
    switch (event)
    {
        case GUEST_HEARTBEAT_EVENT_STOP:
            return GUEST_HEARTBEAT_MSG_EVENT_STOP;
        case GUEST_HEARTBEAT_EVENT_REBOOT:
            return GUEST_HEARTBEAT_MSG_EVENT_REBOOT;
        case GUEST_HEARTBEAT_EVENT_SUSPEND:
            return GUEST_HEARTBEAT_MSG_EVENT_SUSPEND;
        case GUEST_HEARTBEAT_EVENT_PAUSE:
            return GUEST_HEARTBEAT_MSG_EVENT_PAUSE;
        case GUEST_HEARTBEAT_EVENT_UNPAUSE:
            return GUEST_HEARTBEAT_MSG_EVENT_UNPAUSE;
        case GUEST_HEARTBEAT_EVENT_RESUME:
            return GUEST_HEARTBEAT_MSG_EVENT_RESUME;
        case GUEST_HEARTBEAT_EVENT_RESIZE_BEGIN:
            return GUEST_HEARTBEAT_MSG_EVENT_RESIZE_BEGIN;
        case GUEST_HEARTBEAT_EVENT_RESIZE_END:
            return GUEST_HEARTBEAT_MSG_EVENT_RESIZE_END;
        case GUEST_HEARTBEAT_EVENT_LIVE_MIGRATE_BEGIN:
            return GUEST_HEARTBEAT_MSG_EVENT_LIVE_MIGRATE_BEGIN;
        case GUEST_HEARTBEAT_EVENT_LIVE_MIGRATE_END:
            return GUEST_HEARTBEAT_MSG_EVENT_LIVE_MIGRATE_END;
        case GUEST_HEARTBEAT_EVENT_COLD_MIGRATE_BEGIN:
            return GUEST_HEARTBEAT_MSG_EVENT_COLD_MIGRATE_BEGIN;
        case GUEST_HEARTBEAT_EVENT_COLD_MIGRATE_END:
            return GUEST_HEARTBEAT_MSG_EVENT_COLD_MIGRATE_END;
        default:
            DPRINTFE("Unknown event %i.", event);
            return GUEST_HEARTBEAT_MSG_EVENT_UNKNOWN;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Event (Network to Host)
// =================================================
static GuestHeartbeatEventT guest_heartbeat_msg_event_ntoh(
        const char *event )
{
    if (!strcmp(event, GUEST_HEARTBEAT_MSG_EVENT_STOP))
    {
        return GUEST_HEARTBEAT_EVENT_STOP;
    }
    else if (!strcmp(event, GUEST_HEARTBEAT_MSG_EVENT_REBOOT)) {
        return GUEST_HEARTBEAT_EVENT_REBOOT;
    }
    else if (!strcmp(event, GUEST_HEARTBEAT_MSG_EVENT_SUSPEND)) {
        return GUEST_HEARTBEAT_EVENT_SUSPEND;
    }
    else if (!strcmp(event, GUEST_HEARTBEAT_MSG_EVENT_PAUSE)) {
        return GUEST_HEARTBEAT_EVENT_PAUSE;
    }
    else if (!strcmp(event, GUEST_HEARTBEAT_MSG_EVENT_UNPAUSE)) {
        return GUEST_HEARTBEAT_EVENT_UNPAUSE;
    }
    else if (!strcmp(event, GUEST_HEARTBEAT_MSG_EVENT_RESUME)) {
        return GUEST_HEARTBEAT_EVENT_RESUME;
    }
    else if (!strcmp(event, GUEST_HEARTBEAT_MSG_EVENT_RESIZE_BEGIN)) {
        return GUEST_HEARTBEAT_EVENT_RESIZE_BEGIN;
    }
    else if (!strcmp(event, GUEST_HEARTBEAT_MSG_EVENT_RESIZE_END)) {
        return GUEST_HEARTBEAT_EVENT_RESIZE_END;
    }
    else if (!strcmp(event, GUEST_HEARTBEAT_MSG_EVENT_LIVE_MIGRATE_BEGIN)) {
        return GUEST_HEARTBEAT_EVENT_LIVE_MIGRATE_BEGIN;
    }
    else if (!strcmp(event, GUEST_HEARTBEAT_MSG_EVENT_LIVE_MIGRATE_END)) {
        return GUEST_HEARTBEAT_EVENT_LIVE_MIGRATE_END;
    }
    else if (!strcmp(event, GUEST_HEARTBEAT_MSG_EVENT_COLD_MIGRATE_BEGIN)) {
        return GUEST_HEARTBEAT_EVENT_COLD_MIGRATE_BEGIN;
    }
    else if (!strcmp(event, GUEST_HEARTBEAT_MSG_EVENT_COLD_MIGRATE_END)) {
        return GUEST_HEARTBEAT_EVENT_COLD_MIGRATE_END;
    }
    else {
        DPRINTFE("Unknown event %i.", event);
        return GUEST_HEARTBEAT_EVENT_UNKNOWN;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Notify (Host to Network)
// ==================================================
static const char *guest_heartbeat_msg_notify_hton(
        GuestHeartbeatNotifyT notify )
{
    switch (notify)
    {
        case GUEST_HEARTBEAT_NOTIFY_REVOCABLE:
            return GUEST_HEARTBEAT_MSG_NOTIFY_REVOCABLE;
        case GUEST_HEARTBEAT_NOTIFY_IRREVOCABLE:
            return GUEST_HEARTBEAT_MSG_NOTIFY_IRREVOCABLE;
        default:
            DPRINTFE("Unknown notify %i.", notify);
            return GUEST_HEARTBEAT_MSG_NOTIFY_UNKNOWN;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Notify (Network to Host)
// ==================================================
static GuestHeartbeatNotifyT guest_heartbeat_msg_notify_ntoh(
       const char *notify )
{
    if (!strcmp(notify, GUEST_HEARTBEAT_MSG_NOTIFY_REVOCABLE))
    {
        return GUEST_HEARTBEAT_NOTIFY_REVOCABLE;
    }
    else if (!strcmp(notify, GUEST_HEARTBEAT_MSG_NOTIFY_IRREVOCABLE)) {
        return GUEST_HEARTBEAT_NOTIFY_IRREVOCABLE;
    }
    else {
        DPRINTFE("Unknown notify %i.", notify);
        return GUEST_HEARTBEAT_NOTIFY_UNKNOWN;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Vote Result (Host to Network)
// =======================================================
static const char * guest_heartbeat_msg_vote_result_hton(
        GuestHeartbeatVoteResultT vote_result )
{
    switch (vote_result)
    {
        case GUEST_HEARTBEAT_VOTE_RESULT_ACCEPT:
            return GUEST_HEARTBEAT_MSG_VOTE_RESULT_ACCEPT;
        case GUEST_HEARTBEAT_VOTE_RESULT_REJECT:
            return GUEST_HEARTBEAT_MSG_VOTE_RESULT_REJECT;
        case GUEST_HEARTBEAT_VOTE_RESULT_COMPLETE:
            return GUEST_HEARTBEAT_MSG_VOTE_RESULT_COMPLETE;
        case GUEST_HEARTBEAT_VOTE_RESULT_TIMEOUT:
            return GUEST_HEARTBEAT_MSG_VOTE_RESULT_TIMEOUT;
        case GUEST_HEARTBEAT_VOTE_RESULT_ERROR:
            return GUEST_HEARTBEAT_MSG_VOTE_RESULT_ERROR;
        default:
            DPRINTFE("Unknown vote result %i.", vote_result);
            return GUEST_HEARTBEAT_MSG_VOTE_RESULT_UNKNOWN;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Vote Result (Network to Host)
// =======================================================
static GuestHeartbeatVoteResultT guest_heartbeat_msg_vote_result_ntoh(
        const char *vote_result )
{
    if (!strcmp(vote_result, GUEST_HEARTBEAT_MSG_VOTE_RESULT_ACCEPT))
    {
        return GUEST_HEARTBEAT_VOTE_RESULT_ACCEPT;
    }
    else if (!strcmp(vote_result, GUEST_HEARTBEAT_MSG_VOTE_RESULT_REJECT)) {
        return GUEST_HEARTBEAT_VOTE_RESULT_REJECT;
    }
    else if (!strcmp(vote_result, GUEST_HEARTBEAT_MSG_VOTE_RESULT_COMPLETE)) {
        return GUEST_HEARTBEAT_VOTE_RESULT_COMPLETE;
    }
    else if (!strcmp(vote_result, GUEST_HEARTBEAT_MSG_VOTE_RESULT_TIMEOUT)) {
        return GUEST_HEARTBEAT_VOTE_RESULT_TIMEOUT;
    }
    else if (!strcmp(vote_result, GUEST_HEARTBEAT_MSG_VOTE_RESULT_ERROR)) {
        return GUEST_HEARTBEAT_VOTE_RESULT_ERROR;
    }
    else {
        DPRINTFE("Unknown vote result %i.", vote_result);
        return GUEST_HEARTBEAT_VOTE_RESULT_UNKNOWN;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Send Init
// ===================================
GuestErrorT guest_heartbeat_msg_send_init(
        int invocation_id, GuestHeartbeatMsgInitDataT* data )
{
    GuestErrorT error;

    char msg[GUEST_HEARTBEAT_MSG_MAX_MSG_SIZE];
    snprintf(msg, sizeof(msg), "\n{\"%s\":%d,\"%s\":%d,\"%s\":\"%s\",\"%s\":%d,"
            "\"%s\":%d,\"%s\":\"%s\","
            "\"%s\":%d,\"%s\":%d,\"%s\":%d,\"%s\":%d,\"%s\":%d,\"%s\":%d,"
            "\"%s\":\"%s\"}\n",
            GUEST_HEARTBEAT_MSG_VERSION, GUEST_HEARTBEAT_MSG_VERSION_CURRENT,
            GUEST_HEARTBEAT_MSG_REVISION, GUEST_HEARTBEAT_MSG_REVISION_CURRENT,
            GUEST_HEARTBEAT_MSG_MSG_TYPE, GUEST_HEARTBEAT_MSG_INIT,
            GUEST_HEARTBEAT_MSG_SEQUENCE, ++_msg_sequence,

            GUEST_HEARTBEAT_MSG_INVOCATION_ID, invocation_id,
            GUEST_HEARTBEAT_MSG_NAME, data->name,

            GUEST_HEARTBEAT_MSG_HEARTBEAT_INTERVAL_MS,
                data->heartbeat_interval_ms,
            GUEST_HEARTBEAT_MSG_VOTE_SECS,
                data->vote_ms/1000 + GUEST_HEARTBEAT_PROPAGATION_DELAY_IN_SECS,
            GUEST_HEARTBEAT_MSG_SHUTDOWN_NOTICE_SECS,
                data->shutdown_notice_ms/1000 + GUEST_HEARTBEAT_PROPAGATION_DELAY_IN_SECS,
            GUEST_HEARTBEAT_MSG_SUSPEND_NOTICE_SECS,
                data->suspend_notice_ms/1000 + GUEST_HEARTBEAT_PROPAGATION_DELAY_IN_SECS,
            GUEST_HEARTBEAT_MSG_RESUME_NOTICE_SECS,
                data->resume_notice_ms/1000 + GUEST_HEARTBEAT_PROPAGATION_DELAY_IN_SECS,
            GUEST_HEARTBEAT_MSG_RESTART_SECS,
                data->restart_ms/1000 + GUEST_HEARTBEAT_PROPAGATION_DELAY_IN_SECS,

            GUEST_HEARTBEAT_MSG_CORRECTIVE_ACTION,
                guest_heartbeat_msg_action_hton(data->corrective_action));

    error = guest_channel_send(_channel_id, msg, strlen(msg));
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to send guest heartbeat init message, error=%s.",
                 guest_error_str(error));
        return error;
    }

    DPRINTFI("Sent heartbeat init message, invocation_id=%i.", invocation_id);
    DPRINTFD("Sent heartbeat init message: %s", msg);
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Send Init Ack
// =======================================
GuestErrorT guest_heartbeat_msg_send_init_ack( int invocation_id )
{
    GuestErrorT error;

    char msg[GUEST_HEARTBEAT_MSG_MAX_MSG_SIZE];
    snprintf(msg, sizeof(msg), "\n{\"%s\":%d,\"%s\":%d,\"%s\":\"%s\",\"%s\":%d,"
            "\"%s\":%d}\n",
            GUEST_HEARTBEAT_MSG_VERSION, GUEST_HEARTBEAT_MSG_VERSION_CURRENT,
            GUEST_HEARTBEAT_MSG_REVISION, GUEST_HEARTBEAT_MSG_REVISION_CURRENT,
            GUEST_HEARTBEAT_MSG_MSG_TYPE, GUEST_HEARTBEAT_MSG_INIT_ACK,
            GUEST_HEARTBEAT_MSG_SEQUENCE, ++_msg_sequence,

            GUEST_HEARTBEAT_MSG_INVOCATION_ID, invocation_id);

    error = guest_channel_send(_channel_id, msg, strlen(msg));
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to send guest heartbeat init ack message, error=%s.",
                 guest_error_str(error));
        return error;
    }

    DPRINTFI("Sent heartbeat init ack message: %s", msg);

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Send Exit
// ===================================
GuestErrorT guest_heartbeat_msg_send_exit( char log_msg[] )
{
    GuestErrorT error;

    char log_msg_buf[GUEST_HEARTBEAT_MSG_MAX_LOG_SIZE];
    snprintf(log_msg_buf, GUEST_HEARTBEAT_MSG_MAX_LOG_SIZE, "%s", log_msg);

    char msg[GUEST_HEARTBEAT_MSG_MAX_MSG_SIZE];
    snprintf(msg, sizeof(msg), "\n{\"%s\":%d,\"%s\":%d,\"%s\":\"%s\",\"%s\":%d,"
            "\"%s\":\"%s\"}\n",
            GUEST_HEARTBEAT_MSG_VERSION, GUEST_HEARTBEAT_MSG_VERSION_CURRENT,
            GUEST_HEARTBEAT_MSG_REVISION, GUEST_HEARTBEAT_MSG_REVISION_CURRENT,
            GUEST_HEARTBEAT_MSG_MSG_TYPE, GUEST_HEARTBEAT_MSG_EXIT,
            GUEST_HEARTBEAT_MSG_SEQUENCE, ++_msg_sequence,

            GUEST_HEARTBEAT_MSG_LOG_MSG, log_msg_buf);

    error = guest_channel_send(_channel_id, msg, strlen(msg));

    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to send guest heartbeat exit message, error=%s.",
                 guest_error_str(error));
        return error;
    }

    DPRINTFI("Sent heartbeat exit message: %s", msg);
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Send Challenge
// ========================================
GuestErrorT guest_heartbeat_msg_send_challenge( void )
{
    GuestErrorT error;

    ++_challenge_depth;
    if (GUEST_HEARTBEAT_CHALLENGE_DEPTH <= _challenge_depth)
        _challenge_depth = 0;

    _last_tx_challenge[_challenge_depth] = rand();

    char msg[GUEST_HEARTBEAT_MSG_MAX_MSG_SIZE];
    snprintf(msg, sizeof(msg), "\n{\"%s\":%d,\"%s\":%d,\"%s\":\"%s\",\"%s\":%d,"
            "\"%s\":%d}\n",
            GUEST_HEARTBEAT_MSG_VERSION, GUEST_HEARTBEAT_MSG_VERSION_CURRENT,
            GUEST_HEARTBEAT_MSG_REVISION, GUEST_HEARTBEAT_MSG_REVISION_CURRENT,
            GUEST_HEARTBEAT_MSG_MSG_TYPE, GUEST_HEARTBEAT_MSG_CHALLENGE,
            GUEST_HEARTBEAT_MSG_SEQUENCE, ++_msg_sequence,

            GUEST_HEARTBEAT_MSG_HEARTBEAT_CHALLENGE, _last_tx_challenge[_challenge_depth]);

    error = guest_channel_send(_channel_id, msg, strlen(msg));

    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to send guest heartbeat challenge message, "
                         "error=%s.", guest_error_str(error));
        return error;
    }

    DPRINTFD("Sent heartbeat challenge message, challenge=%i.",
             _last_tx_challenge[_challenge_depth]);

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Send Challenge Response
// =================================================
GuestErrorT guest_heartbeat_msg_send_challenge_response(
        bool health, GuestHeartbeatActionT corrective_action, char log_msg[] )
{
    GuestErrorT error;

    char log_msg_buf[GUEST_HEARTBEAT_MSG_MAX_LOG_SIZE];
    snprintf(log_msg_buf, GUEST_HEARTBEAT_MSG_MAX_LOG_SIZE, "%s", log_msg);

    char msg[GUEST_HEARTBEAT_MSG_MAX_MSG_SIZE];
    snprintf(msg, sizeof(msg), "\n{\"%s\":%d,\"%s\":%d,\"%s\":\"%s\",\"%s\":%d,"
            "\"%s\":%d,\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":\"%s\"}\n",
            GUEST_HEARTBEAT_MSG_VERSION, GUEST_HEARTBEAT_MSG_VERSION_CURRENT,
            GUEST_HEARTBEAT_MSG_REVISION, GUEST_HEARTBEAT_MSG_REVISION_CURRENT,
            GUEST_HEARTBEAT_MSG_MSG_TYPE, GUEST_HEARTBEAT_MSG_CHALLENGE_RESPONSE,
            GUEST_HEARTBEAT_MSG_SEQUENCE, ++_msg_sequence,

            GUEST_HEARTBEAT_MSG_HEARTBEAT_RESPONSE, _last_rx_challenge,
            GUEST_HEARTBEAT_MSG_HEARTBEAT_HEALTH,
                health ? GUEST_HEARTBEAT_MSG_HEALTHY : GUEST_HEARTBEAT_MSG_UNHEALTHY,
            GUEST_HEARTBEAT_MSG_CORRECTIVE_ACTION,
                guest_heartbeat_msg_action_hton(corrective_action),
            GUEST_HEARTBEAT_MSG_LOG_MSG, log_msg_buf);

    error = guest_channel_send(_channel_id, msg, strlen(msg));

    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to send guest heartbeat challenge response message, "
                 "error=%s.", guest_error_str(error));
        return error;
    }
    // print info logs with message content only if not healthy
    if (!health)
    {
        DPRINTFI("Unhealthy, sent heartbeat challenge response message: %s", msg);
    }
    else {
        DPRINTFD("Sent heartbeat challenge response message, challenge=%i.",
                 _last_rx_challenge);
    }
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Send Action Notify
// ============================================
GuestErrorT guest_heartbeat_msg_send_action_notify(
        int invocation_id, GuestHeartbeatEventT event,
        GuestHeartbeatNotifyT notify, int timeout_ms )
{
    GuestErrorT error;

    char msg[GUEST_HEARTBEAT_MSG_MAX_MSG_SIZE];
    snprintf(msg, sizeof(msg), "\n{\"%s\":%d,\"%s\":%d,\"%s\":\"%s\",\"%s\":%d,"
            "\"%s\":%d,\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":%d}\n",
            GUEST_HEARTBEAT_MSG_VERSION, GUEST_HEARTBEAT_MSG_VERSION_CURRENT,
            GUEST_HEARTBEAT_MSG_REVISION, GUEST_HEARTBEAT_MSG_REVISION_CURRENT,
            GUEST_HEARTBEAT_MSG_MSG_TYPE, GUEST_HEARTBEAT_MSG_ACTION_NOTIFY,
            GUEST_HEARTBEAT_MSG_SEQUENCE, ++_msg_sequence,

            GUEST_HEARTBEAT_MSG_INVOCATION_ID, invocation_id,
            GUEST_HEARTBEAT_MSG_EVENT_TYPE, guest_heartbeat_msg_event_hton(event),
            GUEST_HEARTBEAT_MSG_NOTIFICATION_TYPE, guest_heartbeat_msg_notify_hton(notify),
            GUEST_HEARTBEAT_MSG_TIMEOUT_MS, timeout_ms);

    error = guest_channel_send(_channel_id, msg, strlen(msg));

    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to send guest heartbeat action notify message, "
                 "error=%s.", guest_error_str(error));
        return error;
    }

    DPRINTFI("Sent heartbeat action notify message, invocation_id=%i.",
             invocation_id);
    DPRINTFD("Sent heartbeat action notify message: %s", msg);
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Send Action Response
// ==============================================
GuestErrorT guest_heartbeat_msg_send_action_response(
        int invocation_id, GuestHeartbeatEventT event,
        GuestHeartbeatNotifyT notify, GuestHeartbeatVoteResultT vote_result,
        char log_msg[] )
{
    GuestErrorT error;

    char log_msg_buf[GUEST_HEARTBEAT_MSG_MAX_LOG_SIZE];
    snprintf(log_msg_buf, GUEST_HEARTBEAT_MSG_MAX_LOG_SIZE, "%s", log_msg);


    char msg[GUEST_HEARTBEAT_MSG_MAX_MSG_SIZE];
    snprintf(msg, sizeof(msg), "\n{\"%s\":%d,\"%s\":%d,\"%s\":\"%s\",\"%s\":%d,"
            "\"%s\":%d,\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":\"%s\"}\n",
            GUEST_HEARTBEAT_MSG_VERSION, GUEST_HEARTBEAT_MSG_VERSION_CURRENT,
            GUEST_HEARTBEAT_MSG_REVISION, GUEST_HEARTBEAT_MSG_REVISION_CURRENT,
            GUEST_HEARTBEAT_MSG_MSG_TYPE, GUEST_HEARTBEAT_MSG_ACTION_RESPONSE,
            GUEST_HEARTBEAT_MSG_SEQUENCE, ++_msg_sequence,

            GUEST_HEARTBEAT_MSG_INVOCATION_ID, invocation_id,
            GUEST_HEARTBEAT_MSG_EVENT_TYPE, guest_heartbeat_msg_event_hton(event),
            GUEST_HEARTBEAT_MSG_NOTIFICATION_TYPE, guest_heartbeat_msg_notify_hton(notify),
            GUEST_HEARTBEAT_MSG_VOTE_RESULT, guest_heartbeat_msg_vote_result_hton(vote_result),
            GUEST_HEARTBEAT_MSG_LOG_MSG, log_msg_buf);

    error = guest_channel_send(_channel_id, msg, strlen(msg));
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to send guest heartbeat action response message, "
                         "error=%s.", guest_error_str(error));
        return error;
    }

    DPRINTFI("Sent heartbeat action response message: %s", msg);
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Receive Init
// ======================================
static void guest_heartbeat_msg_recv_init( struct json_object *jobj_msg )
{
    char name[GUEST_HEARTBEAT_MSG_MAX_NAME_SIZE];
    uint32_t invocation_id;
    char corrective_action[GUEST_HEARTBEAT_MSG_MAX_VALUE_SIZE];
    GuestHeartbeatMsgInitDataT data;
    uint32_t vote_secs, shutdown_notice_secs, suspend_notice_secs;
    uint32_t resume_notice_secs, restart_secs;

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_INVOCATION_ID, &invocation_id))
        return;

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_NAME, &name))
        return;

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_HEARTBEAT_INTERVAL_MS, &data.heartbeat_interval_ms))
        return;

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_VOTE_SECS, &vote_secs))
        return;

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_SHUTDOWN_NOTICE_SECS, &shutdown_notice_secs))
        return;

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_SUSPEND_NOTICE_SECS, &suspend_notice_secs))
        return;

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_RESUME_NOTICE_SECS, &resume_notice_secs))
        return;

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_RESTART_SECS, &restart_secs))
        return;

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_CORRECTIVE_ACTION, &corrective_action))
        return;

    data.vote_ms = vote_secs*1000;
    data.shutdown_notice_ms = shutdown_notice_secs*1000;
    data.suspend_notice_ms = suspend_notice_secs*1000;
    data.resume_notice_ms= resume_notice_secs*1000;
    data.restart_ms = restart_secs*1000;

    snprintf(data.name, GUEST_NAME_MAX_CHAR, "%s", name);
    data.corrective_action = guest_heartbeat_msg_action_ntoh(corrective_action);

    DPRINTFI("Heartbeat Init received, invocation_id=%i", invocation_id);

    const char *msg = json_object_to_json_string_ext(jobj_msg, JSON_C_TO_STRING_PLAIN);
    DPRINTFD("Heartbeat Init message received: %s", msg);

    if (NULL != _callbacks.recv_init)
        _callbacks.recv_init(invocation_id, &data);
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Receive Init Ack
// ==========================================
static void guest_heartbeat_msg_recv_init_ack( struct json_object *jobj_msg )
{
    uint32_t invocation_id;

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_INVOCATION_ID, &invocation_id))
        return;

    DPRINTFI("Heartbeat Init Ack received, invocation_id=%i.",
             invocation_id);

    const char *msg = json_object_to_json_string_ext(jobj_msg, JSON_C_TO_STRING_PLAIN);
    DPRINTFD("Heartbeat Init Ack message received: %s", msg);

    if (NULL != _callbacks.recv_init_ack)
        _callbacks.recv_init_ack(invocation_id);
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Receive Exit
// ======================================
static void guest_heartbeat_msg_recv_exit( struct json_object *jobj_msg )
{
    char log_msg[GUEST_HEARTBEAT_MSG_MAX_LOG_SIZE];

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_LOG_MSG, &log_msg))
        return;

    DPRINTFI("Heartbeat Exit received, msg=%s.", log_msg);

    const char *msg = json_object_to_json_string_ext(jobj_msg, JSON_C_TO_STRING_PLAIN);
    DPRINTFD("Heartbeat Exit message received: %s", msg);

    if (NULL != _callbacks.recv_exit)
        _callbacks.recv_exit(log_msg);
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Receive Challenge
// ===========================================
static void guest_heartbeat_msg_recv_challenge( struct json_object *jobj_msg )
{
    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_HEARTBEAT_CHALLENGE, &_last_rx_challenge))
        return;

    DPRINTFD("Heartbeat Challenge received, challenge=%i.", _last_rx_challenge);

    if (NULL != _callbacks.recv_challenge)
        _callbacks.recv_challenge();
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Receive Challenge Ack
// ===============================================
static void guest_heartbeat_msg_recv_challenge_ack( struct json_object *jobj_msg )
{
    char health[GUEST_HEARTBEAT_MSG_MAX_VALUE_SIZE];
    char corrective_action_str[GUEST_HEARTBEAT_MSG_MAX_VALUE_SIZE];
    GuestHeartbeatActionT corrective_action;
    char log_msg[GUEST_HEARTBEAT_MSG_MAX_LOG_SIZE];

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_HEARTBEAT_RESPONSE, &_last_rx_challenge))
        return;

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_HEARTBEAT_HEALTH, &health))
        return;

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_CORRECTIVE_ACTION, &corrective_action_str))
        return;

    corrective_action = guest_heartbeat_msg_action_ntoh(corrective_action_str);

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_LOG_MSG, &log_msg))
        return;

    DPRINTFD("Heartbeat Challenge Response received, challenge=%i.",
             _last_rx_challenge);

    unsigned int challenge_i;
    for (challenge_i=0; GUEST_HEARTBEAT_CHALLENGE_DEPTH > challenge_i;
         ++challenge_i)
    {
        if (_last_tx_challenge[challenge_i] == _last_rx_challenge)
            break;
    }

    if (GUEST_HEARTBEAT_CHALLENGE_DEPTH == challenge_i)
    {
        DPRINTFE("Mismatch between last transmitted challenges and last "
                 "received challenge.");
        return;
    }
    if (NULL != _callbacks.recv_challenge_ack)
        _callbacks.recv_challenge_ack(!strcmp(health, GUEST_HEARTBEAT_MSG_HEALTHY),
                                      corrective_action, log_msg);
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Receive Action Notify
// ===============================================
static void guest_heartbeat_msg_recv_action_notify( struct json_object *jobj_msg )
{
    uint32_t invocation_id;
    char event_type[GUEST_HEARTBEAT_MSG_MAX_VALUE_SIZE];
    char notification_type[GUEST_HEARTBEAT_MSG_MAX_VALUE_SIZE];
    uint32_t timeout_ms;
    GuestHeartbeatEventT event;
    GuestHeartbeatNotifyT notify;

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_INVOCATION_ID, &invocation_id))
        return;

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_EVENT_TYPE, &event_type))
        return;

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_NOTIFICATION_TYPE, &notification_type))
        return;

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_TIMEOUT_MS, &timeout_ms))
        return;

    if (timeout_ms > (GUEST_HEARTBEAT_PROPAGATION_DELAY_IN_SECS*1000))
        timeout_ms -= (GUEST_HEARTBEAT_PROPAGATION_DELAY_IN_SECS * 1000);

    event = guest_heartbeat_msg_event_ntoh(event_type);
    notify = guest_heartbeat_msg_notify_ntoh(notification_type);

    DPRINTFI("Heartbeat Action Notify received, invocation_id=%i.",
             invocation_id);
    const char *msg = json_object_to_json_string_ext(jobj_msg, JSON_C_TO_STRING_PLAIN);
    DPRINTFD("Heartbeat Action Notify message received: %s", msg);

    if (NULL != _callbacks.recv_action_notify)
        _callbacks.recv_action_notify(invocation_id, event, notify, timeout_ms);
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Receive Action Response
// =================================================
static void guest_heartbeat_msg_recv_action_response( struct json_object *jobj_msg )
{
    uint32_t invocation_id;
    char event_type[GUEST_HEARTBEAT_MSG_MAX_VALUE_SIZE];
    char notification_type[GUEST_HEARTBEAT_MSG_MAX_VALUE_SIZE];
    char vote_result[GUEST_HEARTBEAT_MSG_MAX_VALUE_SIZE];
    GuestHeartbeatEventT event;
    GuestHeartbeatNotifyT notify;
    GuestHeartbeatVoteResultT result;
    char log_msg[GUEST_HEARTBEAT_MSG_MAX_LOG_SIZE];

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_INVOCATION_ID, &invocation_id))
        return;

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_EVENT_TYPE, &event_type))
        return;

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_NOTIFICATION_TYPE, &notification_type))
        return;

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_VOTE_RESULT, &vote_result))
        return;

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_LOG_MSG, &log_msg))
        return;

    event = guest_heartbeat_msg_event_ntoh(event_type);
    notify = guest_heartbeat_msg_notify_ntoh(notification_type);
    result = guest_heartbeat_msg_vote_result_ntoh(vote_result);

    DPRINTFI("Heartbeat Action Response received, invocation_id=%i.",
             invocation_id);
    const char *msg = json_object_to_json_string_ext(jobj_msg, JSON_C_TO_STRING_PLAIN);
    DPRINTFD("Heartbeat Action Response message received: %s", msg);

    if (NULL != _callbacks.recv_action_response)
        _callbacks.recv_action_response(invocation_id, event, notify,
                                        result, log_msg);
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Receive Nack
// =================================================
static void guest_heartbeat_msg_recv_nack( struct json_object *jobj_msg )
{
    uint32_t invocation_id;
    char log_msg[GUEST_HEARTBEAT_MSG_MAX_LOG_SIZE];

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_INVOCATION_ID, &invocation_id))
        return;

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_LOG_MSG, &log_msg))
        return;

    DPRINTFE("Heartbeat Nack message received, invocation_id=%i, error msg: %s",
             invocation_id, log_msg);

    const char *msg = json_object_to_json_string_ext(jobj_msg, JSON_C_TO_STRING_PLAIN);
    DPRINTFD("Heartbeat Nack message received: %s", msg);

}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Dispatch
// ==================================
void guest_heartbeat_msg_dispatch(json_object *jobj_msg)
{
    int version;
    char msg_type[GUEST_HEARTBEAT_MSG_MAX_VALUE_SIZE];

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_VERSION, &version))
        return;

    if (GUEST_HEARTBEAT_MSG_VERSION_CURRENT != version)
    {
        DPRINTFI("message received version %d, expected %d, dropping\n",
        version, GUEST_HEARTBEAT_MSG_VERSION_CURRENT);
        return;
    }

    if (guest_utils_json_get_value(jobj_msg, GUEST_HEARTBEAT_MSG_MSG_TYPE, &msg_type))
        return;

    if (!strcmp(msg_type, GUEST_HEARTBEAT_MSG_INIT)) {
        guest_heartbeat_msg_recv_init(jobj_msg);
    } else if (!strcmp(msg_type, GUEST_HEARTBEAT_MSG_INIT_ACK)) {
        guest_heartbeat_msg_recv_init_ack(jobj_msg);
    } else if (!strcmp(msg_type, GUEST_HEARTBEAT_MSG_EXIT)) {
        guest_heartbeat_msg_recv_exit(jobj_msg);
    } else if (!strcmp(msg_type, GUEST_HEARTBEAT_MSG_CHALLENGE)) {
        guest_heartbeat_msg_recv_challenge(jobj_msg);
    } else if (!strcmp(msg_type, GUEST_HEARTBEAT_MSG_CHALLENGE_RESPONSE)) {
        guest_heartbeat_msg_recv_challenge_ack(jobj_msg);
    } else if (!strcmp(msg_type, GUEST_HEARTBEAT_MSG_ACTION_NOTIFY)) {
        guest_heartbeat_msg_recv_action_notify(jobj_msg);
    } else if (!strcmp(msg_type, GUEST_HEARTBEAT_MSG_ACTION_RESPONSE)) {
        guest_heartbeat_msg_recv_action_response(jobj_msg);
    } else if (!strcmp(msg_type, GUEST_HEARTBEAT_MSG_NACK)) {
        guest_heartbeat_msg_recv_nack(jobj_msg);
    } else {
        DPRINTFV("Unknown message type %s.", msg_type);
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Parser
// ==================================
/**
 Multiple messages from the host can be bundled together into a single "read"
 so we need to check message boundaries and handle breaking the message apart.
 Assume a valid message does not contain newline '\n', and newline is added to
 the beginning and end of each message by the sender to delimit the boundaries.
*/
void guest_heartbeat_msg_parser(void *buf, ssize_t len, json_tokener* tok, int newline_found)
{
    json_object *jobj = json_tokener_parse_ex(tok, buf, len);
    enum json_tokener_error jerr = json_tokener_get_error(tok);

    if (jerr == json_tokener_success) {
        guest_heartbeat_msg_dispatch(jobj);
        json_object_put(jobj);
        return;
    }

    else if (jerr == json_tokener_continue) {
        // partial JSON is parsed , continue to read from socket.
        if (newline_found) {
            // if newline was found in the middle of the buffer, the message
            // should be completed at this point. Throw out incomplete message
            // by resetting tokener.
            json_tokener_reset(tok);
        }
    }
    else
    {
        // parsing error
        json_tokener_reset(tok);
    }
}
// ****************************************************************************


// ****************************************************************************
// Guest Heartbeat Message - Handler
// ==================================
void guest_heartbeat_msg_handler(void *buf, ssize_t len,json_tokener* tok)
{
    void *newline;
    ssize_t len_head;

next:
    if (len == 0)
        return;

    // search for newline as delimiter
    newline = memchr(buf, '\n', len);

    if (newline) {
        // split buffer to head and tail at the location of newline.
        // feed the head to the parser and recursively process the tail.
        len_head = newline-buf;

        // parse head
        if (len_head > 0)
            guest_heartbeat_msg_parser(buf, len_head, tok, 1);

        // start of the tail: skip newline
        buf += len_head+1;
        // length of the tail: deduct 1 for the newline character
        len -= len_head+1;

        // continue to process the tail.
        goto next;
    }
    else {
        guest_heartbeat_msg_parser(buf, len, tok, 0);
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Receive
// ==================================
static void guest_heartbeat_msg_receiver( int selobj )
{
    int bytes_received;
    GuestErrorT error;
    char buf[4096];

    error = guest_channel_receive(_channel_id, buf, sizeof(buf),
                                  &bytes_received);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to receive message, error=%s.",
                 guest_error_str(error));
        return;
    }

    DPRINTFV("Bytes received is %i.", bytes_received);
    guest_heartbeat_msg_handler(buf, bytes_received, tok);

}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Signal Handler
// ========================================
static void guest_heartbeat_msg_signal_handler( int signum )
{
    int64_t sigval = signum;
    int result;

    if ((SIGIO == signum) && (0 <= _signal_fd))
    {
        result = write(_signal_fd, &sigval, sizeof(sigval));
        if (0 > result)
        {
            DPRINTFE("Failed to write signal, error=%s", strerror(errno));
            return;
        }

        guest_signal_ignore(signum);
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Hangup
// ================================
static void guest_heartbeat_msg_hangup( int selobj )
{
    DPRINTFI("Heartbeat messaging hangup.");

    if (GUEST_CHANNEL_ID_INVALID != _channel_id)
    {
        int selobj;

        selobj = guest_channel_get_selobj(_channel_id);
        if (0 <= selobj)
        {
            GuestErrorT error;

            error = guest_selobj_deregister(selobj);
            if (GUEST_OKAY != error)
            {
                DPRINTFE("Failed to deregister selection object %i, "
                         "error=%s.", selobj, guest_error_str(error));
            }

            guest_signal_register_handler(SIGIO,
                                          guest_heartbeat_msg_signal_handler);

            if (NULL != _callbacks.channel_state_change)
                _callbacks.channel_state_change(false);
        }
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Signal Dispatch
// =========================================
static void guest_heartbeat_msg_signal_dispatch( int selobj )
{
    int signum;
    int64_t sigval = 0;
    int result;
    GuestSelObjCallbacksT callbacks;
    GuestErrorT error;

    result = read(_signal_fd, &sigval, sizeof(sigval));
    if (0 > result)
    {
        if (EINTR == errno)
        {
            DPRINTFD("Interrupted on signal read, error=%s.", strerror(errno));
        } else {
            DPRINTFE("Failed to dispatch signal, error=%s.", strerror(errno));
        }
        return;
    }

    signum = sigval;

    if (SIGIO == signum)
    {
        DPRINTFI("Heartbeat messaging available.");

        if (GUEST_CHANNEL_ID_INVALID != _channel_id)
        {
            selobj = guest_channel_get_selobj(_channel_id);
            if (0 <= selobj)
            {
                memset(&callbacks, 0, sizeof(callbacks));
                callbacks.read_callback = guest_heartbeat_msg_receiver;
                callbacks.hangup_callback = guest_heartbeat_msg_hangup;

                error = guest_selobj_register(selobj, &callbacks);
                if (GUEST_OKAY != error)
                {
                    DPRINTFE("Failed to register selection object %i, "
                             "error=%s.", selobj, guest_error_str(error));
                    abort();
                }

                if (NULL != _callbacks.channel_state_change)
                    _callbacks.channel_state_change(true);
            }
        }
    } else {
        DPRINTFI("Ignoring signal %i.", signum);
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Initialize
// ====================================
GuestErrorT guest_heartbeat_msg_initialize(
        char* comm_device, GuestHeartbeatMsgCallbacksT* callbacks )
{
    int selobj;
    GuestSelObjCallbacksT selobj_callbacks;
    GuestErrorT error;

    _channel_id = GUEST_CHANNEL_ID_INVALID;

    error = guest_channel_open(comm_device, &_channel_id);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to open communication channel over device %s, "
                 "error=%s.", comm_device, guest_error_str(error));
        return error;
    }

    selobj = guest_channel_get_selobj(_channel_id);
    if (0 <= selobj)
    {
        memset(&selobj_callbacks, 0, sizeof(selobj_callbacks));
        selobj_callbacks.read_callback = guest_heartbeat_msg_receiver;
        selobj_callbacks.hangup_callback = guest_heartbeat_msg_hangup;

        error = guest_selobj_register(selobj, &selobj_callbacks);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to register selection object %i, error=%s.",
                     selobj, guest_error_str(error));
            return error;
        }
    }

    _signal_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (0 > _signal_fd)
    {
        DPRINTFE("Failed to open signal file descriptor,error=%s.",
                 strerror(errno));
        return GUEST_FAILED;
    }

    memset(&selobj_callbacks, 0, sizeof(selobj_callbacks));
    selobj_callbacks.read_callback = guest_heartbeat_msg_signal_dispatch;

    error = guest_selobj_register(_signal_fd, &selobj_callbacks);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to register selection object, error=%s.",
                 guest_error_str(error));
        close(_signal_fd);
        _signal_fd = -1;
        return error;
    }

    memcpy(&_callbacks, callbacks, sizeof(GuestHeartbeatMsgCallbacksT));

    tok = json_tokener_new();

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Finalize
// ==================================
GuestErrorT guest_heartbeat_msg_finalize( void )
{
    int selobj;
    GuestErrorT error;

    memset(&_callbacks, 0, sizeof(GuestHeartbeatMsgCallbacksT));
    free(tok);

    if (0 <= _signal_fd)
    {
        error = guest_selobj_deregister(_signal_fd);
        if (GUEST_OKAY != error)
            DPRINTFE("Failed to deregister selection object, error=%s.",
                     guest_error_str(error));

        close(_signal_fd);
        _signal_fd = -1;
    }

    if (GUEST_CHANNEL_ID_INVALID != _channel_id)
    {
        selobj = guest_channel_get_selobj(_channel_id);
        if (0 <= selobj)
        {
            error = guest_selobj_deregister(selobj);
            if (GUEST_OKAY != error)
            {
                DPRINTFE("Failed to deregister selection object %i, error=%s.",
                         selobj, guest_error_str(error));
            }
        }

        error = guest_channel_close(_channel_id);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed close communication channel, error=%s.",
                     guest_error_str(error));
        }
        _channel_id = GUEST_CHANNEL_ID_INVALID;
    }

    return GUEST_OKAY;
}
// ****************************************************************************

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
#include "guest_heartbeat_api.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "guest_api_types.h"
#include "guest_api_debug.h"
#include "guest_api_unix.h"
#include "guest_api_stream.h"

#include "guest_heartbeat_api_msg_defs.h"

static int _sock = -1;
static bool _connected = false;
static uint32_t _msg_sequence;
static GuestApiStreamT _stream;
static GuestHeartbeatApiCallbacksT _callbacks;

// ****************************************************************************
// Guest Heartbeat API - Action String
// ===================================
const char* guest_heartbeat_api_action_str( GuestHeartbeatApiActionT action )
{
    switch (action)
    {
        case GUEST_HEARTBEAT_API_ACTION_NONE:   return "none";
        case GUEST_HEARTBEAT_API_ACTION_REBOOT: return "reboot";
        case GUEST_HEARTBEAT_API_ACTION_STOP:   return "stop";
        case GUEST_HEARTBEAT_API_ACTION_LOG:    return "log";
        default:
            return "action-???";
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Event String
// ==================================
const char* guest_heartbeat_api_event_str( GuestHeartbeatApiEventT event )
{
    switch (event)
    {
        case GUEST_HEARTBEAT_API_EVENT_STOP:               return "stop";
        case GUEST_HEARTBEAT_API_EVENT_REBOOT:             return "reboot";
        case GUEST_HEARTBEAT_API_EVENT_SUSPEND:            return "suspend";
        case GUEST_HEARTBEAT_API_EVENT_PAUSE:              return "pause";
        case GUEST_HEARTBEAT_API_EVENT_UNPAUSE:            return "unpause";
        case GUEST_HEARTBEAT_API_EVENT_RESUME:             return "resume";
        case GUEST_HEARTBEAT_API_EVENT_RESIZE_BEGIN:       return "resize-begin";
        case GUEST_HEARTBEAT_API_EVENT_RESIZE_END:         return "resize-end";
        case GUEST_HEARTBEAT_API_EVENT_LIVE_MIGRATE_BEGIN: return "live-migrate-begin";
        case GUEST_HEARTBEAT_API_EVENT_LIVE_MIGRATE_END:   return "live-migrate-end";
        case GUEST_HEARTBEAT_API_EVENT_COLD_MIGRATE_BEGIN: return "cold-migrate-begin";
        case GUEST_HEARTBEAT_API_EVENT_COLD_MIGRATE_END:   return "cold-migrate-end";
        default:
            return "event-???";
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Notify String
// ===================================
const char* guest_heartbeat_api_notify_str(
        GuestHeartbeatApiNotifyTypeT notify )
{
    switch (notify)
    {
        case GUEST_HEARTBEAT_API_NOTIFY_TYPE_REVOCABLE:   return "revocable";
        case GUEST_HEARTBEAT_API_NOTIFY_TYPE_IRREVOCABLE: return "irrevocable";
        default:
            return "notify-???";
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Vote Result String
// ========================================
const char* guest_heartbeat_api_vote_result_str(
        GuestHeartbeatApiVoteResultT vote_result )
{
    switch (vote_result)
    {
        case GUEST_HEARTBEAT_API_VOTE_RESULT_ACCEPT:   return "accept";
        case GUEST_HEARTBEAT_API_VOTE_RESULT_REJECT:   return "reject";
        case GUEST_HEARTBEAT_API_VOTE_RESULT_COMPLETE: return "complete";
        default:
            return "vote-???";
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Action (Host to Network)
// ==============================================
static GuestHeartbeatApiMsgActionT guest_heartbeat_api_action_hton(
        GuestHeartbeatApiActionT action )
{
    switch (action)
    {
        case GUEST_HEARTBEAT_API_ACTION_NONE:
            return GUEST_HEARTBEAT_API_MSG_ACTION_NONE;
        case GUEST_HEARTBEAT_API_ACTION_REBOOT:
            return GUEST_HEARTBEAT_API_MSG_ACTION_REBOOT;
        case GUEST_HEARTBEAT_API_ACTION_STOP:
            return GUEST_HEARTBEAT_API_MSG_ACTION_STOP;
        case GUEST_HEARTBEAT_API_ACTION_LOG:
            return GUEST_HEARTBEAT_API_MSG_ACTION_LOG;
        default:
            DPRINTFE("Unknown action %i.", action);
            return GUEST_HEARTBEAT_API_MSG_ACTION_UNKNOWN;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Event (Host to Network)
// =============================================
static GuestHeartbeatApiMsgEventT guest_heartbeat_api_event_hton(
        GuestHeartbeatApiEventT event )
{
    switch (event)
    {
        case GUEST_HEARTBEAT_API_EVENT_STOP:
            return GUEST_HEARTBEAT_API_MSG_EVENT_STOP;
        case GUEST_HEARTBEAT_API_EVENT_REBOOT:
            return GUEST_HEARTBEAT_API_MSG_EVENT_REBOOT;
        case GUEST_HEARTBEAT_API_EVENT_SUSPEND:
            return GUEST_HEARTBEAT_API_MSG_EVENT_SUSPEND;
        case GUEST_HEARTBEAT_API_EVENT_PAUSE:
            return GUEST_HEARTBEAT_API_MSG_EVENT_PAUSE;
        case GUEST_HEARTBEAT_API_EVENT_UNPAUSE:
            return GUEST_HEARTBEAT_API_MSG_EVENT_UNPAUSE;
        case GUEST_HEARTBEAT_API_EVENT_RESUME:
            return GUEST_HEARTBEAT_API_MSG_EVENT_RESUME;
        case GUEST_HEARTBEAT_API_EVENT_RESIZE_BEGIN:
            return GUEST_HEARTBEAT_API_MSG_EVENT_RESIZE_BEGIN;
        case GUEST_HEARTBEAT_API_EVENT_RESIZE_END:
            return GUEST_HEARTBEAT_API_MSG_EVENT_RESIZE_END;
        case GUEST_HEARTBEAT_API_EVENT_LIVE_MIGRATE_BEGIN:
            return GUEST_HEARTBEAT_API_MSG_EVENT_LIVE_MIGRATE_BEGIN;
        case GUEST_HEARTBEAT_API_EVENT_LIVE_MIGRATE_END:
            return GUEST_HEARTBEAT_API_MSG_EVENT_LIVE_MIGRATE_END;
        case GUEST_HEARTBEAT_API_EVENT_COLD_MIGRATE_BEGIN:
            return GUEST_HEARTBEAT_API_MSG_EVENT_COLD_MIGRATE_BEGIN;
        case GUEST_HEARTBEAT_API_EVENT_COLD_MIGRATE_END:
            return GUEST_HEARTBEAT_API_MSG_EVENT_COLD_MIGRATE_END;
        default:
            DPRINTFE("Unknown event %i.", event);
            return GUEST_HEARTBEAT_API_MSG_EVENT_UNKNOWN;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Event (Network to Host)
// =============================================
static GuestHeartbeatApiEventT guest_heartbeat_api_event_ntoh(
        GuestHeartbeatApiMsgEventT event )
{
    switch (event)
    {
        case GUEST_HEARTBEAT_API_MSG_EVENT_STOP:
            return GUEST_HEARTBEAT_API_EVENT_STOP;
        case GUEST_HEARTBEAT_API_MSG_EVENT_REBOOT:
            return GUEST_HEARTBEAT_API_EVENT_REBOOT;
        case GUEST_HEARTBEAT_API_MSG_EVENT_SUSPEND:
            return GUEST_HEARTBEAT_API_EVENT_SUSPEND;
        case GUEST_HEARTBEAT_API_MSG_EVENT_PAUSE:
            return GUEST_HEARTBEAT_API_EVENT_PAUSE;
        case GUEST_HEARTBEAT_API_MSG_EVENT_UNPAUSE:
            return GUEST_HEARTBEAT_API_EVENT_UNPAUSE;
        case GUEST_HEARTBEAT_API_MSG_EVENT_RESUME:
            return GUEST_HEARTBEAT_API_EVENT_RESUME;
        case GUEST_HEARTBEAT_API_MSG_EVENT_RESIZE_BEGIN:
            return GUEST_HEARTBEAT_API_EVENT_RESIZE_BEGIN;
        case GUEST_HEARTBEAT_API_MSG_EVENT_RESIZE_END:
            return GUEST_HEARTBEAT_API_EVENT_RESIZE_END;
        case GUEST_HEARTBEAT_API_MSG_EVENT_LIVE_MIGRATE_BEGIN:
            return GUEST_HEARTBEAT_API_EVENT_LIVE_MIGRATE_BEGIN;
        case GUEST_HEARTBEAT_API_MSG_EVENT_LIVE_MIGRATE_END:
            return GUEST_HEARTBEAT_API_EVENT_LIVE_MIGRATE_END;
        case GUEST_HEARTBEAT_API_MSG_EVENT_COLD_MIGRATE_BEGIN:
            return GUEST_HEARTBEAT_API_EVENT_COLD_MIGRATE_BEGIN;
        case GUEST_HEARTBEAT_API_MSG_EVENT_COLD_MIGRATE_END:
            return GUEST_HEARTBEAT_API_EVENT_COLD_MIGRATE_END;
        default:
            DPRINTFE("Unknown event %i.", event);
            return GUEST_HEARTBEAT_API_EVENT_UNKNOWN;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Notify (Host to Network)
// ==============================================
static GuestHeartbeatApiMsgNotifyT guest_heartbeat_api_notify_hton(
        GuestHeartbeatApiNotifyTypeT notify )
{
    switch (notify)
    {
        case GUEST_HEARTBEAT_API_NOTIFY_TYPE_REVOCABLE:
            return GUEST_HEARTBEAT_API_MSG_NOTIFY_REVOCABLE;
        case GUEST_HEARTBEAT_API_NOTIFY_TYPE_IRREVOCABLE:
            return GUEST_HEARTBEAT_API_MSG_NOTIFY_IRREVOCABLE;
        default:
            DPRINTFE("Unknown notify type %i.", notify);
            return GUEST_HEARTBEAT_API_MSG_NOTIFY_UNKNOWN;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Notify (Network to Host)
// ==============================================
static GuestHeartbeatApiNotifyTypeT guest_heartbeat_api_notify_ntoh(
        GuestHeartbeatApiMsgNotifyT notify )
{
    switch (notify)
    {
        case GUEST_HEARTBEAT_API_MSG_NOTIFY_REVOCABLE:
            return GUEST_HEARTBEAT_API_NOTIFY_TYPE_REVOCABLE;
        case GUEST_HEARTBEAT_API_MSG_NOTIFY_IRREVOCABLE:
            return GUEST_HEARTBEAT_API_NOTIFY_TYPE_IRREVOCABLE;
        default:
            DPRINTFE("Unknown notify type %i.", notify);
            return GUEST_HEARTBEAT_API_NOTIFY_TYPE_UNKNOWN;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Vote Result (Host to Network)
// ===================================================
static GuestHeartbeatApiMsgVoteResultT guest_heartbeat_api_vote_result_hton(
        GuestHeartbeatApiVoteResultT vote_result )
{
    switch (vote_result)
    {
        case GUEST_HEARTBEAT_API_VOTE_RESULT_ACCEPT:
            return GUEST_HEARTBEAT_API_MSG_VOTE_RESULT_ACCEPT;
        case GUEST_HEARTBEAT_API_VOTE_RESULT_REJECT:
            return GUEST_HEARTBEAT_API_MSG_VOTE_RESULT_REJECT;
        case GUEST_HEARTBEAT_API_VOTE_RESULT_COMPLETE:
            return GUEST_HEARTBEAT_API_MSG_VOTE_RESULT_COMPLETE;
        default:
            DPRINTFE("Unknown vote result %i.", vote_result);
            return GUEST_HEARTBEAT_API_MSG_VOTE_RESULT_UNKNOWN;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Register
// ==============================
GuestApiErrorT guest_heartbeat_api_register(
        GuestHeartbeatApiInitDataT* init_data )
{
    GuestHeartbeatApiMsgT msg;
    GuestHeartbeatApiMsgHeaderT* hdr = &(msg.header);
    GuestHeartbeatApiMsgInitT* bdy = &(msg.body.init);
    GuestApiErrorT error;

    if (0 > _sock)
    {
        error = guest_api_unix_open(&_sock);
        if (GUEST_API_OKAY != error)
        {
            DPRINTFE("Failed to open unix socket, error=%s.",
                     guest_api_error_str(error));
            return error;
        }
    }

    if (!_connected)
    {
        error = guest_api_unix_connect(_sock, GUEST_HEARTBEAT_API_MSG_ADDRESS);
        if (GUEST_API_OKAY != error)
        {
            if (GUEST_API_TRY_AGAIN != error)
            {
                DPRINTFD("Failed to connect unix socket, error=%s.",
                         guest_api_error_str(error));
                return error;
            } else {
                DPRINTFE("Failed to connect unix socket, error=%s.",
                         guest_api_error_str(error));
                return error;
            }
        }

        _connected = true;
    }

    memset(&msg, 0, sizeof(msg));

    memcpy(&(hdr->magic), GUEST_HEARTBEAT_API_MSG_MAGIC_VALUE,
           GUEST_HEARTBEAT_API_MSG_MAGIC_SIZE);
    hdr->version = GUEST_HEARTBEAT_API_MSG_VERSION_CURRENT;
    hdr->revision = GUEST_HEARTBEAT_API_MSG_REVISION_CURRENT;
    hdr->msg_type = GUEST_HEARTBEAT_API_MSG_INIT;
    hdr->sequence = ++_msg_sequence;
    hdr->size = sizeof(msg);

    snprintf(bdy->application_name,
             GUEST_HEARTBEAT_API_MSG_MAX_APPLICATION_NAME_SIZE, "%s",
             init_data->application_name);
    bdy->heartbeat_interval_ms = init_data->heartbeat_interval_ms;
    bdy->vote_secs = init_data->vote_secs;
    bdy->shutdown_notice_secs = init_data->shutdown_notice_secs;
    bdy->suspend_notice_secs = init_data->suspend_notice_secs;
    bdy->resume_notice_secs = init_data->resume_notice_secs;
    bdy->corrective_action
            = guest_heartbeat_api_action_hton(init_data->corrective_action);

    error = guest_api_unix_send(_sock, &msg, sizeof(msg));
    if (GUEST_API_OKAY != error)
    {
        DPRINTFE("Failed to send guest heartbeat register message, error=%s.",
                 guest_api_error_str(error));
        return error;
    }

    DPRINTFD("Sent register request.");
    return GUEST_API_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Deregister
// ================================
GuestApiErrorT guest_heartbeat_api_deregister( char log_msg[] )
{
    GuestHeartbeatApiMsgT msg;
    GuestHeartbeatApiMsgHeaderT* hdr = &(msg.header);
    GuestHeartbeatApiMsgFinalT* bdy = &(msg.body.final);
    GuestApiErrorT error;

    if (!_connected)
    {
        DPRINTFD("Not connected.");
        return GUEST_API_OKAY;
    }

    memset(&msg, 0, sizeof(msg));

    memcpy(&(hdr->magic), GUEST_HEARTBEAT_API_MSG_MAGIC_VALUE,
           GUEST_HEARTBEAT_API_MSG_MAGIC_SIZE);
    hdr->version = GUEST_HEARTBEAT_API_MSG_VERSION_CURRENT;
    hdr->revision = GUEST_HEARTBEAT_API_MSG_REVISION_CURRENT;
    hdr->msg_type = GUEST_HEARTBEAT_API_MSG_FINAL;
    hdr->sequence = ++_msg_sequence;
    hdr->size = sizeof(msg);

    snprintf(bdy->log_msg, GUEST_HEARTBEAT_API_MSG_MAX_LOG_SIZE, "%s", log_msg);

    error = guest_api_unix_send(_sock, &msg, sizeof(msg));
    if (GUEST_API_OKAY != error)
    {
        DPRINTFE("Failed to send guest heartbeat api deregister message, "
                 "error=%s.", guest_api_error_str(error));
        return error;
    }

    return GUEST_API_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Send Action Response
// ==========================================
GuestApiErrorT guest_heartbeat_api_send_action_response(
        int invocation_id, GuestHeartbeatApiEventT event,
        GuestHeartbeatApiNotifyTypeT notify_type,
        GuestHeartbeatApiVoteResultT vote_result, char log_msg[] )
{
    GuestHeartbeatApiMsgT msg;
    GuestHeartbeatApiMsgHeaderT* hdr = &(msg.header);
    GuestHeartbeatApiMsgActionResponseT* bdy = &(msg.body.action_response);
    GuestApiErrorT error;

    if (!_connected)
    {
        DPRINTFD("Not connected.");
        return GUEST_API_OKAY;
    }

    memset(&msg, 0, sizeof(msg));

    memcpy(&(hdr->magic), GUEST_HEARTBEAT_API_MSG_MAGIC_VALUE,
           GUEST_HEARTBEAT_API_MSG_MAGIC_SIZE);
    hdr->version = GUEST_HEARTBEAT_API_MSG_VERSION_CURRENT;
    hdr->revision = GUEST_HEARTBEAT_API_MSG_REVISION_CURRENT;
    hdr->msg_type = GUEST_HEARTBEAT_API_MSG_ACTION_RESPONSE;
    hdr->sequence = ++_msg_sequence;
    hdr->size = sizeof(msg);

    bdy->invocation_id = invocation_id;
    bdy->event_type = guest_heartbeat_api_event_hton(event);
    bdy->notification_type = guest_heartbeat_api_notify_hton(notify_type);
    bdy->vote_result = guest_heartbeat_api_vote_result_hton(vote_result);
    snprintf(bdy->log_msg, GUEST_HEARTBEAT_API_MSG_MAX_LOG_SIZE, "%s", log_msg);

    error = guest_api_unix_send(_sock, &msg, sizeof(msg));
    if (GUEST_API_OKAY != error)
    {
        DPRINTFE("Failed to send guest heartbeat api action response message, "
                 "error=%s.", guest_api_error_str(error));
        return error;
    }

    return GUEST_API_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Api - Receive Init Ack
// ======================================
static void guest_heartbeat_api_recv_init_ack( void )
{
    uint32_t accepted;
    char* ptr = _stream.bytes + sizeof(GuestHeartbeatApiMsgHeaderT);

    accepted = *(uint32_t*) ptr;
    ptr += sizeof(uint32_t);

    DPRINTFI("Registration %s.", accepted ? "accepted" : "not accepted");

    if (NULL != _callbacks.register_state)
        _callbacks.register_state(accepted);
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Api - Receive Challenge
// =======================================
static void guest_heartbeat_api_recv_challenge( void )
{
    bool health = true;
    GuestHeartbeatApiActionT corrective_action = GUEST_HEARTBEAT_API_ACTION_NONE;
    char log_msg[GUEST_HEARTBEAT_API_LOG_MAX] = "";
    int heartbeat_challenge;
    GuestHeartbeatApiMsgT msg;
    GuestHeartbeatApiMsgHeaderT* hdr = &(msg.header);
    GuestHeartbeatApiMsgChallengeResponseT* bdy = &(msg.body.challenge_response);
    GuestApiErrorT error;
    char* ptr = _stream.bytes + sizeof(GuestHeartbeatApiMsgHeaderT);

    heartbeat_challenge = *(uint32_t*) ptr;
    ptr += sizeof(uint32_t);

    if (NULL != _callbacks.health_check)
        _callbacks.health_check(&health, &corrective_action, log_msg);

    memset(&msg, 0, sizeof(msg));

    memcpy(&(hdr->magic), GUEST_HEARTBEAT_API_MSG_MAGIC_VALUE,
           GUEST_HEARTBEAT_API_MSG_MAGIC_SIZE);
    hdr->version = GUEST_HEARTBEAT_API_MSG_VERSION_CURRENT;
    hdr->revision = GUEST_HEARTBEAT_API_MSG_REVISION_CURRENT;
    hdr->msg_type = GUEST_HEARTBEAT_API_MSG_CHALLENGE_RESPONSE;
    hdr->sequence = ++_msg_sequence;
    hdr->size = sizeof(msg);

    bdy->heartbeat_response = heartbeat_challenge;
    bdy->health = health;
    bdy->corrective_action
            = guest_heartbeat_api_action_hton(corrective_action);
    snprintf(bdy->log_msg, GUEST_HEARTBEAT_API_MSG_MAX_LOG_SIZE, "%s", log_msg);

    error = guest_api_unix_send(_sock, &msg, sizeof(msg));
    if (GUEST_API_OKAY != error)
    {
        DPRINTFE("Failed to send guest heartbeat api challenge response "
                 "message, error=%s.", guest_api_error_str(error));
        return;
    }

    DPRINTFD("Sent guest heartbeat api challenge response sent.");
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Api - Receive Action Notify
// ===========================================
static void guest_heartbeat_api_recv_action_notify( void )
{
    int invocation_id;
    GuestHeartbeatApiEventT event;
    GuestHeartbeatApiNotifyTypeT notify_type;
    char* ptr = _stream.bytes + sizeof(GuestHeartbeatApiMsgHeaderT);

    invocation_id = *(uint32_t*) ptr;
    ptr += sizeof(uint32_t);
    event = guest_heartbeat_api_event_ntoh(*(uint32_t*) ptr);
    ptr += sizeof(uint32_t);
    notify_type = guest_heartbeat_api_notify_ntoh(*(uint32_t*) ptr);
    ptr += sizeof(uint32_t);

    if (NULL != _callbacks.action_notify)
        _callbacks.action_notify(invocation_id, event, notify_type);
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Dispatch
// ==============================
void guest_heartbeat_api_dispatch( int selobj )
{
    static bool have_start = false;
    static bool have_header = false;
    static GuestHeartbeatApiMsgHeaderT hdr;

    bool more;
    int bytes_received;
    GuestApiErrorT error;

    if (selobj != _sock)
        return;

    error = guest_api_unix_receive(_sock, _stream.end_ptr, _stream.avail,
                                   &bytes_received);
    if (GUEST_API_OKAY != error)
    {
        DPRINTFE("Failed to receive message, error=%s.",
                 guest_api_error_str(error));
        return;
    }

    if (0 == bytes_received)
    {
        DPRINTFI("Registration dropped.");
        _connected = false;

        error = guest_api_unix_close(_sock);
        if (GUEST_API_OKAY != error)
        {
            DPRINTFE("Failed to close unix socket, error=%s.",
                     guest_api_error_str(error));
        }
        _sock = -1;

        if (NULL != _callbacks.register_state)
            _callbacks.register_state(false);
    }

    DPRINTFV("Bytes received is %i.", bytes_received);

    _stream.end_ptr += bytes_received;
    _stream.avail -= bytes_received;
    _stream.size += bytes_received;

    do
    {
        more = false;

        if (!have_start)
        {
            memset(&hdr, 0, sizeof(GuestHeartbeatApiMsgHeaderT));
            have_start = guest_api_stream_get_next(&_stream);
        }

        if (have_start && !have_header)
        {
            if (sizeof(GuestHeartbeatApiMsgHeaderT) <= _stream.size)
            {
                char *ptr = _stream.bytes + GUEST_HEARTBEAT_API_MSG_MAGIC_SIZE;

                hdr.version = *(uint8_t*) ptr;
                ptr += sizeof(uint8_t);
                hdr.revision = *(uint8_t*) ptr;
                ptr += sizeof(uint8_t);
                hdr.msg_type = *(uint16_t*) ptr;
                ptr += sizeof(uint16_t);
                hdr.sequence = *(uint32_t*) ptr;
                ptr += sizeof(uint32_t);
                hdr.size = *(uint32_t*) ptr;
                ptr += sizeof(uint32_t);

                DPRINTFD("Message header: version=%i, revision=%i, "
                         "msg_type=%i, sequence=%u, size=%u", hdr.version,
                         hdr.revision, hdr.msg_type, hdr.sequence, hdr.size);

                if (GUEST_HEARTBEAT_API_MSG_VERSION_CURRENT == hdr.version)
                {
                    have_header = true;
                } else {
                    have_start = false;
                    have_header = false;
                    guest_api_stream_advance(GUEST_HEARTBEAT_API_MSG_MAGIC_SIZE,
                                             &_stream);
                    more = true;
                }
            }
        }

        if (have_start && have_header)
        {
            if (sizeof(GuestHeartbeatApiMsgT) <= _stream.size)
            {
                switch (hdr.msg_type)
                {
                    case GUEST_HEARTBEAT_API_MSG_INIT_ACK:
                        guest_heartbeat_api_recv_init_ack();
                        break;

                    case GUEST_HEARTBEAT_API_MSG_CHALLENGE:
                        guest_heartbeat_api_recv_challenge();
                        break;

                    case GUEST_HEARTBEAT_API_MSG_ACTION_NOTIFY:
                        guest_heartbeat_api_recv_action_notify();
                        break;

                    default:
                        DPRINTFV("Unknown message type %i.",
                                 (int) hdr.msg_type);
                        break;
                }

                have_start = false;
                have_header = false;
                guest_api_stream_advance(sizeof(GuestHeartbeatApiMsgT),
                                         &_stream);
                more = true;
            }
        }
    } while (more);

    if (0 >= _stream.avail)
        guest_api_stream_reset(&_stream);
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Get Selection Object
// ==========================================
int guest_heartbeat_api_get_selobj( void )
{
    return _sock;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Initialize
// ================================
GuestApiErrorT guest_heartbeat_api_initialize(
        GuestHeartbeatApiCallbacksT* callbacks )
{
    int stream_size;
    GuestApiErrorT error;

    if ((NULL == callbacks->register_state) ||
        (NULL == callbacks->health_check) ||
        (NULL == callbacks->action_notify))
    {
        DPRINTFE("Not all callbacks are valid.");
        return GUEST_API_FAILED;
    }

    _sock = -1;

    stream_size = sizeof(GuestHeartbeatApiMsgT)*4;
    if (8192 > stream_size)
        stream_size = 8192;

    error = guest_api_stream_setup(GUEST_HEARTBEAT_API_MSG_MAGIC_VALUE,
                                   GUEST_HEARTBEAT_API_MSG_MAGIC_SIZE,
                                   stream_size, &_stream);
    if (GUEST_API_OKAY != error)
    {
        DPRINTFE("Failed to setup stream, error=%s.",
                 guest_api_error_str(error));
        return error;
    }

    memcpy(&_callbacks, callbacks, sizeof(_callbacks));
    return GUEST_API_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Finalize
// ==============================
GuestApiErrorT guest_heartbeat_api_finalize( void )
{
    GuestApiErrorT error;

    memset(&_callbacks, 0, sizeof(_callbacks));

    error = guest_api_stream_release(&_stream);
    if (GUEST_API_OKAY != error)
    {
        DPRINTFE("Failed release stream, error=%s.",
                 guest_api_error_str(error));
    }

    if (0 <= _sock)
    {
        error = guest_api_unix_close(_sock);
        if (GUEST_API_OKAY != error)
        {
            DPRINTFE("Failed to close unix socket, error=%s.",
                     guest_api_error_str(error));
        }
        _sock = -1;
    }

    return GUEST_API_OKAY;
}
// ****************************************************************************

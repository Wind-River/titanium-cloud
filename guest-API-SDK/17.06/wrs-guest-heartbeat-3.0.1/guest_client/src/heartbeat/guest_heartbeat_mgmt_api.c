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
#include "guest_heartbeat_mgmt_api.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "guest_limits.h"
#include "guest_types.h"
#include "guest_debug.h"
#include "guest_selobj.h"
#include "guest_unix.h"
#include "guest_stream.h"
#include "guest_timer.h"

#include "guest_heartbeat_types.h"
#include "guest_heartbeat_api_msg_defs.h"

#define GUEST_HEARTBEAT_MGMT_API_CHALLENGE_DEPTH     4

typedef struct {
    char name[GUEST_HEARTBEAT_API_MSG_MAX_APPLICATION_NAME_SIZE];
    int heartbeat_interval_ms;
    int vote_ms;
    int shutdown_notice_ms;
    int suspend_notice_ms;
    int resume_notice_ms;
    GuestHeartbeatActionT corrective_action;
} GuestHeartbeatMgmtApiAppConfigT;

typedef struct {
    bool healthy;
    GuestHeartbeatActionT corrective_action;
    char log_msg[GUEST_HEARTBEAT_API_MSG_MAX_LOG_SIZE];
} GuestHeartbeatMgmtApiAppHealthT;

typedef struct {
    bool running;
    int invocation_id;
    GuestHeartbeatEventT event;
    GuestHeartbeatNotifyT notify;
    GuestHeartbeatVoteResultT vote_result;
    char log_msg[GUEST_HEARTBEAT_API_MSG_MAX_LOG_SIZE];
} GuestHeartbeatMgmtApiAppActionT;

typedef struct {
    bool inuse;
    bool registered;
    bool final;
    int sock;
    GuestStreamT stream;
    int challenge_depth;
    int last_challenge[GUEST_HEARTBEAT_MGMT_API_CHALLENGE_DEPTH];
    bool send_challenge_response;
    GuestTimerIdT heartbeat_timer;
    GuestTimerIdT heartbeat_timeout_timer;
    GuestTimerIdT action_timer;
    GuestHeartbeatMgmtApiAppConfigT application_config;
    GuestHeartbeatMgmtApiAppHealthT application_health;
    GuestHeartbeatMgmtApiAppActionT application_action;
} GuestHeartbeatMgmtApiConnectionT;

static int _sock = -1;
static uint32_t _msg_sequence;
static GuestHeartbeatMgmtApiActionResponseT _callback;
static GuestHeartbeatMgmtApiConnectionT _connections[GUEST_APPLICATIONS_MAX];

// ****************************************************************************
// Guest Heartbeat Management API - Handle Action Completed
// ========================================================
static void guest_heartbeat_mgmt_api_handle_action_completed( void )
{
    bool update;
    bool invoke_callback;
    GuestHeartbeatMgmtApiAppConfigT* app_config;
    GuestHeartbeatMgmtApiAppActionT* app_action;
    GuestHeartbeatMgmtApiConnectionT* connection;
    GuestHeartbeatEventT event;
    GuestHeartbeatNotifyT notify;
    GuestHeartbeatVoteResultT vote_result;
    char* log_msg;

    unsigned int connection_i;
    for (connection_i=0; GUEST_APPLICATIONS_MAX > connection_i; ++connection_i)
    {
        connection = &(_connections[connection_i]);
        if (connection->inuse && connection->registered)
        {
            if (connection->application_action.running)
            {
                DPRINTFD("Still waiting for application %s to respond.",
                         connection->application_config.name);
                return;
            }
        }
    }

    if (NULL == _callback)
        return;

    vote_result = GUEST_HEARTBEAT_VOTE_RESULT_UNKNOWN;
    invoke_callback = false;

    // All action responses received or timed out.
    for (connection_i=0; GUEST_APPLICATIONS_MAX > connection_i; ++connection_i)
    {
        connection = &(_connections[connection_i]);
        if (connection->inuse && connection->registered)
        {
            app_config = &(connection->application_config);
            app_action = &(connection->application_action);

            DPRINTFI("Application %s vote %s for event %s, notification=%s.",
                     app_config->name,
                     guest_heartbeat_vote_result_str(app_action->vote_result),
                     guest_heartbeat_event_str(app_action->event),
                     guest_heartbeat_notify_str(app_action->notify));

            update = false;

            switch (app_action->vote_result)
            {
                case GUEST_HEARTBEAT_VOTE_RESULT_REJECT:
                    update = true;
                    break;

                case GUEST_HEARTBEAT_VOTE_RESULT_ACCEPT:
                case GUEST_HEARTBEAT_VOTE_RESULT_COMPLETE:
                    if (GUEST_HEARTBEAT_VOTE_RESULT_REJECT != vote_result)
                        update = true;
                    break;

                default:
                    update = false;
                    break;
            }

            if (update)
            {
                event = app_action->event;
                notify = app_action->notify;
                vote_result = app_action->vote_result;
                log_msg = &(app_action->log_msg[0]);
                invoke_callback = true;
            }
        }
    }

    if (invoke_callback)
        _callback(event, notify, vote_result, log_msg);
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Close Connection
// =================================================
static void guest_heartbeat_mgmt_api_close_connection(
        GuestHeartbeatMgmtApiConnectionT* connection )
{
    GuestErrorT error;

    if (0 <= connection->sock)
    {
        error = guest_selobj_deregister(connection->sock);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to deregister select on unix socket, error=%s.",
                     guest_error_str(error));
        }

        close(connection->sock);
    }

    if (GUEST_TIMER_ID_INVALID != connection->heartbeat_timer)
    {
        error = guest_timer_deregister(connection->heartbeat_timer);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to cancel heartbeat timer, error=%s.",
                     guest_error_str(error));
        }
    }

    if (GUEST_TIMER_ID_INVALID != connection->heartbeat_timeout_timer)
    {
        error = guest_timer_deregister(connection->heartbeat_timeout_timer);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to cancel heartbeat timer, error=%s.",
                     guest_error_str(error));
        }
    }

    if (GUEST_TIMER_ID_INVALID != connection->action_timer)
    {
        error = guest_timer_deregister(connection->action_timer);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to cancel action timer, error=%s.",
                     guest_error_str(error));
        }
    }

    memset(connection, 0, sizeof(GuestHeartbeatMgmtApiConnectionT));
    connection->sock = -1;
    connection->heartbeat_timer = GUEST_TIMER_ID_INVALID;
    connection->heartbeat_timeout_timer = GUEST_TIMER_ID_INVALID;
    connection->action_timer = GUEST_TIMER_ID_INVALID;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Action (Network to Host)
// =========================================================
static GuestHeartbeatActionT guest_heartbeat_mgmt_api_action_ntoh(
        GuestHeartbeatApiMsgActionT action )
{
    switch (action)
    {
        case GUEST_HEARTBEAT_API_MSG_ACTION_NONE:
            return GUEST_HEARTBEAT_ACTION_NONE;
        case GUEST_HEARTBEAT_API_MSG_ACTION_REBOOT:
            return GUEST_HEARTBEAT_ACTION_REBOOT;
        case GUEST_HEARTBEAT_API_MSG_ACTION_STOP:
            return GUEST_HEARTBEAT_ACTION_STOP;
        case GUEST_HEARTBEAT_API_MSG_ACTION_LOG:
            return GUEST_HEARTBEAT_ACTION_LOG;
        default:
            DPRINTFE("Unknown action %i.", action);
            return GUEST_HEARTBEAT_ACTION_UNKNOWN;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Event (Host to Network)
// ========================================================
static GuestHeartbeatApiMsgEventT guest_heartbeat_mgmt_api_event_hton(
        GuestHeartbeatEventT event )
{
    switch (event)
    {
        case GUEST_HEARTBEAT_EVENT_STOP:
            return GUEST_HEARTBEAT_API_MSG_EVENT_STOP;
        case GUEST_HEARTBEAT_EVENT_REBOOT:
            return GUEST_HEARTBEAT_API_MSG_EVENT_REBOOT;
        case GUEST_HEARTBEAT_EVENT_SUSPEND:
            return GUEST_HEARTBEAT_API_MSG_EVENT_SUSPEND;
        case GUEST_HEARTBEAT_EVENT_PAUSE:
            return GUEST_HEARTBEAT_API_MSG_EVENT_PAUSE;
        case GUEST_HEARTBEAT_EVENT_UNPAUSE:
            return GUEST_HEARTBEAT_API_MSG_EVENT_UNPAUSE;
        case GUEST_HEARTBEAT_EVENT_RESUME:
            return GUEST_HEARTBEAT_API_MSG_EVENT_RESUME;
        case GUEST_HEARTBEAT_EVENT_RESIZE_BEGIN:
            return GUEST_HEARTBEAT_API_MSG_EVENT_RESIZE_BEGIN;
        case GUEST_HEARTBEAT_EVENT_RESIZE_END:
            return GUEST_HEARTBEAT_API_MSG_EVENT_RESIZE_END;
        case GUEST_HEARTBEAT_EVENT_LIVE_MIGRATE_BEGIN:
            return GUEST_HEARTBEAT_API_MSG_EVENT_LIVE_MIGRATE_BEGIN;
        case GUEST_HEARTBEAT_EVENT_LIVE_MIGRATE_END:
            return GUEST_HEARTBEAT_API_MSG_EVENT_LIVE_MIGRATE_END;
        case GUEST_HEARTBEAT_EVENT_COLD_MIGRATE_BEGIN:
            return GUEST_HEARTBEAT_API_MSG_EVENT_COLD_MIGRATE_BEGIN;
        case GUEST_HEARTBEAT_EVENT_COLD_MIGRATE_END:
            return GUEST_HEARTBEAT_API_MSG_EVENT_COLD_MIGRATE_END;
        default:
            DPRINTFE("Unknown event %i.", event);
            return GUEST_HEARTBEAT_API_MSG_EVENT_UNKNOWN;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Event (Network to Host)
// ========================================================
static GuestHeartbeatEventT guest_heartbeat_mgmt_api_event_ntoh(
        GuestHeartbeatApiMsgEventT event )
{
    switch (event)
    {
        case GUEST_HEARTBEAT_API_MSG_EVENT_STOP:
            return GUEST_HEARTBEAT_EVENT_STOP;
        case GUEST_HEARTBEAT_API_MSG_EVENT_REBOOT:
            return GUEST_HEARTBEAT_EVENT_REBOOT;
        case GUEST_HEARTBEAT_API_MSG_EVENT_SUSPEND:
            return GUEST_HEARTBEAT_EVENT_SUSPEND;
        case GUEST_HEARTBEAT_API_MSG_EVENT_PAUSE:
            return GUEST_HEARTBEAT_EVENT_PAUSE;
        case GUEST_HEARTBEAT_API_MSG_EVENT_UNPAUSE:
            return GUEST_HEARTBEAT_EVENT_UNPAUSE;
        case GUEST_HEARTBEAT_API_MSG_EVENT_RESUME:
            return GUEST_HEARTBEAT_EVENT_RESUME;
        case GUEST_HEARTBEAT_API_MSG_EVENT_RESIZE_BEGIN:
            return GUEST_HEARTBEAT_EVENT_RESIZE_BEGIN;
        case GUEST_HEARTBEAT_API_MSG_EVENT_RESIZE_END:
            return GUEST_HEARTBEAT_EVENT_RESIZE_END;
        case GUEST_HEARTBEAT_API_MSG_EVENT_LIVE_MIGRATE_BEGIN:
            return GUEST_HEARTBEAT_EVENT_LIVE_MIGRATE_BEGIN;
        case GUEST_HEARTBEAT_API_MSG_EVENT_LIVE_MIGRATE_END:
            return GUEST_HEARTBEAT_EVENT_LIVE_MIGRATE_END;
        case GUEST_HEARTBEAT_API_MSG_EVENT_COLD_MIGRATE_BEGIN:
            return GUEST_HEARTBEAT_EVENT_COLD_MIGRATE_BEGIN;
        case GUEST_HEARTBEAT_API_MSG_EVENT_COLD_MIGRATE_END:
            return GUEST_HEARTBEAT_EVENT_COLD_MIGRATE_END;
        default:
            DPRINTFE("Unknown event %i.", event);
            return GUEST_HEARTBEAT_EVENT_UNKNOWN;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Notify (Host to Network)
// =========================================================
static GuestHeartbeatApiMsgNotifyT guest_heartbeat_mgmt_api_notify_hton(
        GuestHeartbeatNotifyT notify )
{
    switch (notify)
    {
        case GUEST_HEARTBEAT_NOTIFY_REVOCABLE:
            return GUEST_HEARTBEAT_API_MSG_NOTIFY_REVOCABLE;
        case GUEST_HEARTBEAT_NOTIFY_IRREVOCABLE:
            return GUEST_HEARTBEAT_API_MSG_NOTIFY_IRREVOCABLE;
        default:
            DPRINTFE("Unknown notify type %i.", notify);
            return GUEST_HEARTBEAT_API_MSG_NOTIFY_UNKNOWN;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Notify (Network to Host)
// =========================================================
static GuestHeartbeatNotifyT guest_heartbeat_mgmt_api_notify_ntoh(
        GuestHeartbeatApiMsgNotifyT notify )
{
    switch (notify)
    {
        case GUEST_HEARTBEAT_API_MSG_NOTIFY_REVOCABLE:
            return GUEST_HEARTBEAT_NOTIFY_REVOCABLE;
        case GUEST_HEARTBEAT_API_MSG_NOTIFY_IRREVOCABLE:
            return GUEST_HEARTBEAT_NOTIFY_IRREVOCABLE;
        default:
            DPRINTFE("Unknown notify type %i.", notify);
            return GUEST_HEARTBEAT_NOTIFY_UNKNOWN;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Vote Result (Network to Host)
// ==============================================================
static GuestHeartbeatVoteResultT guest_heartbeat_mgmt_api_vote_result_ntoh(
        GuestHeartbeatApiMsgVoteResultT vote_result )
{
    switch (vote_result)
    {
        case GUEST_HEARTBEAT_API_MSG_VOTE_RESULT_ACCEPT:
            return GUEST_HEARTBEAT_VOTE_RESULT_ACCEPT;
        case GUEST_HEARTBEAT_API_MSG_VOTE_RESULT_REJECT:
            return GUEST_HEARTBEAT_VOTE_RESULT_REJECT;
        case GUEST_HEARTBEAT_API_MSG_VOTE_RESULT_COMPLETE:
            return GUEST_HEARTBEAT_VOTE_RESULT_COMPLETE;
        default:
            DPRINTFE("Unknown vote result %i.", vote_result);
            return GUEST_HEARTBEAT_VOTE_RESULT_UNKNOWN;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Send Init Ack
// ==============================================
static GuestErrorT guest_heartbeat_mgmt_api_send_init_ack( int s, bool accept )
{
    GuestHeartbeatApiMsgT msg;
    GuestHeartbeatApiMsgHeaderT *hdr = &(msg.header);
    GuestHeartbeatApiMsgInitAckT *bdy = &(msg.body.init_ack);
    GuestErrorT error;

    memset(&msg, 0, sizeof(msg));

    memcpy(&(hdr->magic), GUEST_HEARTBEAT_API_MSG_MAGIC_VALUE,
           GUEST_HEARTBEAT_API_MSG_MAGIC_SIZE);
    hdr->version = GUEST_HEARTBEAT_API_MSG_VERSION_CURRENT;
    hdr->revision = GUEST_HEARTBEAT_API_MSG_REVISION_CURRENT;
    hdr->msg_type = GUEST_HEARTBEAT_API_MSG_INIT_ACK;
    hdr->sequence = ++_msg_sequence;
    hdr->size = sizeof(msg);

    bdy->accepted = accept;

    error = guest_unix_send(s, &msg, sizeof(msg));
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to send guest heartbeat register ack message, "
                 "error=%s.", guest_error_str(error));
        return error;
    }

    DPRINTFD("Sent register ack.");
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Send Heartbeat
// ===============================================
static GuestErrorT guest_heartbeat_mgmt_api_send_heartbeat( int s, int challenge )
{
    GuestHeartbeatApiMsgT msg;
    GuestHeartbeatApiMsgHeaderT *hdr = &(msg.header);
    GuestHeartbeatApiMsgChallengeT *bdy = &(msg.body.challenge);
    GuestErrorT error;

    memset(&msg, 0, sizeof(msg));

    memcpy(&(hdr->magic), GUEST_HEARTBEAT_API_MSG_MAGIC_VALUE,
           GUEST_HEARTBEAT_API_MSG_MAGIC_SIZE);
    hdr->version = GUEST_HEARTBEAT_API_MSG_VERSION_CURRENT;
    hdr->revision = GUEST_HEARTBEAT_API_MSG_REVISION_CURRENT;
    hdr->msg_type = GUEST_HEARTBEAT_API_MSG_CHALLENGE;
    hdr->sequence = ++_msg_sequence;
    hdr->size = sizeof(msg);

    bdy->heartbeat_challenge = challenge;

    error = guest_unix_send(s, &msg, sizeof(msg));
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to send guest heartbeat message, error=%s.",
                 guest_error_str(error));
        return error;
    }

    DPRINTFD("Sent heartbeat message.");
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Send Action Notify
// ===================================================
static GuestErrorT guest_heartbeat_mgmt_api_send_action_notify(
        int s, int invocation_id, GuestHeartbeatEventT event,
        GuestHeartbeatNotifyT notify )
{
    GuestHeartbeatApiMsgT msg;
    GuestHeartbeatApiMsgHeaderT *hdr = &(msg.header);
    GuestHeartbeatApiMsgActionNotifyT *bdy = &(msg.body.action_notify);
    GuestErrorT error;

    memset(&msg, 0, sizeof(msg));

    memcpy(&(hdr->magic), GUEST_HEARTBEAT_API_MSG_MAGIC_VALUE,
           GUEST_HEARTBEAT_API_MSG_MAGIC_SIZE);
    hdr->version = GUEST_HEARTBEAT_API_MSG_VERSION_CURRENT;
    hdr->revision = GUEST_HEARTBEAT_API_MSG_REVISION_CURRENT;
    hdr->msg_type = GUEST_HEARTBEAT_API_MSG_ACTION_NOTIFY;
    hdr->sequence = ++_msg_sequence;
    hdr->size = sizeof(msg);

    bdy->invocation_id = invocation_id;
    bdy->event_type = guest_heartbeat_mgmt_api_event_hton(event);
    bdy->notification_type = guest_heartbeat_mgmt_api_notify_hton(notify);

    error = guest_unix_send(s, &msg, sizeof(msg));
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to send guest heartbeat action notify message, "
                 "error=%s.", guest_error_str(error));
        return error;
    }

    DPRINTFD("Sent action notify.");
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Heartbeat Interval
// ===================================================
static bool guest_heartbeat_mgmt_api_heartbeat_interval( GuestTimerIdT timer_id )
{
    int challenge;
    GuestHeartbeatMgmtApiConnectionT* connection;
    GuestErrorT error;

    unsigned int connection_i;
    for (connection_i=0; GUEST_APPLICATIONS_MAX > connection_i; ++connection_i)
    {
        connection = &(_connections[connection_i]);
        if (connection->inuse)
            if (timer_id == connection->heartbeat_timer)
                break;
    }

    if (GUEST_APPLICATIONS_MAX <= connection_i)
    {
        DPRINTFE("Uknown timer %i.", timer_id);
        return false; // don't rearm
    }

    if (!(connection->send_challenge_response))
    {
        DPRINTFD("Waiting for challenge response for previous iteration.");
        return true; // rearm
    }

    challenge = rand();
    ++connection->challenge_depth;
    if (GUEST_HEARTBEAT_MGMT_API_CHALLENGE_DEPTH <= connection->challenge_depth)
        connection->challenge_depth = 0;

    connection->last_challenge[connection->challenge_depth] = challenge;

    error =  guest_heartbeat_mgmt_api_send_heartbeat( connection->sock,
                                                      challenge );
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to send heartbeat, error=%s.", guest_error_str(error));
        return true; // rearm
    }

    connection->send_challenge_response = false;
    return true; // rearm
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Heartbeat Timeout
// ==================================================
static bool guest_heartbeat_mgmt_api_heartbeat_timeout( GuestTimerIdT timer_id )
{
    bool prev_health;
    GuestHeartbeatMgmtApiAppConfigT* app_config;
    GuestHeartbeatMgmtApiAppHealthT* app_health;
    GuestHeartbeatMgmtApiConnectionT* connection;
    int max_heartbeat_delay;

    unsigned int connection_i;
    for (connection_i=0; GUEST_APPLICATIONS_MAX > connection_i; ++connection_i)
    {
        connection = &(_connections[connection_i]);
        if (connection->inuse)
            if (timer_id == connection->heartbeat_timeout_timer)
                break;
    }

    if (GUEST_APPLICATIONS_MAX <= connection_i)
    {
        DPRINTFE("Uknown timer %i.", timer_id);
        return false; // don't rearm
    }

    app_config = &(connection->application_config);
    app_health = &(connection->application_health);

    max_heartbeat_delay = app_config->heartbeat_interval_ms*2;

    if (!guest_timer_scheduling_on_time_within(max_heartbeat_delay))
    {
        DPRINTFE("Failed to receive a heartbeat in %i ms, but we are not "
                 "scheduling on time.", max_heartbeat_delay);
        return true; // rearm
    }

    prev_health = app_health->healthy;
    app_health->healthy = false;
    app_health->corrective_action = app_config->corrective_action;
    snprintf(app_health->log_msg, GUEST_HEARTBEAT_API_MSG_MAX_LOG_SIZE,
             "Application %s heartbeat timeout %i ms.", app_config->name,
             app_config->heartbeat_interval_ms*2);

    if (prev_health)
    {
        DPRINTFI("Application %s heartbeat timeout %i ms, "
                 "corrective_action=%s, log_msg=%s.", app_config->name,
                 max_heartbeat_delay,
                 guest_heartbeat_action_str(app_health->corrective_action),
                 app_health->log_msg);
    }
    return true; // rearm
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Receive Init
// =============================================
static void guest_heartbeat_mgmt_api_recv_init(
        GuestHeartbeatMgmtApiConnectionT* connection )
{
    bool accepted = true;
    GuestHeartbeatMgmtApiAppConfigT* app_config;
    GuestHeartbeatMgmtApiAppHealthT* app_health;
    GuestHeartbeatMgmtApiAppActionT* app_action;
    char* ptr = connection->stream.bytes + sizeof(GuestHeartbeatApiMsgHeaderT);
    GuestErrorT error;

    DPRINTFD("Heartbeat Init received...");

    app_config = &(connection->application_config);
    app_health = &(connection->application_health);
    app_action = &(connection->application_action);

    snprintf(app_config->name,
             GUEST_HEARTBEAT_API_MSG_MAX_APPLICATION_NAME_SIZE, "%s", ptr);
    ptr += GUEST_HEARTBEAT_API_MSG_MAX_APPLICATION_NAME_SIZE;
    app_config->heartbeat_interval_ms = *(uint32_t*) ptr;
    ptr += sizeof(uint32_t);
    app_config->vote_ms = (*(uint32_t*) ptr) * 1000;
    ptr += sizeof(uint32_t);
    app_config->shutdown_notice_ms = (*(uint32_t*) ptr) * 1000;
    ptr += sizeof(uint32_t);
    app_config->suspend_notice_ms = (*(uint32_t*) ptr) * 1000;
    ptr += sizeof(uint32_t);
    app_config->resume_notice_ms = (*(uint32_t*) ptr) * 1000;
    ptr += sizeof(uint32_t);
    app_config->corrective_action
            = guest_heartbeat_mgmt_api_action_ntoh(*(uint32_t*) ptr);
    ptr += sizeof(uint32_t);

    if (GUEST_HEARTBEAT_MIN_INTERVAL_MS > app_config->heartbeat_interval_ms)
    {
        DPRINTFE("Not accepting application %s registration, unsupported "
                 "heartbeat interval, less than %s ms.", app_config->name,
                 GUEST_HEARTBEAT_MIN_INTERVAL_MS);
        accepted = false;
    }

    error = guest_heartbeat_mgmt_api_send_init_ack(connection->sock, accepted);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to send register ack message, error=%s.",
                 guest_error_str(error));
        return;
    }

    if (!accepted)
        return;

    error = guest_timer_register(app_config->heartbeat_interval_ms,
                                 guest_heartbeat_mgmt_api_heartbeat_interval,
                                 &(connection->heartbeat_timer));
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to register heartbeat timer, error=%s.",
                 guest_error_str(error));
        return;
    }

    error = guest_timer_register(app_config->heartbeat_interval_ms*2,
                                 guest_heartbeat_mgmt_api_heartbeat_timeout,
                                 &(connection->heartbeat_timeout_timer));
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to register heartbeat timeout timer, error=%s.",
                 guest_error_str(error));
        return;
    }

    app_health->healthy = true;
    app_health->corrective_action = GUEST_HEARTBEAT_ACTION_NONE;
    app_health->log_msg[0] = '\0';

    app_action->event = GUEST_HEARTBEAT_EVENT_UNKNOWN;
    app_action->notify = GUEST_HEARTBEAT_NOTIFY_UNKNOWN;
    app_action->vote_result = GUEST_HEARTBEAT_VOTE_RESULT_UNKNOWN;
    app_action->log_msg[0] = '\0';

    connection->send_challenge_response = true;
    connection->registered = true;
    connection->final = false;

    DPRINTFI("Connection accepted from %s.", app_config->name);
    DPRINTFI("  socket:                %i", connection->sock);
    DPRINTFI("  heartbeat_interval_ms: %i", app_config->heartbeat_interval_ms);
    DPRINTFI("  vote_ms:               %i", app_config->vote_ms);
    DPRINTFI("  shutdown_notice_ms:    %i", app_config->shutdown_notice_ms);
    DPRINTFI("  resume_notice_ms:      %i", app_config->resume_notice_ms);
    DPRINTFI("  corrective_action:     %s",
             guest_heartbeat_action_str(app_config->corrective_action));
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Receive Final
// ==============================================
static void guest_heartbeat_mgmt_api_recv_final(
        GuestHeartbeatMgmtApiConnectionT* connection )
{
    char* log_msg;
    char* ptr = connection->stream.bytes + sizeof(GuestHeartbeatApiMsgHeaderT);

    log_msg = ptr;
    ptr += GUEST_HEARTBEAT_API_MSG_MAX_LOG_SIZE;

    DPRINTFD("Heartbeat Final Response received...");

    DPRINTFI("Application %s has deregistered, msg=%s.",
             connection->application_config.name, log_msg);

    connection->registered = false;
    connection->final = true;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Receive Challenge Response
// ===========================================================
static void guest_heartbeat_mgmt_api_recv_challenge_response(
        GuestHeartbeatMgmtApiConnectionT* connection )
{
    uint32_t heartbeat_response;
    uint32_t health;
    GuestHeartbeatActionT corrective_action;
    char* log_msg;
    GuestHeartbeatMgmtApiAppConfigT* app_config;
    GuestHeartbeatMgmtApiAppHealthT* app_health;
    char* ptr = connection->stream.bytes + sizeof(GuestHeartbeatApiMsgHeaderT);

    heartbeat_response = *(uint32_t*) ptr;
    ptr += sizeof(uint32_t);
    health = *(uint32_t*) ptr;
    ptr += sizeof(uint32_t);
    corrective_action
        = guest_heartbeat_mgmt_api_action_ntoh(*(uint32_t*) ptr);
    ptr += sizeof(uint32_t);
    log_msg = ptr;
    ptr += GUEST_HEARTBEAT_API_MSG_MAX_LOG_SIZE;

    unsigned int challenge_i;
    for (challenge_i=0; GUEST_HEARTBEAT_MGMT_API_CHALLENGE_DEPTH > challenge_i;
         ++challenge_i)
    {
        if (connection->last_challenge[challenge_i] == heartbeat_response)
            break;
    }

    if (GUEST_HEARTBEAT_MGMT_API_CHALLENGE_DEPTH == challenge_i)
    {
        DPRINTFE("Mismatch between last transmitted challenges and received "
                 "challenge.");
        return;
    }

    DPRINTFD("Heartbeat Challenge Response received...");
    connection->send_challenge_response = true;

    app_config = &(connection->application_config);
    app_health = &(connection->application_health);

    if (health != app_health->healthy)
    {
        DPRINTFI("Application %s health state change, prev_health=%i, "
                 "health=%i, corrective_action=%s, log_msg=%s.",
                 app_config->name, app_health->healthy, health,
                 guest_heartbeat_action_str(corrective_action),
                 log_msg);
    }

    app_health->healthy = health;
    app_health->corrective_action = corrective_action;
    snprintf(app_health->log_msg, GUEST_HEARTBEAT_API_MSG_MAX_LOG_SIZE, "%s",
             log_msg);

    guest_timer_reset(connection->heartbeat_timeout_timer);
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Receive Action Response
// ========================================================
static void guest_heartbeat_mgmt_api_recv_action_response(
        GuestHeartbeatMgmtApiConnectionT* connection )
{
    int invocation_id;
    GuestHeartbeatEventT event;
    GuestHeartbeatNotifyT notify;
    GuestHeartbeatVoteResultT vote_result;
    GuestHeartbeatMgmtApiAppActionT* app_action;
    char* log_msg;
    char* ptr = connection->stream.bytes + sizeof(GuestHeartbeatApiMsgHeaderT);
    GuestErrorT error;

    app_action = &(connection->application_action);

    invocation_id = *(uint32_t*) ptr;
    ptr += sizeof(uint32_t);
    event = guest_heartbeat_mgmt_api_event_ntoh(*(uint32_t*) ptr);
    ptr += sizeof(uint32_t);
    notify = guest_heartbeat_mgmt_api_notify_ntoh(*(uint32_t*) ptr);
    ptr += sizeof(uint32_t);
    vote_result = guest_heartbeat_mgmt_api_vote_result_ntoh(*(uint32_t*) ptr);
    ptr += sizeof(uint32_t);
    log_msg = ptr;
    ptr += GUEST_HEARTBEAT_API_MSG_MAX_LOG_SIZE;

    if (!(app_action->running))
    {
        DPRINTFD("No action inprogress.");
        return;
    }

    if (invocation_id != app_action->invocation_id)
    {
        DPRINTFE("Unexpected action invocation %i received for %s.",
                 invocation_id, guest_heartbeat_event_str(event));
        return;
    }

    DPRINTFD("Heartbeat Action Response received...");

    app_action->running = false;
    app_action->event = event;
    app_action->notify = notify;
    app_action->vote_result = vote_result;
    snprintf(app_action->log_msg, GUEST_HEARTBEAT_API_MSG_MAX_LOG_SIZE, "%s",
             log_msg);

    if (GUEST_TIMER_ID_INVALID != connection->action_timer)
    {
        error = guest_timer_deregister(connection->action_timer);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to cancel action timer, error=%s.",
                     guest_error_str(error));
        }

        connection->action_timer = GUEST_TIMER_ID_INVALID;
    }

    guest_heartbeat_mgmt_api_handle_action_completed();
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Dispatch
// =========================================
static void guest_heartbeat_mgmt_api_dispatch( int selobj )
{
    static bool have_start = false;
    static bool have_header = false;
    static GuestHeartbeatApiMsgHeaderT hdr;

    bool more;
    int bytes_received;
    GuestHeartbeatMgmtApiConnectionT* connection;
    GuestErrorT error;

    unsigned int connection_i;
    for (connection_i=0; GUEST_APPLICATIONS_MAX > connection_i; ++connection_i)
    {
        connection = &(_connections[connection_i]);
        if (connection->inuse)
            if (selobj == connection->sock)
                break;
    }

    if (GUEST_APPLICATIONS_MAX <= connection_i)
    {
        DPRINTFE("Uknown selection object %i.", selobj);
        close(selobj);
        return;
    }

    error = guest_unix_receive(connection->sock, connection->stream.end_ptr,
                               connection->stream.avail, &bytes_received);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to receive message, error=%s.",
                 guest_error_str(error));
        return;
    }

    if (0 == bytes_received)
    {
        DPRINTFI("Connection closed on %i.", connection->sock);
        guest_heartbeat_mgmt_api_close_connection(connection);
        return;
    }

    DPRINTFD("Bytes received is %i.", bytes_received);

    connection->stream.end_ptr += bytes_received;
    connection->stream.avail -= bytes_received;
    connection->stream.size += bytes_received;

    do
    {
        more = false;

        if (!have_start)
        {
            memset(&hdr, 0 ,sizeof(GuestHeartbeatApiMsgHeaderT));
            have_start = guest_stream_get_next(&(connection->stream));
        }

        if (have_start && !have_header)
        {
            if (sizeof(GuestHeartbeatApiMsgHeaderT) <= connection->stream.size)
            {
                char* ptr = connection->stream.bytes
                          + GUEST_HEARTBEAT_API_MSG_MAGIC_SIZE;

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
                    guest_stream_advance(GUEST_HEARTBEAT_API_MSG_MAGIC_SIZE,
                                         &connection->stream);
                    more = true;
                }
            }
        }

        if (have_start && have_header)
        {
            if (sizeof(GuestHeartbeatApiMsgT) <= connection->stream.size)
            {
                switch(hdr.msg_type)
                {
                    case GUEST_HEARTBEAT_API_MSG_INIT:
                        guest_heartbeat_mgmt_api_recv_init(connection);
                        break;

                    case GUEST_HEARTBEAT_API_MSG_FINAL:
                        guest_heartbeat_mgmt_api_recv_final(connection);
                        break;

                    case GUEST_HEARTBEAT_API_MSG_CHALLENGE_RESPONSE:
                        guest_heartbeat_mgmt_api_recv_challenge_response(connection);
                        break;

                    case GUEST_HEARTBEAT_API_MSG_ACTION_RESPONSE:
                        guest_heartbeat_mgmt_api_recv_action_response(connection);
                        break;

                    default:
                        DPRINTFV("Unknown message type %i.",
                                 (int) hdr.msg_type);
                        break;
                }

                have_start = false;
                have_header = false;
                guest_stream_advance(sizeof(GuestHeartbeatApiMsgT),
                                     &(connection->stream));
                more = true;
            }
        }
    } while (more);

    if (0 >= connection->stream.avail)
        guest_stream_reset(&(connection->stream));
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Connect Handler
// ================================================
static void guest_heartbeat_mgmt_api_connect_handler( int selobj, char* address )
{
    int stream_size;
    GuestSelObjCallbacksT callbacks;
    GuestHeartbeatMgmtApiConnectionT* connection;
    GuestErrorT error;

    DPRINTFD("Connect on socket %i.", selobj);

    // Find unused connection.
    unsigned int connection_i;
    for (connection_i=0; GUEST_APPLICATIONS_MAX > connection_i; ++connection_i)
    {
        connection = &(_connections[connection_i]);
        if (!connection->inuse)
        {
            memset(connection, 0, sizeof(GuestHeartbeatMgmtApiConnectionT));
            connection->inuse = true;
            connection->registered = false;
            connection->sock = selobj;
            connection->heartbeat_timer = GUEST_TIMER_ID_INVALID;
            connection->heartbeat_timeout_timer = GUEST_TIMER_ID_INVALID;
            connection->action_timer = GUEST_TIMER_ID_INVALID;
            break;
        }
    }

    if (GUEST_APPLICATIONS_MAX <= connection_i)
    {
        // Find unregistered connection and replace.
        unsigned int connection_i;
        for (connection_i=0; GUEST_APPLICATIONS_MAX > connection_i; ++connection_i)
        {
            connection = &(_connections[connection_i]);
            if ((connection->inuse) && (!connection->registered))
            {
                guest_heartbeat_mgmt_api_close_connection(connection);
                connection->inuse = true;
                connection->registered = false;
                connection->sock = selobj;
                break;
            }
        }
    }

    if (GUEST_APPLICATIONS_MAX <= connection_i)
    {
        DPRINTFE("Failed to allocate connection.");
        close(selobj);
        return;
    }

    stream_size = sizeof(GuestHeartbeatApiMsgT)*4;
    if (8192 > stream_size)
        stream_size = 8192;

    error = guest_stream_setup(GUEST_HEARTBEAT_API_MSG_MAGIC_VALUE,
                               GUEST_HEARTBEAT_API_MSG_MAGIC_SIZE,
                               stream_size, &(connection->stream));
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to setup stream, error=%s.", guest_error_str(error));
        close(connection->sock);
        memset(connection, 0, sizeof(GuestHeartbeatMgmtApiConnectionT));
        connection->sock = -1;
        connection->heartbeat_timer = GUEST_TIMER_ID_INVALID;
        connection->heartbeat_timeout_timer = GUEST_TIMER_ID_INVALID;
        connection->action_timer = GUEST_TIMER_ID_INVALID;

        return;
    }

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.read_callback = guest_heartbeat_mgmt_api_dispatch;

    error = guest_selobj_register(connection->sock, &callbacks);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to register select on unix socket, error=%s.",
                 guest_error_str(error));
        close(connection->sock);
        guest_stream_release(&(connection->stream));
        memset(connection, 0, sizeof(GuestHeartbeatMgmtApiConnectionT));
        connection->sock = -1;
        connection->heartbeat_timer = GUEST_TIMER_ID_INVALID;
        connection->heartbeat_timeout_timer = GUEST_TIMER_ID_INVALID;
        connection->action_timer = GUEST_TIMER_ID_INVALID;
        return;
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Get Health
// ===========================================
GuestErrorT guest_heartbeat_mgmt_api_get_health(
        bool* health, GuestHeartbeatActionT* corrective_action, char log_msg[],
        int log_msg_size )
{
    GuestHeartbeatMgmtApiAppHealthT* app_health;
    GuestHeartbeatMgmtApiConnectionT* connection;
    GuestHeartbeatActionT update;

    *health = true;
    *corrective_action = GUEST_HEARTBEAT_ACTION_NONE;
    log_msg[0] = '\0';

    unsigned int connection_i;
    for (connection_i=0; GUEST_APPLICATIONS_MAX > connection_i; ++connection_i)
    {
        connection = &(_connections[connection_i]);
        if ((connection->inuse) && (connection->registered))
        {
            app_health = &(connection->application_health);
            if (!(app_health->healthy))
            {
                update = guest_heartbeat_merge_action(
                            *corrective_action, app_health->corrective_action);

                if (update == app_health->corrective_action)
                {
                    *health = false;
                    *corrective_action = update;
                    snprintf(log_msg, log_msg_size, "%s", app_health->log_msg);
                }
            }
        }
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Action Timeout
// ===============================================
static bool guest_heartbeat_mgmt_api_action_timeout( GuestTimerIdT timer_id )
{
    GuestHeartbeatMgmtApiAppActionT* app_action;
    GuestHeartbeatMgmtApiConnectionT* connection;

    unsigned int connection_i;
    for (connection_i=0; GUEST_APPLICATIONS_MAX > connection_i; ++connection_i)
    {
        connection = &(_connections[connection_i]);
        if (connection->inuse)
            if (timer_id == connection->action_timer)
                break;
    }

    if (GUEST_APPLICATIONS_MAX <= connection_i)
    {
        DPRINTFE("Uknown timer %i.", timer_id);
        return false; // don't rearm
    }

    app_action = &(connection->application_action);

    if (app_action->running)
    {
        app_action->running = false;
        app_action->vote_result = GUEST_HEARTBEAT_VOTE_RESULT_TIMEOUT;
    }

    guest_heartbeat_mgmt_api_handle_action_completed();
    connection->action_timer = GUEST_TIMER_ID_INVALID;
    return false; // don't rearm
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Action Abort
// =============================================
void guest_heartbeat_mgmt_api_action_abort( void )
{
    GuestHeartbeatMgmtApiAppConfigT* app_config;
    GuestHeartbeatMgmtApiAppActionT* app_action;
    GuestHeartbeatMgmtApiConnectionT* connection;
    GuestErrorT error;

    unsigned int connection_i;
    for (connection_i=0; GUEST_APPLICATIONS_MAX > connection_i; ++connection_i)
    {
        connection = &(_connections[connection_i]);
        if (connection->inuse && connection->registered)
        {
            app_config = &(connection->application_config);
            app_action = &(connection->application_action);

            if (app_action->running)
            {
                DPRINTFI("Aborting action %s for application %s, "
                         "notification=%s.", app_config->name,
                         guest_heartbeat_event_str(app_action->event),
                         guest_heartbeat_notify_str(app_action->notify));
            }

            app_action->running = false;

            if (GUEST_TIMER_ID_INVALID != connection->action_timer)
            {
                error = guest_timer_deregister(connection->action_timer);
                if (GUEST_OKAY != error)
                {
                    DPRINTFE("Failed to cancel action timer, error=%s.",
                             guest_error_str(error));
                }
                connection->action_timer = GUEST_TIMER_ID_INVALID;
            }
        }
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Action Notify
// ==============================================
GuestErrorT guest_heartbeat_mgmt_api_action_notify(
        GuestHeartbeatEventT event, GuestHeartbeatNotifyT notify, bool* wait,
        GuestHeartbeatMgmtApiActionResponseT callback )
{
    int action_timeout_ms;
    GuestHeartbeatMgmtApiAppConfigT* app_config;
    GuestHeartbeatMgmtApiAppActionT* app_action;
    GuestHeartbeatMgmtApiConnectionT* connection;
    GuestErrorT error;

    *wait = false;
    _callback = NULL;

    unsigned int connection_i;
    for (connection_i=0; GUEST_APPLICATIONS_MAX > connection_i; ++connection_i)
    {
        connection = &(_connections[connection_i]);
        if (connection->inuse && connection->registered)
        {
            app_config = &(connection->application_config);
            app_action = &(connection->application_action);

            app_action->running = true;
            app_action->invocation_id = rand();
            app_action->event = event;
            app_action->notify = notify;
            app_action->vote_result = GUEST_HEARTBEAT_VOTE_RESULT_UNKNOWN;

            error = guest_heartbeat_mgmt_api_send_action_notify(
                        connection->sock, app_action->invocation_id, event,
                        notify );
            if (GUEST_OKAY == error)
            {
                DPRINTFI("Sent action to appplication %s for event %s, "
                         "notification=%s.", app_config->name,
                         guest_heartbeat_event_str(event),
                         guest_heartbeat_notify_str(notify));
            } else {
                DPRINTFE("Failed to send action to application %s for "
                         "event %s, notification=%s.", app_config->name,
                         guest_heartbeat_event_str(event),
                         guest_heartbeat_notify_str(notify));
            }

            if (GUEST_HEARTBEAT_NOTIFY_IRREVOCABLE == notify)
            {
                switch (event)
                {
                    case GUEST_HEARTBEAT_EVENT_STOP:
                    case GUEST_HEARTBEAT_EVENT_REBOOT:
                        action_timeout_ms = app_config->shutdown_notice_ms;
                        break;

                    case GUEST_HEARTBEAT_EVENT_PAUSE:
                    case GUEST_HEARTBEAT_EVENT_SUSPEND:
                    case GUEST_HEARTBEAT_EVENT_RESIZE_BEGIN:
                    case GUEST_HEARTBEAT_EVENT_LIVE_MIGRATE_BEGIN:
                    case GUEST_HEARTBEAT_EVENT_COLD_MIGRATE_BEGIN:
                        action_timeout_ms = app_config->suspend_notice_ms;
                        break;

                    case GUEST_HEARTBEAT_EVENT_UNPAUSE:
                    case GUEST_HEARTBEAT_EVENT_RESUME:
                    case GUEST_HEARTBEAT_EVENT_RESIZE_END:
                    case GUEST_HEARTBEAT_EVENT_LIVE_MIGRATE_END:
                    case GUEST_HEARTBEAT_EVENT_COLD_MIGRATE_END:
                        action_timeout_ms = app_config->resume_notice_ms;
                        break;

                    default:
                        action_timeout_ms = app_config->shutdown_notice_ms;
                        break;
                }
            } else {
                action_timeout_ms = app_config->vote_ms;
            }

            error = guest_timer_register(action_timeout_ms,
                                         guest_heartbeat_mgmt_api_action_timeout,
                                         &(connection->action_timer));
            if (GUEST_OKAY != error)
            {
                DPRINTFE("Failed to register action timeout timer, error=%s.",
                         guest_error_str(error));
                abort();
            }

            *wait = true;
            _callback = callback;
        }
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Initialize
// ===========================================
GuestErrorT guest_heartbeat_mgmt_api_initialize( void )
{
    GuestHeartbeatMgmtApiConnectionT* connection;
    GuestErrorT error;

    memset(&_connections, 0, sizeof(_connections));

    unsigned int connection_i;
    for (connection_i=0; GUEST_APPLICATIONS_MAX > connection_i; ++connection_i)
    {
        connection = &(_connections[connection_i]);
        connection->sock = -1;
        connection->heartbeat_timer = GUEST_TIMER_ID_INVALID;
        connection->heartbeat_timeout_timer = GUEST_TIMER_ID_INVALID;
        connection->action_timer = GUEST_TIMER_ID_INVALID;
    }

    error = guest_unix_open(&_sock);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to open unix socket, error=%s.",
                 guest_error_str(error));
        return error;
    }

    error = guest_unix_listen(_sock, GUEST_HEARTBEAT_API_MSG_ADDRESS,
                              guest_heartbeat_mgmt_api_connect_handler);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to listen on unix socket, error=%s.",
                 guest_error_str(error));
        return error;
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Finalize
// =========================================
GuestErrorT guest_heartbeat_mgmt_api_finalize( void )
{
    GuestHeartbeatMgmtApiConnectionT* connection;
    GuestErrorT error;

    if (0 <= _sock)
    {
        error = guest_selobj_deregister(_sock);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to deregister select on unix socket, error=%s.",
                     guest_error_str(error));
        }

        error = guest_unix_close(_sock);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to close unix socket, error=%s.",
                     guest_error_str(error));
        }
        _sock = -1;
    }

    unsigned int connection_i;
    for (connection_i=0; GUEST_APPLICATIONS_MAX > connection_i; ++connection_i)
    {
        connection = &(_connections[connection_i]);
        guest_heartbeat_mgmt_api_close_connection(connection);
    }

    memset(&_connections, 0, sizeof(_connections));
    return GUEST_OKAY;
}
// ****************************************************************************

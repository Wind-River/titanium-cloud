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
#ifndef __GUEST_HERATBEAT_API_H__
#define __GUEST_HEARTBEAT_API_H__

#include <stdbool.h>

#include "guest_api_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GUEST_HEARTBEAT_API_APPLICATION_NAME_MAX     40
#define GUEST_HEARTBEAT_API_LOG_MAX                 192

typedef enum {

    GUEST_HEARTBEAT_API_ACTION_UNKNOWN,
    GUEST_HEARTBEAT_API_ACTION_NONE,
    GUEST_HEARTBEAT_API_ACTION_REBOOT,
    GUEST_HEARTBEAT_API_ACTION_STOP,
    GUEST_HEARTBEAT_API_ACTION_LOG,
    GUEST_HEARTBEAT_API_ACTION_MAX,
} GuestHeartbeatApiActionT;

typedef enum {
    GUEST_HEARTBEAT_API_EVENT_UNKNOWN,
    GUEST_HEARTBEAT_API_EVENT_STOP,
    GUEST_HEARTBEAT_API_EVENT_REBOOT,
    GUEST_HEARTBEAT_API_EVENT_SUSPEND,
    GUEST_HEARTBEAT_API_EVENT_PAUSE,
    GUEST_HEARTBEAT_API_EVENT_UNPAUSE,
    GUEST_HEARTBEAT_API_EVENT_RESUME,
    GUEST_HEARTBEAT_API_EVENT_RESIZE_BEGIN,
    GUEST_HEARTBEAT_API_EVENT_RESIZE_END,
    GUEST_HEARTBEAT_API_EVENT_LIVE_MIGRATE_BEGIN,
    GUEST_HEARTBEAT_API_EVENT_LIVE_MIGRATE_END,
    GUEST_HEARTBEAT_API_EVENT_COLD_MIGRATE_BEGIN,
    GUEST_HEARTBEAT_API_EVENT_COLD_MIGRATE_END,
    GUEST_HEARTBEAT_API_EVENT_MAX,
} GuestHeartbeatApiEventT;

typedef enum {
    GUEST_HEARTBEAT_API_NOTIFY_TYPE_UNKNOWN,
    GUEST_HEARTBEAT_API_NOTIFY_TYPE_REVOCABLE,      // vote on an action
    GUEST_HEARTBEAT_API_NOTIFY_TYPE_IRREVOCABLE,    // notification of an action
    GUEST_HEARTBEAT_API_NOTIFY_TYPE_MAX,
} GuestHeartbeatApiNotifyTypeT;

typedef enum {
//
    GUEST_HEARTBEAT_API_VOTE_RESULT_UNKNOWN,
    GUEST_HEARTBEAT_API_VOTE_RESULT_ACCEPT,         // vote to accept an action
    GUEST_HEARTBEAT_API_VOTE_RESULT_REJECT,         // vote to reject an action
    GUEST_HEARTBEAT_API_VOTE_RESULT_COMPLETE,       // ready for action
    GUEST_HEARTBEAT_API_VOTE_RESULT_MAX,
} GuestHeartbeatApiVoteResultT;

// ****************************************************************************
// Guest Heartbeat API - Initialization Data
// =========================================
//  Description:
//      Configuration data used on registration.
//
//  Fields:
//      application_name        name of the application, used for logging
//      heartbeat_interval_ms   the interval for heartbeat challenges
//      vote_secs               maximum time to wait for a vote to complete
//      shutdown_notice_secs    maximum time to wait for a shutdown prep
//      suspend_notice_secs     maximum time to wait for a suspend prep
//      resume_notice_secs      maximum time to wait for a resume prep
//      corrective_action       corrective action on heartbeat timeouts
//
//  Note: minimum heartbeat interval is 400 milliseconds.  Anything below this
//        interval will cause the registration to be rejected.
//
typedef struct {
    char application_name[GUEST_HEARTBEAT_API_APPLICATION_NAME_MAX];
    int heartbeat_interval_ms;
    int vote_secs;
    int shutdown_notice_secs;
    int suspend_notice_secs;
    int resume_notice_secs;
    GuestHeartbeatApiActionT corrective_action;
} GuestHeartbeatApiInitDataT;
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Register State Callback
// =============================================
// Description:
//      Called when the registration with the Guest-Client changes.  Situations
//      this callback can be invoked are the following:
//          - Guest-Client accepts the registration,
//          - Guest-Client rejects the registration, and
//          - Guest-Client connection fails.
//
//      If the registration state is False, the application needs to register
//      again with the Guest-Client.
//
// Parameters:
//      state   the registration state of the application.
//
typedef void (*GuestHeartbeatApiRegisterStateCallbackT)
        (bool state);
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Health Check Callback
// ===========================================
//  Description:
//      Called by the Guest-Client to request the current health of the
//      application.
//
//  Parameters:
//      health              the health of the application
//      corrective_action   the corrective action to be taken when unhealthy
//      log_msg             an indication of why the application is unhealthy.
//
typedef void (*GuestHeartbeatApiHealthCheckCallbackT)
        (bool* health, GuestHeartbeatApiActionT* corrective_action,
         char log_msg[]);
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Action Notify Callback
// ============================================
// Description:
//      Called when the Guest-Client wants to notify the application of an
//      action. The notification type indicates if this is a vote or a
//      notification.
//
// Parameters:
//      invocation_id       the unique identifier for the action.
//      event               the type of event for the action.
//      notify_type         the type of notification for the action.
//
typedef void (*GuestHeartbeatApiActionNotifyCallbackT)
        (int invocation_id, GuestHeartbeatApiEventT event,
         GuestHeartbeatApiNotifyTypeT notify_type);
// ****************************************************************************

typedef struct {
    GuestHeartbeatApiRegisterStateCallbackT register_state;
    GuestHeartbeatApiHealthCheckCallbackT health_check;
    GuestHeartbeatApiActionNotifyCallbackT action_notify;
} GuestHeartbeatApiCallbacksT;

// ****************************************************************************
// Guest Heartbeat API - Action String
// ===================================
extern const char* guest_heartbeat_api_action_str(
        GuestHeartbeatApiActionT action );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Event String
// ==================================
extern const char* guest_heartbeat_api_event_str(
        GuestHeartbeatApiEventT event );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Notify String
// ===================================
extern const char* guest_heartbeat_api_notify_str(
        GuestHeartbeatApiNotifyTypeT notify );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Vote Result String
// ========================================
extern const char* guest_heartbeat_api_vote_result_str(
        GuestHeartbeatApiVoteResultT vote_result );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Register
// ==============================
// Description:
//      Sends a registration request to the Guest-Client. A try-again can be
//      returned which indicates that registration should be attempted again
//      at a later time.
//
// Parameters:
//      init_data   configuration parameters and timeout values for
//                  this application.
//
// Returns:
//      GUEST_API_OKAY on success, GUEST_API_TRY_AGAIN if Guest-Client could
//      not be reached, otherwise failure.
//
extern GuestApiErrorT guest_heartbeat_api_register(
        GuestHeartbeatApiInitDataT* init_data );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Deregister
// ================================
// Description:
//      Sends a deregister to the Guest-Client.
//
// Parameters:
//      log_msg     indication of the reason for the de-registration.
//
// Returns:
//      GUEST_API_OKAY on success, otherwise failure.
//
extern GuestApiErrorT guest_heartbeat_api_deregister( char log_msg[] );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Send Action Response
// ==========================================
// Description:
//      Sends an action response to the Guest-Client indicating the action
//      is accepted, rejected or completed.
//
// Parameters:
//      invocation_id   the unique identifier from the action callback.
//      event           the type of event from the action callback.
//      notify_type     the type of notification from the action callback.
//      vote_result     indication of acceptance of the action.
//      log_msg         an indication of why the action was rejected.
//
// Returns:
//      GUEST_API_OKAY on success, otherwise failure.
//
extern GuestApiErrorT guest_heartbeat_api_send_action_response(
        int invocation_id, GuestHeartbeatApiEventT event,
        GuestHeartbeatApiNotifyTypeT notify_type,
        GuestHeartbeatApiVoteResultT vote_result, char log_msg[] );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Dispatch
// ==============================
// Description:
//      Called when the selection object returned by guest_heartbeat_api_get_selobj
//      becomes readable.
//
// Parameters:
//      selobj      the selection object that has become readable.
//
// Returns:
//      Nothing
//
extern void guest_heartbeat_api_dispatch( int selobj );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Get Selection Object
// ==========================================
// Description:
//      Returns a selection object that can be used with poll or select.
//
// Parameters:
//      None
//
// Returns:
//      A valid selection object, otherwise -1.
//
extern int guest_heartbeat_api_get_selobj( void );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Initialize
// ================================
// Description:
//      Initialize the Guest Heartbeat API library for use.
//
// Parameters:
//      callbacks       a listing of callbacks for receiving registration
//                      state changes, health checks, and action notifications
//                      (all are required to be non-NULL).
//
// Returns:
//      GUEST_API_OKAY on success, otherwise failure.
//
extern GuestApiErrorT guest_heartbeat_api_initialize(
        GuestHeartbeatApiCallbacksT* callbacks );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat API - Finalize
// ==============================
// Description:
//      Finalize the Guest Heartbeat API library.
//
// Parameters:
//      None
//
// Returns:
//      GUEST_API_OKAY on success, otherwise failure.
//
extern GuestApiErrorT guest_heartbeat_api_finalize( void );
// ****************************************************************************

#ifdef __cplusplus
}
#endif

#endif /* __GUEST_HEARTBEAT_API_H__ */

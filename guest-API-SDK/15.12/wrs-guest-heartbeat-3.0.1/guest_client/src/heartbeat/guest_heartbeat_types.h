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
#ifndef __GUEST_HEARTBEAT_TYPES_H__
#define __GUEST_HEARTBEAT_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GUEST_HEARTBEAT_ACTION_UNKNOWN,
    GUEST_HEARTBEAT_ACTION_NONE,
    GUEST_HEARTBEAT_ACTION_REBOOT,
    GUEST_HEARTBEAT_ACTION_STOP,
    GUEST_HEARTBEAT_ACTION_LOG,
    GUEST_HEARTBEAT_ACTION_MAX,
} GuestHeartbeatActionT;

typedef enum {
    GUEST_HEARTBEAT_EVENT_UNKNOWN,
    GUEST_HEARTBEAT_EVENT_STOP,
    GUEST_HEARTBEAT_EVENT_REBOOT,
    GUEST_HEARTBEAT_EVENT_SUSPEND,
    GUEST_HEARTBEAT_EVENT_PAUSE,
    GUEST_HEARTBEAT_EVENT_UNPAUSE,
    GUEST_HEARTBEAT_EVENT_RESUME,
    GUEST_HEARTBEAT_EVENT_RESIZE_BEGIN,
    GUEST_HEARTBEAT_EVENT_RESIZE_END,
    GUEST_HEARTBEAT_EVENT_LIVE_MIGRATE_BEGIN,
    GUEST_HEARTBEAT_EVENT_LIVE_MIGRATE_END,
    GUEST_HEARTBEAT_EVENT_COLD_MIGRATE_BEGIN,
    GUEST_HEARTBEAT_EVENT_COLD_MIGRATE_END,
    GUEST_HEARTBEAT_EVENT_MAX,
} GuestHeartbeatEventT;

typedef enum {
    GUEST_HEARTBEAT_NOTIFY_UNKNOWN,
    GUEST_HEARTBEAT_NOTIFY_REVOCABLE,
    GUEST_HEARTBEAT_NOTIFY_IRREVOCABLE,
    GUEST_HEARTBEAT_NOTIFY_MAX,
} GuestHeartbeatNotifyT;

typedef enum {
    GUEST_HEARTBEAT_VOTE_RESULT_UNKNOWN,
    GUEST_HEARTBEAT_VOTE_RESULT_ACCEPT,
    GUEST_HEARTBEAT_VOTE_RESULT_REJECT,
    GUEST_HEARTBEAT_VOTE_RESULT_COMPLETE,
    GUEST_HEARTBEAT_VOTE_RESULT_TIMEOUT,
    GUEST_HEARTBEAT_VOTE_RESULT_ERROR,
    GUEST_HEARTBEAT_VOTE_RESULT_MAX,
} GuestHeartbeatVoteResultT;

// ****************************************************************************
// Guest Heartbeat Types - Action String
// =====================================
extern const char* guest_heartbeat_action_str( GuestHeartbeatActionT action );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Types - Event String
// ====================================
extern const char* guest_heartbeat_event_str( GuestHeartbeatEventT event );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Types - Notify String
// =====================================
extern const char* guest_heartbeat_notify_str( GuestHeartbeatNotifyT notify );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Types - Vote Result String
// ==========================================
extern const char* guest_heartbeat_vote_result_str( GuestHeartbeatVoteResultT result );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Types - Merge Action
// ====================================
extern GuestHeartbeatActionT guest_heartbeat_merge_action(
        GuestHeartbeatActionT current_action, GuestHeartbeatActionT new_action );
// ****************************************************************************

#ifdef __cplusplus
}
#endif

#endif /* __GUEST_HEARTBEAT_TYPES_H__ */

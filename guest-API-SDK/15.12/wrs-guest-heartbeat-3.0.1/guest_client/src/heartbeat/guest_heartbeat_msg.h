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
#ifndef __GUEST_HERATBEAT_MESSAGE_H__
#define __GUEST_HEARTBEAT_MESSAGE_H__

#include <stdint.h>
#include <stdbool.h>

#include "guest_limits.h"
#include "guest_types.h"

#include "guest_heartbeat_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GUEST_HEARTBEAT_MAX_LOG_MSG_SIZE        192

typedef struct {
    char name[GUEST_NAME_MAX_CHAR];
    unsigned int heartbeat_interval_ms;
    unsigned int vote_ms;
    unsigned int shutdown_notice_ms;
    unsigned int suspend_notice_ms;
    unsigned int resume_notice_ms;
    unsigned int restart_ms;
    GuestHeartbeatActionT corrective_action;
} GuestHeartbeatMsgInitDataT;

typedef void (*GuestHeartbeatMsgChannelStateChangeT) (bool state);
typedef void (*GuestHeartbeatMsgRecvInitT)
        (int invocation_id, GuestHeartbeatMsgInitDataT* data);
typedef void (*GuestHeartbeatMsgRecvInitAckT) (int invocation_id);
typedef void (*GuestHeartbeatMsgRecvExitT) (char log_msg[]);
typedef void (*GuestHeartbeatMsgRecvChallengeT) (void);
typedef void (*GuestHeartbeatMsgRecvChallengeAckT)
        (bool health, GuestHeartbeatActionT corrective_action, char log_msg[]);
typedef void (*GuestHeartbeatMsgRecvActionNotifyT)
        (int invocation_id, GuestHeartbeatEventT event,
         GuestHeartbeatNotifyT notify, int timeout_ms);
typedef void (*GuestHeartbeatMsgRecvActionResponseT)
        (int invocation_id, GuestHeartbeatEventT event,
         GuestHeartbeatNotifyT notify, GuestHeartbeatVoteResultT vote_result,
         char log_msg[]);

typedef struct {
    GuestHeartbeatMsgChannelStateChangeT channel_state_change;
    GuestHeartbeatMsgRecvInitT recv_init;
    GuestHeartbeatMsgRecvInitAckT recv_init_ack;
    GuestHeartbeatMsgRecvExitT recv_exit;
    GuestHeartbeatMsgRecvChallengeT recv_challenge;
    GuestHeartbeatMsgRecvChallengeAckT recv_challenge_ack;
    GuestHeartbeatMsgRecvActionNotifyT recv_action_notify;
    GuestHeartbeatMsgRecvActionResponseT recv_action_response;
} GuestHeartbeatMsgCallbacksT;

// ****************************************************************************
// Guest Heartbeat Message - Send Init
// ===================================
extern GuestErrorT guest_heartbeat_msg_send_init( int invocation_id,
        GuestHeartbeatMsgInitDataT* data );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Send Init Ack
// =======================================
extern GuestErrorT guest_heartbeat_msg_send_init_ack( int invocation_id );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Send Exit
// ===================================
extern GuestErrorT guest_heartbeat_msg_send_exit( char log_msg[] );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Send Challenge
// ========================================
extern GuestErrorT guest_heartbeat_msg_send_challenge( void );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Send Challenge Response
// =================================================
extern GuestErrorT guest_heartbeat_msg_send_challenge_response(
        bool health, GuestHeartbeatActionT corrective_action, char log_msg[] );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Send Action Notify
// ============================================
extern GuestErrorT guest_heartbeat_msg_send_action_notify(
        int invocation_id, GuestHeartbeatEventT event,
        GuestHeartbeatNotifyT notify, int timeout_ms );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Send Action Response
// ==============================================
GuestErrorT guest_heartbeat_msg_send_action_response(
        int invocation_id, GuestHeartbeatEventT event,
        GuestHeartbeatNotifyT notify, GuestHeartbeatVoteResultT vote_result,
        char log_msg[] );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Initialize
// ====================================
extern GuestErrorT guest_heartbeat_msg_initialize(
        char* comm_device, GuestHeartbeatMsgCallbacksT* callbacks );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Message - Finalize
// ==================================
extern GuestErrorT guest_heartbeat_msg_finalize( void );
// ****************************************************************************

#ifdef __cplusplus
}
#endif

#endif /* __GUEST_HEARTBEAT_MESSAGE_H__ */

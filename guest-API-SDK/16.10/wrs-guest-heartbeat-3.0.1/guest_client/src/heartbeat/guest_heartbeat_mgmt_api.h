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
#ifndef __GUEST_HERATBEAT_MGMT_API_H__
#define __GUEST_HEARTBEAT_MGMT_API_H__

#include <stdbool.h>

#include "guest_types.h"
#include "guest_heartbeat_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*GuestHeartbeatMgmtApiActionResponseT)
        (GuestHeartbeatEventT event, GuestHeartbeatNotifyT notify,
         GuestHeartbeatVoteResultT vote_result, char log_msg[]);

// ****************************************************************************
// Guest Heartbeat Management API - Get Health
// ===========================================
extern GuestErrorT guest_heartbeat_mgmt_api_get_health(
        bool* health, GuestHeartbeatActionT* corrective_action, char log_msg[],
        int log_msg_size );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Action Abort
// =============================================
extern void guest_heartbeat_mgmt_api_action_abort( void );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Action Notify
// ==============================================
extern GuestErrorT guest_heartbeat_mgmt_api_action_notify(
        GuestHeartbeatEventT event, GuestHeartbeatNotifyT notify, bool* wait,
        GuestHeartbeatMgmtApiActionResponseT callback );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Initialize
// ===========================================
extern GuestErrorT guest_heartbeat_mgmt_api_initialize( void );
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Management API - Finalize
// =========================================
extern GuestErrorT guest_heartbeat_mgmt_api_finalize( void );
// ****************************************************************************

#ifdef __cplusplus
}
#endif

#endif /* __GUEST_HEARTBEAT_MGMT_API_H__ */

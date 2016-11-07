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
#include "guest_heartbeat_types.h"

// ****************************************************************************
// Guest Heartbeat Types - Action String
// =====================================
const char* guest_heartbeat_action_str( GuestHeartbeatActionT action )
{
    switch (action)
    {
        case GUEST_HEARTBEAT_ACTION_NONE:   return "none";
        case GUEST_HEARTBEAT_ACTION_REBOOT: return "reboot";
        case GUEST_HEARTBEAT_ACTION_STOP:   return "stop";
        case GUEST_HEARTBEAT_ACTION_LOG:    return "log";
        default:
            return "action-???";
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Types - Event String
// ====================================
const char* guest_heartbeat_event_str( GuestHeartbeatEventT event )
{
    switch (event)
    {
        case GUEST_HEARTBEAT_EVENT_STOP:               return "stop";
        case GUEST_HEARTBEAT_EVENT_REBOOT:             return "reboot";
        case GUEST_HEARTBEAT_EVENT_SUSPEND:            return "suspend";
        case GUEST_HEARTBEAT_EVENT_PAUSE:              return "pause";
        case GUEST_HEARTBEAT_EVENT_UNPAUSE:            return "unpause";
        case GUEST_HEARTBEAT_EVENT_RESUME:             return "resume";
        case GUEST_HEARTBEAT_EVENT_RESIZE_BEGIN:       return "resize-begin";
        case GUEST_HEARTBEAT_EVENT_RESIZE_END:         return "resize-end";
        case GUEST_HEARTBEAT_EVENT_LIVE_MIGRATE_BEGIN: return "live-migrate-begin";
        case GUEST_HEARTBEAT_EVENT_LIVE_MIGRATE_END:   return "live-migrate-end";
        case GUEST_HEARTBEAT_EVENT_COLD_MIGRATE_BEGIN: return "cold-migrate-begin";
        case GUEST_HEARTBEAT_EVENT_COLD_MIGRATE_END:   return "cold-migrate-end";
        default:
            return "event-???";
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Types - Notify String
// =====================================
const char* guest_heartbeat_notify_str( GuestHeartbeatNotifyT notify )
{
    switch (notify)
    {
        case GUEST_HEARTBEAT_NOTIFY_REVOCABLE:   return "revocable";
        case GUEST_HEARTBEAT_NOTIFY_IRREVOCABLE: return "irrevocable";
        default:
            return "notify-???";
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Types - Vote Result String
// ==========================================
const char* guest_heartbeat_vote_result_str( GuestHeartbeatVoteResultT result )
{
    switch (result)
    {
        case GUEST_HEARTBEAT_VOTE_RESULT_ACCEPT:   return "accept";
        case GUEST_HEARTBEAT_VOTE_RESULT_REJECT:   return "reject";
        case GUEST_HEARTBEAT_VOTE_RESULT_COMPLETE: return "complete";
        case GUEST_HEARTBEAT_VOTE_RESULT_TIMEOUT:  return "timeout";
        case GUEST_HEARTBEAT_VOTE_RESULT_ERROR:    return "error";
        default:
            return "vote-???";
    }
}
// ****************************************************************************

// ****************************************************************************
// Guest Heartbeat Types - Merge Action
// ====================================
GuestHeartbeatActionT guest_heartbeat_merge_action(
        GuestHeartbeatActionT current_action, GuestHeartbeatActionT new_action )
{
    switch (new_action)
    {
        case GUEST_HEARTBEAT_ACTION_STOP:
            return new_action;

        case GUEST_HEARTBEAT_ACTION_REBOOT:
            if (GUEST_HEARTBEAT_ACTION_STOP != current_action)
                return new_action;
            return current_action;

        case GUEST_HEARTBEAT_ACTION_LOG:
            if ((GUEST_HEARTBEAT_ACTION_STOP != current_action) &&
                (GUEST_HEARTBEAT_ACTION_REBOOT != current_action))
                return new_action;
            return current_action;

        default:
            return current_action;
    }
}
// ****************************************************************************

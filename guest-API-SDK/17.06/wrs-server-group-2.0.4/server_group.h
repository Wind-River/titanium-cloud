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

/**
*/

#ifndef GUEST_CLIENT_H
#define GUEST_CLIENT_H

// keys for server group messages
#define VERSION         "version"
#define MSG_TYPE        "msg_type"
#define SEQ             "seq"
#define SOURCE_INSTANCE "source_instance"
#define DATA            "data"
#define LOG_MSG         "log_msg"

/* This implements a library for server group messaging.
 * 
 * The general idea is that everything that goes through this is multiplexed
 * over a single unix socket so that the guest app only needs to monitor one
 * socket for activity.
 *
 * This has not been tested for safe use in multithreaded applications.
 */


/* Function signature for the server group broadcast messaging callback function.
 * source_instance is a null-terminated string of the form "instance-xxxxxxxx".
 * The message contents are entirely up to the sender of the message.
 */
typedef void (*sg_broadcast_msg_handler_t)(char *source_instance, void *msg,
                                           unsigned short msglen);

/* Function signature for the server group notification callback function.  The
 * message is basically the notification as sent out by nova with some information
 * removed as not relevant.  The message is not null-terminated, though it is
 * a JSON representation of a python dictionary.
 */
typedef void (*sg_notification_msg_handler_t)(void *msg, unsigned short msglen);

/* Function signature for the server group status callback function.  The
 * message is a JSON representation of a list of dictionaries, each of which
 * corresponds to a single server.  The message is not null-terminated.
 */
typedef void (*sg_status_msg_handler_t)(void *msg, unsigned short msglen);



/* Get error message from most recent library call that returned an error. */
char *sg_get_error();

/* Allocate socket, set up callbacks, etc.  This must be called once before
 * any other API calls.
 *
 * Returns a socket that must be monitored for activity using select/poll/etc.
 * A negative return value indicates an error of some kind.
 */
int init_sg(sg_broadcast_msg_handler_t broadcast_handler,
		sg_notification_msg_handler_t notification_handler,
		sg_status_msg_handler_t status_handler);

/* This should be called when the socket becomes readable.  This may result in
 * callbacks being called.  Returns 0 on success.
 * A negative return value indicates an error of some kind.
 */
int process_sg_msg();

/* max msg length for a broadcast message */
#define MAX_MSG_DATA_LEN 3050

/* Send a server group broadcast message.  Returns 0 on success.
 * A negative return value indicates an error of some kind.
 * The message must be a null-terminated string without embedded newlines.
 * len is no longer used.
 */
int sg_msg_broadcast(void *msg, unsigned int len);

/* Request a status update for all servers in the group.
 * Returns 0 if the request was successfully sent.
 * A negative return value indicates an error of some kind.
 *
 * A successful response will cause the status_handler callback
 * to be called.
 *
 * If a status update has been requested but the callback has not yet
 * been called this may result in the previous request being cancelled.
 */
int sg_request_status();


#endif


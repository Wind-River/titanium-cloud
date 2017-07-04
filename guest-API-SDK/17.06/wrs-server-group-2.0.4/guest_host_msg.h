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

#ifndef GUESTHOST_MSG_H
#define GUESTHOST_MSG_H

/* This implements a library for generic guest-host messaging.
 * 
 * This handles some of the boilerplate code that would be common to
 * all applications using the generic guest/host communications.  That way
 * any changes can be made in a single place.
 *
 * This should be threadsafe as long as multiple threads don't try to
 * simultaneously process incoming messages using the same gh_info_t pointer.
 *
 */

#include <sys/un.h>
#include <json-c/json.h>

/* Function signature for the guest-host messaging callback function.
 * source_addr is a null-terminated string representing the host source address.
 * The message contents are up to the sender of the message.
 */

typedef void (*gh_msg_handler_t)(const char *source_addr, struct json_object *jobj_msg);

#define GH_ERRORSIZE 400
typedef struct {
    int sock;                   // socket for talking to guest agent
    struct sockaddr_un svaddr;  // address of guest agent
    int svaddrlen;              // length of guest agent address
    char errorbuf[GH_ERRORSIZE];
    gh_msg_handler_t gh_msg_handler;
} gh_info_t;



/* Allocate socket, set up callbacks, etc.  This must be called once before
 * any other API calls.  "addr" is a null-terminated string of 16 chars or less
 * (including the null) that is unique within this guest.  "info" is the address
 * of a value-result pointer that will be updated during the call.
 *
 * On success returns a socket and "info" is updated to point to an allocated chunk of memory.
 * On error will return -1.  If it was unable to allocate memory then "info" will be
 * NULL.  If it was able to allocate memory but something else failed then "info" will
 * be non-NULL and you can call gh_get_error() to get an error message.
 */
int gh_init(gh_msg_handler_t msg_handler, char *addr, gh_info_t **info);

/* This should be called when the socket becomes readable.  This may result in
 * callbacks being called.  Returns 0 on success.
 * A negative return value indicates an error of some kind.
 */
int gh_process_msg(gh_info_t *info);

/* Send a message to an address on the host.
 * Returns 0 on success.
 * A negative return value indicates an error of some kind.
 * The message must be a null-terminated string without embedded newlines.
 */
int gh_send_msg(gh_info_t *info, const char *dest_addr, const char *msg);

/* Get error message from most recent library call that returned an error. */
char *gh_get_error(gh_info_t *info);



#endif

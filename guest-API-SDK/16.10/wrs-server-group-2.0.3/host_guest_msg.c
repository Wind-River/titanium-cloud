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

#define _GNU_SOURCE /* for memmem() */

#include <stddef.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <json-c/json.h>

#include "host_guest_msg_type.h"

/*
 Multiple messages from the host can be bundled together into a single "read"
 so we need to check message boundaries and handle breaking the message apart.
 Assume a valid message does not contain newline '\n', and newline is added to
 the beginning and end of each message by the sender to delimit the boundaries.
*/

void parser(void *buf, ssize_t len, int fd, json_tokener* tok, int newline_found)
{
    json_object *jobj = json_tokener_parse_ex(tok, buf, len);
    enum json_tokener_error jerr = json_tokener_get_error(tok);

    if (jerr == json_tokener_success) {
        process_msg(jobj, fd);
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


void handle_virtio_serial_msg(void *buf, ssize_t len, int fd, json_tokener* tok)
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
            parser(buf, len_head, fd, tok, 1);

        // start of the tail: skip newline
        buf += len_head+1;
        // length of the tail: deduct 1 for the newline character
        len -= len_head+1;

        // continue to process the tail.
        goto next;
    }
    else {
         parser(buf, len, fd, tok, 0);
    }
}

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

#ifndef HOST_GUEST_MSG_TYPE_H
#define HOST_GUEST_MSG_TYPE_H

#include <stdint.h>
#include <json-c/json.h>

#include "misc.h"

#define CUR_VERSION 2

// The server group message is JSON encoded null-terminated string without
// embedded newlines.
// keys for messages:
#define VERSION "version"
#define SOURCE_ADDR "source_addr"
#define DEST_ADDR "dest_addr"
#define SOURCE_INSTANCE "source_instance"
#define DEST_INSTANCE "dest_instance"
#define DATA "data"

// Used by the message reassembly code.
// The underlying implementation uses page-size buffers
#define HOST_GUEST_BUFSIZE 4096

#define MAX_INSTANCES 100
#define MAX_FDS_HOST (MAX_INSTANCES+10)
#define INSTANCE_NAME_SIZE 20

// unix socket abstract namespace address of both host and guest agent
#define AGENT_ADDR "cgcs.messaging"

#define UNIX_ADDR_LEN 16

// Initialize connection message reassembly buffer
void init_msgbuf(int fd);

/* Used to store partial host_guest messages */
typedef struct {
    unsigned short len;
    char buf[HOST_GUEST_BUFSIZE];
} msgbuf_t;

void handle_virtio_serial_msg(void *buf, ssize_t len, int fd, json_tokener* tok);
void process_msg(json_object *jobj_msg, int fd);

#endif

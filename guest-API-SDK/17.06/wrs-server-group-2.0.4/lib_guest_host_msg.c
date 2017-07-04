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


/* This implements a library to be used for guest/host clients to interface with
 * non-standard functionality on the host/guest using the backchannel
 * communications pathway.  (The same library can be used for both directions.)
 * 
 * The general idea is that everything that goes through this is multiplexed
 * over a single unix socket so that the guest app only needs to monitor one
 * socket for activity.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/un.h>
#include <errno.h>
#include <stdlib.h>
#include <json-c/json.h>

#include "guest_host_msg.h"
#include "host_guest_msg_type.h"


/* Send a message to an address on the host.
 * Returns 0 on success.
 * A negative return value indicates an error of some kind.
 */
int gh_send_msg(gh_info_t *info, const char *dest_addr, const char *msg)
{
    int rc;

    //parse msg data
    struct json_object *jobj_data = json_tokener_parse(msg);
    if (jobj_data == NULL) {
        snprintf(info->errorbuf, sizeof(info->errorbuf)-1, "failed to parse msg");
        return -1;
    }

    struct json_object *jobj_outmsg = json_object_new_object();
    if (jobj_outmsg == NULL) {
        snprintf(info->errorbuf, sizeof(info->errorbuf)-1, "failed to allocate json object for outmsg");
        json_object_put(jobj_data);
        return -1;
    }

    json_object_object_add(jobj_outmsg, DATA, jobj_data);
    json_object_object_add(jobj_outmsg, VERSION, json_object_new_int(CUR_VERSION));
    json_object_object_add(jobj_outmsg, DEST_ADDR, json_object_new_string(dest_addr));


    const char *outmsg = json_object_to_json_string_ext(jobj_outmsg, JSON_C_TO_STRING_PLAIN);
    int msglen = strlen(outmsg);

    rc = sendto(info->sock, outmsg, msglen, 0, (struct sockaddr *) &info->svaddr,
                info->svaddrlen);
    if (rc != msglen) {
        if (rc > 0) {
            snprintf(info->errorbuf, sizeof(info->errorbuf)-1, "sendto returned %d, expected %d",
                     rc, msglen);
        } else
            snprintf(info->errorbuf, sizeof(info->errorbuf)-1, "sendto: %m");
        goto failed;
    }

    json_object_put(jobj_outmsg);
    return 0;
failed:
    json_object_put(jobj_outmsg);
    return -1;
}



/* Read a message from the socket and process it. */
int gh_process_msg(gh_info_t *info)
{
    char buf[HOST_GUEST_BUFSIZE];
    int len;

    len = recv(info->sock, buf, sizeof(buf), 0);
    if (len == -1) {
        if (errno == EAGAIN)
            return 0;
        else {
            snprintf(info->errorbuf, sizeof(info->errorbuf)-1, "error receiving msg: %m");
            return -1;
        }
    }
    
    struct json_object *jobj_msg = json_tokener_parse(buf);
    if (jobj_msg == NULL) {
        snprintf(info->errorbuf, sizeof(info->errorbuf)-1, "failed to parse msg");
        return -1;
    }

    // parse version
    struct json_object *jobj_version;
    if (!json_object_object_get_ex(jobj_msg, VERSION, &jobj_version)) {
        snprintf(info->errorbuf, sizeof(info->errorbuf)-1, "failed to parse version");
        goto failed;
    }
    int version = json_object_get_int(jobj_version);

    if (version != CUR_VERSION) {
        snprintf(info->errorbuf, sizeof(info->errorbuf)-1,
            "invalid version %d, expecting %d", version, CUR_VERSION);
        goto failed;
    }

    // parse source address
    struct json_object *jobj_source_addr;
    if (!json_object_object_get_ex(jobj_msg, SOURCE_ADDR, &jobj_source_addr)) {
        snprintf(info->errorbuf, sizeof(info->errorbuf)-1, "failed to parse source_addr");
        goto failed;
    }
    const char *source_addr = json_object_get_string(jobj_source_addr);

    // parse data. data is a json object that is nested inside the msg
    struct json_object *jobj_data;
    if (!json_object_object_get_ex(jobj_msg, DATA, &jobj_data)) {
        snprintf(info->errorbuf, sizeof(info->errorbuf)-1, "failed to parse data");
        goto failed;
    }

    if (info->gh_msg_handler)
        info->gh_msg_handler(source_addr, jobj_data);

    json_object_put(jobj_msg);
    return 0;

failed:
    json_object_put(jobj_msg);
    return -1;
}


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

int gh_init(gh_msg_handler_t msg_handler, char *addr, gh_info_t **in_info)
{
    int flags;
    int addrlen;
    struct sockaddr_un cliaddr;
    gh_info_t *info;
    
    *in_info = malloc(sizeof(**in_info));
    if (!*in_info)
        /* unable to allocate memory */
        return -1;
    
    info = *in_info;
    
    /* socket for talking to guest agent */
    info->sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (info->sock == -1) {
        snprintf(info->errorbuf, sizeof(info->errorbuf)-1, "unable to open socket: %m");
        goto free_out;
    }
    
    flags = fcntl(info->sock, F_GETFL, 0);
    fcntl(info->sock, F_SETFL, flags | O_NONBLOCK);

    /* our address */
    memset(&cliaddr, 0, sizeof(struct sockaddr_un));
    cliaddr.sun_family = AF_UNIX;
    cliaddr.sun_path[0] = '\0';
    strncpy(cliaddr.sun_path+1, addr,
            sizeof(cliaddr.sun_path) - 2);
    addrlen = sizeof(sa_family_t) + strlen(addr) + 1;

    if (bind(info->sock, (struct sockaddr *) &cliaddr, addrlen) == -1) {
        snprintf(info->errorbuf, sizeof(info->errorbuf)-1, "unable to bind socket: %m");
        goto close_out;
    }

    /* guest agent address */
    memset(&info->svaddr, 0, sizeof(struct sockaddr_un));
    info->svaddr.sun_family = AF_UNIX;
    info->svaddr.sun_path[0] = '\0';
    strncpy(info->svaddr.sun_path+1, AGENT_ADDR, sizeof(info->svaddr.sun_path) - 2);
    info->svaddrlen = sizeof(sa_family_t) + strlen(AGENT_ADDR) + 1;
    
    /* set up callback pointers */
    info->gh_msg_handler = msg_handler;
    
    return info->sock;

close_out:
    close(info->sock);
free_out:
    free(info);
    return -1;
}

/* Provide access to the error message if the most recent call failed. */
char *gh_get_error(gh_info_t *info)
{
    return info->errorbuf;
}

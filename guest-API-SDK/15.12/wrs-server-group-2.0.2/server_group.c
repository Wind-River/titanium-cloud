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


/* This implements a library to be used for guest clients to interface with
 * non-standard functionality on the host using the backchannel
 * communications pathway.
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

#include "server_group.h"
#include "host_guest_msg_type.h"

/* unix socket abstract namespace address of guest agent */
#define GUEST_CLIENT_ADDR "cgcs.server_grp"

#define HOST_GUEST_BUFSIZE 4096 // max size of message that can be received


/* Server Group message is encoded in JSON format.
   The message sent out to UNIX socket is a null-terminated JSON format string
   without embedded newlines.

   Format:
   {key:value,key:value,..., key:value}

   Key/value pairs:
   "version": <integer>   - version of the interface
   "type":    < string>   - one of these types “broadcast", "notification",
                            "status_query", "status_response",
                            "status_response_done”

   "req": <integer>      - sequence number for status_query/status_response/
                           status_response_done messages

   "source_instance“: <string> - source instance that send out the broadcast
   "data": <string>  - message content for broadcast, notification,
                       status_query and status_response messages.
                       Must be a null-terminated string without embedded newlines.


   Message Types:
   broadcast            - incoming or outgoing
   notification         - incoming, state change of other servers within the
                            server group
   status_query         - outgoing, query the current state of all servers
                            within the server group
   status_response      - incoming, one or more responses to the status_query
   status_response_done - incoming, last response to the status query

*/

// server group message type
#define GRP_BROADCAST     "broadcast"                // broadcast message from another server
#define GRP_NOTIFICATION  "notification"             // notification of server state change
#define GRP_STATUS_QUERY  "status_query"             // query of status of all servers in group
#define GRP_STATUS_RESP   "status_response"          // query response msg (could be several)
#define GRP_STATUS_RESP_DONE "status_response_done"  // query response done msg (no data)
#define GRP_NACK "nack"  // nack msg indicating parse or protocol error from host


/* Header for incoming server group messages.  The exact contents will differ
 * depending on message type.
 *
 * "len" is the overall length including header.
 *
 * For messages with a TYPE of GRP_BROADCAST only, the "sinstance" field
 * will contain the instance name of the instance that sent the broadcast, and
 * DATA will contain the message that was sent.
 *
 */
static int sock;                   // socket for talking to guest agent
static struct sockaddr_un svaddr;  // address of guest agent
static int svaddrlen;              // length of guest agent address

static unsigned int status_seqnum;  // status query sequence number
static char *status_buf;            // status query reassembly buffer
static unsigned long status_size;   // current status buffer size
static unsigned long status_len;    // currently used buffer length

#define ERRORSIZE 400
static char errorbuf[ERRORSIZE];

static sg_broadcast_msg_handler_t sg_broadcast_msg_callback;
static sg_notification_msg_handler_t sg_notification_msg_callback;
static sg_status_msg_handler_t sg_status_msg_callback;


/* Generic routine to send a server group message down to the host. */
int sg_send_host_msg(const char *msg_type, int seq, const char *data)
{
    int rc;

    struct json_object *jobj_data = json_object_new_object();
    if (jobj_data == NULL) {
        snprintf(errorbuf, ERRORSIZE-1, "failed to allocate json object for data");
        return -1;
    }

    json_object_object_add(jobj_data, VERSION, json_object_new_int(CUR_VERSION));
    json_object_object_add(jobj_data, MSG_TYPE, json_object_new_string(msg_type));
    if (!strcmp(msg_type, GRP_STATUS_QUERY)) {
        json_object_object_add(jobj_data, SEQ, json_object_new_int(seq));
    }

    if (data) {
        json_object_object_add(jobj_data, DATA, json_object_new_string(data));
    }

    struct json_object *jobj_outmsg = json_object_new_object();
    if (jobj_outmsg == NULL) {
        snprintf(errorbuf, ERRORSIZE-1, "failed to allocate json object for outmsg");
        json_object_put(jobj_data);
        return -1;
    }

    json_object_object_add(jobj_outmsg, DATA, jobj_data);
    json_object_object_add(jobj_outmsg, VERSION, json_object_new_int(CUR_VERSION));
    /* This is a known address that nova-compute is listening on */
    json_object_object_add(jobj_outmsg, DEST_ADDR, json_object_new_string(GUEST_CLIENT_ADDR));

    const char *outmsg = json_object_to_json_string_ext(jobj_outmsg, JSON_C_TO_STRING_PLAIN);
    int msglen = strlen(outmsg);

    rc = sendto(sock, outmsg, msglen, 0, (struct sockaddr *) &svaddr, svaddrlen);
    if (rc != msglen) {
        if (rc > 0) {
            snprintf(errorbuf, ERRORSIZE-1, "sendto returned %d, expected %d",
                     rc, msglen);
        } else
            snprintf(errorbuf, ERRORSIZE-1, "sendto: %m");
        json_object_put(jobj_outmsg);
        return -1;
    }

    json_object_put(jobj_outmsg);
    return 0;
}

/* Send a message to all other servers in our server group. */
int sg_msg_broadcast(void *msg, unsigned int len)
{
    return sg_send_host_msg(GRP_BROADCAST, 0, (const char*)msg);
}

/* Ensure the status buffer is at least as big as the specified size.
 * Call realloc() if necessary to grow the buffer.
 */
int ensure_status_buf_size(unsigned int size)
{
    if (size > status_size) {
        /* need to grow the buffer */
        void *ptr = realloc(status_buf, size);
        if (!ptr) {
            /* hopefully shouldn't happen */
            snprintf(errorbuf, ERRORSIZE-1,
                "unable to realloc buffer to size %u", size);

            /* give up on current status query */
            status_seqnum++;
            status_len = 0;
            return -1;
        }
        status_buf = ptr;
        status_size = size;
    }
    return 0;
}

/* Request current status of all servers in server group.  Due to limitations in
 * the current implementation of host-guest comm channel agents we receive
 * information on one server per response, then a final "done" message.  This
 * could be changed if we fix the host-guest comm channel to handle arbitrarily
 * large messages.
 *
 * Yes, the current design is not the most robust...might want to consider adding
 * an indication of how many servers we expect data for and which one we're on
 * in case we lose a message or something.  Better fix might be to just fix the
 * host-guest comm channel to handle arbitrarily large messages.
 */
int sg_request_status()
{
    int rc;
    /* If we were still receiving status updates from a previous query this
     * will cause them to get dropped.
     */
    status_seqnum++;
    status_len = 0;

    /* Ensure we have room for an empty list otherwise give up.
     * Start with a decent size buffer to minimize reallocs.
     */
    if (ensure_status_buf_size(4096) != 0)
        return -1;

    rc = sg_send_host_msg(GRP_STATUS_QUERY, status_seqnum, NULL);
    if (rc == 0) {
        /* start a list in the buffer */
        status_buf[0] = '[';
        status_len = 1;
    }
    return rc;
}

/* Handle a response to the status query.  This should contain the current
 * status of a single server.  We add it to the data accumulating in the buffer.
 */
int handle_status_resp(unsigned int seqnum, const char *msg, unsigned int len)
{
    if (seqnum != status_seqnum) {
        snprintf(errorbuf, ERRORSIZE-1,
            "status resp seqnum %u doesn't match expected %u",
            seqnum, status_seqnum);
        return -1;
    }

    /* Ensure we have room for new data otherwise give up.
     * Add an extra byte for comma between server data.
     */
    if (ensure_status_buf_size(status_len + len + 1) != 0)
        return -1;

    /* comma-separate the status for each instance */
    if (status_len != 1) {
        status_buf[status_len] = ',';
        status_len++;
    }

    /* Now copy the server status into the buffer */
    memcpy((status_buf + status_len), msg, len);
    status_len += len;
    return 0;
}

/* This tells us that we've received all the server status messages
 * so we can call the callback and then reset things for the next one.
 */
int handle_status_resp_done(unsigned int seqnum)
{
    if (seqnum != status_seqnum) {
        snprintf(errorbuf, ERRORSIZE-1,
            "status resp done seqnum %u doesn't match expected %u",
            seqnum, status_seqnum);
        return -1;
    }

    /* Ensure we have room for list terminator otherwise give up */
    if (ensure_status_buf_size(status_len + 1) != 0)
        return -1;

    if (status_buf) {
        /* terminate the list */
        status_buf[status_len] = ']';
        status_len++;

        if (sg_status_msg_callback)
            sg_status_msg_callback(status_buf, status_len);
    }

    /* reset the buffer */
    status_len = 0;
    /* bump seqnum just in case */
    status_seqnum++;
    return 0;
}


int dispatch_sg_msg(json_object *job_msg)
{
    int rc = 0;

    struct json_object *jobj_msg_type;
    if (!json_object_object_get_ex(job_msg, MSG_TYPE, &jobj_msg_type)) {
        snprintf(errorbuf, ERRORSIZE-1, "failed to parse msg_type");
        return -1;
    }
    const char *msg_type = json_object_get_string(jobj_msg_type);

    struct json_object *jobj_data;
    const char *data;

    // type GRP_STATUS_RESP_DONE message does not have a data field
    if (!strcmp(msg_type, GRP_BROADCAST) ||
        !strcmp(msg_type, GRP_NOTIFICATION) ||
        !strcmp(msg_type, GRP_STATUS_RESP)) {
        if (!json_object_object_get_ex(job_msg, DATA, &jobj_data)) {
            snprintf(errorbuf, ERRORSIZE-1, "failed to parse data for type %s", msg_type);
            return -1;
        }
        data = json_object_get_string(jobj_data);
    }

    if (!strcmp(msg_type, GRP_BROADCAST)) {
        struct json_object *jobj_source_instance;
        if (!json_object_object_get_ex(job_msg, SOURCE_INSTANCE, &jobj_source_instance)) {
            snprintf(errorbuf, ERRORSIZE-1, "failed to parse source_instance for type %s", msg_type);
            return -1;
        }
        const char *source_instance = json_object_get_string(jobj_source_instance);

    	if (sg_broadcast_msg_callback)
        	sg_broadcast_msg_callback((char *)source_instance, (void *)data, strlen(data));
    }
    else if (!strcmp(msg_type, GRP_NOTIFICATION)) {
    	if (sg_notification_msg_callback)
        	sg_notification_msg_callback((void *)data, strlen(data));
    }
    else if ((!strcmp(msg_type, GRP_STATUS_RESP)) || (!strcmp(msg_type, GRP_STATUS_RESP_DONE))) {
        struct json_object *jobj_seq;
        if (!json_object_object_get_ex(job_msg, SEQ, &jobj_seq)) {
            snprintf(errorbuf, ERRORSIZE-1, "failed to parse seq for type %s", msg_type);
            return -1;
        }
        int seq = json_object_get_int(jobj_seq);

        if (!strcmp(msg_type, GRP_STATUS_RESP)) {
            rc = handle_status_resp(seq, data, strlen(data));
        }
        else if (!strcmp(msg_type, GRP_STATUS_RESP_DONE)) {
            rc = handle_status_resp_done(seq);
        }
    }
    else if (!strcmp(msg_type, GRP_NACK)) {
        struct json_object *jobj_log_msg;
        if (!json_object_object_get_ex(job_msg, LOG_MSG, &jobj_log_msg)) {
            snprintf(errorbuf, ERRORSIZE-1, "Nack: failed to parse log_msg");
        }
        const char *log_msg = json_object_get_string(jobj_log_msg);
        snprintf(errorbuf, ERRORSIZE-1, "Nack received, error message from host: %s", log_msg);
        return -1;
    } else {
        snprintf(errorbuf, ERRORSIZE-1, "unknown server group message type %s", msg_type);
        return -1;
    }
    return rc;
}


/* Read a message from the socket and process it. */
int process_sg_msg()
{
    char buf[HOST_GUEST_BUFSIZE];
    int len;
    int rc = -1;

    len = recv(sock, buf, sizeof(buf), 0);
    if (len == -1) {
        if (errno == EAGAIN)
            return 0;
        else {
            snprintf(errorbuf, ERRORSIZE-1, "error receiving msg: %m");
            return -1;
        }
    }

    struct json_object *jobj_msg = json_tokener_parse(buf);
    if (jobj_msg == NULL) {
        snprintf(errorbuf, ERRORSIZE-1, "failed to parse msg");
        return -1;
    }

    // parse version
    struct json_object *jobj_version;
    if (!json_object_object_get_ex(jobj_msg, VERSION, &jobj_version)) {
        snprintf(errorbuf, ERRORSIZE-1, "failed to parse version");
        goto done;
    }
    int version = json_object_get_int(jobj_version);

    if (version != CUR_VERSION) {
        snprintf(errorbuf, ERRORSIZE-1, "invalid version");
        goto done;
    }

    // parse source address
    struct json_object *jobj_source_addr;
    if (!json_object_object_get_ex(jobj_msg, SOURCE_ADDR, &jobj_source_addr)) {
        snprintf(errorbuf, ERRORSIZE-1, "failed to parse source_addr");
        goto done;
    }
    const char *source_addr = json_object_get_string(jobj_source_addr);

    /* check the host sender */
    if (strcmp(source_addr, GUEST_CLIENT_ADDR) != 0) {
        snprintf(errorbuf, ERRORSIZE-1, "unknown sender address %s", source_addr);
        goto done;
    }
    
    // parse data. data is a json object that is nested inside the msg
    struct json_object *jobj_data;
    if (!json_object_object_get_ex(jobj_msg, DATA, &jobj_data)) {
        snprintf(errorbuf, ERRORSIZE-1, "failed to parse data");
        goto done;
    }

    rc = dispatch_sg_msg(jobj_data);

done:
    json_object_put(jobj_msg);
    return rc;
}


/* This needs to be called first to initialize sockets, buffers,
 * callback pointers, etc.
 */
int init_sg(sg_broadcast_msg_handler_t broadcast_handler,
		sg_notification_msg_handler_t notification_handler,
                sg_status_msg_handler_t status_handler)
{
    int flags;
    int addrlen;
    struct sockaddr_un cliaddr;
    
    // socket for talking to guest agent
    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock == -1) {
        snprintf(errorbuf, ERRORSIZE-1, "unable to open socket: %m");
        return -1;
    }
    
    flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    /* our address */
    memset(&cliaddr, 0, sizeof(struct sockaddr_un));
    cliaddr.sun_family = AF_UNIX;
    cliaddr.sun_path[0] = '\0';
    strncpy(cliaddr.sun_path+1, GUEST_CLIENT_ADDR,
            sizeof(cliaddr.sun_path) - 2);
    addrlen = sizeof(sa_family_t) + strlen(GUEST_CLIENT_ADDR) + 1;

    if (bind(sock, (struct sockaddr *) &cliaddr, addrlen) == -1) {
        snprintf(errorbuf, ERRORSIZE-1, "unable to bind socket: %m");
        return -1;
    }

    /* guest agent address */
    memset(&svaddr, 0, sizeof(struct sockaddr_un));
    svaddr.sun_family = AF_UNIX;
    svaddr.sun_path[0] = '\0';
    strncpy(svaddr.sun_path+1, AGENT_ADDR, sizeof(svaddr.sun_path) - 2);
    svaddrlen = sizeof(sa_family_t) + strlen(AGENT_ADDR) + 1;
    
    /* set up callback pointers */
    sg_broadcast_msg_callback = broadcast_handler;
    sg_notification_msg_callback = notification_handler;
    sg_status_msg_callback = status_handler;
    
    return sock;
}

/* Provide access to the error message if the most recent call failed. */
char *sg_get_error()
{
    return errorbuf;
}

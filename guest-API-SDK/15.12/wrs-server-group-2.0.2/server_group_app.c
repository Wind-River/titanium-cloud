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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

#include "server_group.h"


void sg_broadcast_msg_handler(char *source_instance, void *msg,
						unsigned short msglen)
{
    printf("got server group broadcast msg: len: %d, sinstance: %.20s, msg: %.*s\n\n",
                msglen, source_instance,  msglen, (char *)msg);
}

void sg_notification_msg_handler(void *msg, unsigned short msglen)
{
    printf("got server group notification msg: %.*s\n\n",
                msglen, (char *)msg);
}

void sg_status_msg_handler(void *msg, unsigned short msglen)
{
    printf("got server group status response msg: %.*s\n\n",
                msglen, (char *)msg);
}

int main(int argc, char **argv)
{
    int nfds;
    int rc;
    fd_set rfds;
    // socket for guest client library
    int sock = init_sg(sg_broadcast_msg_handler,
                       sg_notification_msg_handler,
                       sg_status_msg_handler);
    if (sock < 0) {
        printf("error initializing library: %s\n", sg_get_error());
        exit(-1);
    }
    nfds=sock+1;
    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);
    
    if (argc == 2) {
        rc = sg_msg_broadcast(argv[1], strlen(argv[1])+1);
        if (rc != 0) {
            printf("problem sending broadcast: %s\n", sg_get_error());
            return -1;
        }
    }
    
    rc = sg_request_status();
    if (rc != 0) {
        printf("problem requesting status: %s\n", sg_get_error());
        return -1;
    }
    
    while(1) {
        int retval;
        fd_set tmpfds = rfds;
        retval = select(nfds, &tmpfds, NULL, NULL, NULL);
        
        if (retval > 0) {
            rc = process_sg_msg();
            if (rc < 0) {
                printf("problem processing incoming msg: %s\n", sg_get_error());
            }
        } else if (retval == -1)
            perror("select()");
    }
    return 0;
}



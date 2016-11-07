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
#include <syslog.h>
#include <limits.h>
#include <signal.h>
#include <execinfo.h>
#include <json-c/json.h>

#include "host_guest_msg_type.h"

/*
Notes on virtio-serial in guest:
1) The fact that POLLHUP is always set in revents when host connection is down
means that the only way to get event-driven notification of connection is to
register for SIGIO.  However, then we get a SIGIO every time the device becomes
readable. The solution is to selectively block SIGIO as long as we think the link
is up, then unblock it so we get notified when the link comes back.

2) If the host disconnects we will still process any buffered messages from it.

3) If read() returns 0 or write() returns -1 with errno of EAGAIN that means
the host is disconnected.
*/

// Tokener serves as reassembly buffer for host connection.
struct json_tokener* tok;

int host_fd;
int app_fd;

volatile sig_atomic_t check_host_connection=1;
volatile sig_atomic_t initial_connection=1;

// Currently we only use 2 fds, one for server group messaging and one for the
// connection to the host.
#define MAX_FDS_GUEST 10

// Message has arrived from the host.
// This assumes the message has been validated
void process_msg(json_object *jobj_msg, int fd)
{
    int rc;
    struct sockaddr_un cliaddr;
    int addrlen;

    // parse version
    struct json_object *jobj_version;
    if (!json_object_object_get_ex(jobj_msg, VERSION, &jobj_version)) {
        PRINT_ERR("failed to parse version\n");
        return;
    }
    int version  = json_object_get_int(jobj_version);
    if (version != CUR_VERSION) {
        PRINT_ERR("received version %d, expected %d\n", version, CUR_VERSION);
        return;
    }

    // parse source addr
    struct json_object *jobj_source_addr;
    if (!json_object_object_get_ex(jobj_msg, SOURCE_ADDR, &jobj_source_addr)) {
        PRINT_ERR("failed to parse source_addr\n");
        return;
    }

    // parse dest addr
    struct json_object *jobj_dest_addr;
    if (!json_object_object_get_ex(jobj_msg, DEST_ADDR, &jobj_dest_addr)) {
        PRINT_ERR("failed to parse dest_addr\n");
        return;
    }
    const char *dest_addr = json_object_get_string(jobj_dest_addr);


    // parse msg data
    struct json_object *jobj_data;
    if (!json_object_object_get_ex(jobj_msg, DATA, &jobj_data)) {
        PRINT_ERR("failed to parse data\n");
        return;
    }

    //create outgoing message
    struct json_object *jobj_outmsg = json_object_new_object();
    if (jobj_outmsg == NULL) {
        PRINT_ERR("failed to allocate json object for jobj_outmsg\n");
        return;
    }

    json_object_object_add(jobj_outmsg, DATA, jobj_data);
    json_object_object_add(jobj_outmsg, VERSION, json_object_new_int(CUR_VERSION));
    json_object_object_add(jobj_outmsg, SOURCE_ADDR, jobj_source_addr);

    const char *outmsg = json_object_to_json_string_ext(jobj_outmsg, JSON_C_TO_STRING_PLAIN);

    // Set up destination address
    memset(&cliaddr, 0, sizeof(struct sockaddr_un));
    cliaddr.sun_family = AF_UNIX;
    cliaddr.sun_path[0] = '\0';
    strncpy(cliaddr.sun_path+1, dest_addr, strlen(dest_addr));
    addrlen = sizeof(sa_family_t) + strlen(dest_addr) + 1;

    // Send the message to the client.
    // This will get transparently restarted if interrupted by signal.
    ssize_t outlen = strlen(outmsg);
    rc = sendto(app_fd, outmsg, outlen, 0, (struct sockaddr *) &cliaddr,
                addrlen);
    if (rc == -1) {
        PRINT_ERR("unable to send msg to %.*s: %m\n", UNIX_ADDR_LEN, cliaddr.sun_path+1);
    } else if (rc != outlen) {
        PRINT_ERR("sendto didn't send the whole message\n");
    }

    json_object_put(jobj_outmsg);
}

void handle_app_msg(const char *msg, struct sockaddr_un *cliaddr,
                    socklen_t cliaddrlen)
{
    int rc;
    char *app_addr;

    //parse incoming msg
    struct json_object *jobj_msg = json_tokener_parse(msg);
    if (jobj_msg == NULL) {
        PRINT_ERR("failed to parse msg\n");
        return;
    }

    // parse version
    struct json_object *jobj_version;
    if (!json_object_object_get_ex(jobj_msg, VERSION, &jobj_version)) {
        PRINT_ERR("failed to parse version\n");
        goto done;
    }
    int version = json_object_get_int(jobj_version);

    if (version != CUR_VERSION) {
        PRINT_ERR("message from app version %d, expected %d, dropping\n",
                version, CUR_VERSION);
        goto done;
    }

    // parse dest instance
    struct json_object *jobj_dest_addr;
    if (!json_object_object_get_ex(jobj_msg, DEST_ADDR, &jobj_dest_addr)) {
        PRINT_ERR("failed to parse dest_address\n");
        goto done;
    }

    // parse data
    struct json_object *jobj_data;
    if (!json_object_object_get_ex(jobj_msg, DATA, &jobj_data)) {
        PRINT_ERR("failed to parse data\n");
        goto done;
    }

    if (cliaddr->sun_path[0] == '\0') {
        app_addr = cliaddr->sun_path+1;
        // get length without family or leading null from abstract namespace
        cliaddrlen = cliaddrlen - sizeof(sa_family_t) - 1;
        app_addr[cliaddrlen] = '\0';
    } else {
        PRINT_INFO("client address not in abstract namespace, dropping\n");
        goto done;
    }

    struct json_object *jobj_outmsg = json_object_new_object();
    if (jobj_outmsg == NULL) {
        PRINT_ERR("failed to allocate json object for outmsg\n");
        goto done;
    }

    json_object_object_add(jobj_outmsg, DATA, jobj_data);
    json_object_object_add(jobj_outmsg, VERSION, jobj_version);
    json_object_object_add(jobj_outmsg, DEST_ADDR, jobj_dest_addr);
    json_object_object_add(jobj_outmsg, SOURCE_ADDR, json_object_new_string(app_addr));

    const char *outmsg = json_object_to_json_string_ext(jobj_outmsg, JSON_C_TO_STRING_PLAIN);

    // use '\n' to delimit JSON string: mark the beginning
    rc = write(host_fd, "\n", 1);
    if (rc == -1) {
        PRINT_ERR("unable to send \\n \n");
    }

    // send to host
    ssize_t outlen = strlen(outmsg);
    rc = write(host_fd, outmsg, outlen);
    if (rc == -1) {
        PRINT_ERR("unable to send msg from %.*s: %m\n", UNIX_ADDR_LEN, app_addr);
    } else if (rc != outlen) {
        PRINT_ERR("write didn't write the whole message to host\n");
    }

    // use '\n' to delimit JSON string: mark the ending
    rc = write(host_fd, "\n", 1);
    if (rc == -1) {
        PRINT_ERR("unable to send \\n \n");
    }

    json_object_put(jobj_outmsg);
done:
    json_object_put(jobj_msg);
}


void unmask_sigio(void)
{
    sigset_t mask;
    sigemptyset (&mask);
    sigaddset (&mask, SIGIO);
    sigprocmask(SIG_UNBLOCK, &mask, 0);
}

void mask_sigio(void)
{
    sigset_t mask;
    sigemptyset (&mask);
    sigaddset (&mask, SIGIO);
    sigprocmask(SIG_BLOCK, &mask, 0);
}

void handle_host_conn_down()
{
    check_host_connection = 0;
    unmask_sigio();
}

void scan_host_fd(struct pollfd *pfd)
{
    char buf[10000];
    ssize_t rc;

    if (pfd->revents & POLLIN) {
        // Read all messages from the host device
        while(1) {
            rc = read(pfd->fd, buf, sizeof(buf));
            if (rc == 0) {
                // Connection to host has gone down
                handle_host_conn_down();
                return;
            } else if (rc < 0) {
                if (errno == EAGAIN)
                    // We've read all the messages
                    return;
                else {
                    PRINT_ERR("read from host: %m");
                    return;
                }
            }
            handle_virtio_serial_msg(buf, rc, pfd->fd, tok);
        }
    }
}

void scan_app_fd(struct pollfd *pfd)
{
    char buf[10000];
    struct sockaddr_un cliaddr;
    ssize_t rc;
    
    // Read all messages from the app socket
    if (pfd->revents & POLLIN) {
        while(1) {
            socklen_t addrlen = sizeof(struct sockaddr_un);
            rc = recvfrom(pfd->fd, buf, sizeof(buf), 0,
                    (struct sockaddr *) &cliaddr, &addrlen);
            if (rc < 0) {
                if (errno == EAGAIN)
                    // We've read all the messages
                    return;
                else {
                    PRINT_ERR("recvfrom from app: %m");
                    return;
                }
            }
            handle_app_msg(buf, &cliaddr, addrlen);
        }
    }
}

//we get a SIGIO if the host connection connects/disconnects
static void sigio_handler(int signal)
{
    struct pollfd pfd;
    pfd.fd = host_fd;
    pfd.events = POLLIN;
    poll(&pfd, 1, 0);

    // if host is not connected, just exit
    if (pfd.revents & POLLHUP)
        return;

    // host is connected so check it in main loop
    check_host_connection = 1;
    initial_connection = 1;
}

//dump stack trace on segfault
static void segv_handler(int signum)
{
   int count;
   void *syms[100];
   int fd = open("/var/log/guest_agent_backtrace.log", O_RDWR|O_APPEND|O_CREAT, S_IRWXU);
   if (fd == -1) {
       PRINT_ERR("Unable to open guest agent backtrace file: %m");
       goto out;
   }

   write(fd, "\n", 1);
   count = backtrace(syms, 100);
   if (count == 0) {
       char *log = "Got zero items in backtrace.\n";
       write(fd, log, strlen(log));
       goto out;
   }
   
   backtrace_symbols_fd(syms, count, fd);
out:
   fflush(NULL);
   exit(-1);
}

int main(int argc, char **argv)
{
    int flags;
    int rc;
    struct sockaddr_un svaddr;
    int addrlen;

    PRINT_INFO("%s starting up\n", *argv);

    // optional arg for log level.  Higher number means more logs
    if (argc > 1) {
        char *endptr, *str;
        long val;
        str = argv[1];
        errno = 0;
        val = strtol(str, &endptr, 0);

        if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
                || (errno != 0 && val == 0)) {
            PRINT_ERR("error parsing log level arg: strtol: %m");
            exit(-1);
        }

        if (endptr == str) {
            PRINT_ERR("No digits were found\n");
            exit(EXIT_FAILURE);
        }
        
        if (val > LOG_DEBUG)
            val = LOG_DEBUG;
        
        setlogmask(LOG_UPTO(val));
    } else
    	setlogmask(LOG_UPTO(LOG_WARNING));
    
    signal(SIGIO, sigio_handler);
    signal(SIGSEGV, segv_handler);

    // set up fd for talking to host
    host_fd = open("/dev/virtio-ports/cgcs.messaging", O_RDWR|O_NONBLOCK);
    if (host_fd == -1) {
        PRINT_ERR("problem with open: %m");
        exit(-1);
    }
    
    flags = fcntl(host_fd, F_GETFL);
    rc = fcntl(host_fd, F_SETFL, flags | O_ASYNC);
    if (rc == -1) {
        PRINT_ERR("problem setting host_fd async: %m");
        exit(-1);
    }
	
    rc = fcntl(host_fd, F_SETOWN, getpid());
    if (rc == -1) {
        PRINT_ERR("problem owning host_fd: %m");
        exit(-1);
    }

    // set up socket for talking to apps
    app_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (app_fd == -1) {
        PRINT_ERR("problem with socket: %m");
        exit(-1);
    }
    
    flags = fcntl(app_fd, F_GETFL, 0);
    fcntl(app_fd, F_SETFL, flags | O_NONBLOCK);

    memset(&svaddr, 0, sizeof(struct sockaddr_un));
    svaddr.sun_family = AF_UNIX;
    svaddr.sun_path[0] = '\0';
    strncpy(svaddr.sun_path+1, AGENT_ADDR, sizeof(svaddr.sun_path) - 2);

    addrlen = sizeof(sa_family_t) + strlen(AGENT_ADDR) + 1;
    if (bind(app_fd, (struct sockaddr *) &svaddr, addrlen) == -1) {
        PRINT_ERR("problem with bind: %m");
        exit(-1);
    }

    tok = json_tokener_new();

    while(1) {
        struct pollfd pollfds[MAX_FDS_GUEST];
        int i;
        int nfds = 0;
     
        if (check_host_connection) {
            // we think the host connection is up

            if (initial_connection) {
                //mask SIGIO if we haven't already
                mask_sigio();
                initial_connection=0;
            }

	    pollfds[nfds].fd = host_fd;
	    pollfds[nfds].events = POLLIN;
            nfds++;
            
        }

        pollfds[nfds].fd = app_fd;
        pollfds[nfds].events = POLLIN;
        nfds++;

        if (nfds > 0) {
	    rc = poll(pollfds, nfds, -1);

	    if (rc == -1) {
                if (errno == EINTR)
                    continue;
                PRINT_ERR("problem with poll: %m");
                free(tok);
                exit(-1);
            }
           
            for(i=0;i<nfds;i++) {
                if (pollfds[i].fd == host_fd)
                    scan_host_fd(pollfds+i);
                if (pollfds[i].fd == app_fd)
                    scan_app_fd(pollfds+i);
            }
        } else
            // no connected fds, just wait for SIGIO
            pause();
    }

    free(tok);

    return 0;
}

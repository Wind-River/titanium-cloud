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
#include "guest_api_unix.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "guest_api_types.h"
#include "guest_api_debug.h"

// ****************************************************************************
// Guest API Unix - Connect
// ========================
GuestApiErrorT guest_api_unix_connect( int s, char* address )
{
    struct sockaddr_un remote;
    int len, result;

    memset(&remote, 0, sizeof(remote));

    remote.sun_family = AF_UNIX;
    len = sizeof(remote.sun_family);
    len += snprintf(remote.sun_path, sizeof(remote.sun_path), "%s", address);

    result = connect(s, (struct sockaddr*) &remote, sizeof(remote));
    if (0 > result)
    {
        if ((ENOENT == errno) || (ECONNREFUSED == errno))
        {
            DPRINTFD("Failed to connect to %s, error=%s.", address,
                     strerror(errno));
            return GUEST_API_TRY_AGAIN;
        } else {
            DPRINTFE("Failed to connect to %s, error=%s.", address,
                     strerror(errno));
            return GUEST_API_FAILED;
        }
    }

    return GUEST_API_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest API Unix - Send
// =====================
GuestApiErrorT guest_api_unix_send( int s, void* msg, int msg_size )
{
    int result;

    result = write(s, msg, msg_size);
    if (0 > result)
    {
        if (errno == EPIPE)
        {
            DPRINTFI("Failed to write to socket, error=%s.", strerror(errno));
            return GUEST_API_TRY_AGAIN;
        } else {
            DPRINTFE("Failed to write to socket, error=%s.", strerror(errno));
            return GUEST_API_FAILED;
        }
    }
    return GUEST_API_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest API Unix - Receive
// ========================
GuestApiErrorT guest_api_unix_receive(
        int s, void* msg_buf, int msg_buf_size, int* msg_size )
{
    int result;

    result = read(s, msg_buf, msg_buf_size);
    if (0 > result)
    {
        if (EINTR == errno)
        {
            DPRINTFD("Interrupted on socket read, error=%s.", strerror(errno));
            return GUEST_API_INTERRUPTED;
        } else if (ECONNRESET == errno) {
            DPRINTFD("Peer connection reset, error=%s.", strerror(errno));
            *msg_size = 0;
            return GUEST_API_OKAY;
        } else{
            DPRINTFE("Failed to read from socket, error=%s.", strerror(errno));
            return GUEST_API_FAILED;
        }
    } else if (0 == result) {
        DPRINTFD("No message received from socket.");
        *msg_size = 0;
        return GUEST_API_OKAY;
    } else {
        DPRINTFV("Received message, msg_size=%i.", result);
        *msg_size = result;
    }

    return GUEST_API_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest API Unix - Open
// =====================
GuestApiErrorT guest_api_unix_open( int* s )
{
    int sock;
    int reuse_addr = 1;
    struct sockaddr_un local;
    int result;

    *s = -1;
    memset(&local, 0, sizeof(local));

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (0 > sock)
    {
        DPRINTFE("Failed to open socket, error=%s.", strerror(errno));
        return GUEST_API_FAILED;
    }

    result = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr,
                        sizeof(reuse_addr));
    if (0 > result)
    {
        DPRINTFE("Failed to set socket option (REUSEADDR), error=%s.",
                 strerror(errno));
        close(sock);
        return GUEST_API_FAILED;
    }

    result = fcntl(sock, F_SETFD, FD_CLOEXEC);
    if (0 > result)
    {
        DPRINTFE("Failed to set to close on exec, error=%s.", strerror(errno));
        close(sock);
        return GUEST_API_FAILED;
    }

    result = fcntl(sock, F_GETFL);
    if (0 > result)
    {
        DPRINTFE("Failed to get socket options, error=%s.", strerror(errno));
        close(sock);
        return GUEST_API_FAILED;
    }

    result = fcntl(sock, F_SETFL, result | O_NONBLOCK);
    if (0 > result)
    {
        DPRINTFE("Failed to set socket options, error=%s.", strerror(errno));
        close(sock);
        return GUEST_API_FAILED;
    }

    *s = sock;

    return GUEST_API_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest API Unix - Close
// ======================
GuestApiErrorT guest_api_unix_close( int s )
{
    if (0 <= s)
        close(s);

    return GUEST_API_OKAY;
}
// ****************************************************************************

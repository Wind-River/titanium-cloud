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
#include "guest_channel.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "guest_limits.h"
#include "guest_types.h"
#include "guest_debug.h"
#include "guest_unix.h"

typedef struct {
    bool inuse;
    bool char_device;
    int sock;
    char dev_name[GUEST_DEVICE_NAME_MAX_CHAR];
} GuestChannelT;

static GuestChannelT _channel[GUEST_MAX_CONNECTIONS];

// ****************************************************************************
// Guest Channel - Find Empty
// ==========================
static GuestChannelIdT guest_channel_find_empty( void )
{
    GuestChannelT* channel = NULL;

    unsigned int channel_id;
    for (channel_id=0; GUEST_MAX_CONNECTIONS > channel_id; ++channel_id)
    {
        channel = &(_channel[channel_id]);
        if (!channel->inuse)
            return channel_id;
    }

    return GUEST_CHANNEL_ID_INVALID;
}
// ****************************************************************************

// ****************************************************************************
// Guest Channel - Find
// ====================
static GuestChannelT* guest_channel_find( GuestChannelIdT channel_id )
{
    GuestChannelT* channel = NULL;

    if ((0 <= channel_id)&&(GUEST_MAX_CONNECTIONS > channel_id))
    {
        channel = &(_channel[channel_id]);
        if (channel->inuse)
            return channel;
    }

    return NULL;
}
// ****************************************************************************

// ****************************************************************************
// Guest Channel - Send
// ====================
GuestErrorT guest_channel_send(
        GuestChannelIdT channel_id, void* msg, int msg_size )
{
    GuestChannelT* channel;
    ssize_t result;

    channel = guest_channel_find(channel_id);
    if (NULL == channel)
    {
        DPRINTFE("Invalid channel identifier, channel_id=%i.", channel_id);
        return GUEST_FAILED;
    }

    result = write(channel->sock, msg, msg_size);
    if (0 > result)
    {
        if (ENODEV == errno)
        {
            DPRINTFI("Channel %i on device %s disconnected.", channel_id,
                     channel->dev_name);
            return GUEST_OKAY;
        } else {
            DPRINTFE("Failed to write to channel on device %s, error=%s.",
                     channel->dev_name, strerror(errno));
            return GUEST_FAILED;
        }
    }

    DPRINTFV("Sent message over channel on device %s.", channel->dev_name);
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Channel - Receive
// =======================
GuestErrorT guest_channel_receive(
        GuestChannelIdT channel_id, char* msg_buf, int msg_buf_size,
        int* msg_size )
{
    GuestChannelT* channel;
    ssize_t result;

    channel = guest_channel_find(channel_id);
    if (NULL == channel)
    {
        DPRINTFE("Invalid channel identifier, channel_id=%i.", channel_id);
        return GUEST_FAILED;
    }

    result = read(channel->sock, msg_buf, msg_buf_size);
    if (0 > result)
    {
        if (EINTR == errno)
        {
            DPRINTFD("Interrupted on socket read, error=%s.", strerror(errno));
            return GUEST_INTERRUPTED;

        } else if (ENODEV == errno) {
            DPRINTFI("Channel %i on device %s disconnected.", channel_id,
                     channel->dev_name);
            *msg_size = 0;
            return GUEST_OKAY;

        } else {
            DPRINTFE("Failed to read from socket, error=%s.", strerror(errno));
            return GUEST_FAILED;
        }
    } else if (0 == result) {
        DPRINTFD("No message received from socket.");
        *msg_size = 0;
        return GUEST_OKAY;

    } else {
        DPRINTFV("Received message, msg_size=%i.", result);
        *msg_size = result;
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Channel - Open
// ====================
GuestErrorT guest_channel_open( char dev_name[], GuestChannelIdT* channel_id )
{
    int fd;
    int result;
    struct stat stat_data;
    GuestChannelIdT empty_channel_id;
    GuestChannelT* channel;
    GuestErrorT error;

    empty_channel_id = guest_channel_find_empty();
    if (GUEST_CHANNEL_ID_INVALID == empty_channel_id)
    {
        DPRINTFE("Allocation of channel failed, no free resources.");
        return GUEST_FAILED;
    }

    channel = &(_channel[empty_channel_id]);
    memset(channel, 0, sizeof(GuestChannelT));

    result = stat(dev_name, &stat_data);
    if (0 > result)
    {
        int err = errno;
        if (err == ENOENT) 
        {
            DPRINTFI("Failed to stat, error=%s.", strerror(err));
            DPRINTFI("%s file does not exist, guest heartbeat not configured.",
                     dev_name);
            return GUEST_NOT_CONFIGURED;
        }
        else {
            DPRINTFE("Failed to stat, error=%s.", strerror(err));
            return GUEST_FAILED;
        }
    }

    if (S_ISCHR(stat_data.st_mode))
    {
        fd = open(dev_name, O_RDWR);
        if (0 > fd)
        {
            DPRINTFE("Failed to open device %s, error=%s.", dev_name,
                     strerror(errno));
            return GUEST_FAILED;
        }

        result = fcntl(fd, F_SETFD, FD_CLOEXEC);
        if (0 > result)
        {
            DPRINTFE("Failed to set to close on exec, error=%s.",
                     strerror(errno));
            close(fd);
            return GUEST_FAILED;
        }

        result = fcntl(fd, F_SETOWN, getpid());
        if (0 > result)
        {
            DPRINTFE("Failed to set socket ownership, error=%s.",
                     strerror(errno));
            close(fd);
            return GUEST_FAILED;
        }

        result = fcntl(fd, F_GETFL);
        if (0 > result)
        {
            DPRINTFE("Failed to get socket options, error=%s.",
                     strerror(errno));
            close(fd);
            return GUEST_FAILED;
        }

        result = fcntl(fd, F_SETFL, result | O_NONBLOCK | O_ASYNC);
        if (0 > result)
        {
            DPRINTFE("Failed to set socket options, error=%s.",
                     strerror(errno));
            close(fd);
            return GUEST_FAILED;
        }

        DPRINTFI("Opened character device %s", dev_name);

    } else {
        error = guest_unix_open(&fd);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to open unix socket %s, error=%s.",
                     dev_name, guest_error_str(error));
            return error;
        }
        error = guest_unix_connect(fd, dev_name);
        if (GUEST_OKAY != error)
        {
            DPRINTFE("Failed to connect unix socket %s, error=%s.",
                     dev_name, guest_error_str(error));
            close(fd);
            return error;
        }

        DPRINTFI("Opened unix socket %s", dev_name);
    }

    channel->inuse = true;
    snprintf(channel->dev_name, sizeof(channel->dev_name), "%s", dev_name);
    channel->char_device = S_ISCHR(stat_data.st_mode);
    channel->sock = fd;
    *channel_id = empty_channel_id;

    DPRINTFD("Opened channel over device %s.", channel->dev_name);
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Channel - Close
// =====================
GuestErrorT guest_channel_close( GuestChannelIdT channel_id )
{
    GuestChannelT* channel;

    channel = guest_channel_find(channel_id);
    if (NULL != channel)
    {
        if (channel->inuse)
        {
            if (0 <= channel->sock)
                close(channel->sock);

            DPRINTFD("Closed channel over device %s.", channel->dev_name);
            memset(channel, 0, sizeof(GuestChannelT));
        }
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Channel - Get Selection Object
// ====================================
int guest_channel_get_selobj( GuestChannelIdT channel_id )
{
    GuestChannelT *channel;

    channel = guest_channel_find(channel_id);
    if (NULL != channel)
        return channel->sock;

    return -1;
}
// ****************************************************************************

// ****************************************************************************
// Guest Channel - Initialize
// ==========================
GuestErrorT guest_channel_initialize( void )
{
    memset(_channel, 0, sizeof(_channel));
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Channel - Finalize
// ========================
GuestErrorT guest_channel_finalize( void )
{
    memset(_channel, 0, sizeof(_channel));
    return GUEST_OKAY;
}
// ****************************************************************************

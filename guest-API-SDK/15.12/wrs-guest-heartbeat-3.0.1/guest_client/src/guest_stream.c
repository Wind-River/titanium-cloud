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
#include "guest_stream.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "guest_types.h"
#include "guest_debug.h"

// ****************************************************************************
// Guest Stream - Get
// ==================
int guest_stream_get( GuestStreamT* stream )
{
    char* byte_ptr;
    int delimiter_i = 0;

    if (stream->delimiter_size > stream->size)
        return -1;

    for (byte_ptr = stream->bytes; byte_ptr != stream->end_ptr; ++byte_ptr)
    {
        if (stream->delimiter[delimiter_i] == *byte_ptr)
        {
            ++delimiter_i;
            if (delimiter_i == stream->delimiter_size)
            {
                return (byte_ptr - stream->bytes);
            }
        } else {
            delimiter_i = 0;
        }
    }

    return -1;
}
// ****************************************************************************

// ****************************************************************************
// Guest Stream - Get Next
// =======================
bool guest_stream_get_next( GuestStreamT* stream )
{
    char* byte_ptr;
    int delimiter_i = 0;

    if (stream->delimiter_size > stream->size)
        return false;

    for (byte_ptr = stream->bytes; byte_ptr != stream->end_ptr; ++byte_ptr)
    {
        --stream->size;
        if (stream->delimiter[delimiter_i] == *byte_ptr)
        {
            ++delimiter_i;
            if (delimiter_i == stream->delimiter_size)
            {
                byte_ptr -= (stream->delimiter_size-1);
                stream->size += stream->delimiter_size;
                memmove(stream->bytes, byte_ptr, stream->size);
                stream->avail = stream->max_size - stream->size;
                stream->end_ptr = stream->bytes + stream->size;
                break;
            }
        } else {
            delimiter_i = 0;
        }
    }

    if (byte_ptr == stream->end_ptr)
    {
        // Empty the stream
        memset(stream->bytes, 0, stream->max_size);
        stream->avail = stream->max_size;
        stream->size = 0;
        stream->end_ptr = stream->bytes;
        return false;
    }

    return true;
}
// ****************************************************************************

// ****************************************************************************
// Guest Stream - Advance
// ======================
void guest_stream_advance( int adv, GuestStreamT* stream )
{
    stream->size -= adv;
    memmove(stream->bytes, stream->bytes+adv, stream->size);
    stream->avail = stream->max_size - stream->size;
    stream->end_ptr = stream->bytes + stream->size;
}
// ****************************************************************************

// ****************************************************************************
// Guest Stream - Reset
// ====================
void guest_stream_reset( GuestStreamT* stream )
{
    memset(stream->bytes, 0, stream->max_size);
    stream->avail = stream->max_size;
    stream->size = 0;
    stream->end_ptr = stream->bytes;
}
// ****************************************************************************

// ****************************************************************************
// Guest Stream - Setup
// ====================
GuestErrorT guest_stream_setup(
        const char* delimiter, int delimiter_size, int stream_size,
        GuestStreamT* stream )
{
    stream->delimiter = malloc(delimiter_size);
    if (NULL == stream->delimiter)
    {
        DPRINTFE("Failed to allocated delimiter storage, needed=%i.",
                 delimiter_size);
        return GUEST_FAILED;
    }

    stream->bytes = malloc(stream_size);
    if (NULL == stream->delimiter)
    {
        DPRINTFE("Failed to allocated stream storage, needed=%i.", stream_size);
        free(stream->delimiter);
        return GUEST_FAILED;
    }

    memcpy(stream->delimiter, delimiter, delimiter_size);
    stream->delimiter_size = delimiter_size;
    memset(stream->bytes, 0, stream_size);
    stream->end_ptr = stream->bytes;
    stream->avail = stream_size;
    stream->size = 0;
    stream->max_size = stream_size;

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Stream - Release
// ======================
GuestErrorT guest_stream_release( GuestStreamT* stream )
{
    if (NULL != stream->delimiter)
        free(stream->delimiter);

    if (NULL != stream->bytes)
        free(stream->bytes);

    memset(stream, 0, sizeof(GuestStreamT));
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Stream - Initialize
// =========================
GuestErrorT guest_stream_initialize( void )
{
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Stream - Finalize
// =======================
GuestErrorT guest_stream_finalize( void )
{
    return GUEST_OKAY;
}
// ****************************************************************************

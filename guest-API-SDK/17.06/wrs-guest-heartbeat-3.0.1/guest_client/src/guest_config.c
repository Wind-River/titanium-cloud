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
#include "guest_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "guest_types.h"
#include "guest_debug.h"

#define GUEST_DEFAULT_COMM_DEVICE       "/dev/vport1p1"

static GuestConfigT _config;

// ****************************************************************************
// Guest Configuration - Get
// =========================
GuestConfigT* guest_config_get( void )
{
    return &_config;
}
// ****************************************************************************

// ****************************************************************************
// Guest Configuration - Show Usage
// ================================
void guest_config_show_usage( void )
{
    printf("guest-client [ARGS]\n");
    printf("  where ARGS may be any of: \n");
    printf("    --name      Override the name of the instance\n");
    printf("    --device    Override default communication channel device\n");
    printf("\n");
}
// ****************************************************************************

// ****************************************************************************
// Guest Configuration - Dump
// ==========================
static void guest_config_dump( void )
{
    DPRINTFI("Guest-Client Configuration:");
    DPRINTFI("  name:   %s", _config.name);
    DPRINTFI("  device: %s", _config.comm_device);
}
// ****************************************************************************

// ****************************************************************************
// Guest Configuration - Parse Arguments
// =====================================
static GuestErrorT guest_config_parse_args( int argc, char *argv[] )
{
    unsigned int arg_i;
    for (arg_i=1; arg_i < argc; ++arg_i)
    {
        if (0 == strcmp("--name", argv[arg_i]))
        {
            arg_i++;
            if (arg_i < argc)
                snprintf(_config.name, sizeof(_config.name), "%s", argv[arg_i]);

        } else if (0 == strcmp("--device", argv[arg_i])) {
            arg_i++;
            if (arg_i < argc)
                snprintf(_config.comm_device, sizeof(_config.comm_device),
                         "%s", argv[arg_i]);
        }
    }

    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Configuration - Parse Environment
// =======================================
static GuestErrorT guest_config_parse_env( char *envp[] )
{
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Configuration - Initialize
// ================================
GuestErrorT guest_config_initialize( int argc, char *argv[], char *envp[] )
{
    char name[GUEST_NAME_MAX_CHAR];
    GuestErrorT error;
    int result;

    result = gethostname(name, sizeof(name));
    if (0 > result)
    {
        DPRINTFE("Failed to get hostname, error=%s.", strerror(errno));
        return GUEST_FAILED;
    }
    DPRINTFI("hostname=%s.", name);
    memset(&_config, 0, sizeof(GuestConfigT));
    snprintf(_config.name, sizeof(_config.name), "%s", name);
    snprintf(_config.comm_device, sizeof(_config.comm_device), "%s",
             GUEST_DEFAULT_COMM_DEVICE);

    error = guest_config_parse_args(argc, argv);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to parse arguments, error=%s.",
                 guest_error_str(error));
        return error;
    }

    error = guest_config_parse_env(envp);
    if (GUEST_OKAY != error)
    {
        DPRINTFE("Failed to parse environment, error=%s.",
                 guest_error_str(error));
        return error;
    }

    guest_config_dump();
    return GUEST_OKAY;
}
// ****************************************************************************

// ****************************************************************************
// Guest Configuration - Finalize
// ==============================
GuestErrorT guest_config_finalize( void )
{
    return GUEST_OKAY;
}
// ****************************************************************************

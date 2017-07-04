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
#include "guest_utils.h"
#include "guest_debug.h"

#include <ctype.h>
#include <string.h>

// ****************************************************************************
// Guest Utilities - Trim
// ======================
char* guest_utils_trim( char* str, char* discard )
{
    int len;
    int max_len;

    if (NULL == str)
        return NULL;

    // Remove leading characters
    max_len = strlen(str);
    for (len=0; max_len > len; ++len)
    {
        if (isspace(*str) || '\n' == *str)
            ++str;
        else if ((NULL != discard) && (NULL != strchr(discard, *str)))
            ++str;
    }

    // Remove trailing characters
    for (len=strlen(str)-1; 0 <= len; --len)
    {
        if (isspace(str[len]) || '\n' == str[len])
            str[len] = '\0';
        else if ((NULL != discard) && (NULL != strchr(discard, str[len])))
            str[len] = '\0';
    }

    return str;
}
// ****************************************************************************

// ****************************************************************************
// Guest Utilities - Base Name
// ===========================
char* guest_utils_basename( char* str )
{
    const char* basename = str;

    while ('\0' != *str)
    {
        if (*str++ == '/')
            basename = str;
    }

    return (char*) basename;
}
// ****************************************************************************

// ****************************************************************************
// Guest Utilities - Get JSON Value from Key
// return 0 if success, -1 if fail.
// =========================================
int guest_utils_json_get_value( struct json_object* jobj,
                                const char* key, void * value )
{
    struct json_object *jobj_value;
    if (!json_object_object_get_ex(jobj, key, &jobj_value))
    {
        DPRINTFE("failed to parse %s\n", key);
        return -1;
    }
    enum json_type type = json_object_get_type(jobj_value);
    switch(type)
    {
        case json_type_boolean:
            *(unsigned int *)value = json_object_get_boolean(jobj_value);
            break;
        case json_type_int:
            *(unsigned int *)value = json_object_get_int(jobj_value);
            break;
        case json_type_double:
            *(double *)value = json_object_get_double(jobj_value);
            break;
        case json_type_string:
            strcpy(value, json_object_get_string(jobj_value));
            break;
        default:
            DPRINTFE("failed to parse %s, type %d is not supported\n", key, type);
            return -1;
            break;
    }
    return 0;
}
// ****************************************************************************

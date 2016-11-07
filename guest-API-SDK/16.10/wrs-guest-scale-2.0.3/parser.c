/**
* Copyright (c) <2013-2016>, Wind River Systems, Inc.
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
*  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
* USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <json-c/json.h>
#include "misc.h"


/* Print a range of numbers in a consistent way. */
char print_range(int start, int end, char *str)
{
    int len;
    if (start == end)
        len = sprintf(str, "%d,", start);
    else
        len = sprintf(str, "%d-%d,", start, end);
    return len;
}

void print_array(char *buf, int *array, int len) {
    int i;
    for(i = 0; i < len; i++) {
       sprintf(buf+strlen(buf),"%d,", array[i]);
    }
    // remove the last comma
    buf[strlen(buf)-1]='\0';
}

#define BUFLEN 1024

/* Takes as input a string representation of online cpus of the form
 * "0,1,3-5", and allocates and returns a struct representing whether
 * each given cpu is online.
 */
struct online_cpus *range_to_array(const char *range)
{
    struct online_cpus *cpuarray = (struct online_cpus *) malloc(BUFLEN);
    int start, end;
    int inrange = 0;
    char *token, *tmp;
    int done = 0;
    tmp = strdup(range);
    strcpy(tmp, range);
    token = tmp;

    if (*tmp == '\0') {
        /* empty string, no online cpus */
        cpuarray->numcpus = 0;
        return cpuarray;
    }
        
    while (1) {
        tmp++;
        if (*tmp == '\0')
            done = 1;
        if (done || (*tmp == ',')) {
            /* expect single value or ending a range */
            if (!token) {
                ERR_LOG("format error, missing token, unable to parse range\n");
                goto error;
            }
            *tmp = '\0';
            end = atoi(token);
            token = 0;
            if (inrange) {
                int i;
                for (i=start; i<= end; i++)
                    cpuarray->status[i] = 1;
                inrange = 0;
            } else {
                cpuarray->status[end] = 1;
            }
        } else if (*tmp == '-') {
            if (inrange) {
                ERR_LOG("format error, unable to parse range\n");
                goto error;
            }
            if (!token) {
                ERR_LOG("format error, missing token, unable to parse range\n");
                goto error;
            }
            *tmp = '\0';
            start = atoi(token);
            token = 0;
            inrange = 1;
        } else {
            /* expect a numerical value */
            if ((*tmp < '0') || (*tmp > '9')) {
                ERR_LOG("format error, expected a numerical value, unable to parse range\n");
                goto error;
            }
                
            if (!token)
                token = tmp;
        }
        if (done)
            break;
    }
    cpuarray->numcpus = end+1;
    return cpuarray;
error:
    free(cpuarray);
    return 0;
}


struct json_object *new_json_obj_from_array (struct online_cpus *cpuarray)
{
    int i;

    struct json_object *jobj_array = json_object_new_array();
    if (jobj_array == NULL) {
        return NULL;
    }

    for (i=0;i<cpuarray->numcpus;i++) {
        if (cpuarray->status[i]) {
            json_object_array_add(jobj_array, json_object_new_int(i));
        }
    }
    return jobj_array;
}

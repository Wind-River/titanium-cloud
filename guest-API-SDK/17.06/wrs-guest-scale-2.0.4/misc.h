/**
* Copyright (c) <2015-2016>, Wind River Systems, Inc.
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

#include <syslog.h>

#define CUR_VERSION 2

#define LOG_MSG_SIZE 100
#define SCALE_AGENT_ADDR "cgcs.scale"
#define ERRORSIZE 400

// keys for guest scaling messages
#define VERSION     "version"
#define TIMEOUT_MS  "timeout_ms"
#define RESOURCE    "resource"
#define DIRECTION   "direction"
#define ONLINE_CPU  "online_cpu"
#define OFFLINE_CPU "offline_cpu"
#define ONLINE_CPUS "online_cpus"
#define RESULT      "result"
#define ERR_MSG     "err_msg"
#define MSG_TYPE    "msg_type"
#define LOG_MSG     "log_msg" // for Nack

// message types for scaling messages
#define MSG_TYPE_SCALE_REQUEST "scale_request"
#define MSG_TYPE_NACK          "nack"

char errorbuf[ERRORSIZE];

#define LOG(priority, format, ...) \
    syslog(priority, "%s(%d): " format, __FILE__, __LINE__, ##__VA_ARGS__)

#define ERR_LOG(format, ...) \
    do { \
        LOG(LOG_DAEMON|LOG_ERR, format, ##__VA_ARGS__); \
        fprintf(stderr, format, ##__VA_ARGS__); \
        snprintf(errorbuf, sizeof(errorbuf)-1, format, ##__VA_ARGS__); \
    } while (0)

#define INFO_LOG(format, ...) \
    do { \
        LOG(LOG_DAEMON|LOG_INFO, format, ##__VA_ARGS__); \
    } while (0)


struct online_cpus {
    int numcpus;
    char status[];
};

struct online_cpus *range_to_array(const char *range);
struct json_object *new_json_obj_from_array (struct online_cpus *cpuarray);
void print_array(char *buf, int *array, int len);


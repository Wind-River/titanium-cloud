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
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <cgcs/guest_host_msg.h>
#include <json-c/json.h>

#include "misc.h"

gh_info_t *info;

/* Scaling Request/Response message is encoded in JSON format.
   The message sent out to UNIX socket is a null-terminated JSON format string
   without embedded newlines.

   Format:
   {key:value,key:value,..., key:value}

   Key/value pairs for Scaling Request:
   "version": <integer>     - version of the interface
   "timeout_ms": <integer>  - timeout for app_scale_helper scripts
   "resource": “cpu”        - indicate the resouce to scale.
                              Only cpu is currently supported.
   "direction“: "up” or “down”
   "online_cpu": <integer>  - vcpu number to online when scale up
   "online_cpus": <array of integers> -  array of current online cpus
                                         when request was sent.
                                         example: [0,1,2,3,4,5]

   Key/value pairs for Scaling Response:
   "version": <integer>
   "resource": “cpu”
   "direction“: "up” or “down”
   "online_cpu": <integer>   - vcpu number to online when scale up
   "offline_cpu": <integer>  - actual offlined vcpu number
   "online_cpus": <array of integers> -  array of current online cpus
                                         when response was sent.
   "result": "success" or "fail"
   "err_msg": <string>       - error message if result is fail

*/

#define CPU_SCRIPT "/usr/sbin/app_scale_helper"

// generic function to call out to helper script
// need to add support for timeout in here in case script hangs
int call_helper_script(char *cmd, int timeout_ms)
{
    FILE *fp;
    int rc;

    fp = popen(cmd, "w");
    if (fp) {
        rc = pclose(fp);
        if (rc == -1) {
            ERR_LOG("pclose failed: %m");
            return -1;
        } else {
            if (WIFEXITED(rc)) {
                rc = WEXITSTATUS(rc);
                if (rc == 127) {
                    ERR_LOG("problem with shell or helper script, possibly script missing");
                    return -1;
                } else
                    return rc;
            } else {
                return -1;
            }
        }
    } else {
        ERR_LOG("popen failed due to fork/pipe/memory");
        return -1;
    }
}


int online_cpu(unsigned cpu)
{
    int fd;
    int rc;
    char buf[100];
    char val;
    snprintf(buf, sizeof(buf), "/sys/devices/system/cpu/cpu%u/online", cpu);
    fd = open(buf, O_RDWR);
    if (fd < 0) {
        ERR_LOG("can't open cpu online path: %m");
        return -1;
    }
    rc = read(fd, &val, 1);
    if (rc != 1){
        ERR_LOG("can't read cpu online value: %m");
        return -1;
    }
    if (val == '1') {
        ERR_LOG("cpu %d is already online", cpu);
        return 0;
    }
    val = '1';
    rc = write(fd, &val, 1);
    if (rc != 1){
        ERR_LOG("can't set cpu %d online", cpu);
        return -1;
    }
    return 0;
}

int offline_cpu(unsigned cpu)
{
    int fd;
    int rc;
    char buf[100];
    char val;
    snprintf(buf, sizeof(buf), "/sys/devices/system/cpu/cpu%u/online", cpu);
    fd = open(buf, O_RDWR);
    if (fd < 0) {
        ERR_LOG("can't open cpu online path: %m");
        return -1;
    }
    rc = read(fd, &val, 1);
    if (rc != 1){
        ERR_LOG("can't read cpu online value: %m");
        return -1;
    }
    if (val == '0') {
        ERR_LOG("cpu %d is already offline\n", cpu);
        return 0;
    }
    val = '0';
    rc = write(fd, &val, 1);
    if (rc != 1){
        ERR_LOG("can't set cpu %d offline", cpu);
        return -1;
    }
    return 0;
}

// read /sys/devices/system/cpu/online and get the last cpu listed
int get_highest_online_cpu(void)
{
    int fd, rc;
    char buf[256];
    char *start;
    unsigned int cpu;
    fd = open("/sys/devices/system/cpu/online", O_RDONLY);
    if (fd < 0) {
        ERR_LOG("can't fopen /sys/devices/system/cpu/online: %m");
        return -1;
    }

    rc = read(fd, buf, sizeof(buf));
    if (rc < 2) {
        ERR_LOG("error parsing /sys/devices/system/cpu/online, too few chars");
        return -1;
    }

    // go to the end of the string
    start = buf+rc-1;
    if(*start != '\n') {
        ERR_LOG("error parsing /sys/devices/system/cpu/online, not null-terminated");
        return -1;
    }

    // now go backwards until we get to a separator or the beginning of the string
    while ((*start != ',') && (*start != '-') && (start != buf))
        start--;

    start++;
    rc = sscanf(start, "%u", &cpu);
    if (rc != 1) {
        ERR_LOG("error parsing /sys/devices/system/cpu/online, bad number");
        return -1;
    }

    return cpu;
}


char *get_online_cpu_range(void)
{
    FILE *file;
    int rc;
    char *str = NULL;
    file = fopen("/sys/devices/system/cpu/online", "r");
    if (!file) {
        ERR_LOG("can't fopen /sys/devices/system/cpu/online: %m");
        return 0;
    }
    rc = fscanf(file, "%ms", &str);
    if (rc != 1)
        ERR_LOG("can't read /sys/devices/system/cpu/online: %m");
    fclose(file);
    return str;
}


void cpu_scale_down(json_object *jobj_request,
                    json_object *jobj_response)
{
    char cmd[1000];
    int cpu=-1;
    int rc;

    //build our command to send to the helper script
    rc = snprintf(cmd, sizeof(cmd), "%s --cpu_del\n", CPU_SCRIPT);
    if ((rc > sizeof(cmd)) || rc < 0) {
        ERR_LOG("error generating command: %m");
        goto pick_cpu;
    }

    struct json_object *jobj_timeout_ms;
    int timeout_ms;
    if (!json_object_object_get_ex(jobj_request, TIMEOUT_MS, &jobj_timeout_ms))
    {
        ERR_LOG("failed to parse timeout_ms");
        goto failed;
    }

    errno = 0;
    timeout_ms = json_object_get_int(jobj_timeout_ms);
    if(errno){
        ERR_LOG("Error converting timeout_ms: %s", strerror(errno));
        goto failed;
    }

    // call app helper script to select cpu to offline
    rc = call_helper_script(cmd, timeout_ms);
    if (rc < 0) {
        ERR_LOG("call to app helper script failed\n");
        goto pick_cpu;
    } else if (rc == 0) {
        ERR_LOG("call to app helper script return invalid cpu number 0\n");
        goto pick_cpu;
    } else {
        INFO_LOG("app helper script chose cpu %d to offline\n", rc);
        cpu = rc;
    }

pick_cpu:
    // if the app helper script doesn't exist or didn't return
    // a cpu to offline, pick one ourselves
    if (cpu == -1) {
        cpu = get_highest_online_cpu();
        if (cpu <= 0) {
            ERR_LOG("unable to find cpu to offline\n");
            goto failed;
        }
    }
    
    // try to offline selected cpu
    rc = offline_cpu(cpu);
    if (rc < 0) {
        ERR_LOG("failed to set cpu %d offline\n", cpu);
        goto failed;
    }

    INFO_LOG("set cpu %d offline", cpu);

    // we have successfully offlined the cpu
    json_object_object_add(jobj_response, RESULT, json_object_new_string("success"));
    json_object_object_add(jobj_response, OFFLINE_CPU, json_object_new_int(cpu));
    struct online_cpus *current_online_cpus = range_to_array(get_online_cpu_range());

    // no need to release jobj_array as its ownership is transferred to jobj_response
    struct json_object *jobj_array = new_json_obj_from_array(current_online_cpus);
    json_object_object_add(jobj_response, ONLINE_CPUS, jobj_array);
    return;

failed:
    json_object_object_add(jobj_response, RESULT, json_object_new_string("fail"));
    json_object_object_add(jobj_response, ERR_MSG, json_object_new_string(errorbuf));
    return;
}


void cpu_scale_up(json_object *jobj_request,
                  json_object *jobj_response)
{
    char cmd[1000];
    struct json_object *jobj_timeout_ms;
    if (!json_object_object_get_ex(jobj_request, TIMEOUT_MS, &jobj_timeout_ms)) {
        ERR_LOG("failed to parse timeout_ms");
        goto failed;
    }
    int timeout_ms = json_object_get_int(jobj_timeout_ms);

    struct json_object *jobj_cpu;
    if (!json_object_object_get_ex(jobj_request, ONLINE_CPU, &jobj_cpu)) {
        ERR_LOG("failed to parse online_cpu");
        goto failed;
    }
    int cpu = json_object_get_int(jobj_cpu);

    //online_cpus is optional
    struct json_object *jobj_online_cpus;
    const char *online_cpus;
    if (!json_object_object_get_ex(jobj_request, ONLINE_CPUS, &jobj_online_cpus)) {
        ERR_LOG("failed to parse online_cpus");
        goto failed;
    }

    json_object_object_get_ex(jobj_request, ONLINE_CPUS, &jobj_online_cpus);
    if (!json_object_is_type(jobj_online_cpus, json_type_array)) {
        ERR_LOG("failed to parse online_cpus");
        goto failed;
    }
    online_cpus = json_object_to_json_string_ext(jobj_online_cpus, JSON_C_TO_STRING_PLAIN);

    int rc = online_cpu(cpu);
    if (rc < 0) {
        printf("failed to set cpu %d online\n", cpu);
        goto failed;
    }
    
    INFO_LOG("set cpu %d online", cpu);
    
    // Now try to call out to the helper script
    // If it fails, not the end of the world.

    rc = snprintf(cmd, sizeof(cmd), "%s --cpu_add %d %s\n",
                  CPU_SCRIPT, cpu, online_cpus);

    if ((rc > 0) && (rc < sizeof(cmd))) {
        rc = call_helper_script(cmd, timeout_ms);
        if (rc != 0)
            ERR_LOG("call to app helper script failed, return code: %d\n", rc);
    } else
        ERR_LOG("error generating command: %m");

    json_object_object_add(jobj_response, RESULT, json_object_new_string("success"));
    json_object_object_add(jobj_response, ONLINE_CPU, json_object_new_int(cpu));
    struct online_cpus *current_online_cpus = range_to_array(get_online_cpu_range());

    // no need to release jobj_array as its ownership is transferred to jobj_response
    struct json_object *jobj_array = new_json_obj_from_array(current_online_cpus);
    json_object_object_add(jobj_response, ONLINE_CPUS, jobj_array);

    return;

failed:
    json_object_object_add(jobj_response, RESULT, json_object_new_string("fail"));
    json_object_object_add(jobj_response, ERR_MSG, json_object_new_string(errorbuf));
    return;
}


/* Callback message handler.  This will be called by the generic guest/host
 * messaging library when a valid message arrives from the host.
 */
void msg_handler(const char *source_addr, json_object *jobj_request)
{
    int rc;

    // parse version
    struct json_object *jobj_version;
    if (!json_object_object_get_ex(jobj_request, VERSION, &jobj_version)) {
        ERR_LOG("failed to parse version");
        return;
    }
    int version = json_object_get_int(jobj_version);

    if (version != CUR_VERSION) {
        ERR_LOG("invalid version %d, expecting %d", version, CUR_VERSION);
        return;
    }

    // parse msg_type
    struct json_object *jobj_msg_type;
    if (!json_object_object_get_ex(jobj_request, MSG_TYPE, &jobj_msg_type)) {
        ERR_LOG("failed to parse msg_type");
        return;
    }
    const char *msg_type = json_object_get_string(jobj_msg_type);

    if (!strcmp(msg_type, MSG_TYPE_NACK)) {
        struct json_object *jobj_log_msg;
        if (!json_object_object_get_ex(jobj_request, LOG_MSG, &jobj_log_msg)) {
            ERR_LOG("Nack: failed to parse log_msg");
        }
        const char *log_msg = json_object_get_string(jobj_log_msg);
        ERR_LOG("Nack received, error message from host: %s", log_msg);
        return;
    } else if (!strcmp(msg_type, MSG_TYPE_SCALE_REQUEST)) {
        ;
    } else {
        ERR_LOG("unknown message type: %s", msg_type);
        return;
    }

    struct json_object *jobj_response = json_object_new_object();
    if (jobj_response == NULL) {
        ERR_LOG("failed to allocate json object for response");
        return;
    }

    struct json_object *jobj_resource;
    if (!json_object_object_get_ex(jobj_request, RESOURCE, &jobj_resource)) {
        ERR_LOG("failed to parse resource");
        goto done;
    }
    const char *resource = json_object_get_string(jobj_resource);

    struct json_object *jobj_direction;
    if (!json_object_object_get_ex(jobj_request, DIRECTION, &jobj_direction)) {
        ERR_LOG("failed to parse direction'");
        goto done;
    }
    const char *direction = json_object_get_string(jobj_direction);

    rc = -1;
    if (!strcmp(resource,"cpu")) {
        if (!strcmp(direction,"up")) {
            cpu_scale_up(jobj_request, jobj_response);
        } else if (!strcmp(direction,"down")) {
            cpu_scale_down(jobj_request, jobj_response);
        }
    }

    json_object_object_add(jobj_response, VERSION, json_object_new_int(CUR_VERSION));
    json_object_object_add(jobj_response, RESOURCE, jobj_resource);
    json_object_object_add(jobj_response, DIRECTION, jobj_direction);

    const char *response = json_object_to_json_string_ext(jobj_response, JSON_C_TO_STRING_PLAIN);

    // Send response back to the sender.
    rc = gh_send_msg(info, source_addr, response);
    if (rc < 0) {
        ERR_LOG("gh_send_msg failed: %s\n", gh_get_error(info));
        return;
    }
done:
    json_object_put(jobj_response);
}


void wait_for_messages(int fd)
{
    int rc;
    fd_set rfds, rfds_tmp;
    
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    
    while(1) {
        rfds_tmp = rfds;
        rc = select(fd+1, &rfds_tmp, NULL, NULL, NULL);
        if (rc > 0) {
            if (gh_process_msg(info) < 0) {
                ERR_LOG("problem processing messages: %s\n",
                                                    gh_get_error(info));
            }
        } else if (rc < 0) {
            ERR_LOG("select(): %m");
        }
    }
}


int main()
{
    int fd = gh_init(msg_handler, SCALE_AGENT_ADDR, &info);
    if (fd == -1) {
        if (!info)
            ERR_LOG("Unable to allocate memory for info: %m");
        else
            ERR_LOG("Unable to initialize guest/host messaging: %s\n",
                                                    gh_get_error(info));
        return -1;
    }
    INFO_LOG("Running offline_cpus script");
    system("offline_cpus");
    wait_for_messages(fd);

    return 0;
}

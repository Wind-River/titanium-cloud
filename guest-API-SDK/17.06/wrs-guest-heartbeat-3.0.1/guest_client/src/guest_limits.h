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
#ifndef __GUEST_LIMITS_H__
#define __GUEST_LIMITS_H__

#ifdef __cplusplus
extern "C" {
#endif

#define GUEST_NAME_MAX_CHAR                                              64
#define GUEST_DEVICE_NAME_MAX_CHAR                                      255
#define GUEST_MIN_TICK_INTERVAL_IN_MS                                    25
#define GUEST_TICK_INTERVAL_IN_MS                                       300
#define GUEST_SCHEDULING_MAX_DELAY_IN_MS                                800
#define GUEST_SCHEDULING_DELAY_DEBOUNCE_IN_MS                          2000
#define GUEST_TIMERS_MAX                                                128
#define GUEST_MAX_TIMERS_PER_TICK                        GUEST_TIMERS_MAX / 4
#define GUEST_SELECT_OBJS_MAX                                           128
#define GUEST_MAX_SIGNALS                                                32
#define GUEST_MAX_CONNECTIONS                                            32
#define GUEST_CHILD_PROCESS_MAX                                          16
#define GUEST_APPLICATIONS_MAX                                           16
#define GUEST_HEARTBEAT_MIN_INTERVAL_MS                                 400

#ifdef __cplusplus
}
#endif

#endif /* __GUEST_LIMITS_H__ */

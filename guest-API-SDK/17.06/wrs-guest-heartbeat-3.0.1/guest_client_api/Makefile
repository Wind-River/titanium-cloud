#
#   Copyright(c) 2013-2016, Wind River Systems, Inc. 
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Wind River Systems nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
SHELL = /bin/sh

## Configuration Directory Variables
prefix := /usr/local
exec_prefix := $(prefix)
sysconfdir := $(prefix)/etc
includedir := $(prefix)/include
libdir := $(exec_prefix)/lib
bindir := $(exec_prefix)/bin

MAKEFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
CURRENT_DIR := $(patsubst %/,%,$(dir $(MAKEFILE_PATH)))
BUILD_DIR := $(CURRENT_DIR)/build
PACKAGE_DIR := $(BUILD_DIR)/package
PACKAGE_ROOT_DIR := $(PACKAGE_DIR)/rootdir

.PHONY: all build create_build_dir sample common heartbeat clean distclean package

C_INCLUDES = -I$(CURRENT_DIR) -I$(CURRENT_DIR)/../include

common_NAME := guest_common_api
common_C_SRCS := guest_api_types.c guest_api_debug.c guest_api_stream.c
common_C_SRCS += guest_api_unix.c
common_C_OBJS := ${common_C_SRCS:.c=.o}
common_LDFLAGS := -fPIC -g -shared
common_LIB_NAME := lib$(common_NAME).so
common_LIB_VER_MJR_NAME := $(common_LIB_NAME).3
common_LIB_VER_NAME := $(common_LIB_VER_MJR_NAME).0.1
common_BUILD_OBJS := $(addprefix $(BUILD_DIR)/, $(common_C_OBJS))

heartbeat_NAME := guest_heartbeat_api
heartbeat_C_SRCS := guest_heartbeat_api.c
heartbeat_C_OBJS := ${heartbeat_C_SRCS:.c=.o}
heartbeat_LDFLAGS := -fPIC -g -shared
heartbeat_LIB_NAME := lib$(heartbeat_NAME).so
heartbeat_LIB_VER_MJR_NAME := $(heartbeat_LIB_NAME).3
heartbeat_LIB_VER_NAME := $(heartbeat_LIB_VER_MJR_NAME).0.1
heartbeat_BUILD_OBJS := $(addprefix $(BUILD_DIR)/, $(heartbeat_C_OBJS))

sample_NAME := sample-guest-app
sample_C_SRCS := sample_guest_app.c
sample_C_OBJS := ${sample_C_SRCS:.c=.o}
sample_LDLIBS :=  -L$(BUILD_DIR) -l$(common_NAME) -l$(heartbeat_NAME) -lrt
sample_BUILD_OBJS := $(addprefix $(BUILD_DIR)/, $(sample_C_OBJS))

CFLAGS = -g -O2 -Wall -Werror -Wformat -fPIC -DSYSCONFDIR=$(sysconfdir)

all: build

%.o: %.c
	$(CC) $(CFLAGS) $(C_INCLUDES) -c $^ -o $(BUILD_DIR)/$@

common: $(common_C_OBJS)
	$(CC) $(CFLAGS) $(common_BUILD_OBJS) -Wl,-soname,$(common_LIB_VER_MJR_NAME) \
	-o $(BUILD_DIR)/$(common_LIB_VER_NAME) $(common_LDFLAGS)
	ln -sf $(common_LIB_VER_NAME) $(BUILD_DIR)/$(common_LIB_NAME)
	ln -sf $(common_LIB_VER_NAME) $(BUILD_DIR)/$(common_LIB_VER_MJR_NAME)

heartbeat: common $(heartbeat_C_OBJS)
	$(CC) $(CFLAGS) $(heartbeat_BUILD_OBJS) -Wl,-soname,$(heartbeat_LIB_VER_MJR_NAME) \
	-o $(BUILD_DIR)/$(heartbeat_LIB_VER_NAME) $(heartbeat_LDFLAGS)
	ln -sf $(heartbeat_LIB_VER_NAME) $(BUILD_DIR)/$(heartbeat_LIB_NAME)
	ln -sf $(heartbeat_LIB_VER_NAME) $(BUILD_DIR)/$(heartbeat_LIB_VER_MJR_NAME)

create_build_dir:
	mkdir -p --mode 755 $(BUILD_DIR)

build: create_build_dir common heartbeat

sample: create_build_dir common heartbeat $(sample_C_OBJS)
	$(CC) $(CFLAGS) $(sample_BUILD_OBJS) -o $(BUILD_DIR)/$(sample_NAME) $(sample_LDLIBS)

clean:
	@-($(RM) -Rf $(BUILD_DIR)/*)

distclean: clean

package:
	@(mkdir -p --mode 755 $(PACKAGE_ROOT_DIR)/$(includedir))
	@(mkdir -p --mode 755 $(PACKAGE_ROOT_DIR)/$(libdir))
	@(echo "Packaging guest_api_types.h in $(PACKAGE_ROOT_DIR)/$(includedir)")
	@(cp $(CURRENT_DIR)/guest_api_types.h $(PACKAGE_ROOT_DIR)/$(includedir)/guest_api_types.h)
	@(echo "Packaging guest_heartbeat_api.h in $(PACKAGE_ROOT_DIR)/$(includedir)")
	@(cp $(CURRENT_DIR)/guest_heartbeat_api.h $(PACKAGE_ROOT_DIR)/$(includedir)/guest_heartbeat_api.h)
	@(echo "Packaging $(common_LIB_NAME) in $(PACKAGE_ROOT_DIR)/$(libdir)")
	@(cp $(BUILD_DIR)/$(common_LIB_VER_NAME) $(PACKAGE_ROOT_DIR)/$(libdir)/$(common_LIB_VER_NAME))
	@(chmod 644 $(PACKAGE_ROOT_DIR)/$(libdir)/$(common_LIB_VER_NAME))
	ln -sf $(common_LIB_VER_NAME) $(PACKAGE_ROOT_DIR)/$(libdir)/$(common_LIB_NAME)
	ln -sf $(common_LIB_VER_NAME) $(PACKAGE_ROOT_DIR)/$(libdir)/$(common_LIB_VER_MJR_NAME)
	@(echo "Packaging $(heartbeat_LIB_NAME) in $(PACKAGE_ROOT_DIR)/$(libdir)")
	@(cp $(BUILD_DIR)/$(heartbeat_LIB_VER_NAME) $(PACKAGE_ROOT_DIR)/$(libdir)/$(heartbeat_LIB_VER_NAME))
	@(chmod 644 $(PACKAGE_ROOT_DIR)/$(libdir)/$(heartbeat_LIB_VER_NAME))
	ln -sf $(heartbeat_LIB_VER_NAME) $(PACKAGE_ROOT_DIR)/$(libdir)/$(heartbeat_LIB_NAME)
	ln -sf $(heartbeat_LIB_VER_NAME) $(PACKAGE_ROOT_DIR)/$(libdir)/$(heartbeat_LIB_VER_MJR_NAME)

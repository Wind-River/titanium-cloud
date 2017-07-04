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

ifndef BUILD_DIR
$(error BUILD_DIR undefined)
endif

PACKAGE_DIR := $(BUILD_DIR)/package
PACKAGE_ROOT_DIR := $(PACKAGE_DIR)/rootdir

.SUFFIXES:
.SUFFIXES: .c .o

.PHONY: all build heartbeat clean distclean package

heartbeat_C_SRCS := $(wildcard $(CURRENT_DIR)/heartbeat/*.c)
heartbeat_C_SRCS := $(subst $(CURRENT_DIR)/heartbeat/,,$(heartbeat_C_SRCS))
heartbeat_C_OBJS := ${heartbeat_C_SRCS:.c=.o}

program_NAME := guest-client
program_C_INCLUDES := -I$(CURRENT_DIR) -I$(CURRENT_DIR)/heartbeat
program_C_INCLUDES += -I$(CURRENT_DIR)/../../include
program_C_SRCS := $(wildcard *.c)
program_C_OBJS := ${program_C_SRCS:.c=.o}
program_LDLIBS := -lrt
program_BUILD_OBJS := $(addprefix $(BUILD_DIR)/, $(heartbeat_C_OBJS))
program_BUILD_OBJS += $(addprefix $(BUILD_DIR)/, $(program_C_OBJS))

CFLAGS = -g -O2 -Wall -Werror -Wformat -DSYSCONFDIR=$(sysconfdir)

all: build

heartbeat:
	@(cd heartbeat; make --no-print-directory build \
	sysconfdir=$(sysconfdir) BUILD_DIR=$(BUILD_DIR))

%.o: %.c
	$(CC) $(CFLAGS) $(program_C_INCLUDES) -c $^ -o $(BUILD_DIR)/$@

$(program_NAME): heartbeat $(program_C_OBJS)
	$(CC) $(CFLAGS) $(program_BUILD_OBJS) $(program_LDLIBS) \
	-o $(BUILD_DIR)/$(program_NAME) -ljson-c

build: $(program_NAME)

clean:
	@-($(RM) -Rf $(BUILD_DIR)/*)

distclean: clean

package:
	@(mkdir -p --mode 755 $(PACKAGE_ROOT_DIR)/$(bindir))
	@(echo "Packaging $(program_NAME) in $(PACKAGE_ROOT_DIR)/$(bindir)")
	@(cp $(BUILD_DIR)/$(program_NAME) $(PACKAGE_ROOT_DIR)/$(bindir)/$(program_NAME))
	@(chmod 755 $(PACKAGE_ROOT_DIR)/$(bindir)/$(program_NAME))

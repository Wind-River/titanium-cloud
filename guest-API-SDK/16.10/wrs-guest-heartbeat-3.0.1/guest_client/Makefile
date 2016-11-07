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
CONFIG_DIR := $(sysconfdir)/guest-client/heartbeat
PACKAGE_DIR := $(BUILD_DIR)/package
PACKAGE_ROOT_DIR := $(PACKAGE_DIR)/rootdir

.PHONY: all build sample clean distclean package

all: build

build:
	mkdir -p --mode 755 $(BUILD_DIR)
	@(cd src; make --no-print-directory build \
	sysconfdir=$(sysconfdir) BUILD_DIR=$(BUILD_DIR))

sample:
	@:

clean:
	@-(cd src; make --no-print-directory clean BUILD_DIR=$(BUILD_DIR))

distclean: clean

package:
	@(mkdir -p --mode 755 $(PACKAGE_DIR))
	@(echo "Packaging guest-client.init in $(PACKAGE_DIR)")
	@(cp $(CURRENT_DIR)/scripts/guest-client.init $(PACKAGE_DIR)/guest-client.init)
	@(chmod 755  $(PACKAGE_DIR)/guest-client.init)
	@(echo "Packaging guest-client.service in $(PACKAGE_DIR)")
	@(cp $(CURRENT_DIR)/scripts/guest-client.service $(PACKAGE_DIR)/guest-client.service)
	@(chmod 644  $(PACKAGE_DIR)/guest-client.service)
	@(echo "Packaging guest-client.systemd in $(PACKAGE_DIR)")
	@(cp $(CURRENT_DIR)/scripts/guest-client.systemd $(PACKAGE_DIR)/guest-client.systemd)
	@(mkdir -p --mode 755 $(PACKAGE_ROOT_DIR)/$(CONFIG_DIR))
	@(echo "Packaging guest_heartbeat.conf in $(PACKAGE_ROOT_DIR)/$(CONFIG_DIR)")
	@(cp $(CURRENT_DIR)/scripts/guest_heartbeat.conf $(PACKAGE_ROOT_DIR)/$(CONFIG_DIR)/guest_heartbeat.conf)
	@(echo "Packaging sample_event_handling_script in $(PACKAGE_ROOT_DIR)/$(CONFIG_DIR)")
	@(cp $(CURRENT_DIR)/scripts/sample_event_handling_script $(PACKAGE_ROOT_DIR)/$(CONFIG_DIR)/sample_event_handling_script)
	@(echo "Packaging sample_health_check_script in $(PACKAGE_ROOT_DIR)/$(CONFIG_DIR)")
	@(cp $(CURRENT_DIR)/scripts/sample_health_check_script $(PACKAGE_ROOT_DIR)/$(CONFIG_DIR)/sample_health_check_script)
	@-(cd src; make --no-print-directory package BUILD_DIR=$(BUILD_DIR) \
	PACKAGE_DIR=$(PACKAGE_DIR) prefix=$(prefix) exec_prefix=$(exec_prefix) \
	sysconfdir=$(sysconfdir) includedir=$(includedir) libdir=$(libdir) \
	bindir=$(bindir))

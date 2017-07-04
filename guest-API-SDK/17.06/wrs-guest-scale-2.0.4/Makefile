MAJOR=2
MINOR=0
PATCH=4
#
#   BSD LICENSE
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

.PHONY: all clean

EXECS= guest_scale_agent

CFLAGS= -g -Wall

ODIR=obj
BINDIR=bin

GUEST_SCALE_AGENT_SRCS= guest_scale_agent.c parser.c
GUEST_SCALE_AGENT_OBJ := $(patsubst %,$(ODIR)/%,$(GUEST_SCALE_AGENT_SRCS:.c=.o))

BINEXECS=$(addprefix $(BINDIR)/, $(EXECS))

OBJS = $(GUEST_SCALE_AGENT_OBJ)

_DEPS = *.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))
DEPS = $(OBJS:.o=.d)
-include $(DEPS)


WRS_SERVER_GROUP_DIR ?= $(shell find ../wrs-server-group*[0-9] -maxdepth 0 -type d  | sort | tail -n 1)
WRS_SERVER_GROUP_INC2 = $(WRS_SERVER_GROUP_DIR)/include
WRS_SERVER_GROUP_INC ?= $(shell if [ -d $(WRS_SERVER_GROUP_INC2) ]; then echo "$(WRS_SERVER_GROUP_INC2)"; else echo "/usr/include"; fi)
WRS_SERVER_GROUP_LIB2 = $(WRS_SERVER_GROUP_DIR)/lib
WRS_SERVER_GROUP_LIB3 = $(shell if [ -d /usr/lib64 ]; then echo "/usr/lib64"; else echo "/usr/lib"; fi)
WRS_SERVER_GROUP_LIB ?= $(shell if [ -d $(WRS_SERVER_GROUP_LIB2) ]; then echo "$(WRS_SERVER_GROUP_LIB2)"; else echo "$(WRS_SERVER_GROUP_LIB3)"; fi)

CFLAGS += -I$(WRS_SERVER_GROUP_INC)
LDFLAGS += -L$(WRS_SERVER_GROUP_LIB)

$(ODIR)/%.o: %.c
	$(CC) -c $(CFLAGS) $(CFLAGS2) -MMD -o $@ $<

all: $(BINEXECS)

$(BINDIR)/guest_scale_agent: $(GUEST_SCALE_AGENT_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS) -lguesthostmsg  -ljson-c

clean:
	rm -rf $(ODIR)/* *~ core $(BINDIR)/*

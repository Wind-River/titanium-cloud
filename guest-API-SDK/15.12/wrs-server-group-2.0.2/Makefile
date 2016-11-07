MAJOR=2
MINOR=0
PATCH=2
#
#   Copyright(c) 2013-2016, Wind River Systems, Inc. All rights reserved.
#   All rights reserved.
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

EXECS= guest_agent server_group_app

CFLAGS= -g -Wall
LDFLAGS = 

ODIR=obj
BINDIR=bin
LIBDIR=lib
INCDIR=include/cgcs

_DEPS = *.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

GUEST_AGENT_SRCS = host_guest_msg.c guest_agent.c
GUEST_AGENT_OBJ := $(patsubst %,$(ODIR)/%,$(GUEST_AGENT_SRCS:.c=.o))

GUEST_CLIENT_APP_SRCS = server_group_app.c
GUEST_CLIENT_APP_OBJ := $(patsubst %,$(ODIR)/%,$(GUEST_CLIENT_APP_SRCS:.c=.o))

LIBGUESTHOSTMSG_SRCS := lib_guest_host_msg.c
LIBGUESTHOSTMSG_INCLUDES := guest_host_msg.h 
LIBGUESTHOSTMSG_OBJ := $(patsubst %,$(ODIR)/%,$(LIBGUESTHOSTMSG_SRCS:.c=.o))
LIBGUESTHOSTMSG := guesthostmsg
LIBGUESTHOSTMSG_SO := $(patsubst %,lib%.so,$(LIBGUESTHOSTMSG))
LIBGUESTHOSTMSG_DEP := $(patsubst %,$(LIBDIR)/%,$(LIBGUESTHOSTMSG_SO))

LIBSERVERGROUP_SRCS := server_group.c
LIBSERVERGROUP_INCLUDES := server_group.h
LIBSERVERGROUP_OBJ := $(patsubst %,$(ODIR)/%,$(LIBSERVERGROUP_SRCS:.c=.o))
LIBSERVERGROUP := servergroup
LIBSERVERGROUP_SO := $(patsubst %,lib%.so,$(LIBSERVERGROUP))
LIBSERVERGROUP_DEP := $(patsubst %,$(LIBDIR)/%,$(LIBSERVERGROUP_SO))

LIBS= $(LIBSERVERGROUP) $(LIBGUESTHOSTMSG)
INCLUDES= $(LIBGUESTHOSTMSG_INCLUDES) $(LIBSERVERGROUP_INCLUDES)

BINEXECS=$(addprefix $(BINDIR)/, $(EXECS))

LIBDEPS:=$(patsubst %,$(LIBDIR)/lib%.so,$(LIBS))

INCDEPS:=$(addprefix $(INCDIR)/, $(INCLUDES))

all: $(BINEXECS) $(LIBDEPS) $(INCDIR) $(INCDEPS)

OBJS = $(GUEST_AGENT_OBJ) $(LIBSERVERGROUP_OBJ) $(GUEST_CLIENT_APP_OBJ)
DEPS = $(OBJS:.o=.d)
-include $(DEPS)

$(ODIR)/%.o: %.c
	$(CC) -c $(CFLAGS) $(CFLAGS2) -MMD -o $@ $<

$(INCDIR):
	mkdir -p $(INCDIR)

$(INCDIR)/%.h: %.h $(INCDIR)
	cp $< $@

vpath %.h .

# the version of make in wrl5 seems to have a problem with target-specific
# variable values.  It can't handle just doing "CFLAGS += -fPIC".  Same thing
# for LDFLAGS below.  This seems to work as a workaround.
$(LIBSERVERGROUP_DEP): CFLAGS2 = -fPIC
$(LIBSERVERGROUP_DEP): $(LIBSERVERGROUP_OBJ)
	$(CC) -shared -Wl,-soname,$(LIBSERVERGROUP_SO).$(MAJOR) $^ -o $(LIBSERVERGROUP_DEP).$(MAJOR).$(MINOR).$(PATCH)
	ln -sf $(LIBSERVERGROUP_SO).$(MAJOR).$(MINOR).$(PATCH) $(LIBSERVERGROUP_DEP).$(MAJOR)
	ln -sf $(LIBSERVERGROUP_SO).$(MAJOR) $(LIBSERVERGROUP_DEP)

$(LIBGUESTHOSTMSG_DEP): CFLAGS2 = -fPIC
$(LIBGUESTHOSTMSG_DEP): $(LIBGUESTHOSTMSG_OBJ)
	$(CC) -shared -Wl,-soname,$(LIBGUESTHOSTMSG_SO).$(MAJOR) $^ -o $(LIBGUESTHOSTMSG_DEP).$(MAJOR).$(MINOR).$(PATCH)
	ln -sf $(LIBGUESTHOSTMSG_SO).$(MAJOR).$(MINOR).$(PATCH) $(LIBGUESTHOSTMSG_DEP).$(MAJOR)
	ln -sf $(LIBGUESTHOSTMSG_SO).$(MAJOR) $(LIBGUESTHOSTMSG_DEP)

$(BINDIR)/guest_agent: LDFLAGS2 = -rdynamic
$(BINDIR)/guest_agent: $(GUEST_AGENT_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDFLAGS2) -ljson-c

$(BINDIR)/server_group_app: LDFLAGS2 = -l$(LIBSERVERGROUP) -L$(LIBDIR)
$(BINDIR)/server_group_app: $(GUEST_CLIENT_APP_OBJ) $(LIBSERVERGROUP_DEP)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDFLAGS2) -ljson-c


clean:
	rm -rf $(ODIR)/* *~ core $(BINDIR)/* $(LIBDIR)/*

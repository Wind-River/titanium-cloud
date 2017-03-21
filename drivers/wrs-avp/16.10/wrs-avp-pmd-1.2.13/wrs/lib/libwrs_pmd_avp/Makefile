#   BSD LICENSE
#
#   Copyright(c) 2013-2015, Wind River Systems, Inc. All rights reserved.
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


include $(RTE_SDK)/mk/rte.vars.mk

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

ifneq ($(WRS_PMD_SHARED_LIB),)

ifeq ($(WRS_SDK),)
$(error ERROR: WRS_SDK is not set)
endif
##
## Build an out of tree shared library
##
SHARED = libwrs_pmd_avp.so
CONFIG_WRS_LIBWRS_AVP_PMD = y
CFLAGS += -I${WRS_SDK}/wrs/lib/libwrs_eal/linuxapp/eal/include/
CFLAGS += -I${WRS_SDK}/wrs/lib/libwrs_eal/common/include/
CFLAGS += -DWRS_LIBWRS_AVP_STATS=1

else
##
## Build an in-tree static library
##
LIB = libwrs_pmd_avp.a

# install includes (needed for backwards compatibility with < 1.7)
SYMLINK-$(CONFIG_WRS_LIBWRS_AVP_PMD)-include += avp_ethdev.h

# this lib depends upon:
DEPDIRS-$(CONFIG_WRS_LIBWRS_AVP_PMD) += lib/librte_eal
DEPDIRS-$(CONFIG_WRS_LIBWRS_AVP_PMD) += lib/librte_ether
DEPDIRS-$(CONFIG_WRS_LIBWRS_AVP_PMD) += lib/librte_mempool
DEPDIRS-$(CONFIG_WRS_LIBWRS_AVP_PMD) += lib/librte_mbuf
DEPDIRS-$(CONFIG_WRS_LIBWRS_AVP_PMD) += lib/librte_net
DEPDIRS-$(CONFIG_WRS_LIBWRS_AVP_PMD) += lib/librte_malloc
DEPDIRS-$(CONFIG_WRS_LIBWRS_AVP_PMD) += wrs/lib/libwrs_eal

endif

#
# all source files are stored in SRCS-y
#
SRCS-$(CONFIG_WRS_LIBWRS_AVP_PMD) += avp_ethdev.c

ifneq ($(WRS_PMD_SHARED_LIB),)
include $(RTE_SDK)/mk/rte.extshared.mk
else
include $(RTE_SDK)/mk/rte.lib.mk
endif

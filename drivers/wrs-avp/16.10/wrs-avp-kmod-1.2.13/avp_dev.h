/*-
 * GPL LICENSE SUMMARY
 *
 *   Copyright(c) 2010-2013 Intel Corporation. All rights reserved.
 *   Copyright(c) 2013-2014 Wind River Systems. All rights reserved.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of version 2 of the GNU General Public License as
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *   The full GNU General Public License is included in this distribution
 *   in the file called LICENSE.GPL.
 *
 *   Contact Information:
 *   Wind River Systems, Inc.
 */

#ifndef _AVP_DEV_H_
#define _AVP_DEV_H_

#include <linux/if.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/ratelimit.h>
#include <linux/printk.h>
#include <linux/netdevice.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/percpu.h>
#include <linux/cpumask.h>
#include <linux/if_ether.h>

#include <exec-env/wrs_avp_common.h>


/* Defines the number receive thread CPU yield time in usecs */
#define WRS_AVP_KTHREAD_RESCHEDULE_INTERVAL (1)

/* Defines the maximum number of queues assigned to a receive thread */
#define WRS_AVP_KTHREAD_MAX_RX_QUEUES (256)

/* Defines the maximum number of data buffers stored in the Tx cache */
#define WRS_AVP_QUEUE_MBUF_CACHE_SIZE (32)

/* Defines the current kernel AVP driver version number */
#define WRS_AVP_KERNEL_DRIVER_VERSION WRS_AVP_CURRENT_GUEST_VERSION

/**
 * A structure to hold the per-cpu statistics for a device.
 */
struct avp_stats {
	struct u64_stats_sync tx_syncp;
	struct u64_stats_sync rx_syncp;
	u64	  rx_packets;
	u64	  tx_packets;
	u64	  rx_bytes;
	u64	  tx_bytes;
	u64	  rx_errors;
	u64	  tx_errors;
	u64	  rx_dropped;
	u64	  tx_dropped;
	u64	  rx_fifo_errors;
	u64	  tx_fifo_errors;
};

/** AVP device internal states */
#define WRS_AVP_DEV_STATUS_UNKNOWN 0
#define WRS_AVP_DEV_STATUS_OK 1
#define WRS_AVP_DEV_STATUS_DETACHED 2

/* Defines the device ioctl in progress flag */
#define WRS_AVP_IOCTL_IN_PROGRESS_BIT_NUM 0

/**
 * A structure to hold per-cpu cache of allocated mbufs
 */
struct avp_mbuf_cache {
	struct wrs_avp_mbuf * mbufs[WRS_AVP_QUEUE_MBUF_CACHE_SIZE];
	unsigned count;
} ____cacheline_internodealigned_in_smp;

/**
 * A structure describing the translation parameters for each pool
 */
struct avp_mempool_info {
    void *va; /**< host virtual address */
    void *kva; /**< kernel virtual address */
    size_t length; /**< region length in bytes */
};

/**
 * A structure describing the private information for a avp device.
 */

struct avp_dev {
	/* global device list */
	struct list_head list;

	/* device list that requires response queue polling */
	struct list_head poll;

	/* network statistics */
	struct avp_stats __percpu *stats;

	/* network device name */
	char ifname[WRS_AVP_NAMESIZE];

	/* unique system identifier */
	uint64_t device_id;

	/* supported feature bitmap */
	uint32_t host_features;

	/* enabled feature bitmap */
	uint32_t features;

	/* current device status */
	int status;

	/* device mode */
	unsigned mode;

	/* wait queue for req/resp */
	wait_queue_head_t wq;
	struct mutex sync_lock;

	/* internal device references */
	struct net_device *net_dev;
	struct pci_dev *pci_dev;

	/* transmit queues */
	void *tx_q[WRS_AVP_MAX_QUEUES];

	/* receive queues */
	void *rx_q[WRS_AVP_MAX_QUEUES];

	/* available data buffers */
	void *alloc_q[WRS_AVP_MAX_QUEUES];

	/* used data buffers */
	void *free_q[WRS_AVP_MAX_QUEUES];

	/* cached data buffers for transmit optimization */
	struct avp_mbuf_cache mbuf_cache[WRS_AVP_MAX_QUEUES];

	/* request/response queues */
	void *req_q;
	void *resp_q;
	void *sync_kva;
	void *sync_va;

    /* per-socket mbuf pools */
    struct avp_mempool_info pool[WRS_AVP_MAX_MEMPOOLS];

	/* data buffer address references */
	void *mbuf_kva;
	void *mbuf_va;

	/* mbuf size */
	unsigned mbuf_size;

	/* maximum receive unit */
	unsigned max_rx_pkt_len;

	/* current rx/tx queue counts */
	unsigned num_rx_queues;
	unsigned max_rx_queues;
	unsigned num_tx_queues;
	unsigned max_tx_queues;

	/* work struct for interrupt deferral */
	struct work_struct detached;
	struct work_struct attached;
};

/**
 * Structure to hold the receive queue data
 */
struct avp_dev_rx_queue {
	struct avp_dev * avp;
	unsigned int queue_id;
};

/**
 * Structure to hold the thread context per-cpu
 */
struct avp_thread {
	struct task_struct *avp_kthread;
	int cpu;
        int sched_policy;
        int sched_priority;
	spinlock_t lock;
	unsigned int rx_count;
	struct avp_dev_rx_queue rx_queues[WRS_AVP_KTHREAD_MAX_RX_QUEUES];
};

#define AVP_ERR_RATELIMIT(args...) printk_ratelimited(KERN_ERR "AVP: Error: " args)
#define AVP_ERR(args...) printk(KERN_ERR "AVP: Error: " args)
#define AVP_INFO(args...) printk(KERN_DEBUG "AVP: " args)
#define AVP_PRINT(args...) printk(KERN_DEBUG "AVP: " args)
#ifdef WRS_AVP_KMOD_DEBUG
	#define AVP_DBG(args...) printk(KERN_DEBUG "AVP: " args)
#else
	#define AVP_DBG(args...)
#endif

#endif

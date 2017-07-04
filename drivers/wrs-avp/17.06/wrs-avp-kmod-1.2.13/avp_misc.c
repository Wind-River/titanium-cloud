/*-
 * GPL LICENSE SUMMARY
 *
 *   Copyright(c) 2010-2013 Intel Corporation. All rights reserved.
 *   Copyright(c) 2013-2014 Wind River Systems, Inc. All rights reserved.
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
 *
 */

/*
 * This module and all associated files have been derived from the original
 * RTE KNI module which existed in v1.5.0 of the Intel DPDK.  It was forked
 * off as a new module named AVP to avoid conflicts when merging with new
 * versions and patches to existing DPDK branches.
 */

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/cpu.h>
#include <linux/pci.h>
#include <linux/kthread.h>
#include <linux/rwsem.h>
#include <linux/list.h>
#include <linux/cpumask.h>
#include <linux/percpu.h>
#include <linux/bottom_half.h>
#include <linux/threads.h>
#include <linux/rtnetlink.h>
#include <linux/sched.h>
#include <linux/version.h>

#include "avp_dev.h"
#include "avp_ctrl.h"

#include <rte_avp_fifo.h>

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Wind River Systems");
MODULE_DESCRIPTION("Kernel Module for managing AVP devices");

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
#define NETIF_F_HW_VLAN_TX     NETIF_F_HW_VLAN_CTAG_TX
#define NETIF_F_HW_VLAN_RX     NETIF_F_HW_VLAN_CTAG_RX
#define NETIF_F_HW_VLAN_FILTER NETIF_F_HW_VLAN_CTAG_FILTER
#endif /* >= 3.10.0 */

/* Defines the number of loop interations before yielding to the scheduler */
#define WRS_AVP_RX_LOOP_NUM 1000

#ifndef WRS_AVP_NETDEV_NAME_FORMAT
/* Defines the interface name prefix */
#define WRS_AVP_NETDEV_NAME_FORMAT "eth%d"
#endif

/* Defines the module in use flag */
#define WRS_AVP_DEV_IN_USE_BIT_NUM 0

/* Indicates that the module is inuse by a user of /dev/avp */
unsigned long device_in_use;

/* AVP kernel threads */
static char *kthread_cpulist;
static int kthread_policy = SCHED_RR;
static int kthread_priority = 1;
static cpumask_t avp_cpumask; /* allowed CPU mask for kernel threads */
static struct avp_thread avp_threads[NR_CPUS] ____cacheline_aligned;
static struct avp_thread avp_trace_thread ____cacheline_aligned;
static DEFINE_MUTEX(avp_thread_lock);

/* AVP device list */
static DECLARE_RWSEM(avp_list_lock);
static struct list_head avp_list_head = LIST_HEAD_INIT(avp_list_head);

/* AVP device list that requires response queue polling */
static DECLARE_RWSEM(avp_poll_lock);
static struct list_head avp_poll_head = LIST_HEAD_INIT(avp_poll_head);

/*****************************************************************************
 *
 * AVP RX thread utility functions
 *
 *****************************************************************************/

static int __init
avp_thread_init(void)
{
	struct avp_thread *thread;
	int cpu;

	mutex_init(&avp_thread_lock);

	for_each_possible_cpu(cpu) {
		thread = &avp_threads[cpu];

		thread->cpu = cpu;
		thread->sched_policy = kthread_policy;
		thread->sched_priority = kthread_priority;
		thread->avp_kthread = NULL;
		thread->rx_count = 0;
		spin_lock_init(&thread->lock);
	}

	avp_trace_thread.cpu = 0;
	avp_trace_thread.sched_policy = SCHED_NORMAL;
	avp_trace_thread.sched_priority = 0;
	avp_trace_thread.avp_kthread = NULL;
	avp_trace_thread.rx_count = 0;
	spin_lock_init(&avp_trace_thread.lock);

	return 0;
}

static int
avp_thread_process(void *arg)
{
	struct avp_thread *thread = (struct avp_thread *)arg;
	struct avp_dev_rx_queue *queue;
	struct sched_param schedpar;
	struct avp_dev *dev, *n;
	unsigned rx_count;
	unsigned busy;
	unsigned i;
	unsigned q;

	/* update our priority to the configured value */
	schedpar.sched_priority = thread->sched_priority;
	sched_setscheduler(current, thread->sched_policy, &schedpar);

	while (!kthread_should_stop()) {
		i = 0;
		do {
			busy = 0;

			local_bh_disable();
			spin_lock(&thread->lock);
			rx_count = thread->rx_count;
			for (q = 0; q < rx_count; q++) {
				queue = &thread->rx_queues[q];
				busy |= avp_net_rx(queue->avp, queue->queue_id);
			}
			spin_unlock(&thread->lock);
			local_bh_enable();

		} while (busy && (i++ < WRS_AVP_RX_LOOP_NUM));

		down_read(&avp_poll_lock);
		list_for_each_entry_safe(dev, n, &avp_poll_head, poll) {
			avp_ctrl_poll_resp(dev);
		}
		up_read(&avp_poll_lock);

		if (!busy) {
			/* schedule out for a while */
			schedule_timeout_interruptible(usecs_to_jiffies(
					WRS_AVP_KTHREAD_RESCHEDULE_INTERVAL));
		}
	}

	return 0;
}

static int
_avp_thread_queue_assign(struct avp_dev *avp,
			 unsigned queue_id,
			 const cpumask_t *cpumask)
{
	struct avp_dev_rx_queue *queue;
	struct avp_thread *thread;
	struct avp_thread *thread_ptr;
	unsigned rx_count;
	int cpu;

	/* find thread with the least queues */
	thread = NULL;
	for_each_cpu_and(cpu, &avp_cpumask, cpumask) {
		thread_ptr = &avp_threads[cpu];

		if (thread_ptr->rx_count < WRS_AVP_KTHREAD_MAX_RX_QUEUES) {
			rx_count = thread_ptr->rx_count;
			if ((thread == NULL) || (rx_count < thread->rx_count))
				thread = thread_ptr;
		}
	}

	if (!thread) {
		AVP_ERR("Unable to assign AVP RX queue: %u\n", queue_id);
		return -ESRCH;
	}

	AVP_INFO("Assigning %s/%u to cpu%u\n",
		 netdev_name(avp->net_dev), queue_id, thread->cpu);

	spin_lock(&thread->lock);
	queue = &thread->rx_queues[thread->rx_count];
	queue->avp = avp;
	queue->queue_id = queue_id;
	thread->rx_count++;
	spin_unlock(&thread->lock);

	if (thread->rx_count == 1 && !thread->avp_kthread) {
		/* first queue assignment; create the thread. */
		thread->avp_kthread = kthread_create_on_node(avp_thread_process,
							(void *)thread,
							cpu_to_node(thread->cpu),
							"avp-rx/%d", thread->cpu);

		if (IS_ERR(thread->avp_kthread)) {
			thread->avp_kthread = NULL;
			AVP_ERR("Unable to create kernel thread: %u\n", thread->cpu);
			return -ECANCELED;
		}

		kthread_bind(thread->avp_kthread, thread->cpu);
		wake_up_process(thread->avp_kthread);
	}

	return 0;
}

static void
avp_thread_dev_remove(struct avp_dev *avp)
{
	struct avp_thread *thread;
	int cpu;
	unsigned q;

	mutex_lock(&avp_thread_lock);

	/* remove queues from all assigned threads */
	for_each_cpu_and(cpu, &avp_cpumask, cpu_possible_mask) {
		thread = &avp_threads[cpu];

		spin_lock(&thread->lock);
		for (q = 0; q < thread->rx_count; ) {
			if (thread->rx_queues[q].avp == avp) {
				thread->rx_count--;
				if (q != thread->rx_count) {
					/* replace and recheck the current entry */
					thread->rx_queues[q] = thread->rx_queues[thread->rx_count];
					continue;
				}
			}
			q++;
		}
		spin_unlock(&thread->lock);

		/* stop thread if no queues assigned and thread is running */
		if (thread->rx_count == 0 && thread->avp_kthread) {
			kthread_stop(thread->avp_kthread);
			thread->avp_kthread = NULL;
		}
	}

	mutex_unlock(&avp_thread_lock);
}

static void
avp_thread_dev_add(struct avp_dev *avp)
{
	unsigned q;

	mutex_lock(&avp_thread_lock);

	/* assign RX queues to AVP threads (creating thread if necessary) */
	for (q = 0; q < avp->num_rx_queues; q++)
		_avp_thread_queue_assign(avp, q, cpu_online_mask);

	mutex_unlock(&avp_thread_lock);
}


static int
_avp_trace_queue_assign(struct avp_dev *avp, unsigned queue_id)
{
	struct avp_thread *thread = &avp_trace_thread;
	struct avp_dev_rx_queue *queue;

	AVP_INFO("Assigning %s/%u to trace thread\n",
			 netdev_name(avp->net_dev), queue_id);

	spin_lock(&thread->lock);
	queue = &thread->rx_queues[thread->rx_count];
	queue->avp = avp;
	queue->queue_id = queue_id;
	thread->rx_count++;
	spin_unlock(&thread->lock);

	if (thread->rx_count == 1 && !thread->avp_kthread) {
		/* first queue assignment; create the thread. */
		thread->avp_kthread = kthread_create(avp_thread_process,
							(void *)thread,
							"avp-trace");

		if (IS_ERR(thread->avp_kthread)) {
			thread->avp_kthread = NULL;
			AVP_ERR("Unable to create kernel trace thread\n");
			return -ECANCELED;
		}

		wake_up_process(thread->avp_kthread);
	}

	return 0;
}


static void
avp_trace_dev_add(struct avp_dev *avp)
{
	unsigned q;

	mutex_lock(&avp_thread_lock);

	/* assign RX queues to AVP threads (creating thread if necessary) */
	for (q = 0; q < avp->num_rx_queues; q++)
		_avp_trace_queue_assign(avp, q);

	mutex_unlock(&avp_thread_lock);
}


static void
avp_trace_dev_remove(struct avp_dev *avp)
{
	struct avp_thread *thread = &avp_trace_thread;
	unsigned q;

	mutex_lock(&avp_thread_lock);

	spin_lock(&thread->lock);
	for (q = 0; q < thread->rx_count; ) {
		if (thread->rx_queues[q].avp == avp) {
			thread->rx_count--;
			if (q != thread->rx_count) {
				/* replace and recheck the current entry */
				thread->rx_queues[q] = thread->rx_queues[thread->rx_count];
				continue;
			}
		}
		q++;
	}
	spin_unlock(&thread->lock);

	/* stop thread if no queues assigned and thread is running */
	if (thread->rx_count == 0 && thread->avp_kthread) {
		kthread_stop(thread->avp_kthread);
		thread->avp_kthread = NULL;
	}

	mutex_unlock(&avp_thread_lock);
}


#ifdef CONFIG_HOTPLUG_CPU
static void
avp_cpu_dump(void)
{
#ifdef WRS_AVP_KMOD_DEBUG
	struct avp_dev_rx_queue *queue;
	unsigned q;
#endif
	struct avp_thread *thread;
	int cpu;

	for_each_cpu_and(cpu, &avp_cpumask, cpu_online_mask) {
		thread = &avp_threads[cpu];
		AVP_INFO("CPU %u: %u rx queues\n", cpu, thread->rx_count);
#ifdef WRS_AVP_KMOD_DEBUG
		for (q = 0; q < thread->rx_count; q++) {
			queue = &thread->rx_queues[q];
			AVP_INFO("	queue %u: %s/%u\n",
					 q, netdev_name(queue->avp->net_dev), queue->queue_id);
		}
#endif
	}
}

/*
 * Transfer the last queue (largest index) from the source thread to the
 * destination thread.
 */
static int
_avp_thread_queue_transfer(struct avp_thread *dst, struct avp_thread *src)
{
	struct avp_dev_rx_queue *src_queue, *dst_queue;

	/* remove from source thread */
	spin_lock(&src->lock);
	src->rx_count--;
	src_queue = &src->rx_queues[src->rx_count];
	spin_unlock(&src->lock);

	/* add to target thread */
	spin_lock(&dst->lock);
	dst_queue = &dst->rx_queues[dst->rx_count];
	dst_queue->avp = src_queue->avp;
	dst_queue->queue_id = src_queue->queue_id;
	dst->rx_count++;
	spin_unlock(&dst->lock);

	AVP_INFO("Transferred %s/%u from cpu%u to cpu%u\n",
			 netdev_name(src_queue->avp->net_dev),
			 src_queue->queue_id,
			 src->cpu, dst->cpu);

	return 0;
}

static int
avp_cpu_online_action(unsigned int cpu)
{
	struct avp_thread *busiest, *tmp, *thread = &avp_threads[cpu];
	cpumask_var_t candidates;
	unsigned transferred;
	int ret = -EINVAL;
	int c;

	mutex_lock(&avp_thread_lock);
	if (thread->avp_kthread) {
		ret = 0;
		goto out;
	}

	if (!zalloc_cpumask_var(&candidates, GFP_KERNEL)) {
		ret = -ENOMEM;
		goto out;
	}
	cpumask_andnot(candidates, cpu_online_mask, cpumask_of(cpu));

	thread->avp_kthread = kthread_create_on_node(avp_thread_process,
						     (void *)thread,
						     cpu_to_node(cpu),
						     "avp/%d", cpu);
	if (IS_ERR(thread->avp_kthread)) {
		thread->avp_kthread = NULL;
		AVP_ERR("Unable to create kernel thread: %u\n", thread->cpu);
		ret = -ECANCELED;
		goto out_free;
	}

	kthread_bind(thread->avp_kthread, thread->cpu);
	wake_up_process(thread->avp_kthread);

	do {
		busiest = NULL;
		transferred = 0;
		for_each_cpu_and(c, &avp_cpumask, candidates) {
			tmp = &avp_threads[c];
			if (busiest == NULL || tmp->rx_count > busiest->rx_count)
				busiest = tmp;
		}

		if (busiest->rx_count > (thread->rx_count + 1)) {
			_avp_thread_queue_transfer(thread, busiest);
			transferred = 1;
		}

		/* keep trying until all threads are as balanced as possible */
	} while (transferred);

	AVP_INFO("CPU online: %u\n", cpu);
	avp_cpu_dump();
	ret = 0;

out_free:
	free_cpumask_var(candidates);
out:
	mutex_unlock(&avp_thread_lock);
	return notifier_from_errno(ret);
}

static int
avp_cpu_offline_action(unsigned int cpu)
{
	struct avp_thread *thread = &avp_threads[cpu];
	struct avp_dev_rx_queue *queue;
	cpumask_var_t candidates;
	int ret = -EINVAL;
	unsigned q;

	mutex_lock(&avp_thread_lock);
	if (!thread->avp_kthread) {
		ret = 0;
		goto out;
	}

	if (!zalloc_cpumask_var(&candidates, GFP_KERNEL)) {
		ret = -ENOMEM;
		goto out;
	}
	cpumask_andnot(candidates, cpu_online_mask, cpumask_of(cpu));

	kthread_stop(thread->avp_kthread);
	thread->avp_kthread = NULL;

	spin_lock(&thread->lock);
	for (q = 0; q < thread->rx_count; q++) {
		queue = &thread->rx_queues[q];
		ret = _avp_thread_queue_assign(queue->avp, queue->queue_id, candidates);
		if (ret) {
			spin_unlock(&thread->lock);
			AVP_ERR("Failed to re-assign %s/%u off of avp/%u, ret=%d\n",
					netdev_name(queue->avp->net_dev), queue->queue_id, cpu, ret);
			goto out_free;
		}
	}
	thread->rx_count = 0;
	spin_unlock(&thread->lock);

	AVP_INFO("CPU offline: %u\n", cpu);
	avp_cpu_dump();
	ret = 0;

out_free:
	free_cpumask_var(candidates);
out:
	mutex_unlock(&avp_thread_lock);
	return notifier_from_errno(ret);
}

static int
avp_cpu_hotplug_callback(struct notifier_block *nfb,
			 unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	int ret;

	switch (action) {
	case CPU_DOWN_FAILED:
		/* handle a failure to offline a cpu */
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		ret = avp_cpu_online_action(cpu);
		break;
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		ret = avp_cpu_offline_action(cpu);
		break;
	default:
		ret = NOTIFY_OK;
	}

	return ret;
}

static struct notifier_block avp_cpu_hotplug_notifier = {
	.notifier_call = avp_cpu_hotplug_callback,
};
#endif


/*****************************************************************************
 *
 * AVP device utilities
 *
 *****************************************************************************/

static void
avp_dev_flush_cache(struct avp_dev *dev)
{
	unsigned qnum;

	for (qnum = 0; qnum < RTE_AVP_MAX_QUEUES; qnum++) {
		struct avp_desc_cache *cache = &dev->mbuf_cache[qnum];
		if (cache->count > 0) {
			avp_fifo_put(dev->alloc_q[qnum], (void **)cache->mbufs, cache->count);
			cache->count = 0;
		}
	}
}

static int
avp_dev_free(struct avp_dev *dev)
{
	if (!dev)
		return -ENODEV;

	if (dev->net_dev) {
		unregister_netdev(dev->net_dev);
		free_netdev(dev->net_dev);
		dev->net_dev = NULL;
	}

	if (dev->stats) {
		free_percpu(dev->stats);
		dev->stats = NULL;
	}

	avp_dev_flush_cache(dev);

	return 0;
}

/* lookup an AVP device by unique device id */
static struct avp_dev *
avp_dev_find(uint64_t device_id)
{
	struct avp_dev *n, *dev = NULL;

	down_read(&avp_list_lock);
	list_for_each_entry_safe(dev, n, &avp_list_head, list) {
		if (device_id == dev->device_id) {
			up_read(&avp_list_lock);
			return dev;
		}
	}
	up_read(&avp_list_lock);

	return NULL;
}

static int
avp_dev_validate(struct rte_avp_device_info *info)
{
	if (!info)
		return -EINVAL;

	if (info->min_tx_queues > RTE_AVP_MAX_QUEUES) {
		AVP_ERR("AVP device id 0x%llx min_tx_queues %u exceeds max %u\n",
				info->device_id, info->min_tx_queues, RTE_AVP_MAX_QUEUES);
		return -EINVAL;
	}

	if (info->min_rx_queues > RTE_AVP_MAX_QUEUES) {
		AVP_ERR("AVP device id 0x%llx min_rx_queues %u exceeds max %u\n",
				info->device_id, info->min_rx_queues, RTE_AVP_MAX_QUEUES);
		return -EINVAL;
	}

	return 0;
}

/*
 * During VM live migrations it is necessary to detach the AVP device from its
 * (host-provided) memory.	This is because the contents and layout of the
 * memory will change as the VM is migrated to a new host.	This function
 * essentially stops the net_device but does not remove it from the system so
 * that applications are not impacted beyond minimal message loss.	After the
 * migration the device will be re-attached to its memory.
 */
int
avp_dev_detach(struct avp_dev *avp)
{
	struct net_device *net_dev = avp->net_dev;
	int ret;

	if (avp->status == WRS_AVP_DEV_STATUS_DETACHED) {
		AVP_ERR("netdev %s already detached\n", net_dev->name);
		return 0;
	}

	AVP_INFO("detaching netdev %s from device 0x%llx\n",
			 net_dev->name, avp->device_id);

	/* update status */
	avp->status = WRS_AVP_DEV_STATUS_DETACHED;

	netif_carrier_off(net_dev);

	/* suspend transmit processing */
	netif_tx_stop_all_queues(net_dev);

	/* inform the host that we are shutting down */
	ret = avp_ctrl_shutdown(avp);
	if (ret < 0) {
		AVP_ERR("failed to send/recv shutdown to host, ret=%d\n", ret);
		goto restart;
	}

	/* stop polling for responses */
	down_write(&avp_poll_lock);
	list_del(&avp->poll);
	up_write(&avp_poll_lock);

	/* remove the device from all receive threads */
	avp_thread_dev_remove(avp);

	/* flush cached mbuf pointers */
	avp_dev_flush_cache(avp);

	return 0;

restart:
	avp->status = WRS_AVP_DEV_STATUS_OK;
	if (net_dev->flags & IFF_UP) {
		netif_carrier_on(net_dev);
		netif_tx_start_all_queues(net_dev);
	}

	return ret;
}

/*
 * Sets up the AVP device based on the configuration details found in the
 * incoming device info.
 */
int
avp_dev_configure(struct avp_dev *avp, struct rte_avp_device_info *dev_info)
{
	struct avp_mempool_info *pool;
	unsigned i;

	/* adjust max values */
	avp->max_tx_queues =
		min_t(unsigned, dev_info->max_tx_queues, RTE_AVP_MAX_QUEUES);
	avp->max_rx_queues =
		min_t(unsigned, dev_info->max_rx_queues, RTE_AVP_MAX_QUEUES);

	/* adjust actual values */

	/* the transmit direction is not negotiated beyond respecting the max
	 * number of queues because the host can handle arbitrary guest tx queues
	 * (host rx queues).  To minimize the number of host receive queues we
	 * will set the number of queues to be equal to the host recommended
	 * value since they are not tied to a particular cpu or thread in the
	 * guest.
	 */
	avp->num_tx_queues =
		min_t(unsigned, dev_info->num_tx_queues, avp->max_tx_queues);

	/*
	 * the receive direction is more restrictive.  The host requires a minimum
	 * number of guest rx queues (host tx queues) therefore negotiate a value
	 * that is at least as large as the host minimum requirement.  If the host
	 * and guest values are not identical then some threads will service more
	 * queues and performance may be impacted.
	 */
	avp->num_rx_queues =
		max_t(unsigned, dev_info->min_rx_queues, cpumask_weight(&avp_cpumask));
	if (avp->num_rx_queues > avp->max_rx_queues) {
		/* more threads are enabled than number of supported queues */
		avp->num_rx_queues = avp->max_rx_queues;
	}

	/* Translate user space info into kernel space info */
	for (i = 0; i < avp->max_tx_queues; i++) {
		avp->tx_q[i] = phys_to_virt(dev_info->tx_phys + (i * dev_info->tx_size));
		avp->alloc_q[i] =
			phys_to_virt(dev_info->alloc_phys + (i * dev_info->alloc_size));
	}

	for (i = 0; i < avp->max_rx_queues; i++) {
		avp->rx_q[i] = phys_to_virt(dev_info->rx_phys + (i * dev_info->rx_size));
		avp->free_q[i] =
			phys_to_virt(dev_info->free_phys + (i * dev_info->free_size));
	}

	avp->req_q = phys_to_virt(dev_info->req_phys);
	avp->resp_q = phys_to_virt(dev_info->resp_phys);
	avp->sync_va = dev_info->sync_va;
	avp->sync_kva = phys_to_virt(dev_info->sync_phys);

	for (i = 0; i < RTE_AVP_MAX_MEMPOOLS; i++) {
		pool = &avp->pool[i];
		pool->va = dev_info->pool[i].addr;
		pool->kva = phys_to_virt(dev_info->pool[i].phys_addr);
		pool->length = dev_info->pool[i].length;
		AVP_DBG("pool[%u]: mbuf_phys:  0x%016llx, mbuf_kva: 0x%p\n",
				(unsigned long long) dev_info->pool[i].phys_addr, pool->kva);
	}

	avp->mbuf_kva = phys_to_virt(dev_info->mbuf_phys);
	avp->mbuf_va = dev_info->mbuf_va;

	avp->mbuf_size = dev_info->mbuf_size;
	avp->max_rx_pkt_len = dev_info->max_rx_pkt_len;

	AVP_DBG("tx_phys:	   0x%016llx, tx_q addr:	  0x%p\n",
		(unsigned long long) dev_info->tx_phys, avp->tx_q[0]);
	AVP_DBG("rx_phys:	   0x%016llx, rx_q addr:	  0x%p\n",
		(unsigned long long) dev_info->rx_phys, avp->rx_q[0]);
	AVP_DBG("alloc_phys:   0x%016llx, alloc_q addr:	  0x%p\n",
		(unsigned long long) dev_info->alloc_phys, avp->alloc_q[0]);
	AVP_DBG("free_phys:	   0x%016llx, free_q addr:	  0x%p\n",
		(unsigned long long) dev_info->free_phys, avp->free_q[0]);
	AVP_DBG("req_phys:	   0x%016llx, req_q addr:	  0x%p\n",
		(unsigned long long) dev_info->req_phys, avp->req_q);
	AVP_DBG("resp_phys:	   0x%016llx, resp_q addr:	  0x%p\n",
		(unsigned long long) dev_info->resp_phys, avp->resp_q);
	AVP_DBG("sync_phys:	   0x%016llx, sync_kva:		  0x%p\n",
		(unsigned long long) dev_info->sync_phys, avp->sync_kva);
	AVP_DBG("sync_va:	   0x%p\n", dev_info->sync_va);
	AVP_DBG("mbuf_phys:	   0x%016llx, mbuf_kva:		  0x%p\n",
		(unsigned long long) dev_info->mbuf_phys, avp->mbuf_kva);
	AVP_DBG("mbuf_va:	   0x%p\n", dev_info->mbuf_va);
	AVP_DBG("mbuf_size:	   %u\n", avp->mbuf_size);
	AVP_DBG("num_tx_queues:%u\n", avp->num_tx_queues);
	AVP_DBG("num_rx_queues:%u\n", avp->num_rx_queues);

	rtnl_lock();
	/* inform the stack of our actual number of in use queues
	 *	note:  when setting rx/tx queue counts on netdevices that are already
	 *		   registered (i.e., live migration) we need to hold the RTNL lock.
	 */
	netif_set_real_num_tx_queues(avp->net_dev, avp->num_tx_queues);
	netif_set_real_num_rx_queues(avp->net_dev, avp->num_rx_queues);
	rtnl_unlock();

	return 0;
}

int
avp_dev_create(struct rte_avp_device_info *dev_info,
	       struct device *parent,
	       struct avp_dev **avpptr)
{
	struct net_device *net_dev = NULL;
	struct avp_dev *avp, *dev;
	const char *name;
	unsigned reused;
	int ret;

	if ((!avpptr) || (*avpptr == NULL)) {
		AVP_INFO("creating netdev for AVP device 0x%llx\n", dev_info->device_id);

		dev = avp_dev_find(dev_info->device_id);
		if (dev != NULL) {
			AVP_ERR("device 0x%llx already exists\n", dev_info->device_id);
			return -EEXIST;
		}

		ret = avp_dev_validate(dev_info);
		if (ret != 0) {
			AVP_ERR("device 0x%llx is not valid\n", dev_info->device_id);
			return ret;
		}

		name = (strlen(dev_info->ifname) ?
			dev_info->ifname : WRS_AVP_NETDEV_NAME_FORMAT);

		if (dev_info->mode == RTE_AVP_MODE_TRACE) {
			net_dev = alloc_netdev_mqs(sizeof(struct avp_dev),
						   name,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0))
						   NET_NAME_UNKNOWN,
#endif
						   avp_trace_init,
						   RTE_AVP_MAX_QUEUES,
						   RTE_AVP_MAX_QUEUES);
		} else {
			net_dev = alloc_netdev_mqs(sizeof(struct avp_dev),
						   name,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0))
						   NET_NAME_UNKNOWN,
#endif
						   avp_net_init,
						   RTE_AVP_MAX_QUEUES,
						   RTE_AVP_MAX_QUEUES);
		}

		if (net_dev == NULL) {
			AVP_ERR("error allocating device 0x%llx\n", dev_info->device_id);
			return -EBUSY;
		}

		if (parent) {
			/* set parent device link if attached to a PCI device */
			SET_NETDEV_DEV(net_dev, parent);
		}

		avp_set_ethtool_ops(net_dev);

		avp = netdev_priv(net_dev);
		avp->net_dev = net_dev;
		avp->status = WRS_AVP_DEV_STATUS_OK;
		avp->features = 0;
		avp->stats = alloc_percpu(struct avp_stats);
		if (avp->stats == NULL) {
			AVP_ERR("error allocating statistics for device 0x%llx\n",
					dev_info->device_id);
			avp_dev_free(avp);
			return -ENOMEM;
		}

		avp->host_features = dev_info->features;
		if (avp->host_features & RTE_AVP_FEATURE_VLAN_OFFLOAD) {
			/*
			 * We always enable VLAN offload during initial device setup.  In
			 * the future we may choose to support ethtool operations to
			 * enable/disable these features at runtime.
			 */
			avp->features |= RTE_AVP_FEATURE_VLAN_OFFLOAD;
			net_dev->features = NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX;
			net_dev->hw_features = net_dev->features;
		}

		avp->mode = dev_info->mode;
		if (avp->mode != RTE_AVP_MODE_TRACE) {
			memcpy(net_dev->dev_addr, dev_info->ethaddr, ETH_ALEN);
			if (!is_valid_ether_addr(net_dev->dev_addr)) {
				AVP_ERR("error validating MAC address: %pM\n", &net_dev->dev_addr);
				avp_dev_free(avp);
				return -EINVAL;
			}
		}
		reused = 0;
	} else {
		avp = (*avpptr);
		net_dev = avp->net_dev;
		reused = 1;
		AVP_INFO("attaching netdev %s to new v%u.%u.%u device 0x%llx\n",
			 net_dev->name,
			 RTE_AVP_GET_RELEASE_VERSION(dev_info->version),
			 RTE_AVP_GET_MAJOR_VERSION(dev_info->version),
			 RTE_AVP_GET_MINOR_VERSION(dev_info->version),
			 dev_info->device_id);

		if ((dev_info->features & avp->features) != avp->features) {
			AVP_ERR("netdev %s host features mismatched; 0x%08x, host=0x%08x\n",
					net_dev->name, avp->features, dev_info->features);
			/* This should not be possible; continue for now */
		}
	}

	/* the device id can change during migration */
	avp->device_id = dev_info->device_id;

	ret = avp_dev_configure(avp, dev_info);
	if (ret < 0) {
		AVP_ERR("failed to configure device 0x%llx\n", dev_info->device_id);
		goto cleanup;
	}

	if (!reused) {
		ret = register_netdev(net_dev);
		if (ret) {
			AVP_ERR("error %i registering device 0x%llx\n",
					ret, dev_info->device_id);
			ret = -ENODEV;
			goto cleanup;
		}

		netif_carrier_off(net_dev);

		AVP_INFO("registered netdev %s for v%u.%u.%u device 0x%llx\n",
				 net_dev->name,
			 RTE_AVP_GET_RELEASE_VERSION(dev_info->version),
			 RTE_AVP_GET_MAJOR_VERSION(dev_info->version),
			 RTE_AVP_GET_MINOR_VERSION(dev_info->version),
			 dev_info->device_id);

		/* add to device list (only once) */
		down_write(&avp_list_lock);
		list_add(&avp->list, &avp_list_head);
		up_write(&avp_list_lock);
	} else {
		if (net_dev->flags & IFF_UP) {
			netif_carrier_on(net_dev);
			netif_tx_start_all_queues(net_dev);
		}
	}

	/* add to polling list */
	down_write(&avp_poll_lock);
	list_add(&avp->poll, &avp_poll_head);
	up_write(&avp_poll_lock);

	if (avp->mode == RTE_AVP_MODE_TRACE) {
		/* assign device to thread for trace processing */
		avp_trace_dev_add(avp);
	} else {
		/* assign device to threads for receive processing */
		avp_thread_dev_add(avp);
	}

	avp->status = WRS_AVP_DEV_STATUS_OK;

	if (avpptr)
		(*avpptr) = avp;

	return 0;

cleanup:
	avp_dev_free(avp);
	return ret;
}

static int
_avp_dev_release(struct avp_dev *dev)
{
	/* Inform the host that we are shutting down. */
	avp_ctrl_shutdown(dev);

	if (dev->mode == RTE_AVP_MODE_TRACE) {
		/* unassign from receive threads */
		avp_trace_dev_remove(dev);
	} else {
		/* unassign from receive threads */
		avp_thread_dev_remove(dev);
	}

	/* delete and free netdev object */
	avp_dev_free(dev);

	/* remove the device from the response polling list */
	down_write(&avp_poll_lock);
	list_del(&dev->poll);
	up_write(&avp_poll_lock);

	/* remove from list */
	down_write(&avp_list_lock);
	list_del(&dev->list);
	up_write(&avp_list_lock);

	return 0;
}

int
avp_dev_release(uint64_t device_id)
{
	struct avp_dev *dev;

	dev = avp_dev_find(device_id);
	if (dev == NULL) {
		AVP_ERR("no device found for id 0x%llx\n", device_id);
		return -ENODEV;
	}

	return _avp_dev_release(dev);
}


/*****************************************************************************
 *
 * IOCTL callback functions
 *
 *****************************************************************************/

static int
avp_ioctl_create(unsigned int ioctl_num, unsigned long ioctl_param)
{
	struct rte_avp_device_info dev_info;
	int ret;

	/* Check the buffer size, to avoid warning */
	if (_IOC_SIZE(ioctl_num) > sizeof(dev_info))
		return -EINVAL;

	/* get device info from user space */
	ret = copy_from_user(&dev_info, (void *)ioctl_param, sizeof(dev_info));
	if (ret) {
		AVP_ERR("failed to copy from user space\n");
		return -EIO;
	}

	/* add a new device */
	return avp_dev_create(&dev_info, NULL, NULL);
}

static int
avp_ioctl_release(unsigned int ioctl_num, unsigned long ioctl_param)
{
	struct rte_avp_device_info dev_info;
	struct avp_dev *dev;
	int ret = -EINVAL;

	if (_IOC_SIZE(ioctl_num) > sizeof(dev_info))
		return -EINVAL;

	/* get device id from user space */
	ret = copy_from_user(&dev_info, (void *)ioctl_param, sizeof(dev_info));
	if (ret) {
		AVP_ERR("failed to copy from user space\n");
		return -EIO;
	}

	/* lookup device */
	dev = avp_dev_find(dev_info.device_id);
	if (dev == NULL) {
		AVP_ERR("device 0x%llx not found\n", dev_info.device_id);
		return -ENODEV;
	}

	/* delete device */
	ret = _avp_dev_release(dev);

	return ret;
}

static int
avp_ioctl_query(unsigned int ioctl_num, unsigned long ioctl_param)
{
	struct rte_avp_device_config dev_config;
	struct avp_dev *dev;
	int ret = -EINVAL;

	if (_IOC_SIZE(ioctl_num) < sizeof(dev_config))
		return -EINVAL;

	/* get device id from user space */
	ret = copy_from_user(&dev_config, (void *)ioctl_param, sizeof(dev_config));
	if (ret) {
		AVP_ERR("failed to copy from user space\n");
		return -EIO;
	}

	/* lookup device */
	dev = avp_dev_find(dev_config.device_id);
	if (dev == NULL) {
		AVP_ERR("device 0x%llx not found\n", dev_config.device_id);
		return -ENODEV;
	}

	/* update device info */
	memset(&dev_config, 0, sizeof(dev_config));
	dev_config.device_id = dev->device_id;
	dev_config.driver_type = RTE_AVP_DRIVER_TYPE_KERNEL;
	dev_config.driver_version = WRS_AVP_KERNEL_DRIVER_VERSION;
	dev_config.features = dev->features;
	dev_config.num_tx_queues = dev->num_tx_queues;
	dev_config.num_rx_queues = dev->num_rx_queues;
	dev_config.if_up = !!(dev->net_dev->flags & IFF_UP);

	/* return to user space */
	ret = copy_to_user((void *)ioctl_param, &dev_config, sizeof(dev_config));
	if (ret) {
		AVP_ERR("failed to copy to user space\n");
		return -EIO;
	}

	return 0;
}

static int
avp_ioctl(struct inode *inode,
	  unsigned int ioctl_num,
	  unsigned long ioctl_param)
{
	int ret = -EINVAL;

	AVP_DBG("IOCTL num=0x%0x param=0x%0lx\n", ioctl_num, ioctl_param);

	switch (_IOC_NR(ioctl_num)) {
	case _IOC_NR(RTE_AVP_IOCTL_CREATE):
		ret = avp_ioctl_create(ioctl_num, ioctl_param);
		break;
	case _IOC_NR(RTE_AVP_IOCTL_RELEASE):
		ret = avp_ioctl_release(ioctl_num, ioctl_param);
		break;
	case _IOC_NR(RTE_AVP_IOCTL_QUERY):
		ret = avp_ioctl_query(ioctl_num, ioctl_param);
		break;
	default:
		AVP_ERR("unknown IOCTL number %u\n", _IOC_NR(ioctl_num));
		break;
	}

	return ret;
}

static int
avp_compat_ioctl(struct inode *inode,
		 unsigned int ioctl_num,
		 unsigned long ioctl_param)
{
	/* 32 bit application on 64 bit kernel */
	AVP_ERR("IOCTL 0x%0x not implemented\n", ioctl_num);

	return -EINVAL;
}

/*****************************************************************************
 *
 * Character device callback functions
 *
 *****************************************************************************/

static int
avp_file_open(struct inode *inode, struct file *file)
{
	/* avp device can be opened by one user only, test and set bit */
	if (test_and_set_bit(WRS_AVP_DEV_IN_USE_BIT_NUM, &device_in_use))
		return -EBUSY;

	AVP_DBG("/dev/%s opened\n", RTE_AVP_DEVICE);

	return 0;
}

static int
avp_file_release(struct inode *inode, struct file *file)
{
	struct avp_dev *dev, *n;

	down_write(&avp_poll_lock);
	list_for_each_entry_safe(dev, n, &avp_poll_head, poll) {
		if (dev->mode != RTE_AVP_MODE_GUEST) {
			/* stop polling for responses on all local/host devices */
			list_del(&dev->poll);
		}
	}
	up_write(&avp_poll_lock);

	down_write(&avp_list_lock);
	list_for_each_entry_safe(dev, n, &avp_list_head, list) {
		if (dev->mode != RTE_AVP_MODE_GUEST) {
			/* destroy all local/host devices */
			avp_thread_dev_remove(dev);
			avp_dev_free(dev);
			list_del(&dev->list);
		}
	}
	up_write(&avp_list_lock);

	/* Clear the bit of device in use */
	clear_bit(WRS_AVP_DEV_IN_USE_BIT_NUM, &device_in_use);

	AVP_DBG("/dev/%s closed\n", RTE_AVP_DEVICE);

	return 0;
}

/*****************************************************************************
 *
 * Module init/exit functions
 *
 *****************************************************************************/

const struct file_operations avp_fops = {
	.owner = THIS_MODULE,
	.open = avp_file_open,
	.release = avp_file_release,
	.unlocked_ioctl = (void *)avp_ioctl,
	.compat_ioctl = (void *)avp_compat_ioctl,
};

static struct miscdevice avp_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = RTE_AVP_DEVICE,
	.fops = &avp_fops,
};


static int __init
avp_parse_kthread_cpulist(void)
{
	int ret;

	if (!kthread_cpulist) {
		cpumask_copy(&avp_cpumask, cpu_possible_mask);
		return 0;
	}

	ret = cpulist_parse(kthread_cpulist, &avp_cpumask);
	if (ret != 0) {
		AVP_ERR("Invalid CPU list parameter value: \"%s\"\n", kthread_cpulist);
		return ret;
	}

	if (!cpumask_intersects(&avp_cpumask, cpu_online_mask)) {
		AVP_ERR("CPU mask does not specify any online CPUs\n");
		return -EINVAL;
	}

	cpumask_and(&avp_cpumask, &avp_cpumask, cpu_possible_mask);

	return 0;
}


static int __init
avp_validate_kthread_sched(void)
{
	switch (kthread_policy) {
	case SCHED_NORMAL:
		AVP_INFO("Setting AVP kthread priority to zero for default policy\n");
		kthread_priority = 0;
		break;

	case SCHED_FIFO:
	case SCHED_RR:
		if ((kthread_priority < 1) || (kthread_priority > 99)) {
			AVP_ERR("Invalid AVP kthread RT priority: %d\n", kthread_priority);
			return -EINVAL;
		}
		break;

	default:
		AVP_ERR("Unsupported scheduler policy: %d\n", kthread_policy);
		return -EINVAL;
	}

	return 0;
}


static int __init
avp_init(void)
{
	uint32_t version = WRS_AVP_KERNEL_DRIVER_VERSION;
	int ret;

	AVP_PRINT("######## DPDK AVP module loading v%u.%u.%u ########\n",
		  RTE_AVP_GET_RELEASE_VERSION(version),
		  RTE_AVP_GET_MAJOR_VERSION(version),
		  RTE_AVP_GET_MINOR_VERSION(version));

	if (avp_parse_kthread_cpulist() < 0) {
		AVP_ERR("Invalid parameter for kthread_cpu list\n");
		return -EINVAL;
	}

	if (avp_validate_kthread_sched() < 0) {
		AVP_ERR("Invalid parameter for kthread_policy\n");
		return -EINVAL;
	}

	if (misc_register(&avp_misc) != 0) {
		AVP_ERR("Misc registration failed\n");
		return -EPERM;
	}

	/* Clear the bit of device in use */
	clear_bit(WRS_AVP_DEV_IN_USE_BIT_NUM, &device_in_use);

	ret = avp_thread_init();
	if (ret != 0) {
		AVP_ERR("Failed to initialize threads\n");
		return ret;
	}

#ifdef CONFIG_HOTPLUG_CPU
	register_hotcpu_notifier(&avp_cpu_hotplug_notifier);
#endif

	/* Setup guest PCI driver */
	ret = avp_pci_init();
	if (ret != 0) {
		AVP_ERR("Failed to register PCI driver\n");
		return ret;
	}

	AVP_PRINT("######## DPDK AVP module loaded	########\n");

	return 0;
}

static void __exit
avp_exit(void)
{
	AVP_PRINT("####### DPDK AVP module unloading  #######\n");

#ifdef CONFIG_HOTPLUG_CPU
	unregister_hotcpu_notifier(&avp_cpu_hotplug_notifier);
#endif

	avp_pci_exit();
	misc_deregister(&avp_misc);
	AVP_PRINT("####### DPDK AVP module unloaded	 #######\n");
}

module_init(avp_init);
module_exit(avp_exit);

module_param(kthread_cpulist, charp, S_IRUGO);
MODULE_PARM_DESC(kthread_cpulist, "Kernel thread cpu list (default all)\n");
module_param(kthread_policy, int, S_IRUGO);
MODULE_PARM_DESC(kthread_policy, "Kernel thread scheduling policy\n");
module_param(kthread_priority, int, S_IRUGO);
MODULE_PARM_DESC(kthread_priority, "Kernel thread scheduling priority\n");

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
 */

/*
 * This code is inspired from the book "Linux Device Drivers" by
 * Alessandro Rubini and Jonathan Corbet, published by O'Reilly & Associates
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h> /* eth_type_trans */
#include <linux/skbuff.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/percpu.h>
#include <linux/if_vlan.h>
#include <net/arp.h>

#include <exec-env/wrs_avp_common.h>
#include <exec-env/wrs_avp_fifo.h>
#include "avp_dev.h"
#include "avp_ctrl.h"

/* Defines number of jiffies used to configure the skbuff watchdog */
#define WRS_AVP_WD_TIMEOUT 5

/* Defines the maximum number of packets to be received in one try */
#define WRS_AVP_MBUF_BURST_SZ 32

int avp_net_rx(struct avp_dev *avp, unsigned qnum);

static int
avp_net_open(struct net_device *dev)
{
	struct avp_dev *avp = netdev_priv(dev);

	if (avp->status == WRS_AVP_DEV_STATUS_DETACHED) {
		return -EBUSY;
	}

	netif_carrier_on(dev);
	netif_tx_start_all_queues(dev);

	return avp_ctrl_set_link_state(avp, 1);
}

static int
avp_net_release(struct net_device *dev)
{
	struct avp_dev *avp = netdev_priv(dev);

	if (avp->status == WRS_AVP_DEV_STATUS_DETACHED) {
		return -EBUSY;
	}

	netif_carrier_off(dev);
	netif_tx_stop_all_queues(dev);

	return avp_ctrl_set_link_state(avp, 0);
}

static int
avp_net_config(struct net_device *dev, struct ifmap *map)
{
	if (dev->flags & IFF_UP)
		return -EBUSY;

	return 0;
}

static inline void *
avp_net_translate_buffer(struct avp_dev *avp, void *addr)
{
	struct avp_mempool_info *pool;
	unsigned i;

	for (i = 0; i < WRS_AVP_MAX_MEMPOOLS; i++) {
		pool = &avp->pool[i];
		if ((pool != NULL) && (addr >= pool->va) && (addr < (pool->va + pool->length))) {
			return addr - pool->va + pool->kva;
		}
	}

	BUG_ON(0);
	return NULL;
}

/*
 * Copies data from a set of mbufs to an SKB.  This function assumes that the
 * SKB has been allocated with a sufficient amount of space to contain the
 * entire packet.
 */
static inline int
avp_net_copy_from_mbufs(struct avp_dev *avp,
						struct wrs_avp_mbuf *pkt_kva,
						struct sk_buff *skb)
{
	struct wrs_avp_mbuf *next_va;
	size_t offset = 0;
	void *data_kva;
	int ret;

	/* setup the SKB to the proper length */
	skb_put(skb, pkt_kva->pkt_len);

	if (pkt_kva->ol_flags & WRS_AVP_RX_VLAN_PKT) {
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0) )
		__vlan_hwaccel_put_tag(skb, pkt_kva->vlan_tci);
#else
		__vlan_hwaccel_put_tag(skb, ntohs(ETH_P_8021Q), pkt_kva->vlan_tci);
#endif
	}

	do {
		/* translate the host buffer to guest addressing */
		data_kva = avp_net_translate_buffer(avp, (void*)pkt_kva->data);

		ret = skb_store_bits(skb, offset, data_kva, pkt_kva->data_len);
		if (ret) {
			AVP_ERR_RATELIMIT("skb->len=%u, offset=%zu, data_len=%u\n",
                              skb->len, offset, pkt_kva->data_len);
			return ret;
		}

		/* advance to the next segment */
		offset += pkt_kva->data_len;
		next_va = pkt_kva->next;
		if (next_va) {
			pkt_kva = avp_net_translate_buffer(avp, (void*)next_va);
		}

	} while (next_va);

	return 0;
}

int
avp_net_rx(struct avp_dev *avp, unsigned qnum)
{
	unsigned ret;
	unsigned i, num, num_rq, num_fq;
	struct wrs_avp_mbuf *avp_bufs[WRS_AVP_MBUF_BURST_SZ];
	struct wrs_avp_mbuf *pkt_buf;
	uint32_t pkt_len;
	struct sk_buff *skb;
	struct net_device *dev = avp->net_dev;
	struct avp_stats *stats = this_cpu_ptr(avp->stats);
	struct wrs_avp_fifo *rx_q = avp->rx_q[qnum];
	struct wrs_avp_fifo *free_q = avp->free_q[qnum];

	/* Get the number of entries in rx_q */
	num_rq = avp_fifo_count(rx_q);

	/* Get the number of free entries in free_q */
	num_fq = avp_fifo_free_count(free_q);
	if (num_fq == 0)
		stats->rx_fifo_errors++;

	/* Calculate the number of entries to dequeue in rx_q */
	num = min(num_rq, num_fq);
	num = min(num, (unsigned)WRS_AVP_MBUF_BURST_SZ);

	/* Return if no entry in rx_q and no free entry in free_q */
	if (num == 0)
		return 0;

	/* Burst dequeue from rx_q */
	ret = avp_fifo_get(rx_q, (void **)avp_bufs, num);
	if (ret == 0)
		return 0; /* Failing should not happen */

	/* Transfer received packets to netif */
	for (i = 0; i < num; i++) {

		/* prefetch next entry while process current one */
		if (i < num-1) {
			pkt_buf = avp_net_translate_buffer(avp, (void*)avp_bufs[i+1]);
			prefetch(pkt_buf);
		}

		/* peek in to the first mbuf to determine total length */
		pkt_buf = avp_net_translate_buffer(avp, (void*)avp_bufs[i]);
		pkt_len = pkt_buf->pkt_len;

		/*
		 * Allocate an skb of the full packet length to avoid having to deal
		 * with paged skb handling (until we determine if there is a
		 * performance impact of doing it this way)
		 */
		skb = __dev_alloc_skb(pkt_len + NET_IP_ALIGN, GFP_ATOMIC);
		if (unlikely(!skb)) {
			/* Update statistics */
			stats->rx_dropped += (num-i);
			break;
		}

		/* Align IP on 16B boundary */
		skb_reserve(skb, NET_IP_ALIGN);

		/* Copy data from mbufs */
		ret = avp_net_copy_from_mbufs(avp, pkt_buf, skb);
		if (ret != 0) {
			dev_kfree_skb(skb);
			stats->rx_dropped += (num-1);
			break;
		}

		skb->dev = dev;
		skb->protocol = eth_type_trans(skb, dev);
		skb->ip_summed = CHECKSUM_NONE;

		skb_record_rx_queue(skb, qnum);

		/* Call netif interface */
		netif_rx(skb);

		/* Update statistics */
		u64_stats_update_begin(&stats->rx_syncp);
		stats->rx_bytes += pkt_len;
		stats->rx_packets++;
		u64_stats_update_end(&stats->rx_syncp);
	}

	/* Burst enqueue mbufs into free_q */
	ret = avp_fifo_put(free_q, (void **)avp_bufs, num);
	if (unlikely(ret != num))
		/* Failing should not happen */
		AVP_ERR_RATELIMIT("Fail to enqueue entries into free_q\n");

	return num;
}

/*
 * Copies data from an SKB to a set of mbufs.  This function assumes that
 * there are sufficient mbufs to copy the entire SKB data area as well as any
 * SKB fragments.
 */
static inline int
avp_net_copy_to_mbufs(struct avp_dev *avp,
					  struct sk_buff *skb,
					  struct wrs_avp_mbuf **mbufs,
					  unsigned count)
{
	struct wrs_avp_mbuf *previous_kva = NULL;
	struct wrs_avp_mbuf *first_kva = NULL;
	struct wrs_avp_mbuf *pkt_kva;
	void *first_data_kva = NULL;
	struct wrs_avp_mbuf *buf;
	unsigned copy_length;
	unsigned offset = 0;
	void *data_kva;
	unsigned len;
	unsigned i;
	int ret;

	for (i = 0; i < count; i++)
	{
		buf = mbufs[i];

		/* translate the host buffer to guest addressing */
		pkt_kva = avp_net_translate_buffer(avp, (void *)buf);
		data_kva = avp_net_translate_buffer(avp, pkt_kva->data);

		/* setup the chain of mbufs */
		if (previous_kva) {
			previous_kva->next = buf;
		} else {
			first_kva = pkt_kva;
			first_data_kva = data_kva;
		}
		previous_kva = pkt_kva;

		/* copy the data from the SKB to the mbuf */
		copy_length = min_t(unsigned, skb->len - offset, avp->mbuf_size);
		ret = skb_copy_bits(skb, offset, data_kva, copy_length);
        if (ret) {
            AVP_ERR_RATELIMIT("skb->len=%u, offset=%u, copy=%u, i=%u/%u\n",
                              skb->len, offset, copy_length, i, count);
            return ret;
        }

		offset += copy_length;
		pkt_kva->data_len = copy_length;
		pkt_kva->next = NULL;
	}

	BUG_ON(skb->len != offset);

	len = skb->len;
	if (unlikely(len < ETH_ZLEN)) {
		memset(first_data_kva + len, 0, ETH_ZLEN - len);
		len = ETH_ZLEN;
		first_kva->data_len = len;
	}

	first_kva->nb_segs = count;
	first_kva->pkt_len = len;

#ifdef skb_vlan_tag_present
    if (skb_vlan_tag_present(skb)) {
#else
    if (vlan_tx_tag_present(skb)) {
#endif
        first_kva->ol_flags |= WRS_AVP_TX_VLAN_PKT;
#ifdef skb_vlan_tag_get
        first_kva->vlan_tci = skb_vlan_tag_get(skb);
#else
        first_kva->vlan_tci = vlan_tx_tag_get(skb);
#endif
    } else {
		first_kva->ol_flags = 0;
		first_kva->vlan_tci = 0;
	}

	return 0;
}

static int
avp_net_tx(struct sk_buff *skb, struct net_device *dev)
{
	unsigned ret;
	unsigned i, num, num_aq;
	struct avp_dev *avp = netdev_priv(dev);
	struct avp_stats *stats = this_cpu_ptr(avp->stats);
	struct avp_mbuf_cache *mbuf_cache;
	struct wrs_avp_mbuf *pkt_kva = NULL;
	struct wrs_avp_mbuf *pkt_va = NULL;
	struct wrs_avp_fifo *tx_q;
	struct wrs_avp_fifo *alloc_q;
	unsigned count;
	unsigned qnum;

	dev->trans_start = jiffies; /* save the timestamp */

    /* Determine how many mbufs are required to send this packet */
	count = (skb->len + avp->mbuf_size - 1) / avp->mbuf_size;
    if (unlikely(count == 0)) {
        AVP_ERR_RATELIMIT("dropping zero length packet on %s\n", dev->name);
        goto drop;
    }
    else if (unlikely(count > WRS_AVP_MAX_MBUF_SEGMENTS)) {
        AVP_ERR_RATELIMIT("dropping oversized packet on %s\n", dev->name);
        goto drop;
    }

    qnum = skb_get_queue_mapping(skb);
	BUG_ON(qnum > avp->num_tx_queues);

	tx_q = avp->tx_q[qnum];
	alloc_q = avp->alloc_q[qnum];
	mbuf_cache = &avp->mbuf_cache[qnum];

	/**
	 * Check if it has at least one free entry in tx_q and
	 * sufficient entries in the alloc_q.
	 */
	if (avp_fifo_free_count(tx_q) == 0) {
		goto drop;
	}

	if (mbuf_cache->count < count) {
		/* refill the cache */
		num_aq = avp_fifo_count(alloc_q);
		if (num_aq < (count - mbuf_cache->count)) {
			stats->tx_fifo_errors++;
			goto drop;
		}

		/* cap the number of buffers to be queried to the max cache size */
		num = min(num_aq, (unsigned)WRS_AVP_QUEUE_MBUF_CACHE_SIZE - mbuf_cache->count);

		/* dequeue a mbufs from alloc_q */
		ret = avp_fifo_get(alloc_q, (void **)&mbuf_cache->mbufs[mbuf_cache->count], num);
		if (ret != num) {
			/* Failing should not happen */
			AVP_ERR_RATELIMIT("Fail to enqueue mbuf into tx_q\n");
			goto drop;
		}

		for (i = 0; i < num; i++) {
			pkt_va = mbuf_cache->mbufs[mbuf_cache->count + i];
			pkt_kva = avp_net_translate_buffer(avp,(void*)pkt_va);
			prefetch(pkt_kva);
		}
		mbuf_cache->count += num;
	}

	/* copy the skb to one of more mbufs */
	ret = avp_net_copy_to_mbufs(avp,
								skb,
								&mbuf_cache->mbufs[mbuf_cache->count - count],
								count);
	if (unlikely(ret != 0)) {
		goto drop;
	}
	mbuf_cache->count -= count;

	/* enqueue mbuf into tx_q */
	ret = avp_fifo_put(tx_q, (void **)&mbuf_cache->mbufs[mbuf_cache->count], 1);
	if (unlikely(ret != 1)) {
		/* Failing should not happen */
		AVP_ERR_RATELIMIT("Fail to enqueue mbuf into tx_q\n");
		goto drop;
	}

	/* update statistics */
	u64_stats_update_begin(&stats->tx_syncp);
	stats->tx_bytes += skb->len;
	stats->tx_packets++;
	u64_stats_update_end(&stats->tx_syncp);

	dev_kfree_skb(skb);

	return NETDEV_TX_OK;

drop:
	/* Free skb and update statistics */
	dev_kfree_skb(skb);
	stats->tx_dropped++;

	return NETDEV_TX_OK;
}


static int
avp_trace_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct avp_dev *avp = netdev_priv(dev);
	struct avp_stats *stats = this_cpu_ptr(avp->stats);

	/* trace devices do not support transmit operations */
	dev_kfree_skb(skb);
	stats->tx_dropped++;

	return NETDEV_TX_OK;
}

static rx_handler_result_t
avp_trace_rx(struct sk_buff **pskb)
{
	struct sk_buff *skb = *pskb;
	struct net_device *dev = skb->dev;
	struct avp_dev *avp = netdev_priv(dev);
	struct avp_stats *stats = this_cpu_ptr(avp->stats);

	/* update statistics */
	u64_stats_update_begin(&stats->rx_syncp);
	stats->rx_bytes += skb->len;
	stats->rx_packets++;
	u64_stats_update_end(&stats->rx_syncp);

	/* trace devices do not process received frames */
	dev_kfree_skb(skb);

	return RX_HANDLER_CONSUMED;
}


#ifdef WRS_AVP_TX_TIMEOUTS
static void
avp_net_tx_timeout (struct net_device *dev)
{
	struct avp_dev *avp = netdev_priv(dev);
	struct avp_stats *stats = this_cpu_ptr(avp->stats);

	AVP_DBG("transmit timeout at %ld, latency %ld\n", jiffies,
			jiffies - dev->trans_start);

	stats->tx_errors++;
	netif_wake_queue(dev);
	return;
}
#endif

static int
avp_net_change_mtu(struct net_device *dev, int new_mtu)
{
	struct avp_dev *avp = netdev_priv(dev);
	int max_frame;

	max_frame = new_mtu + ETH_HLEN + ETH_FCS_LEN;
	if (max_frame > avp->max_rx_pkt_len) {
		AVP_ERR("mtu %u + %u exceeds device maximum value of %u\n",
				new_mtu,
				(ETH_HLEN + ETH_FCS_LEN),
				avp->max_rx_pkt_len);
		return -EINVAL;
	}

	AVP_DBG("%s updating mtu to %u\n", new_mtu);

    dev->mtu = new_mtu;

	return 0;
}

static struct rtnl_link_stats64 *
avp_net_stats(struct net_device *dev, struct rtnl_link_stats64 *tot)
{
	struct avp_dev *avp = netdev_priv(dev);
	int cpu;
	unsigned int start;

	for_each_possible_cpu(cpu) {
		struct avp_stats *stats = per_cpu_ptr(avp->stats, cpu);
		u64 rx_packets, tx_packets, rx_bytes, tx_bytes;
		u64 rx_errors, tx_errors, rx_dropped, tx_dropped;
		u64 rx_fifo_errors, tx_fifo_errors;

		do {
			start = u64_stats_fetch_begin(&stats->tx_syncp);
			tx_packets = stats->tx_packets;
			tx_bytes = stats->tx_bytes;
			tx_errors = stats->tx_errors;
			tx_dropped = stats->tx_dropped;
			tx_fifo_errors = stats->tx_fifo_errors;
		} while (u64_stats_fetch_retry(&stats->tx_syncp, start));

		do {
			start = u64_stats_fetch_begin(&stats->rx_syncp);
			rx_packets = stats->rx_packets;
			rx_bytes = stats->rx_bytes;
			rx_errors = stats->rx_errors;
			rx_dropped = stats->rx_dropped;
			rx_fifo_errors = stats->rx_fifo_errors;
		} while (u64_stats_fetch_retry(&stats->rx_syncp, start));

		tot->rx_packets += rx_packets;
		tot->tx_packets += tx_packets;
		tot->rx_bytes += rx_bytes;
		tot->tx_bytes += tx_bytes;
		tot->rx_errors += rx_errors;
		tot->tx_errors += tx_errors;
		tot->rx_dropped += rx_dropped;
		tot->tx_dropped += tx_dropped;
		tot->rx_fifo_errors += rx_fifo_errors;
		tot->tx_fifo_errors += tx_fifo_errors;
	}

	return tot;
}

static int
avp_net_header(struct sk_buff *skb, struct net_device *dev,
		unsigned short type, const void *daddr,
		const void *saddr, unsigned int len)
{
	struct ethhdr *eth = (struct ethhdr *) skb_push(skb, ETH_HLEN);

	memcpy(eth->h_source, saddr ? saddr : dev->dev_addr, dev->addr_len);
	memcpy(eth->h_dest, daddr ? daddr : dev->dev_addr, dev->addr_len);
	eth->h_proto = htons(type);

	return (dev->hard_header_len);
}


#if ( LINUX_VERSION_CODE < KERNEL_VERSION(4,1,0) )
static int
avp_net_rebuild_header(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	struct ethhdr *eth = (struct ethhdr *) skb->data;

	memcpy(eth->h_source, dev->dev_addr, dev->addr_len);
	memcpy(eth->h_dest, dev->dev_addr, dev->addr_len);

	return 0;
}
#endif


static const struct header_ops avp_net_header_ops = {
	.create	 = avp_net_header,
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(4,1,0) )
    .rebuild = avp_net_rebuild_header,
#endif
	.cache	 = NULL,  /* disable caching */
};

static const struct net_device_ops avp_net_netdev_ops = {
	.ndo_open = avp_net_open,
	.ndo_stop = avp_net_release,
	.ndo_set_config = avp_net_config,
	.ndo_start_xmit = avp_net_tx,
	.ndo_change_mtu = avp_net_change_mtu,
	.ndo_get_stats64 = avp_net_stats,
#ifdef WRS_AVP_TX_TIMEOUTS
	.ndo_tx_timeout = avp_net_tx_timeout,
#endif
};

void
avp_net_init(struct net_device *dev)
{
	struct avp_dev *avp = netdev_priv(dev);

	AVP_DBG("avp_net_init\n");

	init_waitqueue_head(&avp->wq);
	mutex_init(&avp->sync_lock);

	ether_setup(dev); /* assign some of the fields */
	dev->netdev_ops = &avp_net_netdev_ops;
	dev->header_ops = &avp_net_header_ops;
#ifdef WRS_AVP_TX_TIMEOUTS
	dev->watchdog_timeo = WRS_AVP_WD_TIMEOUT;
#endif
}


static const struct net_device_ops avp_trace_netdev_ops = {
	.ndo_open = avp_net_open,
	.ndo_stop = avp_net_release,
	.ndo_set_config = avp_net_config,
	.ndo_start_xmit = avp_trace_tx,
	.ndo_get_stats64 = avp_net_stats,
};

void
avp_trace_init(struct net_device *dev)
{
	struct avp_dev *avp = netdev_priv(dev);
	int err;

	AVP_DBG("avp_trace_init\n");

	init_waitqueue_head(&avp->wq);
	mutex_init(&avp->sync_lock);

	ether_setup(dev); /* assign some of the fields */
	dev->netdev_ops = &avp_trace_netdev_ops;
	dev->header_ops = &avp_net_header_ops;

	rtnl_lock();
	err = netdev_rx_handler_register(dev, avp_trace_rx, avp);
	rtnl_unlock();

	if (err)
		AVP_ERR("Failed to register trace rx_handler\n");
}

/*
 * Copyright (c) 2013-2015, Wind River Systems, Inc.
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

#ifndef _WRS_MBUF_H_
#define _WRS_MBUF_H_

#include <stdint.h>

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_version.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifndef RTE_VERSION_NUM
/* only available in >= 1.7.0 */
#define RTE_VERSION_NUM(a,b,c,d) ((a) << 24 | (b) << 16 | (c) << 8 | (d))
#endif


/**
 * Bulk allocate new mbufs (type is pkt) from a mempool.
 *
 * This new mbufs contain one segment, which has a length of 0. The pointer
 * to data is initialized to have some bytes of headroom in the buffer
 * (if buffer size allows).
 *
 * @param mp
 *   The mempool from which the mbuf is allocated.
 * @param mbufs
 *   A pointer to a table of struct rte_mbf * pointers that will be filled.
 * @param n
 *   The number of mbufs to get from the mempool.
 * @return
 *   - 0: Success; mbufs allocated
 *   - -ENOENT: Not enough entries in the mempool; no mbuf is retrieved.
 */
static inline int wrs_pktmbuf_alloc_bulk(struct rte_mempool *mp,
                                         struct rte_mbuf **mbufs,
                                         unsigned n)
{
    struct rte_mbuf *m;
    unsigned i;
    int ret;

    ret = rte_mempool_get_bulk(mp, (void**)mbufs, n);
    if (likely(ret == 0))
    {
#if RTE_VERSION >= RTE_VERSION_NUM(1,8,0,0)
        for (i = 0; i < n; i++)
        {
            m = mbufs[i];
            rte_prefetch0(m->cacheline0);
            rte_prefetch0(m->cacheline1);
        }
#endif
        for (i = 0; i < n; i++)
        {
            m = mbufs[i];
            RTE_MBUF_ASSERT(rte_mbuf_refcnt_read(m) == 0);
            rte_mbuf_refcnt_set(m, 1);
            rte_pktmbuf_reset(m);
        }
    }

	return (ret);
}

#ifdef WRS_LIBWRS_MBUF_BULK_FREE
/**
 * Bulk free the packet mbufs back into their original mempool.
 *
 * Free an mbuf, and all its segments in case of chained buffers. Each
 * segment is added back into its original mempool.
 *
 * @param mbufs
 *   A pointer to a table of packet mbufs to be freed.
 * @param n
 *   The number of mbufs to be freed.
 */
static inline void wrs_pktmbuf_free_bulk(struct rte_mbuf **mbufs, unsigned n)
{
    struct rte_mempool *mp = NULL;
    struct rte_mbuf *m = NULL;
	struct rte_mbuf *m_next;
    struct rte_mbuf *m_seg;
    unsigned i;
    unsigned j;

    for (i = 0; i < n; i++) {
        m = mbufs[i];
        __rte_mbuf_sanity_check(m, 1);

        /* free chained segments individually (if required) */
        if (unlikely(m->next != NULL)) {
            m_seg = m->next;
            while (m_seg != NULL) {
                m_next = m_seg->next;
                rte_pktmbuf_free_seg(m_seg);
                m_seg = m_next;
            }
        }

        if (likely((m = __rte_pktmbuf_prefree_seg(m)) == NULL)) {
            /* mbuf still referenced and can't be freed */
            mp = NULL;
            break; /* abort bulk operation */
        }

        /* Determine the memory pool of mbufs */
        if (mp == NULL) {
            mp = m->pool;
        }
        else if (m->pool != mp) {
            /* memory pool is not homogenous across mbufs */
            mp = NULL;
            break; /* abort bulk operation */
        }

        RTE_MBUF_ASSERT(rte_mbuf_refcnt_read(m) == 0);
    }

    if (likely(mp != NULL)) {
        rte_mempool_put_bulk(mp, (void*)mbufs, n);
    } else {
        /* free the current buffer only if it is not referenced */
        if (m != NULL) {
            __rte_mbuf_raw_free(m);
        }
        /* __rte_pktmbuf_prefree_seg has already been invoked, just do raw */
        for (j = 0; j < i; j++) {
            __rte_mbuf_raw_free(mbufs[j]);
        }
        /* invoke full free of remaining buffers */
        for (j = i+1; j < n; j++) {
            rte_pktmbuf_free(mbufs[j]);
        }
    }
}
#endif


#ifdef WRS_LIBWRS_MBUF_COPY
/**
 * Creates a copy of a packet buffer.
 *
 * @param m
 * A pointer to the packet buffer to be cloned.
 * @param pool
 * The mempool from which the mbufs are allocated.
 * @return
 * A pointer to the new packet buffer if successful or NULL if an error
 * occurred.
 */
static inline struct rte_mbuf *
wrs_pktmbuf_copy(struct rte_mbuf *m, struct rte_mempool *pool)
{
	struct rte_mbuf *mc, *mp, **prev;

	if (unlikely((mc = rte_pktmbuf_alloc(pool)) == NULL))
    {
		return NULL;
    }

	mp = mc;
	prev = &mp->next;

	mp->nb_segs = m->nb_segs;
    mp->port = m->port;
    mp->ol_flags = m->ol_flags;
    mp->packet_type = m->packet_type;
	mp->pkt_len = m->pkt_len;
    mp->vlan_tci = m->vlan_tci;
    mp->hash = m->hash;
    mp->tx_offload = m->tx_offload;

	do
    {
        mp->data_off = m->data_off;
        mp->data_len = m->data_len;

        /*
         * copy packet headroom (which includes the context data) and
         * packet data.
         */
        rte_memcpy(mp->buf_addr, m->buf_addr, m->data_off + m->data_len);

		*prev = mp;
		prev = &mp->next;
	} while ((m = m->next) != NULL && (mp = rte_pktmbuf_alloc(pool)) != NULL);

	*prev = NULL;

	/* Allocation of new segment failed */
	if (unlikely(mp == NULL))
    {
		rte_pktmbuf_free(mc);
		return NULL;
	}

	__rte_mbuf_sanity_check(mc, 1);
	return mc;
}
#endif


#ifdef __cplusplus
}
#endif

#endif /* _WRS_MBUF_H_ */

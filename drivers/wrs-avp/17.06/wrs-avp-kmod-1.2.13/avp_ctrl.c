/*-
 * GPL LICENSE SUMMARY
 *
 *   Copyright(c) 2010-2013 Intel Corporation. All rights reserved.
 *   Copyright(c) 2014 Wind River Systems, Inc. All rights reserved.
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

#include <linux/device.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/percpu.h>
#include <linux/jiffies.h>

#include "avp_ctrl.h"

#include <rte_avp_fifo.h>

/* Control request response timeout in milliseconds */
#define WRS_AVP_CTRL_RESPONSE_TIMEOUT 500

int
avp_ctrl_set_link_state(struct avp_dev *avp, unsigned state)
{
	struct rte_avp_request req;
	int ret;

	memset(&req, 0, sizeof(req));
	req.req_id = RTE_AVP_REQ_CFG_NETWORK_IF;
	req.if_up = state;
	ret = avp_ctrl_process_request(avp, &req);

	return (ret == 0 ? req.result : ret);
}

int
avp_ctrl_set_mtu(struct avp_dev *avp, int new_mtu)
{
	struct rte_avp_request req;
	int ret;

	memset(&req, 0, sizeof(req));
	req.req_id = RTE_AVP_REQ_CHANGE_MTU;
	req.new_mtu = new_mtu;

	ret = avp_ctrl_process_request(avp, &req);

	return (ret == 0 ? req.result : ret);
}

int
avp_ctrl_set_config(struct avp_dev *avp, struct rte_avp_device_config *config)
{
	struct rte_avp_request req;
	int ret;

	memset(&req, 0, sizeof(req));
	req.req_id = RTE_AVP_REQ_CFG_DEVICE;
	memcpy(&req.config, config, sizeof(req.config));

	ret = avp_ctrl_process_request(avp, &req);

	return (ret == 0 ? req.result : ret);
}

int
avp_ctrl_shutdown(struct avp_dev *avp)
{
	struct rte_avp_request req;
	int ret;

	memset(&req, 0, sizeof(req));
	req.req_id = RTE_AVP_REQ_SHUTDOWN_DEVICE;

	ret = avp_ctrl_process_request(avp, &req);

	return (ret == 0 ? req.result : ret);
}

void
avp_ctrl_poll_resp(struct avp_dev *avp)
{
	if (avp_fifo_count(avp->resp_q))
		wake_up_interruptible(&avp->wq);
}

int
avp_ctrl_process_request(struct avp_dev *avp,
			 struct rte_avp_request *req)
{
	int ret = -1;
	void *resp_va;
	unsigned num;
	unsigned retry;

	if (!avp || !req) {
		AVP_ERR("No AVP instance or request\n");
		return -EINVAL;
	}

	if (avp->mode != RTE_AVP_MODE_GUEST) {
		/*
		 * We are running this request from ioctl context so do not
		 * send a request to the vswitch since it will lead to
		 * deadlock; both because we would deadlock on the request
		 * (i.e., ioctl() will not return until this request is replied
		 * to, and this request will not be replied to until the
		 * ioctl() finishes), and we may deadlock on rtnl_lock() if two
		 * requests come in simultaneously; one from userspace and one
		 * from vswitch.
		 */
		AVP_DBG("not sending control request on host device, req=%u\n",
				req->req_id);
		req->result = 0;
		return 0;
	}

	/* prevent any other processes from touching the sync_kva area */
	mutex_lock(&avp->sync_lock);

	req->result = -ENOTSUPP;
	AVP_DBG("Sending request %u\n", req->req_id);

	/* Discard any stale responses before starting a new request */
	while (avp_fifo_get(avp->resp_q, (void **)&resp_va, 1))
		AVP_DBG("Discarding stale response\n");

	/* Construct data */
	memcpy(avp->sync_kva, req, sizeof(struct rte_avp_request));
	num = avp_fifo_put(avp->req_q, &avp->sync_va, 1);
	if (num < 1) {
		AVP_ERR("Cannot send to req_q\n");
		ret = -EBUSY;
		goto unlock;
	}

	retry = 2;
	do {
		AVP_DBG("Waiting for request %u, retry=%u\n", req->req_id, retry);
		ret = wait_event_interruptible_timeout(
				  avp->wq,
				  avp_fifo_count(avp->resp_q),
				  msecs_to_jiffies(WRS_AVP_CTRL_RESPONSE_TIMEOUT));
	} while (ret == 0 && retry--);

	if (signal_pending(current) || (ret < 0)) {
		AVP_ERR("Interrupted while waiting for request %u, ret=%d\n",
				req->req_id, ret);
		ret = -ERESTARTSYS;
		goto unlock;
	}

	if ((ret == 0) && !avp_fifo_count(avp->resp_q)) {
		AVP_ERR("No response to request %u\n", req->req_id);
		ret = -ETIME;
		goto unlock;
	}

	AVP_DBG("Response received for %u after %lu/%lu jiffies, retry=%u\n",
			req->req_id,
			msecs_to_jiffies(WRS_AVP_CTRL_RESPONSE_TIMEOUT) - ret,
			msecs_to_jiffies(WRS_AVP_CTRL_RESPONSE_TIMEOUT), retry);

	num = avp_fifo_get(avp->resp_q, (void **)&resp_va, 1);
	if (num != 1 || resp_va != avp->sync_va) {
		/* This should never happen */
		AVP_ERR("No data in resp_q\n");
		ret = -ENODATA;
		goto unlock;
	}

	memcpy(req, avp->sync_kva, sizeof(struct rte_avp_request));
	ret = 0;

	AVP_DBG("Result %d received for request %u\n",
		req->result, req->req_id);

unlock:
	mutex_unlock(&avp->sync_lock);
	return ret;
}

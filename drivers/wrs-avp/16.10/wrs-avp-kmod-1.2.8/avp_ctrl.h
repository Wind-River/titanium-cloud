/*-
 * GPL LICENSE SUMMARY
 *
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

#ifndef _AVP_CTRL_H_
#define _AVP_CTRL_H_

#include <avp_dev.h>
#include <exec-env/wrs_avp_common.h>

int avp_ctrl_set_link_state(struct avp_dev *avp, unsigned state);

int avp_ctrl_set_mtu(struct avp_dev *avp, int new_mtu);

int avp_ctrl_set_config(struct avp_dev *avp, struct wrs_avp_device_config *config);

int avp_ctrl_shutdown(struct avp_dev *avp);

void avp_ctrl_poll_resp(struct avp_dev *avp);

int avp_ctrl_process_request(struct avp_dev *avp, struct wrs_avp_request *req);

#endif

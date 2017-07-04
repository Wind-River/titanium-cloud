/*-
 * GPL LICENSE SUMMARY
 *
 *   Copyright(c) 2015 Wind River Systems, Inc. All rights reserved.
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

#include <linux/string.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/pci.h>

#include "avp_dev.h"

#define WRS_AVP_DRIVER_NAME "wrs-avp"


static void
avp_get_drvinfo(struct net_device *netdev,
	struct ethtool_drvinfo *drvinfo)
{
	uint32_t version = WRS_AVP_KERNEL_DRIVER_VERSION;
	struct avp_dev *avp = netdev_priv(netdev);
	char driver_version[32];

	snprintf(driver_version, sizeof(driver_version), "%u.%u.%u",
		RTE_AVP_GET_RELEASE_VERSION(version),
		RTE_AVP_GET_MAJOR_VERSION(version),
		RTE_AVP_GET_MINOR_VERSION(version));

	strlcpy(drvinfo->driver, WRS_AVP_DRIVER_NAME, sizeof(drvinfo->driver));
	strlcpy(drvinfo->version, driver_version, sizeof(drvinfo->version));

	if (avp->pci_dev)
		strlcpy(drvinfo->bus_info, pci_name(avp->pci_dev),
			sizeof(drvinfo->bus_info) - 1);
}

static int
avp_get_settings(struct net_device *netdev,
	struct ethtool_cmd *ecmd)
{
	ecmd->supported = SUPPORTED_10000baseT_Full | SUPPORTED_TP;
	ecmd->advertising = ADVERTISED_TP;
	ecmd->duplex = DUPLEX_FULL;
	ecmd->port = PORT_TP;
	ecmd->phy_address = 0;
	ecmd->transceiver = XCVR_INTERNAL;
	ecmd->autoneg = AUTONEG_DISABLE;
	ecmd->maxtxpkt = 0;
	ecmd->maxrxpkt = 0;

	ethtool_cmd_speed_set(ecmd, SPEED_10000);

	return 0;
}

static void
avp_get_channels(struct net_device *dev,
	struct ethtool_channels *channels)
{
	struct avp_dev *avp = netdev_priv(dev);

	channels->max_rx = avp->max_rx_queues;
	channels->max_tx = avp->max_tx_queues;
	channels->max_other = 0;
	channels->max_combined = 0;
	channels->rx_count = avp->num_rx_queues;
	channels->tx_count = avp->num_tx_queues;
	channels->other_count = 0;
	channels->combined_count = 0;
}


static const struct ethtool_ops avp_ethtool_ops = {
	.get_settings = avp_get_settings,
	.get_drvinfo = avp_get_drvinfo,
	.get_link = ethtool_op_get_link,
	.get_channels = avp_get_channels,
};


void
avp_set_ethtool_ops(struct net_device *netdev)
{
	netdev->ethtool_ops =  &avp_ethtool_ops;
}

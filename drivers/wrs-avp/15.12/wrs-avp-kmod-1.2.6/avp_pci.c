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

/*
 *   Loosely based on the DPDK igb_uio.c PCI handling and other PCI based
 *   kernel modules.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/uio_driver.h>
#include <linux/io.h>
#include <linux/msi.h>
#include <linux/version.h>
#include <linux/if_ether.h>

#include <exec-env/wrs_avp_common.h>
#include <exec-env/wrs_avp_fifo.h>
#include "avp_dev.h"
#include "avp_ctrl.h"

/* Utility functions from avp_misc.c */
extern int avp_dev_create(struct wrs_avp_device_info *dev_info,
						  struct device *parent,
						  struct avp_dev **avpptr);
extern int avp_dev_detach(struct avp_dev *avp);
extern int avp_dev_release(uint64_t device_id);

/* Defines the default number of characters allowed in an MSI-X vector name */
#define WRS_AVP_PCI_MSIX_NAME_LEN 64

/* Defines the name of the PCI interrupt workqueue */
#define WRS_AVP_PCI_WORKQUEUE_NAME "avp-worker"

/* Structure defining a guest AVP PCI device */
struct wrs_avp_pci_dev {
	struct pci_dev *pci_dev;
	struct wrs_avp_device_info info;
	struct avp_dev *avp;
	void *addresses[PCI_STD_RESOURCE_END+1];
	char (*msix_names)[WRS_AVP_PCI_MSIX_NAME_LEN];
	struct msix_entry *msix_entries;
	int msix_vectors;
	spinlock_t lock;
	struct work_struct detach;
	struct work_struct attach;
	struct workqueue_struct *workqueue;
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
#define PCI_DEVICE_SUB(vend, dev, subvend, subdev) \
	.vendor = (vend), .device = (dev), \
	.subvendor = (subvend), .subdevice = (subdev)
#endif

static DEFINE_PCI_DEVICE_TABLE(avp_pci_ids) = {
	{ PCI_DEVICE_SUB(WRS_AVP_PCI_VENDOR_ID,
	 WRS_AVP_PCI_DEVICE_ID,
	 WRS_AVP_PCI_SUB_VENDOR_ID,
	                 WRS_AVP_PCI_SUB_DEVICE_ID) },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, avp_pci_ids);

/* map memory regions in to kernel virtual address space */
static int
avp_pci_map_regions(struct pci_dev *dev,
					struct wrs_avp_pci_dev *avp_dev)
{
	unsigned long addr, length, flags;
	phys_addr_t phys_addr;
	unsigned i;
	void *ptr;

	for (i = 0; i <= PCI_STD_RESOURCE_END; i++) {
		length = pci_resource_len(dev, i);
		addr = pci_resource_start(dev, i);

		if ((length == 0) && (addr == 0)) {
			continue;
		}

		if ((length == 0) || (addr == 0)) {
			AVP_ERR("BAR%u has invalid length %lu and address %lu\n",
					i, addr, length);
			return -EINVAL;
		}

		flags = pci_resource_flags(dev, i);
		if (flags & IORESOURCE_MEM) {
			/* map addresses into kernel address space */
			ptr = ioremap(addr, length);
			if (ptr == NULL) {
				return -1;
			}
			avp_dev->addresses[i] = ptr;
			AVP_DBG("BAR%u 0x%016llx ioremap to 0x%p", i,
					(unsigned long long)addr, ptr);

			phys_addr = virt_to_phys(ptr);
			AVP_DBG("virt_to_phys(0x%p) = 0x%016llx\n", ptr, phys_addr);
			AVP_DBG("phys_to_virt(0x%016llx) = 0x%p\n",
					(unsigned long long)phys_addr, phys_to_virt(phys_addr));
		}
	}

	return 0;
}

/* verify that the incoming device version is compatible with our own version */
static int
avp_pci_device_version_check(uint32_t version)
{
    uint32_t driver = WRS_AVP_STRIP_MINOR_VERSION(WRS_AVP_KERNEL_DRIVER_VERSION);
    uint32_t device = WRS_AVP_STRIP_MINOR_VERSION(version);

    if (device <= driver) {
        /* the incoming device version is less than or equal to our own */
        return 0;
    }

    return 1;
}

/* verify that memory regions have expected version and validation markers */
static int
avp_pci_check_regions(struct pci_dev *dev,
					  struct wrs_avp_pci_dev *avp_dev)
{
	struct wrs_avp_memmap_info *memmap;
	struct wrs_avp_device_info *info;
	unsigned i;
	void *ptr;

	for (i = 0; i <= PCI_STD_RESOURCE_END; i++) {
		ptr = avp_dev->addresses[i];

		switch (i) {
			case WRS_AVP_PCI_MEMMAP_BAR:
				if (ptr == NULL) {
					AVP_ERR("Missing address space for BAR%u\n", i);
					return -EINVAL;
				}
				memmap = (struct wrs_avp_memmap_info*)ptr;
				if ((memmap->magic != WRS_AVP_MEMMAP_MAGIC) ||
					(memmap->version != WRS_AVP_MEMMAP_VERSION)) {
					AVP_ERR("Invalid memmap magic 0x%08x and version %u\n",
							memmap->magic, memmap->version);
					return -EINVAL;
				}
				break;

			case WRS_AVP_PCI_DEVICE_BAR:
				if (ptr == NULL) {
					AVP_ERR("Missing address space for BAR%u\n", i);
					return -EINVAL;
				}
				info = (struct wrs_avp_device_info*)ptr;
				if ((info->magic != WRS_AVP_DEVICE_MAGIC) ||
					avp_pci_device_version_check(info->version)) {
					AVP_ERR("Invalid device info magic 0x%08x or version 0x%08x > 0x%08x\n",
							info->magic, info->version,
                            WRS_AVP_KERNEL_DRIVER_VERSION);
					return -EINVAL;
				}
				break;

			case WRS_AVP_PCI_MEMORY_BAR:
			case WRS_AVP_PCI_MMIO_BAR:
				if (ptr == NULL) {
					AVP_ERR("Missing address space for BAR%u\n", i);
					return -EINVAL;
				}
				break;

			case WRS_AVP_PCI_MSIX_BAR:
			default:
				/* no validation required */
				break;
		}
	}

	return 0;
}

/* release kernel virtual address mapping */
static void
avp_pci_unmap_regions(struct pci_dev *dev,
					  struct wrs_avp_pci_dev *avp_dev)
{
	unsigned i;

	spin_lock(&avp_dev->lock);

	for (i = 0; i <= PCI_STD_RESOURCE_END; i++) {
		if (avp_dev->addresses[i]) {
			AVP_DBG("BAR%u iounmap %p\n", i, avp_dev->addresses[i]);
			iounmap(avp_dev->addresses[i]);
			avp_dev->addresses[i] = NULL;
		}
	}

	spin_unlock(&avp_dev->lock);
}

/* translate from host physical address to guest physical address */
static phys_addr_t
avp_pci_translate_address(struct wrs_avp_pci_dev *avp_dev,
						  phys_addr_t host_phys_addr)
{
	struct wrs_avp_memmap_info *info;
	struct wrs_avp_memmap *map;
	phys_addr_t phys_addr;
	phys_addr_t offset;
	unsigned i;

	phys_addr = virt_to_phys(avp_dev->addresses[WRS_AVP_PCI_MEMORY_BAR]);
	info = (struct wrs_avp_memmap_info*)
		avp_dev->addresses[WRS_AVP_PCI_MEMMAP_BAR];

	offset = 0;
	for (i = 0; i < info->nb_maps; i++) {
		/* search all segments looking for a matching address */
		map = &info->maps[i];

		if ((host_phys_addr >= map->phys_addr) &&
			(host_phys_addr < (map->phys_addr + map->length))) {
			/* address is within this segment */
			offset += (host_phys_addr - map->phys_addr);
			phys_addr += offset;

			AVP_DBG("Translating host physical 0x%016llx to guest 0x%016llx\n",
					host_phys_addr, phys_addr);

			return phys_addr;
		}
		offset += map->length;
	}

	return 0;
}

/*
 * create a AVP device using the supplied device info by first translating it
 * to the guest address space.
 */
static int
avp_pci_create(struct pci_dev *dev,
			   struct wrs_avp_pci_dev *avp_dev)
{
	struct wrs_avp_device_info *dev_info = &avp_dev->info;
	struct wrs_avp_device_config dev_config;
	struct wrs_avp_device_info *info;
	struct net_device *netdev;
	unsigned long addr;
    unsigned i;
	int ret;

	addr = pci_resource_start(dev, WRS_AVP_PCI_DEVICE_BAR);
	if (addr == 0) {
		AVP_ERR("BAR%u is not mapped\n", WRS_AVP_PCI_DEVICE_BAR);
		return -EFAULT;
	}
	info = (struct wrs_avp_device_info*)
		avp_dev->addresses[WRS_AVP_PCI_DEVICE_BAR];

	/* translate incoming host dev_info to guest address space */
	memcpy(dev_info, info, sizeof(*dev_info));
	dev_info->tx_phys = avp_pci_translate_address(avp_dev, info->tx_phys);
	dev_info->rx_phys = avp_pci_translate_address(avp_dev, info->rx_phys);
	dev_info->alloc_phys = avp_pci_translate_address(avp_dev, info->alloc_phys);
	dev_info->free_phys = avp_pci_translate_address(avp_dev, info->free_phys);
	dev_info->req_phys = avp_pci_translate_address(avp_dev, info->req_phys);
	dev_info->resp_phys = avp_pci_translate_address(avp_dev, info->resp_phys);
	dev_info->sync_phys = avp_pci_translate_address(avp_dev, info->sync_phys);
	dev_info->mbuf_phys = avp_pci_translate_address(avp_dev, info->mbuf_phys);

	for (i = 0; i < WRS_AVP_MAX_MEMPOOLS; i++) {
		if (info->pool[i].phys_addr != 0) {
			dev_info->pool[i].phys_addr =
				avp_pci_translate_address(avp_dev, info->pool[i].phys_addr);
		}
	}

	/* create the actual device using the translated addresses */
	ret = avp_dev_create(dev_info, &dev->dev, &avp_dev->avp);
	if (ret < 0) {
		return ret;
	}

	netdev = avp_dev->avp->net_dev;

	/* setup current device configuration */
	memset(&dev_config, 0, sizeof(dev_config));
	dev_config.device_id = info->device_id;
	dev_config.driver_type = WRS_AVP_DRIVER_TYPE_KERNEL;
    dev_config.driver_version = WRS_AVP_KERNEL_DRIVER_VERSION;
    dev_config.features = avp_dev->avp->features;
	dev_config.num_tx_queues = avp_dev->avp->num_tx_queues;
	dev_config.num_rx_queues = avp_dev->avp->num_rx_queues;
    dev_config.if_up = !!(netdev->flags & IFF_UP);

	/* inform the device of negotiated values */
	ret = avp_ctrl_set_config(avp_dev->avp, &dev_config);
	if (ret < 0) {
		AVP_ERR("Failed to set device configuration, ret=%d\n", ret);
		goto release_device;
	}

	avp_dev->avp->pci_dev = dev;
	return 0;

release_device:
	avp_dev_release(info->device_id);
	return ret;
}

/* workqueue handler to run interrupt task from process context */
static void
avp_pci_detach(struct work_struct *work)
{
	struct wrs_avp_pci_dev *avp_pci_dev =
		container_of(work, struct wrs_avp_pci_dev, detach);
	void *registers = avp_pci_dev->addresses[WRS_AVP_PCI_MMIO_BAR];
	uint32_t status = WRS_AVP_MIGRATION_DETACHED;
	int ret;

	AVP_DBG("Running VM migration detach interrupt callback\n");

	ret = avp_dev_detach(avp_pci_dev->avp);
	if (ret < 0) {
		AVP_ERR("Failed to detach AVP device\n");
		status = WRS_AVP_MIGRATION_ERROR;
	}

	/* acknowledge that we have changed our state */
	iowrite32(status, registers + WRS_AVP_MIGRATION_ACK_OFFSET);
}

/* workqueue handler to run interrupt task from process context */
static void
avp_pci_attach(struct work_struct *work)
{
	struct wrs_avp_pci_dev *avp_pci_dev =
		container_of(work, struct wrs_avp_pci_dev, attach);
	void *registers = avp_pci_dev->addresses[WRS_AVP_PCI_MMIO_BAR];
	uint32_t status = WRS_AVP_MIGRATION_ATTACHED;
	struct avp_dev *avp;
	int ret;

	AVP_DBG("Running VM migration attach interrupt callback\n");

	ret = avp_pci_create(avp_pci_dev->pci_dev, avp_pci_dev);
	if (ret < 0) {
		AVP_ERR("Failed to attach AVP device\n");
		status = WRS_AVP_MIGRATION_ERROR;
		goto done;
	}

	avp = avp_pci_dev->avp;
	BUG_ON(!avp);
	BUG_ON(!avp->net_dev);

done:
	/* acknowledge that we have changed our state */
	iowrite32(status, registers + WRS_AVP_MIGRATION_ACK_OFFSET);
}

static irqreturn_t
avp_pci_interrupt_actions(struct wrs_avp_pci_dev *avp_pci_dev, int irq)
{
	void *registers = avp_pci_dev->addresses[WRS_AVP_PCI_MMIO_BAR];
	uint32_t value;

	/* read the interrupt status register
	 *	 note:	this register clears on read so all raised interrupts must be
	 *			handled or remembered for later processing
	 */
	value = ioread32(registers + WRS_AVP_INTERRUPT_STATUS_OFFSET);

	if (value | WRS_AVP_MIGRATION_INTERRUPT_MASK) {
		/*
		 * Handle migration interrupt by deferring the work to a workqueue
		 * since we need to do some things that are unsafe within an interrupt
		 * context.
		 */
		value = ioread32(registers + WRS_AVP_MIGRATION_STATUS_OFFSET);
		switch (value) {
			case WRS_AVP_MIGRATION_DETACHED:
				queue_work(avp_pci_dev->workqueue, &avp_pci_dev->detach);
				break;
			case WRS_AVP_MIGRATION_ATTACHED:
				queue_work(avp_pci_dev->workqueue, &avp_pci_dev->attach);
				break;
			default:
				AVP_ERR("unexpected migration status, status=%u\n", value);
		}
	} else {
		AVP_ERR("unexpected interrupt received, status=0x%08x\n", value);
		goto error;
	}

	AVP_DBG("IRQ %u handled, status=0x%08x\n", irq, value);

error:
	return IRQ_HANDLED;
}

static irqreturn_t
avp_pci_interrupt_handler(int irq, void *data)
{
	struct wrs_avp_pci_dev *avp_pci_dev = data;
	struct pci_dev *pci_dev = avp_pci_dev->pci_dev;
	void *registers = avp_pci_dev->addresses[WRS_AVP_PCI_MMIO_BAR];
	irqreturn_t ret = IRQ_NONE;
	unsigned long flags;
	uint32_t value;
	uint16_t status;

	if (avp_pci_dev == NULL) {
		return IRQ_NONE;
	}

	spin_lock_irqsave(&avp_pci_dev->lock, flags);

	if (registers == NULL) {
		/* device is being shutdown */
		goto unlock;
	}

	if (avp_pci_dev->msix_vectors == 0) {
		/* IRQ interrupt mode */
		pci_read_config_dword(pci_dev, PCI_COMMAND, &value);
		status = value >> 16;
		if (!(status & PCI_STATUS_INTERRUPT)) {
			/* No pending interrupt */
			AVP_DBG("Interrupt handler invoked for status 0x%08x\n", status);
			goto unlock;
		}
	}

	ret = avp_pci_interrupt_actions(avp_pci_dev, irq);

unlock:
	spin_unlock_irqrestore(&avp_pci_dev->lock, flags);
	return ret;
}

static int
avp_pci_setup_msi_interrupts(struct pci_dev *dev,
							 struct wrs_avp_pci_dev *avp_pci_dev)
{
	struct msix_entry *entry;
	size_t size;
	char *name;
	unsigned i;
	int ret;

	/* Use the maximum number of vectors */
	avp_pci_dev->msix_vectors = WRS_AVP_MAX_MSIX_VECTORS;

	/* Allocate MSI-X vectors */
	size = avp_pci_dev->msix_vectors * sizeof(avp_pci_dev->msix_entries[0]);
	avp_pci_dev->msix_entries = kmalloc(size, GFP_KERNEL);
	if (avp_pci_dev->msix_entries == NULL) {
		AVP_ERR("Failed to allocate memory %d MSI-X entries\n",
				avp_pci_dev->msix_vectors);
		return -ENOMEM;
	}

	/* Allocate MSI-X vectors */
	size = avp_pci_dev->msix_vectors * WRS_AVP_PCI_MSIX_NAME_LEN;
	avp_pci_dev->msix_names = kmalloc(size, GFP_KERNEL);
	if (avp_pci_dev->msix_names == NULL) {
		AVP_ERR("Failed to allocate memory %d MSI-X names\n",
				avp_pci_dev->msix_vectors);
		return -ENOMEM;
	}

	/* Setup vector descriptors */
	for (i = 0; i < avp_pci_dev->msix_vectors; i++) {
		entry = &avp_pci_dev->msix_entries[i];
		entry->entry = i;
	}

retry:
	/* Enable interrupt vectors */
	ret = pci_enable_msix(dev,
						  avp_pci_dev->msix_entries,
						  avp_pci_dev->msix_vectors);
	if ((ret < 0) || (ret == avp_pci_dev->msix_vectors)) {
		AVP_ERR("Failed to enable MSI-X interrupts, ret=%d\n", ret);
		goto cleanup;
	}
	else if (ret > 0) {
		/* The device has a smaller number of vectors available */
		AVP_INFO("Reducing MSI-X vectors to %d to match device limits\n", ret);
		avp_pci_dev->msix_vectors = ret;
		goto retry;
	}

	/* Setup interrupt handlers */
	for (i = 0; i < avp_pci_dev->msix_vectors; i++) {
		entry = &avp_pci_dev->msix_entries[i];
		name = avp_pci_dev->msix_names[i];

		snprintf(name, WRS_AVP_PCI_MSIX_NAME_LEN, "avp-msi%d", i);
		ret = request_irq(entry->vector,
						  avp_pci_interrupt_handler,
						  0, /* flags */
						  name,
						  avp_pci_dev);
		if (ret != 0) {
			AVP_ERR("Failed to allocate IRQ for MSI-X vector %d, ret=%d\n",
					i, ret);
			goto cleanup;
		}
		AVP_DBG("MSI-X vector %u IRQ %u enabled\n",
				entry->entry, entry->vector);
	}

	AVP_DBG("MSI-X enabled with %d vector(s) allocated\n",
			avp_pci_dev->msix_vectors);

	return 0;

cleanup:
	avp_pci_dev->msix_vectors = 0;
	if (avp_pci_dev->msix_names) kfree(avp_pci_dev->msix_names);
	if (avp_pci_dev->msix_entries) kfree(avp_pci_dev->msix_entries);
	pci_disable_msix(dev);
	return ret;
}

static int
avp_pci_setup_irq_interrupts(struct pci_dev *dev,
							 struct wrs_avp_pci_dev *avp_pci_dev)
{
	char name[WRS_AVP_PCI_MSIX_NAME_LEN];
	int ret;

	snprintf(name, WRS_AVP_PCI_MSIX_NAME_LEN, "avp-irq%d", dev->irq);
	ret = request_irq(dev->irq,
					  avp_pci_interrupt_handler,
					  IRQF_SHARED,
					  name,
					  avp_pci_dev);
	if (ret != 0) {
		AVP_ERR("Failed to setup IRQ based interrupt, ret=%d\n", ret);
		return ret;
	}

	AVP_DBG("IRQ interrupt %u enabled\n", dev->irq);

	return 0;
}

static int
avp_pci_setup_interrupts(struct pci_dev *dev,
						 struct wrs_avp_pci_dev *avp_pci_dev)
{
	void *registers = avp_pci_dev->addresses[WRS_AVP_PCI_MMIO_BAR];
	int ret;

	ret = pci_find_capability(dev, PCI_CAP_ID_MSIX);
	if (ret) {
		/* enable MSI-X as preferred interrupt mode */
		ret = avp_pci_setup_msi_interrupts(dev, avp_pci_dev);
	}
	else {
		/* fall back to IRQ based interrupts */
		AVP_INFO("MSI-X not supported; using IRQ based interrupts\n");
		ret = avp_pci_setup_irq_interrupts(dev, avp_pci_dev);
	}

	if (ret == 0) {
		/* enable all interrupts */
		iowrite32(WRS_AVP_ALL_INTERRUPTS_MASK,
				  registers + WRS_AVP_INTERRUPT_MASK_OFFSET);
	}

	return 0;
}

static int
avp_pci_release_interrupts(struct pci_dev *dev,
						   struct wrs_avp_pci_dev *avp_pci_dev)
{
	void *registers = avp_pci_dev->addresses[WRS_AVP_PCI_MMIO_BAR];
	struct msix_entry *entry;
	unsigned i;

	if (registers) {
		/* disable device interrupts */
		iowrite32(WRS_AVP_NO_INTERRUPTS_MASK,
				  registers + WRS_AVP_INTERRUPT_MASK_OFFSET);
	}

	if (avp_pci_dev->msix_vectors > 0) {
		for (i = 0; i < avp_pci_dev->msix_vectors; i++) {
			/* release all IRQ entries */
			entry = &avp_pci_dev->msix_entries[i];
			free_irq(entry->vector, avp_pci_dev);
			AVP_DBG("MSI-X vector %u IRQ %u disabled\n",
					entry->entry, entry->vector);
		}
		/* disable MSI-X processing */
		pci_disable_msix(dev);
		/* free resources */
		kfree(avp_pci_dev->msix_entries);
		kfree(avp_pci_dev->msix_names);
		AVP_DBG("MSI-X disabled %d vector(s)\n", avp_pci_dev->msix_vectors);
	} else {
		/* release IRQ based device interrupt */
		free_irq(dev->irq, avp_pci_dev);
		AVP_DBG("IRQ interrupt %u disabled\n", dev->irq);
	}

	return 0;
}

static int
avp_pci_migration_pending(struct wrs_avp_pci_dev *avp_pci_dev)
{
	void *registers = avp_pci_dev->addresses[WRS_AVP_PCI_MMIO_BAR];
	uint32_t status;

	if (registers) {
		status = ioread32(registers + WRS_AVP_MIGRATION_STATUS_OFFSET);
		if (status == WRS_AVP_MIGRATION_DETACHED) {
			/* migration is in progress; ack it if we have not already */
			iowrite32(status, registers + WRS_AVP_MIGRATION_ACK_OFFSET);
			return 1;
		}
	}
	return 0;
}

static int
avp_pci_release(struct pci_dev *dev,
				struct wrs_avp_pci_dev *avp_dev)
{
	return avp_dev_release(avp_dev->info.device_id);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
static int __devinit
#else
static int
#endif
avp_pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	struct wrs_avp_pci_dev *avp_dev;
	int ret;

	AVP_DBG("Probing %04x:%04x (%04x:%04x)\n",
			id->vendor, id->device, id->subvendor, id->subdevice);

	avp_dev = kzalloc(sizeof(*avp_dev), GFP_KERNEL);
	if (!avp_dev) {
		AVP_ERR("Failed to allocate %zu bytes\n", sizeof(*avp_dev));
		return -ENOMEM;
	}

	/* Setup spin lock for interrupt processing */
	spin_lock_init(&avp_dev->lock);

	/* Setup work queue structs for deferred interrupt processing */
	INIT_WORK(&avp_dev->detach, avp_pci_detach);
	INIT_WORK(&avp_dev->attach, avp_pci_attach);

	/* Allocate a workqueue that will handle deferred interrupt work. */
	avp_dev->workqueue = create_singlethread_workqueue(WRS_AVP_PCI_WORKQUEUE_NAME);
	if (avp_dev->workqueue == NULL) {
		AVP_ERR("Failed to allocate a AVP workqueue thread\n");
		ret = -ENOMEM;
		goto cleanup_memory;
	}

	/* enable PCI device */
	ret = pci_enable_device(dev);
	if (ret != 0) {
		AVP_ERR("Failed to enable PCI device, ret=%d\n", ret);
		goto cleanup_workqueue;
	}

	/* get memory regions */
	ret = pci_request_regions(dev, "wrs_avp");
	if (ret != 0) {
		AVP_ERR("Failed to get PCI memory regions, ret=%d\n", ret);
		goto disable_device;
	}

	/* enable bus mastering */
	pci_set_master(dev);

	/* query BAR resources */
	ret = avp_pci_map_regions(dev, avp_dev);
	if (ret != 0) {
		AVP_ERR("Failed to read BAR resources, ret=%d\n", ret);
		goto disable_device;
	}

	/* check current migration status */
	if (avp_pci_migration_pending(avp_dev)) {
		AVP_ERR("VM live migration operation in progress\n");
		ret = -EBUSY;
		goto release_resources;
	}

	/* check BAR resources */
	ret = avp_pci_check_regions(dev, avp_dev);
	if (ret != 0) {
		AVP_ERR("Failed to validate BAR resources, ret=%d\n", ret);
		goto release_resources;
	}

	/* TODO register with sysfs to dump debug data? */

	/* setup interrupts */
	ret = avp_pci_setup_interrupts(dev, avp_dev);
	if (ret != 0) {
		AVP_ERR("Failed to setup interrupts, ret=%d\n", ret);
		goto release_resources;
	}

	/* Setup back pointer */
	pci_set_drvdata(dev, avp_dev);
	avp_dev->pci_dev = dev;

	/* Create AVP device */
	ret = avp_pci_create(dev, avp_dev);
	if (ret != 0) {
		AVP_ERR("Failed to create device, ret=%d\n", ret);
		goto release_interrupts;
	}

	return 0;

release_interrupts:
	avp_pci_release_interrupts(dev, avp_dev);
release_resources:
	avp_pci_unmap_regions(dev, avp_dev);
	pci_release_regions(dev);
disable_device:
	pci_disable_device(dev);
cleanup_workqueue:
	destroy_workqueue(avp_dev->workqueue);
cleanup_memory:
	kfree(avp_dev);

	(void)dev;
	(void)id;
	return ret;
}

static void
avp_pci_remove(struct pci_dev *dev)
{
	struct wrs_avp_pci_dev *avp_dev = pci_get_drvdata(dev);

	if (avp_dev == NULL) {
		/* already deregistered or not fully initialized */
		return;
	}

	/* cancel pending interrupt work */
	cancel_work_sync(&avp_dev->detach);
	cancel_work_sync(&avp_dev->attach);
	flush_workqueue(avp_dev->workqueue);
	destroy_workqueue(avp_dev->workqueue);

	/* TODO release sysfs group */
	avp_pci_release_interrupts(dev, avp_dev);
	avp_pci_release(dev, avp_dev);
	avp_pci_unmap_regions(dev, avp_dev);
	pci_release_regions(dev);
	pci_disable_device(dev);
	pci_set_drvdata(dev, NULL);
	kfree(avp_dev);
}

static void
avp_pci_shutdown(struct pci_dev *dev)
{
    /* shutdown the device on reboot or shutdown */
    avp_pci_remove(dev);
}

static struct pci_driver avp_pci_driver = {
	.name = "wrs_avp",
	.id_table = avp_pci_ids,
	.probe = avp_pci_probe,
	.remove = avp_pci_remove,
	.shutdown = avp_pci_shutdown,
};

int __init
avp_pci_init(void)
{
	return pci_register_driver(&avp_pci_driver);
}

void __exit
avp_pci_exit(void)
{
	pci_unregister_driver(&avp_pci_driver);
}

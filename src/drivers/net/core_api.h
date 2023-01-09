// SPDX-License-Identifier: GPL-2.0-only
/* Copyright(c) 2022 Pensando Systems, Inc */

#ifndef _PDS_CORE_API_H_
#define _PDS_CORE_API_H_

#include <linux/device.h>

#define PDS_CORE_BARS_MAX                      4

struct pds_core_dev_bar {
	void __iomem *vaddr;
	phys_addr_t bus_addr;
	unsigned long len;
	int res_index;
};

struct pds_core {
	struct device *dev;

	/* SR-IOV */
	int total_vfs;
	int max_vfs;
	int num_vfs;

	/* PCI */
	struct pds_core_dev_bar bars[PDS_CORE_BARS_MAX];
	unsigned int num_bars;

	/* Device BARs */
	union pds_core_dev_info_regs __iomem *info_regs;
	
	struct mutex devcmd_lock;
	union pds_core_dev_cmd_regs __iomem *cmd_regs;
	
	struct ionic_intr __iomem *intr_ctrl;
	
	u64 __iomem *intr_status;
	
	u64 __iomem *db_pages;
	dma_addr_t phy_db_pages;
};

int pds_core_devcmd(struct pds_core *dev, u8 *cmd, u8 *comp, u16 timeout_s);

#endif	/* _PDS_CORE_API_H_ */

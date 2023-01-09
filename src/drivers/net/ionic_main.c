/*
 * Copyright 2017-2019 Pensando Systems, Inc.  All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "ionic.h"

/* Find last set bit; fls(0) = 0, fls(1) = 1, fls(0x80000000) = 32 */
static int fls(int x)
{
	int r = 32;

	if (!x)
		return 0;
	if (!(x & 0xffff0000u)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff000000u)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf0000000u)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc0000000u)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x80000000u)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}

static inline u32 ilog2(u32 mem)
{
	return (fls(mem) - 1);
}

static int ionic_dev_cmd_wait(struct ionic_dev *idev, unsigned long max_seconds)
{
	unsigned long wait;
	int done;
	struct ionic *ionic = container_of(idev, struct ionic, idev);

	// Wait for dev cmd to complete...but no more than max_seconds
	for (wait = 0; wait < max_seconds; wait++) {
		done = ionic_dev_cmd_done(idev);
		if (done)
			return 0;

		// Delay
		mdelay(1000);
	}

	DBG_OPROM_INFO(ionic, "timeout happened\n");
	return -ETIMEDOUT;
}

static int ionic_dev_cmd_check_error(struct ionic_dev *idev)
{
	return ionic_dev_cmd_status(idev);
}

int ionic_dev_cmd_wait_check(struct ionic_dev *idev, unsigned long max_seconds)
{
	int err;

	err = ionic_dev_cmd_wait(idev, max_seconds);
	if (err) {
		return err;
	}

	return ionic_dev_cmd_check_error(idev);
}

u8 ionic_dev_cmd_status(struct ionic_dev *idev)
{
	return readb(&idev->dev_cmd->comp.status);
}

bool ionic_dev_cmd_done(struct ionic_dev *idev)
{
	return readl(&idev->dev_cmd->done) & IONIC_DEV_CMD_DONE;
}

int ionic_dev_cmd_go(struct ionic_dev *idev, union ionic_dev_cmd *cmd,
		     unsigned long max_seconds)
{
	unsigned int i;
	unsigned long retry;
	int err;
	struct ionic *ionic = container_of(idev, struct ionic, idev);

	for (retry = 0; retry < RETRY_COUNT; retry++) {
		// Send the devcmd
		for (i = 0; i < ARRAY_SIZE(cmd->words); i++) {
			writel(cmd->words[i], &idev->dev_cmd->cmd.words[i]);
		}

		writel(0, &idev->dev_cmd->done);
		writel(1, &idev->dev_cmd->doorbell);

		// Check the status
		err = ionic_dev_cmd_wait_check(idev, max_seconds);
		if (err == IONIC_RC_EAGAIN) {
			DBG_OPROM_INFO(ionic, "%04x:%04x - NIC is initializing\n",
				       ionic->pdev->vendor, ionic->pdev->device);
			// Delay
			mdelay(1000);
			continue;
		} else {
			return err;
		}
	}
	return -EIO;
}

int ionic_dev_cmd_init(struct ionic_dev *idev, unsigned long max_seconds)
{
	union ionic_dev_cmd cmd = {
		.init.opcode = IONIC_CMD_INIT,
		.init.type = 0,
	};

	return ionic_dev_cmd_go(idev, &cmd, max_seconds);
}

int ionic_dev_cmd_reset(struct ionic_dev *idev, unsigned long max_seconds)
{
	union ionic_dev_cmd cmd = {
		.reset.opcode = IONIC_CMD_RESET,
	};

	return ionic_dev_cmd_go(idev, &cmd, max_seconds);
}

int ionic_dev_cmd_identify(struct ionic_dev *idev, u16 ver,
	unsigned long max_seconds)
{
	union ionic_dev_cmd cmd = {
		.identify.opcode = IONIC_CMD_IDENTIFY,
		.identify.ver = ver,
	};

	return ionic_dev_cmd_go(idev, &cmd, max_seconds);
}

int ionic_dev_cmd_debug_msg(struct ionic_dev *idev, unsigned long max_seconds)
{
	union ionic_dev_cmd cmd = {
		.debug.opcode = IONIC_CMD_DEBUG,
		.debug.debug_type = IONIC_DEBUG_TYPE_MSG,
	};

	return ionic_dev_cmd_go(idev, &cmd, max_seconds);
}

int ionic_dev_cmd_hii_identify(struct ionic_dev *idev, unsigned long max_seconds)
{
	union ionic_dev_cmd cmd = {
		.hii_identify.opcode = IONIC_CMD_HII_IDENTIFY,
		.hii_identify.ver = 1,
	};

	return ionic_dev_cmd_go(idev, &cmd, max_seconds);
}

static int ionic_dev_cmd_system_led(struct ionic_dev *idev,
             unsigned long max_seconds, bool uid_led_status)
{
	union ionic_dev_cmd cmd = {
		.hii_setattr.opcode = IONIC_CMD_HII_SETATTR,
		.hii_setattr.attr = IONIC_HII_ATTR_UID_LED,
		.hii_setattr.uid_led_on = uid_led_status,
	};

	return ionic_dev_cmd_go(idev, &cmd, max_seconds);
}

static int ionic_dev_cmd_oob_en(struct ionic_dev *idev,
         unsigned long max_seconds, bool oob_en)
{
	union ionic_dev_cmd cmd = {
		.hii_setattr.opcode = IONIC_CMD_HII_SETATTR,
		.hii_setattr.attr = IONIC_HII_ATTR_OOB_EN,
		.hii_setattr.oob_en = oob_en,
	};

	return ionic_dev_cmd_go(idev, &cmd, max_seconds);
}

static int ionic_dev_cmd_reset_hii(struct ionic_dev *idev,
                 unsigned long max_seconds)
{
	union ionic_dev_cmd cmd = {
		.hii_init.opcode = IONIC_CMD_HII_RESET,
	};
	return ionic_dev_cmd_go(idev, &cmd, max_seconds);
}

static int ionic_dev_cmd_vlan_setattr(struct ionic_dev *idev,
             unsigned long max_seconds, u32 vlan_id, bool vlan_en)
{
	union ionic_dev_cmd cmd = {
		.hii_setattr.opcode = IONIC_CMD_HII_SETATTR,
		.hii_setattr.attr = IONIC_HII_ATTR_VLAN,
		.hii_setattr.vlan.id = vlan_id,
		.hii_setattr.vlan.enable = vlan_en,
	};

	return ionic_dev_cmd_go(idev, &cmd, max_seconds);
}

char *ionic_dev_asic_name(u8 asic_type)
{
	switch (asic_type) {
	case IONIC_ASIC_TYPE_CAPRI:
		return "Capri";
	default:
		return "Unknown";
	}
}

int ionic_dev_cmd_lif_init(struct ionic_dev *idev, u32 index, dma_addr_t addr,
	unsigned long max_seconds)
{
	union ionic_dev_cmd cmd = {
		.lif_init.opcode = IONIC_CMD_LIF_INIT,
		.lif_init.index = index,
		.lif_init.info_pa = addr,
	};

	return ionic_dev_cmd_go(idev, &cmd, max_seconds);
}

void ionic_dev_cmd_comp(struct ionic_dev *idev, void *mem)
{
	union ionic_dev_cmd_comp *comp = mem;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(comp->words); i++)
		comp->words[i] = readl(&idev->dev_cmd->comp.words[i]);
}

int ionic_dev_cmd_adminq_init(struct ionic_dev *idev, struct qcq *qcq,
					   unsigned int lif_index, unsigned long max_seconds)
{
	struct queue *q = &qcq->q;
	struct cq *cq = &qcq->cq;

	union ionic_dev_cmd cmd = {
		.q_init.opcode = IONIC_CMD_Q_INIT,
		.q_init.lif_index = lif_index,
		.q_init.type = q->type,
		.q_init.index = q->index,
		.q_init.flags = IONIC_QINIT_F_ENA,
		.q_init.pid = q->pid,
		.q_init.ring_size = ilog2(q->num_descs),
		.q_init.ring_base = q->base_pa,
		.q_init.cq_ring_base = cq->base_pa,
	};

	return ionic_dev_cmd_go(idev, &cmd, max_seconds);
}

int ionic_dev_cmd_lif_reset(struct ionic_dev *idev, u32 index, unsigned long max_seconds)
{
	union ionic_dev_cmd cmd = {
		.lif_init.opcode = IONIC_CMD_LIF_RESET,
		.lif_init.index = index,
	};

	return ionic_dev_cmd_go(idev, &cmd, max_seconds);
}

int ionic_lif_reset(struct ionic *ionic)
{
	struct ionic_dev *idev = &ionic->idev;

	return ionic_dev_cmd_lif_reset(idev, ionic->lif->index, devcmd_timeout);
}

/**
 * Probe helper functions.
 * */
int ionic_dev_setup(struct ionic_dev *idev, struct ionic_device_bar bars[],
					unsigned int num_bars)
{
	struct ionic_device_bar *bar = &bars[0];
	u32 sig;
	u8 oprom_debug_flag = 0;
	struct ionic *ionic = container_of(idev, struct ionic, idev);

	// BAR0 resources
	if (num_bars < 1 || bar->len != IONIC_BAR0_SIZE) {
		DBG_OPROM_ERR_CONSOLE(ionic->pdev, "cannot setup BAR0\n");
		return -EFAULT;
	}

	idev->dev_info = bar->vaddr + IONIC_BAR0_DEV_INFO_REGS_OFFSET;
	idev->dev_cmd = bar->vaddr + IONIC_BAR0_DEV_CMD_REGS_OFFSET;
	idev->intr_status = bar->vaddr + IONIC_BAR0_INTR_STATUS_OFFSET;
	idev->intr_ctrl = bar->vaddr + IONIC_BAR0_INTR_CTRL_OFFSET;

	sig = readl(&idev->dev_info->signature);
	if (sig != IONIC_DEV_INFO_SIGNATURE) {
		DBG_OPROM_ERR_CONSOLE(ionic->pdev, "Incompatible firmware signature %x\n",
				      sig);
		return -EFAULT;
	}

	oprom_debug_flag = readb(&idev->dev_info->oprom_regs.oprom_log_level);
	if (oprom_debug_flag) {
		ionic->oprom_debug_enable = 1;
		DBG_OPROM_INFO(ionic, "******** OPROM DEBUG LOGS START PRINT ********\n");
	}

	// BAR1 resources
	bar++;
	if (num_bars < 2) {
		DBG_OPROM_ERR(ionic, "num bars is less than 2\n");
		return -EFAULT;
	}

	idev->db_pages = bar->vaddr;
	idev->phy_db_pages = bar->bus_addr;

	// BAR2 resources
	bar++;
	if (num_bars < 3) {
		idev->phy_hbm_pages = 0;
		idev->hbm_npages = 0;
		idev->hbm_inuse = NULL;
	}

	// Device info region
	idev->port_info_sz = ALIGN(sizeof(*idev->port_info), PAGE_SIZE);

	idev->port_info = malloc_phys(idev->port_info_sz, PAGE_SIZE);
	if (idev->port_info == NULL) {
		DBG_OPROM_ERR(ionic, "failed to allocate device info region\n");
		return -ENOMEM;
	}
	memset(idev->port_info, 0, sizeof (*idev->port_info));

	idev->port_info_pa = virt_to_phys(idev->port_info);

	return 0;
}

/**
 * Initialize the ionic and the setup the command registers.
**/
int ionic_setup(struct ionic *ionic)
{
	int err;

	err = ionic_dev_setup(&ionic->idev, ionic->bars, ionic->num_bars);
	if (err) {
		return err;
	}

	return 0;
}

/**
 * Identify the Ionic device.
 **/
int ionic_identify(struct ionic *ionic)
{
	struct ionic_dev *idev = &ionic->idev;
	struct ionic_identity *ident = &ionic->ident;
	int err;
	unsigned int i;
	unsigned int nwords;

	ident->drv.os_type = IONIC_OS_TYPE_IPXE;
	ident->drv.os_dist = 0;
	strncpy(ident->drv.os_dist_str, IPXE_OS_DIST_STR,
		sizeof(ident->drv.os_dist_str) - 1);
	ident->drv.kernel_ver = IPXE_VERSION_CODE;
	strncpy(ident->drv.kernel_ver_str, IPXE_KERNEL_VERSION_STR,
		sizeof(ident->drv.kernel_ver_str) - 1);
	strncpy(ident->drv.driver_ver_str, DRV_VERSION,
			sizeof(ident->drv.driver_ver_str) - 1);

	nwords = MIN(ARRAY_SIZE(ident->drv.words), ARRAY_SIZE(idev->dev_cmd->data));
	for (i = 0; i < nwords; i++)
		writel(ident->drv.words[i], &idev->dev_cmd->data[i]);

	err = ionic_dev_cmd_identify(idev, IONIC_IDENTITY_VERSION_1, devcmd_timeout);
	if (err)
		goto err_free_identify;

	nwords = MIN(ARRAY_SIZE(ident->dev.words), ARRAY_SIZE(idev->dev_cmd->data));
	for (i = 0; i < nwords; i++)
		ident->dev.words[i] = readl(&idev->dev_cmd->data[i]);

	return 0;

err_free_identify:
	return err;
}

/**
 * Initialize the q
 * */
int ionic_q_init(struct lif *lif, struct ionic_dev *idev, struct queue *q,
				 unsigned int index, const char *base, unsigned int num_descs,
				 size_t desc_size, size_t sg_desc_size, unsigned int pid)
{
	struct desc_info *cur;
	unsigned int ring_size;
	unsigned int i;

	if (desc_size == 0 || !is_power_of_2(num_descs)) {
		return -EINVAL;
	}

	ring_size = ilog2(num_descs);
	if (ring_size < 2 || ring_size > 16) {
		return -EINVAL;
	}

	q->lif = lif;
	q->idev = idev;
	q->index = index;
	q->num_descs = num_descs;
	q->desc_size = desc_size;
	q->sg_desc_size = sg_desc_size;
	q->head = q->tail = q->info;
	q->pid = pid;

	snprintf(q->name, sizeof(q->name), "%s%u", base, index);

	cur = q->info;

	for (i = 0; i < num_descs; i++) {
		if (i + 1 == num_descs) {
			cur->next = q->info;
		}
		else {
			cur->next = cur + 1;
		}
		cur->index = i;
		cur->left = num_descs - i;
		cur++;
	}

	return 0;
}

/**
 * Initialize the cq
 * */
int ionic_cq_init(struct lif *lif, struct cq *cq,
				  unsigned int num_descs, size_t desc_size)
{
	struct cq_info *cur;
	unsigned int ring_size;
	unsigned int i;

	if (desc_size == 0 || !is_power_of_2(num_descs)) {
		return -EINVAL;
	}

	ring_size = ilog2(num_descs);
	if (ring_size < 2 || ring_size > 16) {
		return -EINVAL;
	}

	cq->lif = lif;
	cq->num_descs = num_descs;
	cq->desc_size = desc_size;
	cq->tail = cq->info;
	cq->done_color = 1;

	cur = cq->info;

	for (i = 0; i < num_descs; i++) {
		if (i + 1 == num_descs) {
			cur->next = cq->info;
			cur->last = true;
		}
		else {
			cur->next = cur + 1;
		}
		cur->index = i;
		cur++;
	}

	return 0;
}

/**
 * Map the q with the descriptors
 * */
void ionic_q_map(struct queue *q, void *base, dma_addr_t base_pa)
{
	struct desc_info *cur;
	unsigned int i;

	q->base = base;
	q->base_pa = base_pa;

	for (i = 0, cur = q->info; i < q->num_descs; i++, cur++) {
		cur->desc = base + (i * q->desc_size);
	}
}

/**
 * Map the cq with the descriptors
 * */
void ionic_cq_map(struct cq *cq, void *base, dma_addr_t base_pa)
{
	struct cq_info *cur;
	unsigned int i;

	cq->base = base;
	cq->base_pa = base_pa;

	for (i = 0, cur = cq->info; i < cq->num_descs; i++, cur++) {
		cur->cq_desc = base + (i * cq->desc_size);
	}
}

/**
 * Map the q sg.
 * */
void ionic_q_sg_map(struct queue *q, void *base, dma_addr_t base_pa)
{
	struct desc_info *cur;
	unsigned int i;

	q->sg_base = base;
	q->sg_base_pa = base_pa;

	for (i = 0, cur = q->info; i < q->num_descs; i++, cur++) {
		cur->sg_desc = base + (i * q->sg_desc_size);
	}
}

/**
 * Bind the q with cq.
 * */
void ionic_cq_bind(struct cq *cq, struct queue *q)
{
	// TODO support many:1 bindings using qid as index into bound_q array
	cq->bound_q = q;
}

/**
 * Allocating and initializing the qcq.
 * */
static int ionic_qcq_alloc(struct lif *lif, unsigned int type,
			   unsigned int index,
			   const char *base, unsigned int flags,
			   unsigned int num_descs, unsigned int desc_size,
			   unsigned int cq_desc_size,
			   unsigned int sg_desc_size,
			   unsigned int pid, struct qcq **qcq)
{
	struct ionic_dev *idev = &lif->ionic->idev;
	struct qcq *new;

	unsigned int q_size = num_descs * desc_size;
	unsigned int cq_size = num_descs * cq_desc_size;
	unsigned int sg_size = num_descs * sg_desc_size;
	unsigned int total_size;
	void *q_base, *cq_base, *sg_base;
	dma_addr_t q_base_pa, cq_base_pa, sg_base_pa;
	int err;

	*qcq = NULL;

	total_size = ALIGN(q_size, PAGE_SIZE) + ALIGN(cq_size, PAGE_SIZE);
	if (flags & QCQ_F_SG)
		total_size += ALIGN(sg_size, PAGE_SIZE);

	new = zalloc(sizeof(*new));
	if (!new)
		return -ENOMEM;

	new->flags = flags;

	new->q.info = zalloc(sizeof(*new->q.info) * num_descs);
	if (!new->q.info) {
		err = -ENOMEM;
		goto err_free_new_qcq;
	}

	new->q.type = type;

	err = ionic_q_init(lif, idev, &new->q, index, base, num_descs,
					   desc_size, sg_desc_size, pid);
	if (err) {
		err = -ENOMEM;
		goto err_free_q_info;
	}

	new->cq.info = zalloc(sizeof(*new->cq.info) * num_descs);
	if (!new->cq.info) {
		err = -ENOMEM;
		goto err_free_q_info;
	}

	err = ionic_cq_init(lif, &new->cq, num_descs, cq_desc_size);
	if (err) {
		err = -ENOMEM;
		goto err_free_cq_info;
	}

	new->base = malloc_phys(total_size, PAGE_SIZE);
	if (!new->base) {
		err = -ENOMEM;
		goto err_free_cq_info;
	}
	memset (new->base, 0, total_size);

	new->total_size = total_size;
	new->base_pa = virt_to_phys(new->base);
	q_base = new->base;
	q_base_pa = new->base_pa;

	cq_base = (void *)ALIGN((uintptr_t)q_base + q_size, PAGE_SIZE);
	cq_base_pa = ALIGN(q_base_pa + q_size, PAGE_SIZE);

	if (flags & QCQ_F_SG) {
		sg_base = (void *)ALIGN((uintptr_t)cq_base + cq_size,
								PAGE_SIZE);
		sg_base_pa = ALIGN(cq_base_pa + cq_size, PAGE_SIZE);
		ionic_q_sg_map(&new->q, sg_base, sg_base_pa);
	}

	ionic_q_map(&new->q, q_base, q_base_pa);
	ionic_cq_map(&new->cq, cq_base, cq_base_pa);
	ionic_cq_bind(&new->cq, &new->q);

	*qcq = new;

	return 0;

err_free_cq_info:
	free(new->cq.info);
err_free_q_info:
	free(new->q.info);
err_free_new_qcq:
	free(new);
	return err;
}

/**
 * deallocate the qcq
 * */
void ionic_qcq_dealloc(struct qcq *qcq)
{
	if (!qcq)
		return;

	free_phys(qcq->base, qcq->total_size);
	free(qcq->cq.info);
	free(qcq->q.info);
	free(qcq);
}

/**
 * Allocate the adminq, txq and rxq.
 * */
static int ionic_qcqs_alloc(struct lif *lif)
{
	unsigned int flags;
	unsigned int pid;
	int err = -ENOMEM;

	pid = 0;
	flags = 0;
	err = ionic_qcq_alloc(lif, IONIC_QTYPE_ADMINQ, 0, "admin", flags,
			      1 << 4,
			      sizeof(struct ionic_admin_cmd),
			      sizeof(struct ionic_admin_comp),
			      0, pid, &lif->adminqcq);
	if (err)
		goto err_free_adminqcq;

	flags = QCQ_F_NOTIFYQ;
	err = ionic_qcq_alloc(lif, IONIC_QTYPE_NOTIFYQ, 0, "notifyq", flags,
			      NOTIFYQ_LENGTH,
			      sizeof(struct ionic_notifyq_cmd),
			      sizeof(union ionic_notifyq_comp),
			      0, pid, &lif->notifyqcqs);
	if (err)
		goto err_free_notifyqcq;

	flags = QCQ_F_TX_STATS;
	err = ionic_qcq_alloc(lif, IONIC_QTYPE_TXQ, 0, "tx", flags,
			      NTXQ_DESC,
			      sizeof(struct ionic_txq_desc),
			      sizeof(struct ionic_txq_comp),
			      0, pid, &lif->txqcqs);
	if (err)
		goto err_free_txqcq;

	flags = QCQ_F_RX_STATS;
	err = ionic_qcq_alloc(lif, IONIC_QTYPE_RXQ, 0, "rx", flags,
			      NRXQ_DESC,
			      sizeof(struct ionic_rxq_desc),
			      sizeof(struct ionic_rxq_comp),
			      0, pid, &lif->rxqcqs);
	if (err)
		goto err_free_rxqcq;

	return 0;

err_free_rxqcq:
	ionic_qcq_dealloc(lif->txqcqs);
err_free_txqcq:
	ionic_qcq_dealloc(lif->notifyqcqs);
err_free_notifyqcq:
	ionic_qcq_dealloc(lif->adminqcq);
err_free_adminqcq:
	return err;
}

/**
 * Allocate for the lif and call the qcqs alloc
 * */
int ionic_lif_alloc(struct ionic *ionic, unsigned int index)
{
	struct lif *lif;
	int err = -ENOMEM;

	// allocate memory for lif
	lif = zalloc(sizeof(*lif));
	if (!lif) {
		DBG_OPROM_ERR(ionic, "could not allocate for lif\n");
		return err;
	}

	snprintf(lif->name, sizeof(lif->name), "lif");
	lif->ionic = ionic;
	lif->index = index;

	// allocate lif info region
	lif->info_sz = ALIGN(sizeof(*lif->info), PAGE_SIZE);

	lif->info = malloc_phys(lif->info_sz, PAGE_SIZE);
	if (!lif->info) {
		goto err_freelif;
	}
	memset (lif->info, 0, lif->info_sz);

	lif->info_pa = virt_to_phys(lif->info);

	// allocate the qcqs
	err = ionic_qcqs_alloc(lif);
	if (err)
		goto err_freenotify;

	ionic->lif = lif;
	return 0;

err_freenotify:
	free_phys(lif->info, sizeof(lif->info_sz));
err_freelif:
	free(lif);
	return err;
}

/**
 * Get the doorbell from the mapped pages.
 * */
struct ionic_doorbell __iomem *ionic_db_map(struct ionic_dev *idev,
					    struct queue *q)
{
	struct ionic_doorbell __iomem *db;

	db = (void *)idev->db_pages + (q->pid * PAGE_SIZE);
	db += q->hw_type;

	return db;
}

/**
 * Initialize the Adminq
 * */
static int ionic_lif_adminq_init(struct lif *lif)
{
	struct ionic_dev *idev = &lif->ionic->idev;
	struct qcq *qcq = lif->adminqcq;
	struct queue *q = &qcq->q;
	struct ionic *ionic = lif->ionic;
	struct ionic_q_init_comp comp;
	int err;

	DBG_OPROM_INFO(ionic, "\n");

	err = ionic_dev_cmd_adminq_init(idev, qcq, lif->index, devcmd_timeout);
	if (err) {
		DBG_OPROM_ERR(ionic, "lif adminq initiation failed\n");
		return err;
	}

	ionic_dev_cmd_comp(idev, &comp);
	q->hw_index = comp.hw_index;
	q->hw_type = comp.hw_type;
	q->db = ionic_db_map(idev, q);
	qcq->flags |= QCQ_F_INITED;

	return 0;
}

static const char *ionic_error_to_str(enum ionic_status_code code)
{
	switch (code) {
	case IONIC_RC_SUCCESS:
		return "IONIC_RC_SUCCESS";
	case IONIC_RC_EVERSION:
		return "IONIC_RC_EVERSION";
	case IONIC_RC_EOPCODE:
		return "IONIC_RC_EOPCODE";
	case IONIC_RC_EIO:
		return "IONIC_RC_EIO";
	case IONIC_RC_EPERM:
		return "IONIC_RC_EPERM";
	case IONIC_RC_EQID:
		return "IONIC_RC_EQID";
	case IONIC_RC_EQTYPE:
		return "IONIC_RC_EQTYPE";
	case IONIC_RC_ENOENT:
		return "IONIC_RC_ENOENT";
	case IONIC_RC_EINTR:
		return "IONIC_RC_EINTR";
	case IONIC_RC_EAGAIN:
		return "IONIC_RC_EAGAIN";
	case IONIC_RC_ENOMEM:
		return "IONIC_RC_ENOMEM";
	case IONIC_RC_EFAULT:
		return "IONIC_RC_EFAULT";
	case IONIC_RC_EBUSY:
		return "IONIC_RC_EBUSY";
	case IONIC_RC_EEXIST:
		return "IONIC_RC_EEXIST";
	case IONIC_RC_EINVAL:
		return "IONIC_RC_EINVAL";
	case IONIC_RC_ENOSPC:
		return "IONIC_RC_ENOSPC";
	case IONIC_RC_ERANGE:
		return "IONIC_RC_ERANGE";
	case IONIC_RC_BAD_ADDR:
		return "IONIC_RC_BAD_ADDR";
	case IONIC_RC_DEV_CMD:
		return "IONIC_RC_DEV_CMD";
	case IONIC_RC_ERROR:
		return "IONIC_RC_ERROR";
	case IONIC_RC_ERDMA:
		return "IONIC_RC_ERDMA";
	default:
		return "IONIC_RC_UNKNOWN";
	}
}

static const char *ionic_opcode_to_str(enum ionic_cmd_opcode opcode)
{
	switch (opcode) {
	case IONIC_CMD_NOP:
		return "IONIC_CMD_NOP";
	case IONIC_CMD_IDENTIFY:
		return "IONIC_CMD_IDENTIFY";
	case IONIC_CMD_INIT:
		return "IONIC_CMD_INIT";
	case IONIC_CMD_RESET:
		return "IONIC_CMD_RESET";
	case IONIC_CMD_GETATTR:
		return "IONIC_CMD_GETATTR";
	case IONIC_CMD_SETATTR:
		return "IONIC_CMD_SETATTR";
	case IONIC_CMD_DEBUG:
		return "IONIC_CMD_DEBUG";
	case IONIC_CMD_PORT_IDENTIFY:
		return "IONIC_CMD_PORT_IDENTIFY";
	case IONIC_CMD_PORT_INIT:
		return "IONIC_CMD_PORT_INIT";
	case IONIC_CMD_PORT_RESET:
		return "IONIC_CMD_PORT_RESET";
	case IONIC_CMD_PORT_GETATTR:
		return "IONIC_CMD_PORT_GETATTR";
	case IONIC_CMD_PORT_SETATTR:
		return "IONIC_CMD_PORT_SETATTR";
	case IONIC_CMD_LIF_IDENTIFY:
		return "IONIC_CMD_LIF_IDENTIFY";
	case IONIC_CMD_LIF_INIT:
		return "IONIC_CMD_LIF_INIT";
	case IONIC_CMD_LIF_RESET:
		return "IONIC_CMD_LIF_RESET";
	case IONIC_CMD_LIF_GETATTR:
		return "IONIC_CMD_LIF_GETATTR";
	case IONIC_CMD_LIF_SETATTR:
		return "IONIC_CMD_LIF_SETATTR";
	case IONIC_CMD_RX_MODE_SET:
		return "IONIC_CMD_RX_MODE_SET";
	case IONIC_CMD_RX_FILTER_ADD:
		return "IONIC_CMD_RX_FILTER_ADD";
	case IONIC_CMD_RX_FILTER_DEL:
		return "IONIC_CMD_RX_FILTER_DEL";
	case IONIC_CMD_Q_INIT:
		return "IONIC_CMD_Q_INIT";
	case IONIC_CMD_Q_CONTROL:
		return "IONIC_CMD_Q_CONTROL";
	case IONIC_CMD_RDMA_RESET_LIF:
		return "IONIC_CMD_RDMA_RESET_LIF";
	case IONIC_CMD_RDMA_CREATE_EQ:
		return "IONIC_CMD_RDMA_CREATE_EQ";
	case IONIC_CMD_RDMA_CREATE_CQ:
		return "IONIC_CMD_RDMA_CREATE_CQ";
	case IONIC_CMD_RDMA_CREATE_ADMINQ:
		return "IONIC_CMD_RDMA_CREATE_ADMINQ";
	case IONIC_CMD_FW_DOWNLOAD:
		return "IONIC_CMD_FW_DOWNLOAD";
	case IONIC_CMD_FW_CONTROL:
		return "IONIC_CMD_FW_CONTROL";
	case IONIC_CMD_FW_DOWNLOAD_V1:
		return "IONIC_CMD_FW_DOWNLOAD_V1";
	case IONIC_CMD_FW_CONTROL_V1:
		return "IONIC_CMD_FW_CONTROL_V1";

	default:
		return "IONIC_CMD_UNKNOWN";
	}
}

int ionic_adminq_check_err(struct ionic *ionic, struct ionic_admin_ctx *ctx)
{
	const char *name;
	const char *status;

	if (ctx->comp.comp.status) {
		name = ionic_opcode_to_str(ctx->cmd.cmd.opcode);
		status = ionic_error_to_str(ctx->comp.comp.status);
		DBG_OPROM_ERR(ionic, "%s (%d) failed: %s (%d)\n",
			      name, ctx->cmd.cmd.opcode, status, ctx->comp.comp.status);
		return -EIO;
	}

	return 0;
}

int polladminq(struct lif *lif, struct ionic_admin_ctx *ctx)
{
	unsigned long wait;
	struct queue *adminq = &lif->adminqcq->q;
	struct cq *admincq = &lif->adminqcq->cq;
	struct ionic_admin_comp *comp = admincq->tail->cq_desc;
	struct ionic *ionic = lif->ionic;

	if (!lif->ionic->fw_running) {
		DBG_OPROM_ERR(ionic, "fw down\n");
		return -EIO;
	}

	for (wait = 0; wait < devcmd_timeout; wait++) {
		if (color_match(comp->color, admincq->done_color)) {
			// update the cq_tail and check if the last descriptor.
			if (admincq->tail->last) {
				admincq->done_color = !admincq->done_color;
			}

			// update the tail to point to the next one.
			admincq->tail = admincq->tail->next;
			// update the q tail index;
			adminq->tail = adminq->tail->next;
			memcpy(&ctx->comp, comp, sizeof(*comp));
			return ionic_adminq_check_err(ionic, ctx);
		}

		// Delay
		mdelay(1000);
	}

	DBG_OPROM_ERR(ionic, "timeout happened\n");
	return -ETIMEDOUT;
}

/**
 *
 * */
int ionic_adminq_post_wait(struct lif *lif, struct ionic_admin_ctx *ctx)
{
	struct queue *adminq = &lif->adminqcq->q;
	struct ionic_admin_cmd *cmd = adminq->head->desc;

	if (!ionic_q_has_space(adminq, 1)) {
		return -ENOSPC;
	}

	memcpy(cmd, &ctx->cmd, sizeof(ctx->cmd));
	adminq->head = adminq->head->next;
	struct ionic_doorbell db = {
		.qid_lo = adminq->hw_index,
		.qid_hi = adminq->hw_index >> 8,
		.ring = 0,
		.p_index = adminq->head->index,
	};

	//ring the doorbell
	writeq(*(u64 *)&db, adminq->db);
	return polladminq(lif, ctx);

}

/**
 * Initialize the NotifyQ.
 * */
static int ionic_lif_notifyq_init(struct lif *lif, struct qcq *qcq)
{
	struct queue *q = &qcq->q;
	struct ionic *ionic = lif->ionic;
	struct ionic_admin_ctx ctx = {
		.cmd.q_init = {
			.opcode = IONIC_CMD_Q_INIT,
			.lif_index = lif->index,
			.type = q->type,
			.index = q->index,
			.flags = IONIC_QINIT_F_ENA,
			.pid = q->pid,
			.ring_size = ilog2(q->num_descs),
			.ring_base = q->base_pa,
		},
	};
	int err;

	DBG_OPROM_INFO(ionic, "\n");

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		return err;

	q->hw_index = ctx.comp.q_init.hw_index;
	q->hw_type = ctx.comp.q_init.hw_type;
	q->db = NULL;
	qcq->flags |= QCQ_F_INITED;

	lif->ionic->last_eid = 0;

	return 0;
}

static void ionic_qcq_sanitize(struct qcq *qcq)
{
	unsigned int q_size = ALIGN(qcq->q.num_descs * qcq->q.desc_size, PAGE_SIZE);
	unsigned int cq_size = ALIGN(qcq->cq.num_descs * qcq->cq.desc_size, PAGE_SIZE);
	qcq->q.head = qcq->q.tail = qcq->q.info;
	qcq->cq.tail = qcq->cq.info;
	qcq->cq.done_color = 1;

	memset(qcq->q.base, 0, q_size);
	memset(qcq->cq.base, 0, cq_size);
}

/**
 * Initialize the TXQ.
 * */
static int ionic_lif_txq_init(struct lif *lif, struct qcq *qcq)
{
	struct queue *q = &qcq->q;
	struct cq *cq = &qcq->cq;
	struct ionic_dev *idev = &lif->ionic->idev;
	struct ionic *ionic = lif->ionic;
	struct ionic_admin_ctx ctx = {
		.cmd.q_init = {
			.opcode = IONIC_CMD_Q_INIT,
			.lif_index = lif->index,
			.type = q->type,
			.index = q->index,
			.pid = q->pid,
			.ring_size = ilog2(q->num_descs),
			.ring_base = q->base_pa,
			.cq_ring_base = cq->base_pa,
		},
	};
	int err;

	DBG_OPROM_INFO(ionic, "\n");

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		return err;

	q->hw_index = ctx.comp.q_init.hw_index;
	q->hw_type = ctx.comp.q_init.hw_type;
	q->db = ionic_db_map(idev, q);
	qcq->flags |= QCQ_F_INITED;

	ionic_qcq_sanitize(qcq);
	return 0;
}

/**
 * Initialize the RXQ.
 * */
static int ionic_lif_rxq_init(struct lif *lif, struct qcq *qcq)
{
	struct queue *q = &qcq->q;
	struct cq *cq = &qcq->cq;
	struct ionic_dev *idev = &lif->ionic->idev;
	struct ionic *ionic = lif->ionic;
	struct ionic_admin_ctx ctx = {
		.cmd.q_init = {
			.opcode = IONIC_CMD_Q_INIT,
			.lif_index = lif->index,
			.type = q->type,
			.index = q->index,
			.pid = q->pid,
			.ring_size = ilog2(q->num_descs),
			.ring_base = q->base_pa,
			.cq_ring_base = cq->base_pa,
		},
	};
	int err;

	DBG_OPROM_INFO(ionic, "\n");

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		return err;

	q->hw_index = ctx.comp.q_init.hw_index;
	q->hw_type = ctx.comp.q_init.hw_type;
	q->db = ionic_db_map(idev, q);
	qcq->flags |= QCQ_F_INITED;
	ionic_qcq_sanitize(qcq);
	return 0;
}

/**
 * Sets the mac address using the adminq_post
 * */
static int ionic_lif_add_rx_filter(struct lif *lif, const u8 *addr)
{
	struct ionic_admin_ctx ctx = {
		.cmd.rx_filter_add = {
			.opcode = IONIC_CMD_RX_FILTER_ADD,
			.match = IONIC_RX_FILTER_MATCH_MAC,
		},
	};
	int err;

	memcpy(ctx.cmd.rx_filter_add.mac.addr, addr, ETH_ALEN);
	err = ionic_adminq_post_wait(lif, &ctx);
	if (err)
		return err;

	return 0;
}

/**
 * Gets the mac address using a dev command and
 * sets the rx_filter for that mac address.
 * */
static int ionic_lif_station_mac_addr(struct lif *lif, struct net_device *netdev)
{

	int err;
	struct ionic *ionic = lif->ionic;
	struct ionic_admin_ctx ctx = {
		.cmd.lif_getattr = {
			.opcode = IONIC_CMD_LIF_GETATTR,
			.attr = IONIC_LIF_ATTR_MAC,
		},
	};

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err) {
		return err;
	}

	DBG_OPROM_INFO(ionic, "Station MAC Success %02x:%02x:%02x:%02x:%02x:%02x\n",
		       ctx.comp.lif_getattr.mac[0], ctx.comp.lif_getattr.mac[1],
		       ctx.comp.lif_getattr.mac[2], ctx.comp.lif_getattr.mac[3],
		       ctx.comp.lif_getattr.mac[4], ctx.comp.lif_getattr.mac[5]);
	memcpy(netdev->hw_addr, ctx.comp.lif_getattr.mac, ETH_ALEN);

	// adding the rx filter for the mac
	err = ionic_lif_add_rx_filter(lif, ctx.comp.lif_getattr.mac);
	if (err) {
		DBG_OPROM_ERR(ionic, "Adding RX filter failed\n");
		return err;
	}

	return 0;
}

/**
 * Sets the rx_filter hw feature
 * */
static int ionic_set_hw_features(struct lif *lif, u32 features)
{
	int err;
	struct ionic *ionic = lif->ionic;
	struct ionic_admin_ctx ctx = {
		.cmd.lif_setattr = {
			.opcode = IONIC_CMD_LIF_SETATTR,
			.index = lif->index,
			.attr = IONIC_LIF_ATTR_FEATURES,
			.features = features,
		},
	};

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err) {
		return err;
	}

	lif->hw_features = ctx.cmd.lif_setattr.features &
						ctx.comp.lif_setattr.features;
	if (lif->hw_features & IONIC_ETH_HW_VLAN_RX_FILTER)
		DBG_OPROM_INFO(ionic, "RX VLAN Filter set as HW feature\n");
	if (lif->hw_features & IONIC_ETH_HW_VLAN_TX_TAG)
		DBG_OPROM_INFO(ionic, "VLAN TX HW offload set as a HW feature\n");
	if (lif->hw_features & IONIC_ETH_HW_VLAN_RX_STRIP)
		DBG_OPROM_INFO(ionic, "VLAN RX Strip offload set as HW feature\n");
	return 0;
}

/**
 * Remove previous vlan id.
 * */
static int ionic_remove_vlan(struct lif *lif, u32 vlan_filter_id)
{
	int err;
	struct ionic_admin_ctx ctx = {
		.cmd.rx_filter_del = {
			.opcode = IONIC_CMD_RX_FILTER_DEL,
			.filter_id = vlan_filter_id,
		},
	};

	err = ionic_adminq_post_wait(lif, &ctx);
	return err;
}

/**
 * Add new vlan id and rx filter.
 * */
int ionic_add_vlan(struct lif *lif, u32 vlan_id)
{
	int err;
	struct ionic *ionic = lif->ionic;
	struct ionic_admin_ctx ctx = {
		.cmd.rx_filter_add = {
			.opcode = IONIC_CMD_RX_FILTER_ADD,
			.match = IONIC_RX_FILTER_MATCH_VLAN,
			.vlan.vlan = vlan_id,
		},
	};

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err) {
		DBG_OPROM_ERR(ionic, "failed to add VLAN: %d (filter id: %d), error: %d\n",
			      vlan_id, ctx.comp.rx_filter_add.filter_id, err);
		return err;
	}
	lif->vlan_filter_id = ctx.comp.rx_filter_add.filter_id;
	return err;
}

/**
 * setup hardware features and vlan id
 * */
static int ionic_setup_vlan(struct ionic_dev *idev, struct lif *lif, u32 vlan_id,
			     bool vlan_en)
{
	int err;
	struct ionic *ionic = container_of(idev, struct ionic, idev);

	if (lif->vlan_en == vlan_en && lif->vlan_id == vlan_id) {
		DBG_OPROM_INFO(ionic, "Vlan mode and id has not changed\n");
		return 0;
	}

	// remove previously vlan id
	err = ionic_remove_vlan(lif, lif->vlan_filter_id);
	if (err) {
		DBG_OPROM_ERR(ionic, "failed to remove filter id: %d, warning: %d\n",
			      lif->vlan_filter_id, err);
	}

	err = ionic_dev_cmd_vlan_setattr(idev, devcmd_timeout, vlan_id, vlan_en);
	if (err) {
		DBG_OPROM_ERR(ionic, "Failed to set vlan attribute\n");
		return err;
	}

	if (vlan_en == 1) {
		// VLAN enabled from HII, enable vlan offloads in hardware
		err = ionic_set_hw_features(lif, IONIC_ETH_HW_VLAN_RX_FILTER |
					IONIC_ETH_HW_VLAN_TX_TAG |
					IONIC_ETH_HW_VLAN_RX_STRIP);
		if (err) {
			// unable to set hw features correctly.
			DBG_OPROM_ERR(ionic, "vlan enable setting hw features failed:%d\n",
				      err);
			return err;
		}
		// Adding new vlan id.
		err = ionic_add_vlan(lif, vlan_id);
		if (err) {
			return err;
		}
	} else {
		// VLAN disabled from HII, disable vlan offloads in hardware
		err = ionic_set_hw_features(lif, IONIC_ETH_HW_VLAN_RX_FILTER);
		if (err) {
			// unable to set hw features correctly.
			DBG_OPROM_ERR(ionic, "vlan enable setting hw features failed\n");
			return err;
		}
	}
	lif->vlan_en = vlan_en;
	lif->vlan_id = vlan_id;
	return 0;
}

/**
 * Get the HII settings from naples
 * */
static int ionic_hii_identify(struct ionic_dev *idev, struct lif *lif)
{
	int err;
	unsigned int i;
	unsigned int nwords;
	struct ionic_hii_identify_comp hii_comp;
	union ionic_hii_dev_identity hii_ident;
	struct ionic *ionic = container_of(idev, struct ionic, idev);

	err = ionic_dev_cmd_hii_identify(idev, devcmd_timeout);
	if (err) {
		DBG_OPROM_ERR(ionic, "Failed identifying the hii settings\n");
		return err;
	}
	ionic_dev_cmd_comp(idev, &hii_comp);
	if (hii_comp.status != 0) {
		DBG_OPROM_ERR(ionic, "HII_IDENTIFY_ERR: status: %d\n", hii_comp.status);
		return -EFAULT;
	}
	nwords = MIN(ARRAY_SIZE(hii_ident.words), ARRAY_SIZE(idev->dev_cmd->data));
	for (i = 0; i < nwords; i++)
		hii_ident.words[i] = readl(&idev->dev_cmd->data[i]);

	DBG_OPROM_INFO(ionic,
			"HII_DEFAULT_VALUE: oob_en: %d blink_led %d vlan: %d vlan_en %d cap %x\n",
			hii_ident.oob_en, hii_ident.uid_led_on, hii_ident.vlan,
			hii_ident.vlan_en, hii_ident.capabilities);
	lif->oob_en = hii_ident.oob_en;
	lif->uid_led_on = hii_ident.uid_led_on;
	lif->ncsi_cap = (hii_ident.capabilities >> IONIC_HII_CAPABILITY_NCSI) & 1;
	lif->oob_cap = (hii_ident.capabilities >> IONIC_HII_CAPABILITY_OOB) & 1;
	ionic_setup_vlan(idev, lif, hii_ident.vlan, hii_ident.vlan_en);
	return 0;
}

/**
 * deinitialize txq and rxq
 * */
void ionic_lif_queue_deinit(struct ionic *ionic)
{
	struct lif *lif = ionic->lif;

	lif->txqcqs->flags &= ~QCQ_F_INITED;
	lif->rxqcqs->flags &= ~QCQ_F_INITED;
}

/**
 * Initialize the lif
 * 1. Initialize all the queues.
 * 2. Get the MAC address.
 * */
int ionic_lif_init(struct net_device *netdev)
{
	struct ionic *ionic = netdev->priv;
	struct lif *lif = ionic->lif;
	struct ionic_dev *idev = &lif->ionic->idev;
	int err;

	err = ionic_dev_cmd_lif_init(idev, lif->index, lif->info_pa, devcmd_timeout);
	if (err) {
		DBG_OPROM_ERR(ionic, "lif initiation failed: %d\n", err);
		return err;
	}

	err = ionic_lif_adminq_init(lif);
	if (err) {
		DBG_OPROM_ERR(ionic, "adminq initiation failed: %d\n", err);
		goto err_reset_lif;
	}

	err = ionic_lif_notifyq_init(lif, lif->notifyqcqs);
	if (err) {
		DBG_OPROM_ERR(ionic, "failed to initialize the notifyq: %d\n", err);
		goto err_reset_lif;
	}

	err = ionic_lif_txq_init(lif, lif->txqcqs);
	if (err) {
		DBG_OPROM_ERR(ionic, "failed to initialize the txq: %d\n", err);
		goto err_reset_lif;
	}

	err = ionic_lif_rxq_init(lif, lif->rxqcqs);
	if (err) {
		DBG_OPROM_ERR(ionic, "failed to initialize the rxq: %d\n", err);
		goto err_reset_lif;
	}

	err = ionic_lif_station_mac_addr(lif, netdev);
	if (err) {
		DBG_OPROM_ERR(ionic, "lif station mac addr failed: %d\n", err);
		goto err_reset_lif;
	}

	err = ionic_set_hw_features(lif, IONIC_ETH_HW_VLAN_RX_FILTER);
	if (err) {
		// ignoring the error as this is not fatal at this point.
		DBG_OPROM_ERR(ionic, "lif setting hw features failed: %d\n", err);
	}

	err = ionic_hii_identify(idev, lif);
	if (err) {
		DBG_OPROM_ERR(ionic, "lif getting hii settings failed: %d\n", err);
		goto err_reset_lif;
	}
//	netdev->max_pkt_len = IONIC_MAX_MTU;
	return 0;

err_reset_lif:
	ionic_lif_reset(ionic);
	return err;
}

/**
 * Sets the rx mode for the lif, using adminq_post
 * */
int ionic_lif_rx_mode(struct lif *lif, unsigned int rx_mode)
{
	int err;
	struct ionic *ionic = lif->ionic;
	struct ionic_admin_ctx ctx = {
		.cmd.rx_mode_set = {
			.opcode = IONIC_CMD_RX_MODE_SET,
			.rx_mode = rx_mode,
		},
	};

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err) {
		DBG_OPROM_ERR(ionic, "setting mode failed\n");
		return err;
	}

	return 0;
}

/**
 * Enables the QCQ, using adminq_post
 * */
int ionic_qcq_enable(struct qcq *qcq)
{
	struct queue *q = &qcq->q;
	struct lif *lif = q->lif;
	struct ionic *ionic = lif->ionic;
	int err;
	struct ionic_admin_ctx ctx = {
		.cmd.q_control = {
			.opcode = IONIC_CMD_Q_CONTROL,
			.lif_index = lif->index,
			.type = q->type,
			.index = q->index,
			.oper = IONIC_Q_ENABLE,
		},
	};

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err) {
		DBG_OPROM_ERR(ionic, "enable queue failed\n");
		return err;
	}

	return 0;
}

/**
 * Disables the QCQ, using adminq_post
 * */
int ionic_qcq_disable(struct qcq *qcq)
{
	struct queue *q = &qcq->q;
	struct lif *lif = q->lif;
	struct ionic *ionic = lif->ionic;
	int err;
	struct ionic_admin_ctx ctx = {
		.cmd.q_control = {
			.opcode = IONIC_CMD_Q_CONTROL,
			.lif_index = lif->index,
			.type = q->type,
			.index = q->index,
			.oper = IONIC_Q_DISABLE,
		},
	};

	if (!lif->ionic->fw_running)
		return 0;

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err) {
		DBG_OPROM_ERR(ionic, "disable queue failed\n");
		return err;
	}
	return 0;
}

/**
 * Quiesce the lif, using adminq_post
 * */
int ionic_lif_quiesce(struct lif *lif)
{
	int err;
	struct ionic *ionic = lif->ionic;
	struct ionic_admin_ctx ctx = {
		.cmd.lif_setattr = {
			.opcode = IONIC_CMD_LIF_SETATTR,
			.attr = IONIC_LIF_ATTR_STATE,
			.index = lif->index,
			.state = IONIC_LIF_QUIESCE
		},
	};

	if (!lif->ionic->fw_running)
		return 0;

	err = ionic_adminq_post_wait(lif, &ctx);
	if (err) {
		DBG_OPROM_ERR(ionic, "failed to quiesce lif error = %d\n",  err);
		return err;
	}
	return 0;
}

/**
 * Cleans up the rxq of the remaining IOB
 * */
void ionic_rx_flush(struct lif *lif)
{
	unsigned int i;
	struct queue *rxq = &lif->rxqcqs->q;
	struct cq *rxcq = &lif->rxqcqs->cq;

	for (i = 0; i < NRXQ_DESC; i++) {
		if (lif->rx_iobuf[i])
			free_iob(lif->rx_iobuf[i]);
		lif->rx_iobuf[i] = NULL;
	}

	// increment the tail counter to mark that the queue is empty
	while (rxq->tail->index != rxq->head->index) {
		rxq->tail = rxq->tail->next;

		if (rxcq->tail->last) {
			rxcq->done_color = !rxcq->done_color;
		}
		rxcq->tail = rxcq->tail->next;
	}
}

/**
 * Cleans up the txq of the remaining IOB
 * */
void ionic_tx_flush(struct net_device *netdev, struct lif *lif)
{
	struct queue *txq = &lif->txqcqs->q;
	struct cq *txcq = &lif->txqcqs->cq;

	// increment the tail counter to mark that the queue is empty
	while (txq->tail->index != txq->head->index) {
		// Cancel remaining transmits.
		if (txq->lif->tx_iobuf[txq->tail->index]) {
			netdev_tx_complete_err(netdev, txq->lif->tx_iobuf[txq->tail->index], -ECANCELED );
			txq->lif->tx_iobuf[txq->tail->index] = NULL;
		}

		txq->tail = txq->tail->next;
		if (txcq->tail->last) {
			txcq->done_color = !txcq->done_color;
		}
		txcq->tail = txcq->tail->next;
	}
}

/**
 * Returns the available space in the q.
 * */
unsigned int ionic_q_space_avail(struct queue *q)
{
	unsigned int avail = q->tail->index;

	if (q->head->index >= avail)
		avail += q->head->left - 1;
	else
		avail -= q->head->index + 1;

	return avail;
}

/**
 * Checks if the q has space in it.
 * */
bool ionic_q_has_space(struct queue *q, unsigned int want)
{
	return ionic_q_space_avail(q) >= want;
}

/**
 * Allocates and assigns IOB for the rxq.
 * */
void ionic_rx_fill(struct net_device *netdev, u16 length)
{
	struct ionic *ionic = netdev->priv;
	struct queue *rxq = &ionic->lif->rxqcqs->q;
	struct ionic_rxq_desc *desc;
	struct io_buffer *iobuf;
	unsigned int i;

	for (i = ionic_q_space_avail(rxq); i; i--) {

		// Allocate I/O buffer
		iobuf = alloc_iob(length);
		if (!iobuf) {
			return;
		}
		desc = rxq->head->desc;
		desc->addr = virt_to_bus(iobuf->data);
		desc->len = length;
		desc->opcode = IONIC_RXQ_DESC_OPCODE_SIMPLE;
		rxq->lif->rx_iobuf[rxq->head->index] = iobuf;
		rxq->head = rxq->head->next;

		struct ionic_doorbell db = {
			.qid_lo = rxq->hw_index,
			.qid_hi = rxq->hw_index >> 8,
			.ring = 0,
			.p_index = rxq->head->index,
		};
		writeq(*(u64 *)&db, rxq->db);
	}
}

/**
 * Poll for received packets
 *
 * @v netdev				Network device
 */
void ionic_poll_rx(struct net_device *netdev)
{
	struct ionic *ionic = netdev->priv;

	struct queue *rxq = &ionic->lif->rxqcqs->q;
	struct cq *rxcq = &ionic->lif->rxqcqs->cq;
	struct ionic_rxq_desc *desc;
	struct ionic_rxq_comp *comp = rxcq->tail->cq_desc;
	struct io_buffer *iobuf;

	while (color_match(comp->pkt_type_color, rxcq->done_color)) {
		rmb();

		// update the cq_tail and check if the last descriptor.
		if (rxcq->tail->last) {
			rxcq->done_color = !rxcq->done_color;
		}
		rxcq->tail = rxcq->tail->next;

		// Populate I/O buffer
		iobuf = ionic->lif->rx_iobuf[rxq->tail->index];
		ionic->lif->rx_iobuf[rxq->tail->index] = NULL;

		if (comp->status ||
			(comp->csum_flags & IONIC_RXQ_COMP_CSUM_F_TCP_BAD) ||
			(comp->csum_flags & IONIC_RXQ_COMP_CSUM_F_UDP_BAD) ||
			(comp->csum_flags & IONIC_RXQ_COMP_CSUM_F_IP_BAD)) {
			desc = rxq->tail->desc;
			iob_put(iobuf, desc->len);
			netdev_rx_err(netdev, iobuf, -EIO);
		} else {
			iob_put(iobuf, comp->len);
			netdev_rx(netdev, iobuf);
		}

		// update the q tail index;
		rxq->tail = rxq->tail->next;
		// update the comp struct
		comp = rxcq->tail->cq_desc;
	}
}

/**
 * Poll for transmitted packets
 *
 * @v netdev				Network device
 */
void ionic_poll_tx(struct net_device *netdev)
{
	struct ionic *ionic = netdev->priv;
	struct queue *txq = &ionic->lif->txqcqs->q;
	struct cq *txcq = &ionic->lif->txqcqs->cq;
	struct ionic_txq_comp *comp = txcq->tail->cq_desc;
	unsigned int index;

	while (color_match(comp->color, txcq->done_color)) {
		rmb();

		// update the cq_tail and check if the last descriptor.
		if (txcq->tail->last) {
			txcq->done_color = !txcq->done_color;
		}
		txcq->tail = txcq->tail->next;

		// tx coalesing, go to end of the completion index
		do {
			// Complete transmit
			netdev_tx_complete(netdev, txq->lif->tx_iobuf[txq->tail->index]);
			txq->lif->tx_iobuf[txq->tail->index] = NULL;

			// update the q tail index;
			index = txq->tail->index;
			txq->tail = txq->tail->next;
		} while(index != comp->comp_index);

		// update the comp struct
		comp = txcq->tail->cq_desc;

	}
}

/**
 * Process all events in the notifyq
 */
static void ionic_process_notifyq(struct ionic *ionic, int process)
{
	struct cq *cq = &ionic->lif->notifyqcqs->cq;
	union ionic_notifyq_comp *comp = cq->tail->cq_desc;
	u16 ecode;

	while ((s64)(comp->event.eid - ionic->last_eid) > 0) {
		ionic->last_eid = comp->event.eid;

		rmb();

		// update the cq_tail and check if the last descriptor.
		if (cq->tail->last) {
			cq->done_color = !cq->done_color;
		}
		cq->tail = cq->tail->next;

		DBG_OPROM_INFO(ionic, "event id %d%s\n",
				comp->event.ecode, process ? "" : " (drain)");

		ecode = comp->event.ecode;
		comp = cq->tail->cq_desc;

		if (!process)
			continue;

		switch (ecode) {
		case IONIC_EVENT_LINK_CHANGE:
			ionic_check_link(ionic->netdev);
			if (ionic->link_status == IONIC_PORT_OPER_STATUS_UP &&
			    !ionic->qs_running) {
				ionic_start_queues(ionic);
			}
			else if (ionic->link_status != IONIC_PORT_OPER_STATUS_UP &&
				 ionic->qs_running) {
				ionic_stop_queues(ionic);
			}
			break;
		case IONIC_EVENT_RESET:
			ionic_handle_fw_down(ionic);
			break;
		}
	}
}

/**
 * Drain any out-of-date events
 */
void ionic_drain_notifyq(struct ionic *ionic)
{
	ionic_process_notifyq(ionic, false);
}

/**
 * Process waiting events
 */
void ionic_poll_notifyq(struct ionic *ionic)
{
	ionic_process_notifyq(ionic, true);
}

/**
 * Set System LED callback for HII page.
 * */
int ionic_set_system_led_cb(struct net_device *netdev, bool uid_led_status)
{
	struct ionic *ionic = netdev->priv;
	struct lif *lif = ionic->lif;
	int err;

	err = ionic_dev_cmd_system_led(&ionic->idev, devcmd_timeout, uid_led_status);
	if (err) {
		DBG_OPROM_ERR(ionic, "Failed blinking the led\n");
		return err;
	}
	lif->uid_led_on = uid_led_status;
	return 0;
}

/**
 * Add VLAN ID HII Page.
 * */
int ionic_add_vlan_cb(struct net_device *netdev, u32 vlan_id, bool vlan_en)
{
	struct ionic *ionic = netdev->priv;
	int err;

	err = ionic_setup_vlan(&ionic->idev, ionic->lif, vlan_id, vlan_en);
	if (err) {
		DBG_OPROM_ERR(ionic, "Failed to set vlan\n");
		return err;
	}

	return 0;
}

/**
 * OOB Enable HII Callback.
 * */
int ionic_oob_en_cb(struct net_device *netdev, bool oob_en)
{
	struct ionic *ionic = netdev->priv;
	struct lif *lif = ionic->lif;
	int err;

	err = ionic_dev_cmd_oob_en(&ionic->idev, devcmd_timeout, oob_en);
	if (err) {
		DBG_OPROM_ERR(ionic, "Failed to enable oob\n");
		return err;
	}
	lif->oob_en = oob_en;
	return 0;
}

/**
 * Load defaults HII Callback.
 * */
int ionic_load_defaults_cb(struct net_device *netdev)
{
	struct ionic *ionic = netdev->priv;
	struct lif *lif = ionic->lif;
	int err;

	err = ionic_dev_cmd_reset_hii(&ionic->idev, devcmd_timeout);
	if (err) {
		DBG_OPROM_ERR(ionic, "Failed to reset to defaults\n");
		return err;
	}

	err = ionic_hii_identify(&ionic->idev, lif);
	if (err) {
		DBG_OPROM_ERR(ionic, "lif getting hii settings failed\n");
		return err;
	}
	return 0;
}

/**
 * Oprom checkpoint callback.
 * Log 5 words in the dev command region as checkpoints
 * */
int ionic_checkpoint_cb(struct net_device *netdev,
                        __le32 checkpoint)
{
	struct ionic *ionic = netdev->priv;
	// write the checkpoint
	writel(0xAA55, &ionic->idev.dev_cmd->words[1]);
	writel(checkpoint, &ionic->idev.dev_cmd->words[2]);
	writel(2, &ionic->idev.dev_cmd->words[3]);
	writel(3, &ionic->idev.dev_cmd->words[4]);
	writel(0xAA55, &ionic->idev.dev_cmd->words[5]);
	return 0;
}

void ionic_vlan_id_sync_up(struct net_device *netdev __unused,
                          EFI_VLAN_FIND_DATA *VlanEntries, u16 NumofVlan)
{
	struct ionic *ionic = netdev->priv;
	struct lif *lif = ionic->lif;
	int i;

	// Check if hii_vlan is enabled.
	// Only install vlan filter if hii vlan is disabled.
	// else ignore.
	if (lif->vlan_en == 0) {
		// VlanEntries is what mnp driver create vlan id entries
		for(i = 0; i < NumofVlan; i++){
			ionic_add_vlan(lif, VlanEntries[i].VlanId);
			lif->vlan_id = VlanEntries[i].VlanId;
		}
	}
}
void ionic_oprom_dbg_msg_to_nic(struct ionic *ionic, char *string)
{
	struct ionic_dev *idev = &ionic->idev;
	char  *dest_data;
	u32   string_len = 0;
	int err;
	unsigned int i;

	string_len = MIN(strlen(string), (sizeof(idev->dev_cmd->data)-1));
	dest_data = (char *)&idev->dev_cmd->data[0];
	for (i = 0; i < string_len; i++)
		writeb(string[i], (dest_data+i));

	writeb('\0', (dest_data+i));

	err = ionic_dev_cmd_debug_msg(idev, devcmd_timeout);
	if ((err != -ETIMEDOUT) && (err != IONIC_RC_SUCCESS)) {
		// Stop sending msg to nic if err is other than timedout and success.
		ionic->oprom_msg_to_nic = 0;
		return;
	}
}


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

FILE_LICENCE(GPL2_OR_LATER_OR_UBDL);

#include "ionic.h"

/** @file
 *
 * Ionic network driver
 *
 */
/******************************************************************************
 *
 * Device
 *
 ******************************************************************************
 */

/**
 * Init hardware
 *
 * @v ionic				ionic device
 * @ret rc				Return status code
 */
static int ionic_init(struct ionic *ionic)
{

	struct ionic_dev *idev = &ionic->idev;

	return ionic_dev_cmd_init(idev, devcmd_timeout);
}

/**
 * Reset hardware
 *
 * @v ionic				ionic device
 * @ret rc				Return status code
 */
static int ionic_reset(struct ionic *ionic)
{

	struct ionic_dev *idev = &ionic->idev;

	return ionic_dev_cmd_reset(idev, devcmd_timeout);
}

/******************************************************************************
 *
 * Link state
 *
 ******************************************************************************
 */

/**
 * Check link state
 *
 * @v netdev				Network device
 */
int ionic_check_link ( struct net_device *netdev )
{
	struct ionic *ionic = netdev->priv;
	u16 link_up;

	link_up = ionic->lif->info->status.link_status;
	if (link_up != ionic->link_status) {
		ionic->link_status = link_up;
		if (link_up == IONIC_PORT_OPER_STATUS_UP) {
			netdev_link_up ( netdev );
		} else {
			netdev_link_down ( netdev );
		}
	}
	return 0;
}
/******************************************************************************
 *
 * Network device interface
 *
 ******************************************************************************
 */

/**
 * Start the queues
 */
int ionic_start_queues(struct ionic *ionic)
{
	u16 mtu;
	int err;

	DBG_OPROM_INFO(ionic, "\n");

	if (ionic->qs_running) {
		DBG_OPROM_INFO(ionic, "queues already running\n");
		return 0;
	}

	if (!ionic->fw_running) {
		DBG_OPROM_INFO(ionic, "fw not running\n");
		return 0;
	}

	// skip any old events
	ionic_drain_notifyq(ionic);

	err = ionic_qcq_enable(ionic->lif->txqcqs);
	if (err)
		return err;

	// fill rx buffers
	mtu = ETH_HLEN + ionic->netdev->mtu + 4;
	ionic_rx_fill(ionic->netdev, mtu);

	err = ionic_lif_rx_mode(ionic->lif, 0x1F);
	if (err)
		return err;

	err = ionic_qcq_enable(ionic->lif->rxqcqs);
	if (err)
		return err;

	ionic->qs_running = 1;
	return 0;
}


/**
 * Open network device
 *
 * @v netdev				Network device
 * @ret rc				Return status code
 */
static int ionic_open(struct net_device *netdev)
{
	struct ionic *ionic = netdev->priv;
	int err;

	DBG_OPROM_INFO(ionic, "\n");

	err = ionic_start_device(ionic);
	if (err)
		return err;

	err = ionic_start_queues(ionic);
	if (err) {
		ionic_stop_device(ionic);
		return err;
	}

	ionic_check_link(netdev);

	return 0;
}

/**
 * Stop the queues
 */
void ionic_stop_queues(struct ionic *ionic)
{
	DBG_OPROM_INFO(ionic, "\n");

	if (!ionic->qs_running) {
		DBG_OPROM_INFO(ionic, "queues already stopped\n");
		return;
	}
	ionic->qs_running = 0;

	// setting QCQ_F_INITED flag to 0.
	ionic_lif_queue_deinit(ionic);

	if (ionic_qcq_disable(ionic->lif->rxqcqs))
		DBG_OPROM_ERR(ionic, "Unable to disable rxqcq\n");

	if (ionic_qcq_disable(ionic->lif->txqcqs))
		DBG_OPROM_ERR(ionic, "Unable to disable txqcq\n");

	if (ionic_lif_quiesce(ionic->lif))
		DBG_OPROM_ERR(ionic, "Unable to quiesce lif\n");

	ionic_tx_flush(ionic->netdev, ionic->lif);

	ionic_rx_flush(ionic->lif);
}

/**
 * Close network device
 *
 * @v netdev				Network device
 */
static void ionic_close(struct net_device *netdev)
{
	struct ionic *ionic = netdev->priv;

	DBG_OPROM_INFO(ionic, "\n");

	ionic_stop_queues(ionic);
	ionic_stop_device(ionic);
}

/**
 * Transmit packet
 *
 * @v netdev			Network device
 * @v iobuf				I/O buffer
 * @ret rc				Return status code
 */
static int ionic_transmit(struct net_device *netdev,
						  struct io_buffer *iobuf)
{
	struct ionic *ionic = netdev->priv;
	struct queue *txq = &ionic->lif->txqcqs->q;
	struct ionic_txq_desc *desc = txq->head->desc;
	uint8_t flags = 0;

	if (!ionic_q_has_space(txq, 1)) {
		DBG_OPROM_ERR(ionic, "no more desc available the txq is full\n");
		return -ENOBUFS;
	}

	// fill the descriptor
	if (ionic->lif->vlan_en) {
		flags = IONIC_TXQ_DESC_FLAG_VLAN;
		desc->vlan_tci = cpu_to_le16(ionic->lif->vlan_id);
	} else {
		desc->hword0 = 0;
	}
	desc->len = cpu_to_le16(iob_len(iobuf));
	desc->hword1 = 0;
	desc->hword2 = 0;
	desc->cmd = encode_txq_desc_cmd(IONIC_TXQ_DESC_OPCODE_CSUM_NONE,
					flags, 0, virt_to_bus(iobuf->data));

	// store the iobuf in the txq
	txq->lif->tx_iobuf[txq->head->index] = iobuf;

	// increment the head for the q.
	txq->head = txq->head->next;

	// ring the doorbell
	struct ionic_doorbell db = {
		.qid_lo = txq->hw_index,
		.qid_hi = txq->hw_index >> 8,
		.ring = 0,
		.p_index = txq->head->index,
	};
	writeq(*(u64 *)&db, txq->db);

	return 0;
}

/**
 * Check for FW running
 */
static int ionic_get_fw_status(struct ionic *ionic)
{
	u8 fw_status_ready;
	u8 fw_status;

	/* firmware is useful only if the running bit is set and
	 * fw_status != 0xff (bad PCI read)
	 */
	fw_status = readb(&ionic->idev.dev_info->fw_status);
	fw_status_ready = (fw_status != 0xff) && (fw_status & IONIC_FW_STS_F_RUNNING);

	return fw_status_ready;
}

/**
 * Poll for completed and received packets
 *
 * @v netdev				Network device
 */
static void ionic_poll(struct net_device *netdev)
{
	struct ionic *ionic = netdev->priv;
	u8 fw_running;
	u8 change;
	u16 mtu;

	// Check FW status
	fw_running = ionic_get_fw_status(ionic);
	change = (ionic->fw_running != fw_running);

	if (change && fw_running) {
		DBG_OPROM_INFO(ionic, "... fw_running=%d\n", fw_running);
		ionic_handle_fw_up(ionic);
	}
	else if (change && !ionic->fw_running) {
		DBG_OPROM_INFO(ionic, "... fw_running=%d\n", fw_running);
		ionic_handle_fw_down(ionic);
	}

	// If FW is not running, skip the rest of the poll
	if (!ionic->fw_running)
		return;

	// Poll for transmit completions
	ionic_poll_tx(netdev);

	// Poll for receive completions
	ionic_poll_rx(netdev);

	// Refill receive ring
	mtu = ETH_HLEN + netdev->mtu + 4;
	ionic_rx_fill(netdev, mtu);

	//Update Link Status
	ionic_check_link(netdev);

	// Check for Notify events
	ionic_poll_notifyq(netdev->priv);
}

/** Skeleton network device operations */
static struct net_device_operations ionic_operations = {
	.open = ionic_open,
	.close = ionic_close,
	.transmit = ionic_transmit,
	.poll = ionic_poll,
};

/******************************************************************************
 *
 * Ionic PCI interface
 *
 ******************************************************************************
 */

/**
 * Map the bar registers and addresses.
 **/
static int ionic_map_bars(struct ionic *ionic, struct pci_device *pci)
{
	struct ionic_device_bar *bars = ionic->bars;
	unsigned int i, j;

	ionic->num_bars = 0;
	for (i = 0, j = 0; i < IONIC_IPXE_BARS_MAX; i++) {
		bars[j].len = pci_bar_size(pci, PCI_BASE_ADDRESS(i * 2));
		bars[j].bus_addr = pci_bar_start(pci, PCI_BASE_ADDRESS(i * 2));
		bars[j].vaddr = ioremap(bars[j].bus_addr, bars[j].len);
		if (!bars[j].vaddr) {
			DBG_OPROM_ERR_CONSOLE(pci, "Cannot memory-map BAR %d, aborting\n", j);
			return -ENODEV;
		}
		ionic->num_bars++;
		j++;
	}
	return 0;
}

/**
 * Unmap the bar registers and addresses.
 **/
static void ionic_unmap_bars(struct ionic *ionic)
{
	struct ionic_device_bar *bars = ionic->bars;
	unsigned int i;

	for (i = 0; i < IONIC_IPXE_BARS_MAX; i++)
		if (bars[i].vaddr) {
			iounmap(bars[i].vaddr);
			bars[i].bus_addr = 0;
			bars[i].vaddr = NULL;
			bars[i].len = 0;
		}
}

/**
 * Quiesce the device activity
 */
void ionic_stop_device(struct ionic *ionic)
{
	if (!ionic->fw_running) {
		DBG_OPROM_INFO(ionic, "already in down state\n");
		return;
	}

	// Stop the notify queue
	// We can't stop the adminqcq with the adminq, but it doesn't
	// really matter since we're resetting the whole mess anyway.
	if (ionic_qcq_disable(ionic->lif->notifyqcqs))
		DBG_OPROM_ERR(ionic, "Unable to disable notifyqcq\n");

	ionic_qcq_dealloc(ionic->lif->adminqcq);
	ionic->lif->adminqcq = NULL;
	ionic_qcq_dealloc(ionic->lif->notifyqcqs);
	ionic->lif->notifyqcqs = NULL;
	ionic_qcq_dealloc(ionic->lif->txqcqs);
	ionic->lif->txqcqs = NULL;
	ionic_qcq_dealloc(ionic->lif->rxqcqs);
	ionic->lif->rxqcqs = NULL;
	free_phys(ionic->lif->info, ionic->lif->info_sz);

	// Reset lif
	if (ionic_lif_reset(ionic))
		DBG_OPROM_ERR(ionic, "Unable to reset lif\n");

	free(ionic->lif);
	ionic->lif = NULL;

	// Reset card
	if (ionic_reset(ionic))
		DBG_OPROM_ERR(ionic, "Unable to reset card\n");

	ionic->fw_running = 0;
}

/**
 * Start device, either from probe or from fw restart
 */
int ionic_start_device(struct ionic *ionic)
{
	int errorcode;

	if (ionic->fw_running) {
		DBG_OPROM_INFO(ionic, "is already running\n");
		return 0;
	}
	ionic->fw_running = 1;

	// Init the NIC
	if ((errorcode = ionic_init(ionic)) != 0)
		goto err_out;

	// Identify the Ionic
	errorcode = ionic_identify(ionic);
	if (errorcode) {
		DBG_OPROM_ERR(ionic, "Cannot identify device: %d, aborting\n", errorcode);
		goto err_reset;
	}

	errorcode = ionic_lif_alloc(ionic, 0);
	if (errorcode) {
		DBG_OPROM_ERR(ionic, "Cannot allocate LIFs: %d, aborting\n", errorcode);
		goto err_reset;
	}

	errorcode = ionic_lif_init(ionic->netdev);
	if (errorcode) {
		DBG_OPROM_ERR(ionic, "Cannot initiate LIFs: %d, aborting\n", errorcode);
		goto err_alloc;
	}

	return 0;

err_alloc:
	ionic_qcq_dealloc(ionic->lif->adminqcq);
	ionic->lif->adminqcq = NULL;
	ionic_qcq_dealloc(ionic->lif->notifyqcqs);
	ionic->lif->notifyqcqs = NULL;
	ionic_qcq_dealloc(ionic->lif->txqcqs);
	ionic->lif->txqcqs = NULL;
	ionic_qcq_dealloc(ionic->lif->rxqcqs);
	ionic->lif->rxqcqs = NULL;
	free_phys(ionic->lif->info, sizeof(ionic->lif->info_sz));
	free(ionic->lif);
err_reset:
	ionic_reset(ionic);
err_out:
	ionic->fw_running = 0;
	return errorcode;
}

/**
 * Quiesce the queue activity for FW down
 *
 * Essentially, this is a short version of ionic_remove(), but
 * without actually removing the netdev
 */
void ionic_handle_fw_down(struct ionic *ionic)
{
	DBG_OPROM_INFO(ionic, "\n");

	// Stop the queues
	netdev_link_down(ionic->netdev);
	if (netdev_is_open(ionic->netdev))
		ionic_stop_queues(ionic);

	// Stop the device
	ionic_stop_device(ionic);
}

/**
 * Restart the queues after FW comes back up
 */
void ionic_handle_fw_up(struct ionic *ionic)
{
	DBG_OPROM_INFO(ionic, "\n");

	// Get the device running
	if (ionic_start_device(ionic) != 0)
		return;

	// Get the Tx/Rx queues running
	if (netdev_is_open(ionic->netdev))
		ionic_start_queues(ionic);

	ionic_check_link(ionic->netdev);
}

/**
 * Probe PCI device and setup the ionic driver.
 *
 * @v pci				PCI device
 * @ret rc				Return status code
 */
static int ionic_probe(struct pci_device *pci)
{
	struct net_device *netdev; // network device information.
	struct ionic *ionic = NULL;	   // ionic device information.
	int errorcode;
	uint32_t rom_base_addr;

	// Return if no opt ROM configured for this dev
	pci_read_config_dword ( pci, PCI_ROM_ADDRESS, &rom_base_addr);
	if (!rom_base_addr) {
		pci_write_config_dword(pci, PCI_ROM_ADDRESS, 0xfffff800);
		pci_read_config_dword(pci, PCI_ROM_ADDRESS, &rom_base_addr);
		pci_write_config_dword(pci, PCI_ROM_ADDRESS, 0);
		if (!rom_base_addr) {
			DBG_OPROM_ERR_CONSOLE(pci, "oprom disabled. dev %p\n", pci);
			return -ENODEV;
		}
	}

	// Allocate and initialise net device
	netdev = alloc_etherdev(sizeof(*ionic));
	if (!netdev) {
		errorcode = -ENOMEM;
		DBG_OPROM_ERR_CONSOLE(pci, "alloc_etherdev failed\n");
		goto err_alloc;
	}

	netdev_init(netdev, &ionic_operations);
	ionic = netdev->priv;
	pci_set_drvdata(pci, netdev);
	netdev->dev = &pci->dev;
	memset(ionic, 0, sizeof(*ionic));
	ionic->netdev = netdev;

	ionic->pdev = pci;
	// Fix up PCI device
	adjust_pci_device(pci);

	// Map registers
	errorcode = ionic_map_bars(ionic, pci);
	if (errorcode) {
		DBG_OPROM_ERR_CONSOLE(pci, "Failed to map bars\n");
		goto err_ionicunmap;
	}

	errorcode = ionic_setup(ionic);
	if (errorcode) {
		DBG_OPROM_ERR_CONSOLE(pci, "Cannot setup device, aborting\n");
		goto err_ionicunmap;
	}

	// Enable the bit here to indicate that debug messages can send to NIC
	ionic->oprom_msg_to_nic = 0;

	errorcode = ionic_start_device(ionic);
	if (errorcode) {
		DBG_OPROM_ERR(ionic, "Cannot start device, aborting\n");
		goto err_ionicunmap;
	}
	DBG_OPROM_INFO(ionic, "Ionic oprom init done\n");
	ionic_checkpoint_cb(netdev, IONIC_OPROM_INIT_DONE);

	// Register network device
	if ((errorcode = register_netdev(netdev)) != 0)
		goto err_register_netdev;

	ionic_checkpoint_cb(netdev, IONIC_OPROM_REGISTER_NETDEV_DONE);
	DBG_OPROM_INFO(ionic, "%s: Ionic oprom NETDEV Register done\n",
			ionic->netdev->name);

	return 0;

err_register_netdev:
	ionic_lif_reset(ionic);
err_ionicunmap:
	ionic_unmap_bars(ionic);
	netdev_nullify(netdev);
	netdev_put(netdev);
err_alloc:
	return errorcode;
}

/**
 * Remove PCI device
 *
 * @v pci				PCI device
 */
static void ionic_remove(struct pci_device *pci)
{
	struct net_device *netdev = pci_get_drvdata(pci);
	struct ionic *ionic = netdev->priv;

	// Unregister network device
	unregister_netdev(netdev);

	// Stop the device if not already stopped
	ionic_stop_device(ionic);

	ionic_unmap_bars(ionic);
	free_phys(ionic->idev.port_info, ionic->idev.port_info_sz);
	netdev_nullify(netdev);
	netdev_put(netdev);
}

/** Ionic PCI device IDs */
static struct pci_device_id ionic_nics[] = {
	PCI_ROM(0x1DD8, 0x1002, "ionic", "Pensando Eth-NIC PF", 0),
	PCI_ROM(0x1DD8, 0x1003, "ionic-vf", "Pensando Eth-NIC VF", 0),
};

/** Ionic PCI driver */
struct pci_driver ionic_driver __pci_driver = {
	.ids = ionic_nics,
	.id_count = ARRAY_SIZE(ionic_nics),
	.probe = ionic_probe,
	.remove = ionic_remove,
};

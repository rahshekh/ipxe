// SPDX-License-Identifier: GPL-2.0-only
/* Copyright(c) 2022 Pensando Systems, Inc */

#ifndef _PDS_MIG_IF_H_
#define _PDS_MIG_IF_H_

#include "common.h"

/**
 * enum pds_mig_cmd_opcode - Device commands
 */
enum pds_mig_cmd_opcode {
    /* Device state commands */
    PDS_MIG_CMD_STATUS          = 16,
    PDS_MIG_CMD_THROTTLE        = 17,
    PDS_MIG_CMD_SUSPEND         = 18,
    PDS_MIG_CMD_RESUME          = 19,
    PDS_MIG_CMD_SAVE            = 20,
    PDS_MIG_CMD_RESTORE         = 21,
    /* Dirty page tracking commands */
    PDS_MIG_CMD_DIRTY_STATUS    = 32,
    PDS_MIG_CMD_DIRTY_ENABLE    = 33,
    PDS_MIG_CMD_DIRTY_DISABLE   = 34,
    PDS_MIG_CMD_DIRTY_READ_SEQ  = 35,
    PDS_MIG_CMD_DIRTY_WRITE_ACK = 36,
};

/**
 * struct pds_mig_status_cmd - STATUS command
 * @opcode:	Opcode
 * @vf_id:  VF id
 */
struct pds_mig_status_cmd {
	u8 opcode;
	u8 rsvd;
	__le16 vf_id;
	u8 rsvd1[60];
};
PDS_CORE_CHECK_CMD_LENGTH(pds_mig_status_cmd);

/**
 * struct pds_mig_status_comp - STATUS command completion
 * @status:	Status of the command (enum pds_mig_status_code)
 * @size:   Size of the device state
 */
struct pds_mig_status_comp {
	u8 status;
	u8 rsvd[7];
	__le64 size;
};
PDS_CORE_CHECK_COMP_LENGTH(pds_mig_status_comp);

/**
 * struct pds_mig_throttle_cmd - THROTTLE command
 * @opcode:	Opcode
 * @vf_id:  VF id
 */
struct pds_mig_throttle_cmd {
	u8 opcode;
	u8 rsvd;
	__le16 vf_id;
	u8 rsvd1[60];
};
PDS_CORE_CHECK_CMD_LENGTH(pds_mig_throttle_cmd);

/**
 * struct pds_mig_throttle_comp - THROTTLE command completion
 * @status:	Status of the command (enum pds_mig_status_code)
 */
struct pds_mig_throttle_comp {
	u8 status;
	u8 rsvd[15];
};
PDS_CORE_CHECK_COMP_LENGTH(pds_mig_throttle_comp);

/**
 * struct pds_mig_suspend_cmd - SUSPEND command
 * @opcode:	Opcode
 * @vf_id:  VF id
 */
struct pds_mig_suspend_cmd {
	u8 opcode;
	u8 rsvd;
	__le16 vf_id;
	u8 rsvd1[60];
};
PDS_CORE_CHECK_CMD_LENGTH(pds_mig_suspend_cmd);

/**
 * struct pds_mig_suspend_comp - SUSPEND command completion
 * @status:	    Status of the command (enum pds_mig_status_code)
 * @state_size:	Size of the device state computed post suspend.
 */
struct pds_mig_suspend_comp {
	u8 status;
	u8 rsvd[7];
    __le64 state_size;
};
PDS_CORE_CHECK_COMP_LENGTH(pds_mig_suspend_comp);

/**
 * struct pds_mig_resume_cmd - RESUME command
 * @opcode:	Opcode
 * @vf_id:  VF id
 */
struct pds_mig_resume_cmd {
	u8 opcode;
	u8 rsvd;
	__le16 vf_id;
	u8 rsvd1[60];
};
PDS_CORE_CHECK_CMD_LENGTH(pds_mig_resume_cmd);

/**
 * struct pds_mig_resume_comp - RESUME command completion
 * @status:	Status of the command (enum pds_mig_status_code)
 */
struct pds_mig_resume_comp {
	u8 status;
	u8 rsvd[15];
};
PDS_CORE_CHECK_COMP_LENGTH(pds_mig_resume_comp);

/**
 * struct ionic_txq_sg_elem - Transmit scatter-gather (SG) descriptor element
 * @addr:      DMA address of SG element data buffer
 * @len:       Length of SG element data buffer, in bytes
 */
struct pds_mig_sg_elem {
	__le64 addr;
	__le16 len;
	__le16 rsvd[3];
};

/**
 * struct pds_mig_save_cmd - SAVE command
 * @opcode:	Opcode
 * @vf_id:  VF id
 * @sgl_addr: IOVA address of the SGL to dma the device state
 * @sgl_len: Total number of SG elements
 */
struct pds_mig_save_cmd {
	u8 opcode;
	u8 rsvd;
	__le16 vf_id;
	u8 rsvd2[4];
	__le64 sgl_addr;
	__le32 sgl_len;
	u8 rsvd3[44];
};
PDS_CORE_CHECK_CMD_LENGTH(pds_mig_save_cmd);

/**
 * struct pds_mig_save_comp - SAVE command completion
 * @status:	Status of the command (enum pds_mig_status_code)
 */
struct pds_mig_save_comp {
	u8 status;
	u8 rsvd[15];
};
PDS_CORE_CHECK_COMP_LENGTH(pds_mig_save_comp);

/**
 * struct pds_mig_restore_cmd - RESTORE command
 * @opcode:	Opcode
 * @vf_id:  VF id
 * @sgl_addr: IOVA address of the SGL to dma the device state
 * @sgl_len: Total number of SG elements
 */
struct pds_mig_restore_cmd {
	u8 opcode;
	u8 rsvd;
	__le16 vf_id;
	u8 rsvd2[4];
	__le64 sgl_addr;
	__le32 sgl_len;
	u8 rsvd3[44];
};
PDS_CORE_CHECK_CMD_LENGTH(pds_mig_restore_cmd);

/**
 * struct pds_mig_restore_comp - RESTORE command completion
 * @status:	Status of the command (enum pds_mig_status_code)
 */
struct pds_mig_restore_comp {
	u8 status;
	u8 rsvd[15];
};
PDS_CORE_CHECK_COMP_LENGTH(pds_mig_restore_comp);

/**
 * union pds_core_dev_state - device state information
 */
union pds_mig_dev_state {
	__le32 words[2048];
};
PDS_CORE_CHECK_DEVICE_STATE_LENGTH(pds_mig_dev_state);

/**
 * struct pds_mig_dirty_region_info - Memory region info for STATUS and ENABLE
 * @dma_base:   Base address of the DMA-contiguous memory region.
 * @page_count: Number of pages in the memory region.
 * @page_size_log2: Log2 page size in the memory region.
 */
struct pds_mig_dirty_region_info {
    __le64 dma_base;
    __le32 page_count;
    u8 page_size_log2;
    u8 rsvd[3];
};

/**
 * struct pds_mig_dirty_status_cmd - DIRTY_STATUS command
 * @opcode:     Opcode = PDS_MIG_CMD_DIRTY_STATUS
 * @vf_id:      VF id
 * @max_regions: Capacity of the region info buffer.
 * @regions_dma: DMA address of the region info buffer.
 *
 * The minimum of max_regions (from the command) and num_regions (from the
 * completion) of struct pds_mig_dirty_region_info will be written to
 * regions_dma.
 *
 * The max_regions may be zero, in which case regions_dma is ignored.  In that
 * case, the completion will only report the maximum number of regions
 * supported by the device, and the number of regions currently enabled.
 */
struct pds_mig_dirty_status_cmd {
    u8 opcode;
    u8 rsvd;
    __le16 vf_id;
    u8 max_regions;
    u8 rsvd1[3];
    __le64 regions_dma;
    u8 rsvd2[48];
};
PDS_CORE_CHECK_CMD_LENGTH(pds_mig_dirty_status_cmd);

/**
 * enum pds_mig_dirty_bmp_type - Type of dirty page bitmap.
 * @PDS_MIG_DIRTY_BMP_TYPE_NONE: No bitmap / disabled.
 * @PDS_MIG_DIRTY_BMP_TYPE_SEQ_ACK: Seq/Ack bitmap representation.
 */
enum pds_mig_dirty_bmp_type {
    PDS_MIG_DIRTY_BMP_TYPE_NONE     = 0,
    PDS_MIG_DIRTY_BMP_TYPE_SEQ_ACK  = 1,
};

/**
 * struct pds_mig_dirty_status_comp - STATUS command completion
 * @status:         Status of the command (enum pds_mig_status_code)
 * @max_regions:    Maximum number of regions supported by the device.
 * @num_regions:    Number of regions currently enabled.
 * @bmp_type:       Type of dirty bitmap representation.
 * @bmp_type_mask:  Mask of supported bitmap types, bit index per type.
 *
 * This completion descriptor is used for STATUS, ENABLE, and DISABLE.
 */
struct pds_mig_dirty_status_comp {
    u8 status;
    u8 max_regions;
    u8 num_regions;
    u8 bmp_type;
    __le32 bmp_type_mask;
    u8 rsvd[8];
};
PDS_CORE_CHECK_COMP_LENGTH(pds_mig_dirty_status_comp);

/**
 * struct pds_mig_dirty_enable_cmd - DIRTY_ENABLE command
 * @opcode:     Opcode = PDS_MIG_CMD_DIRTY_ENABLE
 * @vf_id:      VF id
 * @bmp_type: Type of dirty bitmap representation.
 * @num_regions: Number of entries in the region info buffer.
 * @regions_dma: DMA address of the region info buffer.
 *
 * The num_regions must be nonzero, and less than or equal to the maximum
 * number of regions supported by the device.
 *
 * The memory regions should not overlap.
 *
 * The information should be initialized by the driver.  The device may modify
 * the information on successful completion, such as by size-aligning the
 * number of pages in a region.
 *
 * The modified number of pages will be greater than or equal to the page count
 * given in the enable command, and at least as coarsly aligned as the given
 * value.  For example, the count might be aligned to a multiple of 64, but
 * if the value is already a multiple of 128 or higher, it will not change.
 * If the driver requires its own minimum alignment of the number of pages, the
 * driver should account for that already in the region info of this command.
 *
 * This command uses struct pds_mig_dirty_status_comp for its completion.
 */
struct pds_mig_dirty_enable_cmd {
    u8 opcode;
    u8 rsvd;
    __le16 vf_id;
    u8 bmp_type;
    u8 num_regions;
    u8 rsvd1[2];
    __le64 regions_dma;
    u8 rsvd2[48];
};
PDS_CORE_CHECK_CMD_LENGTH(pds_mig_dirty_enable_cmd);

/**
 * struct pds_mig_dirty_disable_cmd - DIRTY_DISABLE command
 * @opcode:     Opcode = PDS_MIG_CMD_DIRTY_DISABLE
 * @vf_id:      VF id
 *
 * Dirty page tracking will be disabled.  This may be called in any state, as
 * long as dirty page tracking is supported by the device, to ensure that dirty
 * page tracking is disabled.
 *
 * This command uses struct pds_mig_dirty_status_comp for its completion.  On
 * success, num_regions will be zero.
 */
struct pds_mig_dirty_disable_cmd {
    u8 opcode;
    u8 rsvd;
    __le16 vf_id;
    u8 rsvd2[60];
};
PDS_CORE_CHECK_CMD_LENGTH(pds_mig_dirty_disable_cmd);

/**
 * struct pds_mig_dirty_seq_ack_cmd - DIRTY_READ_SEQ or _WRITE_ACK command
 * @opcode:     Opcode = PDS_MIG_CMD_DIRTY_READ_SEQ or _WRITE_ACK
 * @vf_id:      VF id
 * @off_bytes: Byte offset in the bitmap.
 * @len_bytes: Number of bytes to transfer.
 * @num_sge: Number of DMA scatter gather elements.
 * @sgl_addr: DMA address of scatter gather list.
 *
 * Read bytes from the SEQ bitmap, or write bytes into the ACK bitmap.
 *
 * This command treats the entire bitmap as a byte buffer.  It does not
 * distinguish between guest memory regions.  The driver should refer to the
 * number of pages in each region, according to PDS_MIG_CMD_DIRTY_STATUS, to
 * determine the region boundaries in the bitmap.  Each region will be
 * represented by exactly the number of bits as the page count for that region,
 * immediately following the last bit of the previous region.
 */
struct pds_mig_dirty_seq_ack_cmd {
    u8 opcode;
    u8 rsvd;
    __le16 vf_id;
    __le32 off_bytes;
    __le32 len_bytes;
    __le16 num_sge;
    u8 rsvd2[2];
    __le64 sgl_addr;
    u8 rsvd3[40];
};
PDS_CORE_CHECK_CMD_LENGTH(pds_mig_dirty_seq_ack_cmd);

/**
 * struct pds_mig_dirty_seq_ack_comp - DIRTY_READ_SEQ or _WRITE_ACK completion
 * @status:     Status of the command (enum pds_mig_status_code)
 */
struct pds_mig_dirty_seq_ack_comp {
    u8 status;
    u8 rsvd[15];
};
PDS_CORE_CHECK_COMP_LENGTH(pds_mig_dirty_seq_ack_comp);

#endif /* _PDS_MIG_IF_H_ */

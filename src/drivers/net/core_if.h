// SPDX-License-Identifier: GPL-2.0-only
/* Copyright(c) 2022 Pensando Systems, Inc */

#ifndef _PDS_CORE_IF_H_
#define _PDS_CORE_IF_H_

#include "common.h"

#define PDS_CORE_BARS_MAX                      4
#define PDS_CORE_PCI_BAR_DBELL                 1

/* Bar0 */
#define PDS_CORE_DEV_INFO_SIGNATURE            0x44455649 /* 'DEVI' */
#define PDS_CORE_BAR0_SIZE                     0x8000
#define PDS_CORE_BAR0_DEV_INFO_REGS_OFFSET     0x0000
#define PDS_CORE_BAR0_DEV_CMD_REGS_OFFSET      0x0800
#define PDS_CORE_BAR0_DEV_CMD_DATA_REGS_OFFSET 0x0c00
#define PDS_CORE_BAR0_INTR_STATUS_OFFSET       0x1000
#define PDS_CORE_BAR0_INTR_CTRL_OFFSET         0x2000
#define PDS_CORE_DEV_CMD_DONE                  0x00000001

#define PDS_CORE_DEVCMD_TIMEOUT                5

// TODO: Don't assume cmd/comp lengths. Get at client registration.
// TODO: Use client context in cmds
/**
 * enum pds_core_cmd_opcode - Device commands
 */
enum pds_core_cmd_opcode {
    PDS_CORE_CMD_NOP   = 0,
    PDS_CORE_CMD_MAX   = 15,
    PDS_CORE_CMD_COUNT = 16,
};

/**
 * enum pds_core_status_code - Device command return codes
 */
enum pds_core_status_code {
    PDS_CORE_RC_SUCCESS  = 0,  /* Success */
    PDS_CORE_RC_EVERSION = 1,  /* Incorrect version for request */
    PDS_CORE_RC_EOPCODE  = 2,  /* Invalid cmd opcode */
    PDS_CORE_RC_EIO      = 3,  /* I/O error */
    PDS_CORE_RC_EPERM    = 4,  /* Permission denied */
    PDS_CORE_RC_EQID     = 5,  /* Bad qid */
    PDS_CORE_RC_EQTYPE   = 6,  /* Bad qtype */
    PDS_CORE_RC_ENOENT   = 7,  /* No such element */
    PDS_CORE_RC_EINTR    = 8,  /* operation interrupted */
    PDS_CORE_RC_EAGAIN   = 9,  /* Try again */
    PDS_CORE_RC_ENOMEM   = 10, /* Out of memory */
    PDS_CORE_RC_EFAULT   = 11, /* Bad address */
    PDS_CORE_RC_EBUSY    = 12, /* Device or resource busy */
    PDS_CORE_RC_EEXIST   = 13, /* object already exists */
    PDS_CORE_RC_EINVAL   = 14, /* Invalid argument */
    PDS_CORE_RC_ENOSPC   = 15, /* No space left or alloc failure */
    PDS_CORE_RC_ERANGE   = 16, /* Parameter out of range */
    PDS_CORE_RC_BAD_ADDR = 17, /* Descriptor contains a bad ptr */
    PDS_CORE_RC_DEV_CMD  = 18, /* Device cmd attempted on AdminQ */
    PDS_CORE_RC_ENOSUPP  = 19, /* Operation not supported */
    PDS_CORE_RC_ERROR    = 29, /* Generic error */
    PDS_CORE_RC_ERDMA    = 30, /* Generic RDMA error */
    PDS_CORE_RC_EVFID    = 31, /* VF ID does not exist */
    PDS_CORE_RC_BAD_FW   = 32, /* FW file is invalid or corrupted */
};

/**
 * struct pds_core_admin_cmd - General admin command format
 * @opcode:     Opcode for the command
 * @lif_index:  LIF index
 * @cmd_data:   Opcode-specific command bytes
 */
struct pds_core_admin_cmd {
    u8     opcode;
    u8     rsvd;
    __le16 lif_index;
    u8     cmd_data[60];
};
PDS_CORE_CHECK_CMD_LENGTH(pds_core_admin_cmd);

/**
 * struct pds_core_admin_comp - General admin command completion format
 * @status:     Status of the command (enum pds_core_status_code)
 * @comp_index: Index in the descriptor ring for which this is the completion
 * @cmd_data:   Command-specific bytes
 * @color:      Color bit (Always 0 for commands issued to the
 *              Device Cmd Registers)
 */
struct pds_core_admin_comp {
    u8     status;
    u8     rsvd;
    __le16 comp_index;
    u8     cmd_data[11];
    u8     color;
#define IONIC_COMP_COLOR_MASK 0x80
};
PDS_CORE_CHECK_COMP_LENGTH(pds_core_admin_comp);

/**
 * struct pds_core_nop_cmd - NOP command
 * @opcode: opcode
 */
struct pds_core_nop_cmd {
    u8 opcode;
    u8 rsvd[63];
};
PDS_CORE_CHECK_CMD_LENGTH(pds_core_nop_cmd);

/**
 * struct pds_core_nop_comp - NOP command completion
 * @status: Status of the command (enum pds_core_status_code)
 */
struct pds_core_nop_comp {
    u8 status;
    u8 rsvd[15];
};
PDS_CORE_CHECK_COMP_LENGTH(pds_core_nop_comp);

union pds_core_dev_cmd {
    u32                       words[16];
    struct pds_core_admin_cmd cmd;
    struct pds_core_nop_cmd   nop;
};
PDS_CORE_CHECK_CMD_LENGTH_UN(pds_core_dev_cmd);

union pds_core_dev_cmd_comp {
    u32                           words[4];
    u8                            status;
    struct pds_core_admin_comp    comp;
    struct pds_core_nop_comp      nop;
};
PDS_CORE_CHECK_COMP_LENGTH_UN(pds_core_dev_cmd_comp);

/**
 * union pds_core_dev_info_regs - Device info register format (read-only)
 * @signature:       Signature value of 0x44455649 ('DEVI')
 * @version:         Current version of info
 * @asic_type:       Asic type
 * @asic_rev:        Asic revision
 * @fw_version:      Firmware version
 * @serial_num:      Serial number
 */
union pds_core_dev_info_regs {
#define PDS_CORE_DEVINFO_FWVERS_BUFLEN 32
#define PDS_CORE_DEVINFO_SERIAL_BUFLEN 32
    struct {
        u32  signature;
        u8   version;
        u8   asic_type;
        u8   asic_rev;
        char fw_version[PDS_CORE_DEVINFO_FWVERS_BUFLEN];
        char serial_num[PDS_CORE_DEVINFO_SERIAL_BUFLEN];
    };
    u32 words[512];
};

/**
 * union pds_core_dev_cmd_regs - Device command register format (read-write)
 * @doorbell:        Device Cmd Doorbell, write-only
 *                   Write a 1 to signal device to process cmd,
 *                   poll done for completion.
 * @done:            Done indicator, bit 0 == 1 when command is complete
 * @cmd:             Opcode-specific command bytes
 * @comp:            Opcode-specific response bytes
 * @data:            Opcode-specific side-data
 */
union pds_core_dev_cmd_regs {
    struct {
        u32                         doorbell;
        u32                         done;
        union pds_core_dev_cmd      cmd;
        union pds_core_dev_cmd_comp comp;
        u8                          rsvd[48];
        u32                         data[478];
    } __attribute__((packed));
    u32 words[512];
};

/**
 * union pds_core_dev_regs - Device register format for bar 0 page 0
 * @info:            Device info registers
 * @devcmd:          Device command registers
 */
union pds_core_dev_regs {
    struct {
        union pds_core_dev_info_regs info;
        union pds_core_dev_cmd_regs  devcmd;
    } __attribute__((packed));
    __le32 words[1024];
};
PDS_CORE_CHECK_CMD_REGS_LENGTH(pds_core_dev_regs);
#endif /* _PDS_CORE_REGS_H_ */

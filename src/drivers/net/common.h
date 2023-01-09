// SPDX-License-Identifier: GPL-2.0-only
/* Copyright(c) 2022 Pensando Systems, Inc */

#ifndef _PDS_COMMON_H_
#define _PDS_COMMON_H_

/* Static size checks */
#define PDS_CORE_SIZE_CHECK(type, N, X)                                        \
    enum pds_core_static_assert_enum_##X                                       \
    {                                                                          \
        pds_core_static_assert_##X = (N) / (sizeof(type X) == (N))             \
    }
#define PDS_CORE_CHECK_CMD_LENGTH(X)      PDS_CORE_SIZE_CHECK(struct, 64, X)
#define PDS_CORE_CHECK_CMD_LENGTH_UN(X)   PDS_CORE_SIZE_CHECK(union, 64, X)
#define PDS_CORE_CHECK_COMP_LENGTH(X)     PDS_CORE_SIZE_CHECK(struct, 16, X)
#define PDS_CORE_CHECK_COMP_LENGTH_UN(X)  PDS_CORE_SIZE_CHECK(union, 16, X)
#define PDS_CORE_CHECK_CMD_DATA_LENGTH(X) PDS_CORE_SIZE_CHECK(union, 1912, X)
#define PDS_CORE_CHECK_CMD_REGS_LENGTH(X) PDS_CORE_SIZE_CHECK(union, 4096, X)
#define PDS_CORE_CHECK_DEVICE_STATE_LENGTH(X)                                  \
    PDS_CORE_SIZE_CHECK(union, 8192, X)

#endif /* _PDS_COMMON_H_ */

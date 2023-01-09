// SPDX-License-Identifier: GPL-2.0-only
/* Copyright(c) 2022 Pensando Systems, Inc */

#ifndef _PDS_REGS_H_
#define _PDS_REGS_H_

#include <linux/io.h>

/* DMA constraints */
#define PDS_DMA_ADDR_LEN	52

/**
 * struct pds_doorbell - Doorbell register layout
 * @p_index: Producer index
 * @ring:    Selects the specific ring of the queue to update
 *           Type-specific meaning:
 *              ring=0: Default producer/consumer queue
 *              ring=1: (CQ, EQ) Re-Arm queue.  RDMA CQs
 *              send events to EQs when armed.  EQs send
 *              interrupts when armed.
 * @qid_lo:  Queue destination for the producer index and flags (low bits)
 * @qid_hi:  Queue destination for the producer index and flags (high bits)
 */
struct pds_doorbell {
	__le16 p_index;
	u8     ring;
	u8     qid_lo;
	__le16 qid_hi;
	u16    rsvd2;
};

/**
 * struct pds_intr_ctrl - Interrupt control register
 * @coalescing_init:  Coalescing timer initial value, in
 *                    device units.  Use @identity->intr_coal_mult
 *                    and @identity->intr_coal_div to convert from
 *                    usecs to device units:
 *
 *                      coal_init = coal_usecs * coal_mutl / coal_div
 *
 *                    When an interrupt is sent the interrupt
 *                    coalescing timer current value
 *                    (@coalescing_curr) is initialized with this
 *                    value and begins counting down.  No more
 *                    interrupts are sent until the coalescing
 *                    timer reaches 0.  When @coalescing_init=0
 *                    interrupt coalescing is effectively disabled
 *                    and every interrupt assert results in an
 *                    interrupt.  Reset value: 0
 * @mask:             Interrupt mask.  When @mask=1 the interrupt
 *                    resource will not send an interrupt.  When
 *                    @mask=0 the interrupt resource will send an
 *                    interrupt if an interrupt event is pending
 *                    or on the next interrupt assertion event.
 *                    Reset value: 1
 * @int_credits:      Interrupt credits.  This register indicates
 *                    how many interrupt events the hardware has
 *                    sent.  When written by software this
 *                    register atomically decrements @int_credits
 *                    by the value written.  When @int_credits
 *                    becomes 0 then the "pending interrupt" bit
 *                    in the Interrupt Status register is cleared
 *                    by the hardware and any pending but unsent
 *                    interrupts are cleared.
 *                    !!!IMPORTANT!!! This is a signed register.
 * @flags:            Interrupt control flags
 *                       @unmask -- When this bit is written with a 1
 *                       the interrupt resource will set mask=0.
 *                       @coal_timer_reset -- When this
 *                       bit is written with a 1 the
 *                       @coalescing_curr will be reloaded with
 *                       @coalescing_init to reset the coalescing
 *                       timer.
 * @mask_on_assert:   Automatically mask on assertion.  When
 *                    @mask_on_assert=1 the interrupt resource
 *                    will set @mask=1 whenever an interrupt is
 *                    sent.  When using interrupts in Legacy
 *                    Interrupt mode the driver must select
 *                    @mask_on_assert=0 for proper interrupt
 *                    operation.
 * @coalescing_curr:  Coalescing timer current value, in
 *                    microseconds.  When this value reaches 0
 *                    the interrupt resource is again eligible to
 *                    send an interrupt.  If an interrupt event
 *                    is already pending when @coalescing_curr
 *                    reaches 0 the pending interrupt will be
 *                    sent, otherwise an interrupt will be sent
 *                    on the next interrupt assertion event.
 */
struct pds_intr_ctrl {
	u8 coalescing_init;
	u8 rsvd[3];
	u8 mask;
	u8 rsvd2[3];
	u16 int_credits;
	u16 flags;
#define INTR_F_UNMASK		0x0001
#define INTR_F_TIMER_RESET	0x0002
	u8 mask_on_assert;
	u8 rsvd3[3];
	u8 coalescing_curr;
	u8 rsvd4[3];
	u32 rsvd6[3];
};

#define PDS_INTR_CTRL_REGS_MAX	2048
#define PDS_INTR_CTRL_COAL_MAX	0x3F

#define intr_to_coal(intr_ctrl)		\
		((void __iomem *)&(intr_ctrl)->coalescing_init)
#define intr_to_mask(intr_ctrl)		\
		((void __iomem *)&(intr_ctrl)->mask)
#define intr_to_credits(intr_ctrl)	\
		((void __iomem *)&(intr_ctrl)->int_credits)
#define intr_to_mask_on_assert(intr_ctrl)\
		((void __iomem *)&(intr_ctrl)->mask_on_assert)

/** struct pds_intr - interrupt control register set.
 * @coal_init:			coalesce timer initial value.
 * @mask:			interrupt mask value.
 * @credits:			interrupt credit count and return.
 * @mask_assert:		interrupt mask value on assert.
 * @coal:			coalesce timer time remaining.
 */
struct pds_intr {
	u32 coal_init;
	u32 mask;
	u32 credits;
	u32 mask_assert;
	u32 coal;
	u32 rsvd[3];
};

/** enum pds_intr_mask_vals - valid values for mask and mask_assert.
 * @PDS_INTR_MASK_CLEAR:	unmask interrupt.
 * @PDS_INTR_MASK_SET:	mask interrupt.
 */
enum pds_intr_mask_vals {
	PDS_INTR_MASK_CLEAR		= 0,
	PDS_INTR_MASK_SET		= 1,
};

/** enum pds_intr_credits_bits - bitwise composition of credits values.
 * @PDS_INTR_CRED_COUNT:	bit mask of credit count, no shift needed.
 * @PDS_INTR_CRED_COUNT_SIGNED: bit mask of credit count, including sign bit.
 * @PDS_INTR_CRED_UNMASK:	unmask the interrupt.
 * @PDS_INTR_CRED_RESET_COALESCE: reset the coalesce timer.
 * @PDS_INTR_CRED_REARM:	unmask the and reset the timer.
 */
enum pds_intr_credits_bits {
	PDS_INTR_CRED_COUNT		= 0x7fffu,
	PDS_INTR_CRED_COUNT_SIGNED	= 0xffffu,
	PDS_INTR_CRED_UNMASK		= 0x10000u,
	PDS_INTR_CRED_RESET_COALESCE	= 0x20000u,
	PDS_INTR_CRED_REARM		= (PDS_INTR_CRED_UNMASK |
					   PDS_INTR_CRED_RESET_COALESCE),
};

static inline void pds_intr_coal_init(struct pds_intr __iomem *intr_ctrl,
					int intr_idx, u32 coal)
{
	iowrite32(coal, &intr_ctrl[intr_idx].coal_init);
}

static inline void pds_intr_mask(struct pds_intr __iomem *intr_ctrl,
				   int intr_idx, u32 mask)
{
	iowrite32(mask, &intr_ctrl[intr_idx].mask);
}

static inline void pds_intr_credits(struct pds_intr __iomem *intr_ctrl,
				      int intr_idx, u32 cred, u32 flags)
{
	if (WARN_ON_ONCE(cred > PDS_INTR_CRED_COUNT)) {
		cred = ioread32(&intr_ctrl[intr_idx].credits);
		cred &= PDS_INTR_CRED_COUNT_SIGNED;
	}

	iowrite32(cred | flags, &intr_ctrl[intr_idx].credits);
}

static inline void pds_intr_clean_flags(struct pds_intr __iomem *intr_ctrl,
					  int intr_idx, u32 flags)
{
	u32 cred;

	cred = ioread32(&intr_ctrl[intr_idx].credits);
	cred &= PDS_INTR_CRED_COUNT_SIGNED;
	cred |= flags;
	iowrite32(cred, &intr_ctrl[intr_idx].credits);
}

static inline void pds_intr_clean(struct pds_intr __iomem *intr_ctrl,
				    int intr_idx)
{
	pds_intr_clean_flags(intr_ctrl, intr_idx,
			       PDS_INTR_CRED_RESET_COALESCE);
}

static inline void pds_intr_mask_assert(struct pds_intr __iomem *intr_ctrl,
					  int intr_idx, u32 mask)
{
	iowrite32(mask, &intr_ctrl[intr_idx].mask_assert);
}

/** enum pds_dbell_bits - bitwise composition of dbell values.
 *
 * @PDS_DBELL_QID_MASK:	unshifted mask of valid queue id bits.
 * @PDS_DBELL_QID_SHIFT:	queue id shift amount in dbell value.
 * @PDS_DBELL_QID:		macro to build QID component of dbell value.
 *
 * @PDS_DBELL_RING_MASK:	unshifted mask of valid ring bits.
 * @PDS_DBELL_RING_SHIFT:	ring shift amount in dbell value.
 * @PDS_DBELL_RING:		macro to build ring component of dbell value.
 *
 * @PDS_DBELL_RING_0:		ring zero dbell component value.
 * @PDS_DBELL_RING_1:		ring one dbell component value.
 * @PDS_DBELL_RING_2:		ring two dbell component value.
 * @PDS_DBELL_RING_3:		ring three dbell component value.
 *
 * @PDS_DBELL_INDEX_MASK:	bit mask of valid index bits, no shift needed.
 */
enum pds_dbell_bits {
	PDS_DBELL_QID_MASK		= 0xffffff,
	PDS_DBELL_QID_SHIFT		= 24,

#define PDS_DBELL_QID(n) \
	(((u64)(n) & PDS_DBELL_QID_MASK) << PDS_DBELL_QID_SHIFT)

	PDS_DBELL_RING_MASK		= 0x7,
	PDS_DBELL_RING_SHIFT		= 16,

#define PDS_DBELL_RING(n) \
	(((u64)(n) & PDS_DBELL_RING_MASK) << PDS_DBELL_RING_SHIFT)

	PDS_DBELL_RING_0		= 0,
	PDS_DBELL_RING_1		= PDS_DBELL_RING(1),
	PDS_DBELL_RING_2		= PDS_DBELL_RING(2),
	PDS_DBELL_RING_3		= PDS_DBELL_RING(3),

	PDS_DBELL_INDEX_MASK		= 0xffff,
};

static inline void pds_dbell_ring(u64 __iomem *db_page, int qtype, u64 val)
{
#if defined(CONFIG_PDS_MNIC)
	wmb();
	writeq_relaxed(val, &db_page[qtype]);
#else
	writeq(val, &db_page[qtype]);
#endif
}

#endif	/* _PDS_REGS_H_ */
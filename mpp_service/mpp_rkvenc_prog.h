/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Progressive input (leafkvm): software line-count gating of ordinary
 * (offline) VEPU511 tasks. The userspace contract is in
 * include/rk-vcodec-prog.h; this is the kernel-internal part shared by the
 * vcodec channel layer (owner of the slots) and the rkvenc2 driver (owner
 * of the registers).
 *
 * A slot describes one tracked input frame. The vcodec layer attaches it to
 * the encoder task through MPP_DEV_SET_PROG -> mpp_session.prog_next ->
 * mpp_task.prog. rkvenc_run() arms the gate when the slot is progressive
 * and not yet complete; every later update writes the new line count while
 * the slot's pass is on the hardware.
 *
 * Every field is protected by rkvenc_prog_lock (irq-safe, also taken from
 * the encoder irq and the stall timer).
 */
#ifndef __MPP_RKVENC_PROG_H__
#define __MPP_RKVENC_PROG_H__

#include <linux/types.h>
#include <linux/spinlock.h>

struct mpp_dev;

#define RKVENC_PROG_SLOTS	3	/* one running + MAX_FRAME_NUM queued */
#define RKVENC_PROG_NOT_RUN	(~0U)

enum rkvenc_prog_state {
	RKVENC_PROG_FREE = 0,
	RKVENC_PROG_QUEUED,		/* in the channel's frame list */
	RKVENC_PROG_ACTIVE,		/* taken by the encode worker */
};

struct rkvenc_prog_slot {
	u32 state;		/* enum rkvenc_prog_state */
	u32 flags;		/* KMPP_FRM_IN_* */
	u32 final;		/* outcome decided, updates rejected */
	s64 dts;
	s64 pts;
	void *frame;		/* KmppFrame while queued/active */
	u32 rows;		/* ready luma rows */
	u32 height;
	u32 status;		/* KMPP_IN_STATUS_*, ENCODED until ended otherwise */
	u32 rows_start;		/* rows when the first pass started, or NOT_RUN */
	u32 hw_err;		/* the pass ended with a hardware error */
	u64 update_ns;		/* last update */
	u64 complete_ns;	/* rows reached height, 0 = not yet */
	u64 run_ns;		/* first pass started */
	u64 done_ns;		/* first pass ended */
	struct mpp_dev *mpp;	/* != NULL while a gated pass is on the hardware */
};

extern spinlock_t rkvenc_prog_lock;

/*
 * Called with rkvenc_prog_lock held. Write the slot's line count to the
 * hardware if its gated pass is running (update), or open the gate for good
 * (release: rows = height, used for abort and stall).
 */
void rkvenc_prog_update(struct rkvenc_prog_slot *slot);
void rkvenc_prog_release(struct rkvenc_prog_slot *slot);

#endif

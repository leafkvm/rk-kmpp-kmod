/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Progressive input for /dev/vcodec encoder channels (leafkvm extension).
 *
 * Implemented in vcodec/mpp_vcodec_prog.c (channel side) and
 * mpp_service/mpp_rkvenc2.c (registers). Design: rolling-encoding.md,
 * section "KMPP progressive input: production API".
 *
 * Model. A frame is submitted once, with VCODEC_CHAN_IN_FRM_RDY, before its
 * capture has finished. The encoder hardware then reads the source no
 * further than the rows the caller has declared ready. The caller advances
 * that count with VCODEC_CHAN_IN_FRM_PROGRESS until the frame is complete,
 * or aborts the frame. Every tracked frame ends in exactly one packet
 * record, which is the encoder's release of the input buffer.
 *
 * Submission: two KmppFrame fields, resolved by name like every other
 * field. A kernel without them rejects the name lookup, which is the
 * capability check.
 *
 *   in_flags    u32  KMPP_FRM_IN_* below; 0 = ordinary frame (unchanged
 *                    behaviour, drops stay silent)
 *   ready_rows  u32  with KMPP_FRM_IN_PROGRESSIVE: luma rows of the buffer
 *                    already in memory, counted from buffer row 0;
 *                    >= height means complete. Ignored otherwise.
 *
 * Tracked frames (in_flags != 0) are identified by their dts, which must be
 * strictly greater than the dts of every tracked frame submitted on the
 * channel before. Identity is scoped to the channel fd.
 *
 * Output: one KmppPacket per tracked frame, carrying its pts/dts. A record
 * with KMPP_PACKET_FLAG_DISCARD has no bitstream to send (length 0); its
 * in_status says why. Encoded packets keep their order; DISCARD records of
 * frames dropped before they ran can overtake them. New KmppPacket fields,
 * meaningful on tracked frames only (0 otherwise):
 *
 *   in_status      u32  KMPP_IN_STATUS_*
 *   in_rows_start  u32  ready rows when the hardware task started
 *                       (>= height: the encoder never had to wait)
 *   in_tail_us     u32  time from the update that completed the frame to the
 *                       end of the encode; 0 when it was complete at start
 */
#ifndef __RK_VCODEC_PROG_H__
#define __RK_VCODEC_PROG_H__

#include <linux/types.h>

/* KmppFrame in_flags */
#define KMPP_FRM_IN_TRACKED	(1U << 0)	/* one packet record guaranteed */
#define KMPP_FRM_IN_PROGRESSIVE	(1U << 1)	/* gate reads; implies TRACKED */

/* KmppPacket flag bit, next to KMPP_PACKET_FLAG_EOI (0x40) */
#define KMPP_PACKET_FLAG_DISCARD	(0x00000080)

/* KmppPacket in_status */
#define KMPP_IN_STATUS_ENCODED	0	/* normal packet */
#define KMPP_IN_STATUS_ABORTED	1	/* VCODEC_PROG_ABORT */
#define KMPP_IN_STATUS_STALLED	2	/* no progress for prog_stall_ms */
#define KMPP_IN_STATUS_DROPPED	3	/* encoder dropped it: rate control,
					 * size/config mismatch, stream
					 * overflow, channel stop/destroy */
#define KMPP_IN_STATUS_HW_ERROR	4	/* hardware error or task timeout */

/*
 * /dev/vcodec command (struct vcodec_msg.cmd), leafkvm range. Payload:
 * struct vcodec_frm_progress, msg.size must equal its size.
 *
 * Callable from any thread, serialized with other ioctls on the same fd.
 * It can wait for an earlier ioctl (including destruction), but does not
 * itself wait for frame completion. Destruction releases progressive gates
 * before waiting. The shared channel lifetime lock is acquired without
 * waiting; ABORT allocates its record.
 *
 * Returns 0, -ENOENT (no tracked frame with this dts is pending: it already
 * ended, its outcome is already decided, it was never submitted, or the
 * channel is being created/destroyed;
 * harmless for a late update), -EINVAL (bad size, unknown flags, non-zero
 * reserved field, or rows on a frame that is not progressive) or -ENOMEM.
 *
 * Submission errors (VCODEC_CHAN_IN_FRM_RDY): -EAGAIN queue full, -EINVAL
 * for a tracked frame the channel cannot take: not H.264/H.265, online,
 * smart pre-pass, rotation or vertical flip, offset_y != 0 (progressive),
 * eos, combo or force-pskip metadata, or a dts not greater than the last
 * tracked one. Progressive input is rejected on blocking-submission channels
 * (input_timeout == MPP_POLL_BLOCK); use input_timeout = 0.
 *
 * STOP suspends scheduling without cancelling tracked frames; progress and
 * COMPLETE/ABORT remain valid afterward. Bitrate-only SET_CFG remains an
 * in-place update. Live codec replacement (MPP_SET_VENC_INIT_KCFG) is rejected
 * once a channel has accepted tracked input: destroy and recreate it instead.
 * This preserves completion records and DTS identity for the channel lifetime.
 *
 * Kernel stall watchdog: module parameter prog_stall_ms (default 200, 0 =
 * off). A gated frame without an update for that long gets its gate opened
 * and ends as KMPP_IN_STATUS_STALLED.
 */
#define VCODEC_CHAN_IN_FRM_PROGRESS	0x00000480

#define VCODEC_PROG_COMPLETE	(1U << 0)	/* rows = height */
#define VCODEC_PROG_ABORT	(1U << 1)	/* end the frame without output */

struct vcodec_frm_progress {
	__s64 dts;		/* frame to update */
	__u32 rows;		/* cumulative ready luma rows; smaller than
				 * the stored value is ignored */
	__u32 flags;		/* VCODEC_PROG_* */
	__u32 reserved[2];	/* must be 0 */
};

#endif /* __RK_VCODEC_PROG_H__ */

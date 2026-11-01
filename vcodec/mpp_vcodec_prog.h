/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Progressive input (leafkvm): channel side. Contract in
 * include/rk-vcodec-prog.h, register side in mpp_service/mpp_rkvenc_prog.h.
 */
#ifndef __MPP_VCODEC_PROG_H__
#define __MPP_VCODEC_PROG_H__

#include "mpp_vcodec_base.h"
#include "rk-vcodec-prog.h"

/* channel create / destroy */
void mpp_vcodec_prog_reset(struct mpp_chan *chan);
void mpp_vcodec_prog_abort_all(struct mpp_chan *chan);

/* VCODEC_CHAN_IN_FRM_RDY, before queueing: 0, -EINVAL or -EAGAIN */
int mpp_vcodec_prog_submit(struct mpp_chan *chan, KmppFrame frame);

/* encode worker: frame dequeued; its slot (now active) or NULL */
struct rkvenc_prog_slot *mpp_vcodec_prog_take(struct mpp_chan *chan, KmppFrame frame);
/* encode worker: the active frame was dropped before reaching the hardware */
void mpp_vcodec_prog_dropped(struct mpp_chan *chan);

/* frame irq: before / after mpp_enc_int_process(); post returns the packet to emit */
void mpp_vcodec_prog_pre_int(struct mpp_chan *chan);
KmppPacket mpp_vcodec_prog_post_int(struct mpp_chan *chan, KmppPacket packet, MPP_RET ret);

/* VCODEC_CHAN_IN_FRM_PROGRESS */
int mpp_vcodec_prog_progress(int chan_id, const struct vcodec_frm_progress *p);

#endif

// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Progressive input (leafkvm): per-channel tracked frames, the
 * VCODEC_CHAN_IN_FRM_PROGRESS command and the per-frame records.
 *
 * A tracked frame owns one slot from submission until its record is
 * emitted. Slots live in the (static) channel entry and are protected by
 * rkvenc_prog_lock, which the encoder driver also takes from its irq; the
 * channel's chan_lock, when needed, is taken first.
 *
 * Every tracked frame ends in exactly one record: the encoder's packet with
 * in_status ENCODED, or a zero-length KMPP_PACKET_FLAG_DISCARD packet.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/errno.h>

#include "kmpp_log.h"
#include "kmpp_obj.h"
#include "kmpp_frame.h"
#include "kmpp_meta.h"
#include "kmpp_packet_impl.h"
#include "kmpp_venc_objs_impl.h"
#include "mpp_enc.h"
#include "mpp_rkvenc_prog.h"
#include "mpp_vcodec_prog.h"

#define PROG_IN_FLAGS   (KMPP_FRM_IN_TRACKED | KMPP_FRM_IN_PROGRESSIVE)

static void prog_slot_free(struct mpp_chan *chan, struct rkvenc_prog_slot *slot)
{
    if (chan->prog_active == slot)
        chan->prog_active = NULL;
    memset(slot, 0, sizeof(*slot));
}

void mpp_vcodec_prog_reset(struct mpp_chan *chan)
{
    unsigned long flags;
    u32 i;

    spin_lock_irqsave(&rkvenc_prog_lock, flags);
    for (i = 0; i < RKVENC_PROG_SLOTS; i++) {
        /* never leave a slot attached to the hardware */
        chan->prog_slot[i].rows = chan->prog_slot[i].height;
        rkvenc_prog_update(&chan->prog_slot[i]);
        prog_slot_free(chan, &chan->prog_slot[i]);
    }
    chan->prog_active = NULL;
    chan->prog_have_dts = 0;
    chan->prog_last_dts = 0;
    spin_unlock_irqrestore(&rkvenc_prog_lock, flags);
}

void mpp_vcodec_prog_abort_all(struct mpp_chan *chan)
{
    struct rkvenc_prog_slot *slot;
    unsigned long flags;
    u32 i;

    spin_lock_irqsave(&rkvenc_prog_lock, flags);
    for (i = 0; i < RKVENC_PROG_SLOTS; i++) {
        slot = &chan->prog_slot[i];
        if (slot->state == RKVENC_PROG_FREE)
            continue;
        if (slot->status == KMPP_IN_STATUS_ENCODED)
            slot->status = KMPP_IN_STATUS_ABORTED;
        rkvenc_prog_release(slot);
    }
    spin_unlock_irqrestore(&rkvenc_prog_lock, flags);
}

static struct rkvenc_prog_slot *prog_find(struct mpp_chan *chan, s64 dts)
{
    struct rkvenc_prog_slot *slot;
    u32 i;

    for (i = 0; i < RKVENC_PROG_SLOTS; i++) {
        slot = &chan->prog_slot[i];
        if (slot->state != RKVENC_PROG_FREE && !slot->final && slot->dts == dts)
            return slot;
    }

    return NULL;
}

int mpp_vcodec_prog_submit(struct mpp_chan *chan, KmppFrame frame)
{
    struct rkvenc_prog_slot *slot = NULL;
    rk_u32 in_flags = 0, ready = 0, height = 0, offset_y = 0, eos = 0;
    rk_s64 dts = 0, pts = 0;
    KmppShmPtr sptr;
    unsigned long flags;
    int ret = 0;
    u32 i;

    kmpp_frame_get_in_flags(frame, &in_flags);
    if (!in_flags)
        return 0;
    if (in_flags & ~PROG_IN_FLAGS)
        return -EINVAL;
    /* Submission holds ioctl_sem, also needed by progress. Never admit a
     * gated frame on a channel whose submit can wait for its output. */
    if ((in_flags & KMPP_FRM_IN_PROGRESSIVE) &&
        chan->input_timeout == MPP_POLL_BLOCK)
        return -EINVAL;
    in_flags |= KMPP_FRM_IN_TRACKED;

    kmpp_frame_get_ready_rows(frame, &ready);
    kmpp_frame_get_height(frame, &height);
    kmpp_frame_get_offset_y(frame, &offset_y);
    kmpp_frame_get_eos(frame, &eos);
    kmpp_frame_get_dts(frame, &dts);
    kmpp_frame_get_pts(frame, &pts);

    if (eos || !height)
        return -EINVAL;
    /* combo (JPEG) and force-pskip frames do not take the tracked path */
    if (!kmpp_frame_get_meta(frame, &sptr)) {
        KmppMeta meta = kmpp_obj_from_shmptr(&sptr);
        KmppFrame comb = NULL;
        rk_s32 pskip = 0, pskip_num = 0;

        if (meta) {
            kmpp_meta_get_obj_d(meta, KEY_COMBO_FRAME, &comb, NULL);
            kmpp_meta_get_s32_d(meta, KEY_INPUT_PSKIP, &pskip, 0);
            kmpp_meta_get_s32_d(meta, KEY_INPUT_PSKIP_NUM, &pskip_num, 0);
            if (comb || pskip || pskip_num)
                return -EINVAL;
        }
    }
    if (mpp_enc_prog_check(chan->handle, in_flags & KMPP_FRM_IN_PROGRESSIVE))
        return -EINVAL;
    /* rows are counted from buffer row 0; crop offsets not supported */
    if ((in_flags & KMPP_FRM_IN_PROGRESSIVE) && offset_y)
        return -EINVAL;

    spin_lock_irqsave(&rkvenc_prog_lock, flags);
    if (chan->prog_have_dts && dts <= chan->prog_last_dts) {
        ret = -EINVAL;
        goto out;
    }
    for (i = 0; i < RKVENC_PROG_SLOTS; i++) {
        if (chan->prog_slot[i].state == RKVENC_PROG_FREE) {
            slot = &chan->prog_slot[i];
            break;
        }
    }
    if (!slot) {
        ret = -EAGAIN;
        goto out;
    }

    memset(slot, 0, sizeof(*slot));
    slot->state = RKVENC_PROG_QUEUED;
    slot->flags = in_flags;
    slot->dts = dts;
    slot->pts = pts;
    slot->frame = frame;
    slot->height = height;
    slot->rows = (in_flags & KMPP_FRM_IN_PROGRESSIVE) ? ready : height;
    slot->status = KMPP_IN_STATUS_ENCODED;
    slot->rows_start = RKVENC_PROG_NOT_RUN;
    slot->update_ns = ktime_get_ns();
    if (slot->rows >= height)
        slot->complete_ns = slot->update_ns;
    chan->prog_last_dts = dts;
    chan->prog_have_dts = 1;
out:
    spin_unlock_irqrestore(&rkvenc_prog_lock, flags);

    return ret;
}

struct rkvenc_prog_slot *mpp_vcodec_prog_take(struct mpp_chan *chan, KmppFrame frame)
{
    struct rkvenc_prog_slot *slot, *found = NULL;
    unsigned long flags;
    u32 i;

    spin_lock_irqsave(&rkvenc_prog_lock, flags);
    for (i = 0; i < RKVENC_PROG_SLOTS; i++) {
        slot = &chan->prog_slot[i];
        if (slot->state == RKVENC_PROG_QUEUED && slot->frame == frame) {
            slot->state = RKVENC_PROG_ACTIVE;
            chan->prog_active = slot;
            found = slot;
            break;
        }
    }
    spin_unlock_irqrestore(&rkvenc_prog_lock, flags);

    return found;
}

/* the record fields of a slot, copied under the lock */
struct prog_rec {
    s64 dts, pts;
    u32 status, rows_start, tail_us;
};

static void prog_rec_fill(struct prog_rec *r, struct rkvenc_prog_slot *slot, u32 status)
{
    r->dts = slot->dts;
    r->pts = slot->pts;
    r->status = status;
    r->rows_start = slot->rows_start == RKVENC_PROG_NOT_RUN ? 0 : slot->rows_start;
    r->tail_us = 0;
    if (slot->rows_start < slot->height && slot->complete_ns && slot->done_ns > slot->complete_ns)
        r->tail_us = (u32)min_t(u64, (slot->done_ns - slot->complete_ns) / NSEC_PER_USEC, U32_MAX);
}

/*
 * Fill the record fields into the encoder's packet, or (discard) make a
 * DISCARD record, in a new packet or in the preallocated one passed.
 */
static KmppPacket prog_rec_packet(KmppPacket packet, const struct prog_rec *r, bool discard)
{
    KmppPacketImpl *p;

    if (discard) {
        if (!packet && (kmpp_packet_get(&packet) || !packet)) {
            kmpp_loge_f("dts %lld: no packet for the discard record\n", r->dts);
            return NULL;
        }
        kmpp_packet_set_pts(packet, r->pts);
        kmpp_packet_set_dts(packet, r->dts);
        kmpp_packet_set_length(packet, 0);
        kmpp_packet_set_flag(packet, KMPP_PACKET_FLAG_DISCARD);
    }
    p = (KmppPacketImpl *)kmpp_obj_to_entry(packet);
    if (p) {
        p->in_status = r->status;
        p->in_rows_start = r->rows_start;
        p->in_tail_us = r->tail_us;
    }

    return packet;
}

/*
 * Queue a record. Like mpp_vcodec_enc_add_packet_list() but without touching
 * the channel's re-encode state: a record for a queued frame can be emitted
 * while another frame is between its irq and its packet.
 */
static void prog_emit(struct mpp_chan *chan, KmppPacket packet)
{
    unsigned long flags;

    kmpp_spin_lock_irqsave(chan->pkt_lock, &flags);
    kmpp_list_add_tail(kmpp_obj_to_list_head(packet), &chan->pkt_list);
    kmpp_spin_unlock_irqrestore(chan->pkt_lock, &flags);

    kmpp_atomic_inc(chan->stream_count);
    kmpp_atomic_inc(chan->pkt_total_num);
    kmpp_wait_wakeup(chan->wait);
}

void mpp_vcodec_prog_dropped(struct mpp_chan *chan)
{
    struct rkvenc_prog_slot *slot;
    struct prog_rec r;
    KmppPacket packet;
    unsigned long flags;

    spin_lock_irqsave(&rkvenc_prog_lock, flags);
    slot = chan->prog_active;
    if (!slot) {
        spin_unlock_irqrestore(&rkvenc_prog_lock, flags);
        return;
    }
    prog_rec_fill(&r, slot, slot->status != KMPP_IN_STATUS_ENCODED ?
                  slot->status : KMPP_IN_STATUS_DROPPED);
    prog_slot_free(chan, slot);
    spin_unlock_irqrestore(&rkvenc_prog_lock, flags);

    packet = prog_rec_packet(NULL, &r, true);
    if (packet)
        prog_emit(chan, packet);
}

void mpp_vcodec_prog_pre_int(struct mpp_chan *chan)
{
    struct rkvenc_prog_slot *slot;
    unsigned long flags;
    u32 discard = 0;

    spin_lock_irqsave(&rkvenc_prog_lock, flags);
    slot = chan->prog_active;
    if (slot) {
        /* outcome decided now: a later abort gets -ENOENT */
        slot->final = 1;
        discard = slot->status != KMPP_IN_STATUS_ENCODED;
    }
    spin_unlock_irqrestore(&rkvenc_prog_lock, flags);

    if (discard)
        mpp_enc_set_prog_discard(chan->handle);
}

KmppPacket mpp_vcodec_prog_post_int(struct mpp_chan *chan, KmppPacket packet, MPP_RET ret)
{
    struct rkvenc_prog_slot *slot;
    struct prog_rec r;
    unsigned long flags;
    u32 status;

    spin_lock_irqsave(&rkvenc_prog_lock, flags);
    slot = chan->prog_active;
    if (!slot) {
        spin_unlock_irqrestore(&rkvenc_prog_lock, flags);
        return packet;
    }
    if (!ret && !packet) {
        /* a re-encode pass follows (ungated: the frame was complete) */
        slot->final = 0;
        spin_unlock_irqrestore(&rkvenc_prog_lock, flags);
        return NULL;
    }
    status = slot->status;
    if (!packet && status == KMPP_IN_STATUS_ENCODED)
        status = slot->hw_err ? KMPP_IN_STATUS_HW_ERROR : KMPP_IN_STATUS_DROPPED;
    prog_rec_fill(&r, slot, status);
    prog_slot_free(chan, slot);
    spin_unlock_irqrestore(&rkvenc_prog_lock, flags);

    if (status != KMPP_IN_STATUS_ENCODED && status != KMPP_IN_STATUS_ABORTED)
        kmpp_logi_f("chan %d dts %lld: input record status %u\n",
                    chan->chan_id, r.dts, status);

    return prog_rec_packet(packet, &r, !packet);
}

int mpp_vcodec_prog_progress(int chan_id, const struct vcodec_frm_progress *p)
{
    struct mpp_chan *chan = mpp_vcodec_get_chan_entry(chan_id, MPP_CTX_ENC);
    struct rkvenc_prog_slot *slot;
    KmppPacket rec = NULL, emit = NULL;
    struct prog_rec r;
    unsigned long cflags;
    int ret = 0;
    u32 rows;

    if (!chan)
        return -ENOENT;
    if ((p->flags & ~(VCODEC_PROG_COMPLETE | VCODEC_PROG_ABORT)) ||
        p->reserved[0] || p->reserved[1])
        return -EINVAL;

    /* Do not wait behind teardown (or creation of a replacement channel).
     * Keep resources alive until the discard record has been emitted. */
    if (!down_read_trylock(&chan->prog_lifetime))
        return -ENOENT;
    if (!chan->handle) {
        ret = -ENOENT;
        goto out_lifetime;
    }

    /* a frame aborted while queued gets its record here; allocate first */
    if ((p->flags & VCODEC_PROG_ABORT) && (kmpp_packet_get(&rec) || !rec)) {
        ret = -ENOMEM;
        goto out_lifetime;
    }

    kmpp_spin_lock_irqsave(chan->chan_lock, &cflags);
    spin_lock(&rkvenc_prog_lock);
    slot = prog_find(chan, p->dts);
    if (!slot) {
        ret = -ENOENT;
    } else if (p->flags & VCODEC_PROG_ABORT) {
        kmpp_list_head *head = kmpp_obj_to_list_head(slot->frame);

        if (slot->status == KMPP_IN_STATUS_ENCODED)
            slot->status = KMPP_IN_STATUS_ABORTED;
        if (slot->state == RKVENC_PROG_QUEUED && head && !kmpp_list_empty(head)) {
            /* still in the frame list: never reaches the encoder */
            kmpp_list_del_init(head);
            chan->frame_cnt--;
            prog_rec_fill(&r, slot, slot->status);
            prog_slot_free(chan, slot);
            emit = rec;
            rec = NULL;
        } else {
            /* with the worker or on the hardware: run to the end, discard */
            rkvenc_prog_release(slot);
        }
    } else if (!(slot->flags & KMPP_FRM_IN_PROGRESSIVE)) {
        ret = (p->flags & VCODEC_PROG_COMPLETE) ? 0 : -EINVAL;
    } else {
        rows = (p->flags & VCODEC_PROG_COMPLETE) ? slot->height : p->rows;
        slot->update_ns = ktime_get_ns();
        if (rows > slot->rows) {
            slot->rows = rows;
            if (rows >= slot->height && !slot->complete_ns)
                slot->complete_ns = slot->update_ns;
            rkvenc_prog_update(slot);
        }
    }
    spin_unlock(&rkvenc_prog_lock);
    kmpp_spin_unlock_irqrestore(chan->chan_lock, &cflags);

    if (emit)
        prog_emit(chan, prog_rec_packet(emit, &r, true));
    if (rec)
        kmpp_packet_put(rec);

out_lifetime:
    up_read(&chan->prog_lifetime);
    return ret;
}

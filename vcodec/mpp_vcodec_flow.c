/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define MODULE_TAG "mpp_vcodec"

#include "mpp_vcodec_flow.h"
#include "mpp_vcodec_base.h"
#include "mpp_vcodec_thread.h"
#include "mpp_enc.h"
#include "kmpp_log.h"
#include "mpp_buffer_impl.h"
#include "mpp_vcodec_debug.h"
#include "kmpp_packet_impl.h"
#include "kmpp_time.h"
#include "kmpp_obj.h"
#include "kmpp_venc_objs_impl.h"
#include "kmpp_meta.h"
#include "kmpp_atomic.h"

void mpp_vcodec_enc_add_packet_list(struct mpp_chan *chan_entry, KmppPacket packet);

static MPP_RET enc_chan_process_single_chan(RK_U32 chan_id)
{
    struct mpp_chan *chan_entry = mpp_vcodec_get_chan_entry(chan_id, MPP_CTX_ENC);
    struct mpp_chan *comb_chan = NULL;
    KmppFrame frame = NULL;
    KmppFrame comb_frame = NULL;
    unsigned long lock_flag;
    RK_U64 cfg_start = 0, cfg_end = 0;
    RK_U64 dts = 0;
    KmppShmPtr sptr;
    KmppMeta meta = NULL;
    RK_U32 pskip_num = 0;
    RK_U32 pskip = 0;
    void *comb_handle = NULL;
    MPP_RET ret = MPP_OK;

    if (!chan_entry) {
        kmpp_loge_f("chan_entry is NULL\n");
        return MPP_NOK;
    }

    kmpp_mutex_lock(chan_entry->chan_mutex);
    /* 1. check channel state */
    kmpp_spin_lock_irqsave(chan_entry->chan_lock, &lock_flag);
    if (chan_entry->state != CHAN_STATE_RUN) {
        kmpp_loge_f("chan %d state is no runing\n", chan_id);
        kmpp_spin_unlock_irqrestore(chan_entry->chan_lock, &lock_flag);
        kmpp_mutex_unlock(chan_entry->chan_mutex);
        return MPP_OK;
    }
    if (kmpp_atomic_read(chan_entry->runing) > 0) {
        mpp_vcodec_detail("chan %d state is wating irq\n", chan_id);
        kmpp_spin_unlock_irqrestore(chan_entry->chan_lock, &lock_flag);
        kmpp_mutex_unlock(chan_entry->chan_mutex);
        return MPP_OK;
    }
    kmpp_atomic_inc(chan_entry->runing);
    kmpp_spin_unlock_irqrestore(chan_entry->chan_lock, &lock_flag);

    /* 2. check re-encode */
    if (chan_entry->reenc) {
        mpp_vcodec_detail("chan %d re-encode\n", chan_id);
        mpp_enc_cfg_reg((MppEnc)chan_entry->handle, NULL);
        mpp_enc_hw_start((MppEnc)chan_entry->handle, NULL);
        goto __RETURN;
    }

    /* 3. try to get frame to process */
    kmpp_spin_lock_irqsave(chan_entry->chan_lock, &lock_flag);
    if (kmpp_list_empty(&chan_entry->frm_list)) {
        kmpp_atomic_dec(chan_entry->runing);
        kmpp_wait_wakeup(chan_entry->stop_wait);
        kmpp_spin_unlock_irqrestore(chan_entry->chan_lock, &lock_flag);
        kmpp_mutex_unlock(chan_entry->chan_mutex);
        return MPP_OK;
    }

    frame = kmpp_obj_from_list_head(chan_entry->frm_list.next);
    kmpp_list_del_init(chan_entry->frm_list.next);
    chan_entry->frame_cnt--;
    kmpp_spin_unlock_irqrestore(chan_entry->chan_lock, &lock_flag);
    chan_entry->gap_time = (RK_S32)(kmpp_time_us() - chan_entry->last_yuv_time);
    chan_entry->last_yuv_time = kmpp_time_us();
    mpp_vcodec_flow("chan %d get frame %p\n", chan_id, frame);

    /* 4. check combo frame */
    if (!kmpp_frame_get_meta(frame, &sptr)) {
        meta = kmpp_obj_from_shmptr(&sptr);
        kmpp_meta_get_obj_d(meta, KEY_COMBO_FRAME, &comb_frame, NULL);
    }
    if (comb_frame) {
        RK_S32 jpeg_chan_id = -1;
        RK_S32 drop = 0;

        if (!kmpp_frame_get_meta(comb_frame, &sptr)) {
            KmppMeta comb_meta = kmpp_obj_from_shmptr(&sptr);
            kmpp_meta_get_s32(comb_meta, KEY_CHANNEL_ID, &jpeg_chan_id);
        }

        mpp_vcodec_detail("combo jpeg id %d\n", jpeg_chan_id);
        comb_chan = mpp_vcodec_get_chan_entry(jpeg_chan_id, MPP_CTX_ENC);
        if (comb_chan) {
            kmpp_mutex_lock(comb_chan->chan_mutex);
            kmpp_spin_lock_irqsave(comb_chan->chan_lock, &lock_flag);
            if (comb_chan->state != CHAN_STATE_RUN) {
                kmpp_loge_f("chan %d combo chan %d state is no runing\n",
                            chan_id, jpeg_chan_id);
                drop = 1;
            }

            if (kmpp_atomic_read(comb_chan->runing) > 0) {
                kmpp_loge_f("chan %d combo chan %d state is wating irq\n",
                            chan_id, jpeg_chan_id);
                drop = 1;
            }
            kmpp_atomic_inc(comb_chan->runing);
            kmpp_atomic_inc(chan_entry->cfg.comb_runing);
            kmpp_spin_unlock_irqrestore(comb_chan->chan_lock, &lock_flag);

            if (drop) {
                KmppVencNtfy ntfy = mpp_enc_get_notify(chan_entry->handle);

                if (ntfy) {
                    KmppVencNtfyImpl* ntfy_impl = kmpp_obj_to_entry(ntfy);

                    ntfy_impl->cmd = KMPP_NOTIFY_VENC_TASK_DROP;
                    ntfy_impl->drop_type = KMPP_VENC_DROP_CFG_FAILED;
                    ntfy_impl->chan_id = comb_chan->chan_id;
                    ntfy_impl->frame = comb_frame;
                    kmpp_venc_notify(ntfy);
                }
                kmpp_atomic_dec(chan_entry->cfg.comb_runing);
                kmpp_atomic_dec(comb_chan->runing);
                kmpp_mutex_unlock(comb_chan->chan_mutex);
                comb_frame = NULL;
                comb_chan = NULL;
            }

            if (comb_chan && comb_chan->handle) {
                chan_entry->combo_gap_time = (RK_S32)(kmpp_time_us() - chan_entry->last_jeg_combo_start);
                chan_entry->last_jeg_combo_start = kmpp_time_us();
                chan_entry->binder_chan_id = jpeg_chan_id;
            }
        }
    }

    /* 5. check pskip frame */
    if (meta)
        kmpp_meta_get_s32(meta, KEY_INPUT_PSKIP, &pskip);
    if (pskip) {
        KmppPacket packet = NULL;
        KmppVencNtfy ntfy;

        mpp_vcodec_detail("chan %d force pskip\n", chan_id);
        ret = mpp_enc_force_pskip(chan_entry->handle, frame, &packet);
        if (packet)
            mpp_vcodec_enc_add_packet_list(chan_entry, packet);

        ntfy = mpp_enc_get_notify(chan_entry->handle);
        if (ntfy) {
            KmppVencNtfyImpl *ntfy_impl = kmpp_obj_to_entry(ntfy);

            ntfy_impl->chan_id = chan_id;
            ntfy_impl->frame = frame;

            ntfy_impl->cmd = KMPP_NOTIFY_VENC_TASK_DONE;
            ntfy_impl->is_intra = 0;
            kmpp_venc_notify(ntfy);
        }

        frame = NULL;
        kmpp_atomic_dec(chan_entry->runing);
        kmpp_wait_wakeup(chan_entry->stop_wait);
        goto __RETURN;
    }

    if (meta)
        kmpp_meta_get_s32(meta, KEY_INPUT_PSKIP_NUM, &pskip_num);

    /* 6. generate enc reg cfg */
    cfg_start = kmpp_time_us();
    mpp_vcodec_detail("chan %d generate enc reg cfg\n", chan_id);
    ret = mpp_enc_cfg_reg((MppEnc)chan_entry->handle, frame);
    if (ret)
        goto __RETURN;

    if (pskip_num > 0) {
        kmpp_frame_get(&chan_entry->pskip_frame);
        kmpp_frame_copy(chan_entry->pskip_frame, frame);
        if (!kmpp_frame_get_meta(chan_entry->pskip_frame, &sptr)) {
            meta = sptr.kptr;
            kmpp_meta_set_s32(meta, KEY_INPUT_PSKIP_NUM, pskip_num);
        }
    }

    /* 7. generate combo frame reg cfg */
    if (comb_chan && comb_chan->handle) {
        comb_handle = (void*)comb_chan->handle;
        comb_chan->master_chan_id = chan_entry->chan_id;
        ret = mpp_enc_cfg_reg((MppEnc)comb_chan->handle, comb_frame);
        if (ret) {
            KmppVencNtfy ntfy = mpp_enc_get_notify(comb_chan->handle);

            if (ntfy) {
                KmppVencNtfyImpl* ntfy_impl = kmpp_obj_to_entry(ntfy);

                ntfy_impl->cmd = KMPP_NOTIFY_VENC_TASK_DROP;
                ntfy_impl->drop_type = KMPP_VENC_DROP_CFG_FAILED;
                ntfy_impl->chan_id = comb_chan->chan_id;
                ntfy_impl->frame = comb_frame;
                kmpp_venc_notify(ntfy);
            }
            kmpp_loge_f("combo cfg fail\n");
            comb_frame = NULL;
            kmpp_atomic_dec(chan_entry->cfg.comb_runing);
            kmpp_atomic_dec(comb_chan->runing);
            kmpp_wait_wakeup(comb_chan->stop_wait);
            comb_handle = NULL;
            comb_chan->master_chan_id = -1;
        }
    }

    kmpp_frame_get_dts(frame, &dts);
    mpp_vcodec_detail("chan %d send reg task\n", chan_id);
    /* 8. send reg task to hw */
    ret = mpp_enc_hw_start((MppEnc)chan_entry->handle, comb_handle);

    chan_entry->seq_encoding = dts;
    cfg_end = kmpp_time_us();
    chan_entry->last_cfg_time = cfg_end - cfg_start;

__RETURN:
    if (ret) {
        struct venc_module *venc = NULL;

        venc = mpp_vcodec_get_enc_module_entry();
        if (frame) {
            KmppVencNtfy ntfy = mpp_enc_get_notify(chan_entry->handle);

            if (ntfy) {
                KmppVencNtfyImpl* ntfy_impl = kmpp_obj_to_entry(ntfy);

                ntfy_impl->cmd = KMPP_NOTIFY_VENC_TASK_DROP;
                ntfy_impl->drop_type = KMPP_VENC_DROP_CFG_FAILED;
                ntfy_impl->chan_id = chan_entry->chan_id;
                ntfy_impl->frame = frame;
                kmpp_venc_notify(ntfy);
            }
            frame = NULL;
        }
        if (comb_frame) {
            KmppVencNtfy ntfy = mpp_enc_get_notify(comb_chan->handle);

            if (ntfy) {
                KmppVencNtfyImpl* ntfy_impl = kmpp_obj_to_entry(ntfy);

                ntfy_impl->cmd = KMPP_NOTIFY_VENC_TASK_DROP;
                ntfy_impl->drop_type = KMPP_VENC_DROP_CFG_FAILED;
                ntfy_impl->chan_id = comb_chan->chan_id;
                ntfy_impl->frame = comb_frame;
                kmpp_venc_notify(ntfy);
            }
            comb_frame = NULL;
            comb_chan->master_chan_id = -1;
            kmpp_atomic_dec(chan_entry->cfg.comb_runing);
            kmpp_atomic_dec(comb_chan->runing);
            kmpp_wait_wakeup(comb_chan->stop_wait);
        }
        if (chan_entry->cfg.online)
            mpp_enc_online_task_failed(chan_entry->handle);

        kmpp_atomic_dec(chan_entry->runing);
        kmpp_wait_wakeup(chan_entry->stop_wait);
        vcodec_thread_trigger(venc->thd);
    }
    enc_chan_update_tab_after_enc(chan_id);
    if (comb_chan)
        kmpp_mutex_unlock(comb_chan->chan_mutex);
    kmpp_mutex_unlock(chan_entry->chan_mutex);

    return MPP_OK;
}

void mpp_vcodec_enc_add_packet_list(struct mpp_chan *chan_entry, KmppPacket packet)
{
    unsigned long flags;

    kmpp_spin_lock_irqsave(chan_entry->pkt_lock, &flags);
    kmpp_list_add_tail(kmpp_obj_to_list_head(packet), &chan_entry->pkt_list);
    kmpp_spin_unlock_irqrestore(chan_entry->pkt_lock, &flags);

    kmpp_atomic_inc(chan_entry->stream_count);
    kmpp_atomic_inc(chan_entry->pkt_total_num);
    kmpp_packet_get_dts(packet, &chan_entry->seq_encoded);
    kmpp_wait_wakeup(chan_entry->wait);

    mpp_vcodec_flow("chan %d add packet dts %lld total %d\n",
                    chan_entry->chan_id, chan_entry->seq_encoded,
                    kmpp_atomic_read(chan_entry->stream_count));

    chan_entry->reenc = 0;
}

static void mpp_vcodec_event_frame(int chan_id)
{
    struct mpp_chan *chan_entry = mpp_vcodec_get_chan_entry(chan_id, MPP_CTX_ENC);
    struct mpp_chan *comb_entry = NULL;
    struct venc_module *venc = NULL;
    KmppPacket jpeg_packet = NULL;
    KmppPacket packet = NULL;
    MPP_RET ret = MPP_OK;

    chan_entry->reenc = 1;
    venc = mpp_vcodec_get_enc_module_entry();
    if (!venc) {
        kmpp_loge_f("get_enc_module_entry fail");
        return;
    }
    if (kmpp_atomic_read(chan_entry->cfg.comb_runing)) {
        comb_entry = mpp_vcodec_get_chan_entry(chan_entry->binder_chan_id, MPP_CTX_ENC);
        if (comb_entry && comb_entry->handle) {
            chan_entry->last_jeg_combo_end = kmpp_time_us();
            ret = mpp_enc_int_process((MppEnc)chan_entry->handle, comb_entry->handle,
                                      &packet, &jpeg_packet);
            if (jpeg_packet)
                mpp_vcodec_enc_add_packet_list(comb_entry, jpeg_packet);
        }
        kmpp_atomic_dec(chan_entry->cfg.comb_runing);
        kmpp_atomic_dec(comb_entry->runing);
        kmpp_wait_wakeup(comb_entry->stop_wait);
        chan_entry->binder_chan_id = -1;
        comb_entry->master_chan_id = -1;
    } else {
        ret = mpp_enc_int_process((MppEnc)chan_entry->handle, NULL, &packet, &jpeg_packet);
    }

    if (packet)
        mpp_vcodec_enc_add_packet_list(chan_entry, packet);

    if (ret) {
        chan_entry->reenc = 0;
    } else {
        if (!chan_entry->reenc && chan_entry->pskip_frame) {
            KmppShmPtr sptr;
            RK_S64 pts_diff = 1000000 / mpp_enc_get_fps_out(chan_entry->handle);
            RK_S64 pts;
            RK_U32 pskip_num = 0;
            MPP_RET ret;
            RK_U32 i;

            if (!kmpp_frame_get_meta(chan_entry->pskip_frame, &sptr)) {
                KmppMeta meta = kmpp_obj_from_shmptr(&sptr);

                kmpp_meta_get_s32(meta, KEY_INPUT_PSKIP_NUM, &pskip_num);
            }

            kmpp_frame_get_pts(chan_entry->pskip_frame, &pts);
            for (i = 0; i < pskip_num; i++) {
                packet = NULL;
                kmpp_frame_set_pts(chan_entry->pskip_frame, pts + i * pts_diff);
                ret = mpp_enc_force_pskip(chan_entry->handle, chan_entry->pskip_frame, &packet);
                if (packet)
                    mpp_vcodec_enc_add_packet_list(chan_entry, packet);
            }

            kmpp_frame_put(chan_entry->pskip_frame);
            chan_entry->pskip_frame = NULL;
        }
    }

    kmpp_atomic_dec(chan_entry->runing);
    kmpp_wait_wakeup(chan_entry->stop_wait);
    vcodec_thread_trigger(venc->thd);

    return;
}

static void mpp_vcodec_event_slice(int chan_id, void *param)
{
    struct mpp_chan *chan_entry = mpp_vcodec_get_chan_entry(chan_id, MPP_CTX_ENC);
    KmppPacket packet = NULL;

    if (kmpp_atomic_read(chan_entry->cfg.comb_runing)) {
        kmpp_loge_f("combo running fail because slice fifo.");
    }

    mpp_enc_slice_info((MppEnc)chan_entry->handle, param, &packet);
    if (packet)
        mpp_vcodec_enc_add_packet_list(chan_entry, packet);

    return;
}

void mpp_vcodec_enc_int_handle(RK_S32 chan_id, RK_S32 event, void *param)
{
    switch (event) {
    case MPP_VCODEC_EVENT_FRAME: {
        mpp_vcodec_event_frame(chan_id);
    } break;
    case MPP_VCODEC_EVENT_SLICE: {
        mpp_vcodec_event_slice(chan_id, param);
    } break;
    default: {
        kmpp_loge_f("mpp vcodec event %d is invalid.\n", event);
    } break;
    }

    return;
}

int mpp_vcodec_enc_run_task(RK_U32 chan_id, RK_S64 pts, RK_S64 dts)
{
    struct mpp_chan *chan_entry = mpp_vcodec_get_chan_entry(chan_id, MPP_CTX_ENC);
    unsigned long lock_flag;

    if (!chan_entry)
        return MPP_NOK;

    kmpp_spin_lock_irqsave(chan_entry->chan_lock, &lock_flag);
    if (chan_entry->state != CHAN_STATE_RUN) {
        kmpp_spin_unlock_irqrestore(chan_entry->chan_lock, &lock_flag);
        return MPP_NOK;
    }
    kmpp_spin_unlock_irqrestore(chan_entry->chan_lock, &lock_flag);

    return mpp_enc_run_task(chan_entry->handle, pts, dts);
}

int mpp_vcodec_enc_set_online_mode(RK_U32 chan_id, RK_U32 mode)
{
    struct mpp_chan *chan_entry = mpp_vcodec_get_chan_entry(chan_id, MPP_CTX_ENC);
    unsigned long lock_flag;

    if (!chan_entry)
        return MPP_NOK;

    kmpp_spin_lock_irqsave(chan_entry->chan_lock, &lock_flag);
    if (chan_entry->state != CHAN_STATE_RUN) {
        kmpp_spin_unlock_irqrestore(chan_entry->chan_lock, &lock_flag);
        return MPP_NOK;
    }
    kmpp_spin_unlock_irqrestore(chan_entry->chan_lock, &lock_flag);

    if (mode >= MPP_ENC_ONLINE_MODE_BUT) {
        kmpp_loge_f("online mode invalid %d\n", mode);
        return MPP_NOK;
    }

    chan_entry->cfg.online = mode;

    return mpp_enc_set_online_mode(chan_entry->handle, mode);
}

int mpp_vcodec_enc_routine(void *param)
{
    RK_U32 started_chan_num = 0;
    RK_U32 next_chan;
    unsigned long lock_flag;
    struct venc_module *venc = mpp_vcodec_get_enc_module_entry();

    if (!venc) {
        kmpp_loge_f("get_enc_module_entry fail\n");
        return MPP_ERR_VALUE;
    }
    kmpp_spin_lock_irqsave(venc->enc_lock, &lock_flag);
    started_chan_num = venc->started_chan_num;
    venc->chan_pri_tab_index = 0;
    kmpp_spin_unlock_irqrestore(venc->enc_lock, &lock_flag);
    mpp_vcodec_detail("mpp_vcodec_enc_routine started_chan_num %d\n", started_chan_num);
    while (started_chan_num--) {
        /* get high prior chan id */
        enc_chan_get_high_prior_chan();
        next_chan = venc->curr_high_prior_chan;
        if (next_chan >= MAX_ENC_NUM)
            continue;
        if (enc_chan_process_single_chan(next_chan) != MPP_OK)
            break;
    }
    mpp_vcodec_detail("mpp_vcodec_enc_routine end\n");

    return MPP_OK;
}

void *mpp_vcodec_dec_routine(void *param)
{
    return NULL;
}

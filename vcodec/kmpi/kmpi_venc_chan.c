/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */
#include "mpp_mem.h"
#include "mpp_enc.h"
#include "kmpp_log.h"
#include "mpp_enc_cfg.h"
#include "kmpp_packet_impl.h"
#include "kmpp_obj.h"
#include "kmpp_venc_objs_impl.h"
#include "kmpp_atomic.h"

#include "mpp_vcodec_base.h"
#include "mpp_vcodec_thread.h"
#include "mpp_vcodec_chan.h"
#include "mpp_vcodec_flow.h"
#include "mpp_vcodec_debug.h"

static rk_s32 check_chan_idle(const void *param)
{
    return !kmpp_atomic_read((KmppAtomic *)param);
}

rk_s32 kmpp_venc_chan_init(KmppChanId *id, KmppVencInitCfg cfg)
{
    MppEnc enc = NULL;
    MPP_RET ret = MPP_OK;
    KmppChanId chan_id = *id;
    struct mpp_chan *chan = NULL;
    KmppVencInitCfgImpl *cfg_impl = (KmppVencInitCfgImpl*)kmpp_obj_to_entry(cfg);

    chan_id = mpp_vcodec_get_free_chan(MPP_CTX_ENC, chan_id);
    if (chan_id < 0)
        return MPP_NOK;

    chan = mpp_vcodec_get_chan_entry(chan_id, MPP_CTX_ENC);

    cfg_impl->chan_id = chan_id;
    cfg_impl->shared_buf = &chan->shared_buf;

    ret = mpp_enc_init(&enc, cfg_impl);
    if (ret) {
        kmpp_loge_f("mpp_enc_init failed ret %d\n", ret);
        return ret;
    }

    mpp_enc_register_chl(enc, mpp_vcodec_enc_int_handle, cfg_impl->chan_id);
    mpp_vcodec_chan_entry_init(chan, MPP_CTX_ENC, cfg_impl->coding, (void *)enc);

    *id = chan_id;

    return ret;
}

rk_s32 kmpp_venc_chan_deinit(KmppChanId id, KmppVencDeinitCfg cfg)
{
    struct mpp_chan *chan = NULL;
    RK_U8 wait = 1;
    KmppChanId chan_id = id;

    chan = mpp_vcodec_get_chan_entry(chan_id, MPP_CTX_ENC);
    if (!chan || !chan->handle) {
        kmpp_loge_f("invalid chan id %d\n", chan_id);
        return MPP_NOK;
    }
    kmpp_logi_f("destroy chan %d hnd %p online %d combo %d mst %d\n",
                chan_id, chan->handle, chan->cfg.online,
                chan->binder_chan_id, chan->master_chan_id);

    kmpp_venc_chan_stop(chan_id, cfg);
    if (chan->cfg.online) {
        struct mpp_chan *chan_tmp = chan;

        if (chan->master_chan_id != -1) {
            chan_tmp = mpp_vcodec_get_chan_entry(chan->master_chan_id, MPP_CTX_ENC);
            mpp_enc_unbind_jpeg_task(chan_tmp->handle);
        }
        if (!mpp_enc_check_hw_running(chan_tmp->handle) &&
            !mpp_enc_check_is_int_process(chan_tmp->handle))
            wait = 0;
    }

    if (wait)
        kmpp_wait_timeout_uninterruptible(chan->stop_wait, check_chan_idle, chan->runing, 2000);

    // if (chan->cfg.online)
    //     mpp_vcodec_chan_unbind(chan);
    kmpp_mutex_lock(chan->chan_mutex);
    mpp_enc_deinit(chan->handle);
    mpp_vcodec_stream_clear(chan);
    mpp_vcodec_chan_entry_deinit(chan);
    kmpp_mutex_unlock(chan->chan_mutex);

    kmpp_logi_f("destroy chan %d done\n", chan_id);

    return 0;
}

rk_s32 kmpp_venc_chan_start(KmppChanId id, KmppVencStartCfg cfg)
{
    unsigned long lock_flag;
    struct mpp_chan *chan = NULL;
    KmppChanId chan_id = id;

    chan = mpp_vcodec_get_chan_entry(chan_id, MPP_CTX_ENC);
    if (!chan || !chan->handle) {
        kmpp_loge_f("invalid chan id %d\n", chan_id);
        return MPP_NOK;
    }

    mpp_enc_start(chan->handle);
    kmpp_spin_lock_irqsave(chan->chan_lock, &lock_flag);
    chan->state = CHAN_STATE_RUN;
    kmpp_spin_unlock_irqrestore(chan->chan_lock, &lock_flag);
    enc_chan_update_chan_prior_tab();

    return 0;
}

rk_s32 kmpp_venc_chan_stop(KmppChanId id, KmppVencStopCfg cfg)
{
    unsigned long lock_flag;
    struct mpp_chan *chan = NULL;
    KmppChanId chan_id = id;

    chan = mpp_vcodec_get_chan_entry(chan_id, MPP_CTX_ENC);
    if (!chan || !chan->handle) {
        kmpp_loge_f("invalid chan id %d\n", chan_id);
        return MPP_NOK;
    }

    mpp_enc_stop(chan->handle);
    kmpp_spin_lock_irqsave(chan->chan_lock, &lock_flag);
    if (chan->state != CHAN_STATE_RUN) {
        kmpp_spin_unlock_irqrestore(chan->chan_lock, &lock_flag);
        return 0;
    }
    chan->state = CHAN_STATE_SUSPEND;
    kmpp_spin_unlock_irqrestore(chan->chan_lock, &lock_flag);
    kmpp_wait_wakeup(chan->wait);
    enc_chan_update_chan_prior_tab();

    return 0;
}

/* runtime query / status check */
rk_s32 kmpp_venc_chan_ctrl(KmppChanId id, rk_s32 cmd, void *arg)
{
    struct mpp_chan *chan = NULL;
    KmppChanId chan_id = id;

    chan = mpp_vcodec_get_chan_entry(chan_id, MPP_CTX_ENC);
    if (!chan || !chan->handle) {
        kmpp_loge_f("invalid chan %d\n", chan_id);
        return MPP_NOK;
    }

    if (cmd == MPP_SET_VENC_INIT_KCFG) {
        //mpp_vcodec_chan_change_coding_type(chan_id, arg);
    } else if (cmd == MPP_ENC_SET_CFG || cmd == MPP_ENC_GET_CFG) {
        KmppVencStCfg cfg = (KmppVencStCfg)arg;
        MppEncCfgSet *cfg_impl = kmpp_obj_to_entry(cfg);
        rk_u32 prep_change = *mpp_enc_cfg_prep_change(cfg_impl);

        mpp_enc_control(chan->handle, cmd, (void *)cfg);
        /*
         * In the case of wrapping, when switching resolutions,
         * need to ensure that the wrapping frame being encoded finish.
         */
        if (cmd == MPP_ENC_SET_CFG && chan->cfg.online && prep_change) {
            if (mpp_enc_check_hw_running(chan->handle)) {
                kmpp_wait_timeout_uninterruptible(chan->stop_wait, check_chan_idle, chan->runing, 2000);
            }
        }
    } else {
        mpp_enc_control(chan->handle, cmd, arg);
    }

    return 0;
}

/* static config */
rk_s32 kmpp_venc_chan_get_cfg(KmppChanId id, KmppVencStCfg cfg)
{
    return 0;
}

rk_s32 kmpp_venc_chan_set_cfg(KmppChanId id, KmppVencStCfg cfg)
{
    return 0;
}

/* runtime config */
rk_s32 kmpp_venc_chan_get_rt_cfg(KmppChanId id, KmppVencRtCfg cfg)
{
    return 0;
}

rk_s32 kmpp_venc_chan_set_rt_cfg(KmppChanId id, KmppVencRtCfg cfg)
{
    return 0;
}

rk_s32 kmpp_venc_chan_encode(KmppChanId id, KmppFrame frm, KmppPacket *pkt)
{
    return 0;
}

rk_s32 kmpp_venc_chan_put_frm(KmppChanId id, KmppFrame frm)
{
    struct mpp_chan *chan = NULL;
    struct venc_module *venc = NULL;
    struct vcodec_threads *thd;
    KmppChanId chan_id = id;
    unsigned long flags;
    rk_s32 ret = MPP_OK;

    chan = mpp_vcodec_get_chan_entry(chan_id, MPP_CTX_ENC);
    if (!chan || !chan->handle) {
        kmpp_loge_f("invalid chan id %d\n", chan_id);
        return MPP_NOK;
    }

    kmpp_spin_lock_irqsave(chan->chan_lock, &flags);
    if (chan->state != CHAN_STATE_RUN) {
        kmpp_loge_f("chan %d state %d is not run\n", chan_id, chan->state);
        kmpp_spin_unlock_irqrestore(chan->chan_lock, &flags);
        return MPP_NOK;
    }

    venc = mpp_vcodec_get_enc_module_entry();
    thd = venc->thd;
    if (chan->frame_cnt >= MAX_FRAME_NUM) {
        kmpp_loge_f("chan %d frame list is full\n", chan->chan_id);
        ret = MPP_NOK;
    } else {
        kmpp_list_add_tail(kmpp_obj_to_list_head(frm), &chan->frm_list);
        chan->frame_cnt++;
        mpp_vcodec_flow("chan %d put frame %p\n", chan->chan_id, frm);
    }
    kmpp_spin_unlock_irqrestore(chan->chan_lock, &flags);
    vcodec_thread_trigger(thd);

    return ret;
}

static rk_s32 check_chan_stream(const void *param)
{
    return kmpp_atomic_read((KmppAtomic *)param);
}

rk_s32 kmpp_venc_chan_get_pkt(KmppChanId id, KmppPacket *pkt)
{
    struct mpp_chan *chan = NULL;
    KmppPacketImpl *packet = NULL;
    KmppChanId chan_id = id;
    rk_ul flags;

    chan = mpp_vcodec_get_chan_entry(chan_id, MPP_CTX_ENC);
    if (!chan || !chan->handle) {
        kmpp_loge_f("invalid chan id %d\n", chan_id);
        return MPP_NOK;
    }

    if (!kmpp_atomic_read(chan->stream_count))
        kmpp_wait_timeout_uninterruptible(chan->wait, check_chan_stream, chan->stream_count, 500);

    kmpp_spin_lock_irqsave(chan->pkt_lock, &flags);
    if (!kmpp_list_empty(&chan->pkt_list)) {
        kmpp_list_head *next = chan->pkt_list.next;

        *pkt = kmpp_obj_from_list_head(next);
        kmpp_list_del_init(next);
    }
    kmpp_spin_unlock_irqrestore(chan->pkt_lock, &flags);
    if (!(*pkt)) {
        kmpp_loge_f("no packet in packet list\n");
        return MPP_NOK;
    }

    packet = kmpp_obj_to_entry(*pkt);
    /* flush cache before get packet*/
    if (packet->buf.buf)
        ring_buf_flush(&packet->buf, 1);
    kmpp_atomic_dec(chan->stream_count);
    chan->seq_user_get = packet->dts;

    return 0;
}

#include <linux/export.h>

EXPORT_SYMBOL(kmpp_venc_chan_init);
EXPORT_SYMBOL(kmpp_venc_chan_deinit);
EXPORT_SYMBOL(kmpp_venc_chan_start);
EXPORT_SYMBOL(kmpp_venc_chan_stop);
EXPORT_SYMBOL(kmpp_venc_chan_ctrl);
EXPORT_SYMBOL(kmpp_venc_chan_get_cfg);
EXPORT_SYMBOL(kmpp_venc_chan_set_cfg);
EXPORT_SYMBOL(kmpp_venc_chan_get_rt_cfg);
EXPORT_SYMBOL(kmpp_venc_chan_set_rt_cfg);
EXPORT_SYMBOL(kmpp_venc_chan_encode);
EXPORT_SYMBOL(kmpp_venc_chan_put_frm);
EXPORT_SYMBOL(kmpp_venc_chan_get_pkt);

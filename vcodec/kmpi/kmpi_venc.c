/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */
#include "rk_type.h"
#include "rk_err_def.h"

#include "mpp_mem.h"
#include "mpp_enc.h"
#include "kmpp_log.h"

#include "kmpi_venc.h"
#include "kmpp_obj.h"
#include "mpp_enc_cfg.h"
#include "kmpp_packet_impl.h"
#include "kmpp_venc_objs_impl.h"

#include "mpp_vcodec_base.h"
#include "mpp_vcodec_thread.h"
#include "mpp_vcodec_chan.h"
#include "mpp_vcodec_flow.h"

typedef struct KmppVencCtx_t {
    MppEnc enc;
    RK_U32 chan_id;
} KmppVencCtx;

static rk_s32 check_chan_idle(const void *param)
{
    return !kmpp_atomic_read((KmppAtomic *)param);
}

rk_s32 kmpp_venc_init(KmppCtx *ctx, KmppVencInitCfg cfg)
{
    KmppVencCtx *p = NULL;
    MppEnc enc = NULL;
    MPP_RET ret = MPP_OK;
    struct mpp_chan *chan = NULL;
    KmppVencInitCfgImpl *cfg_impl = (KmppVencInitCfgImpl*)kmpp_obj_to_entry(cfg);
    RK_S32 chan_id = cfg_impl->chan_id;

    chan_id = mpp_vcodec_get_free_chan(MPP_CTX_ENC, chan_id);
    if (chan_id < 0)
        return MPP_NOK;

    chan = mpp_vcodec_get_chan_entry(chan_id, MPP_CTX_ENC);
    cfg_impl->chan_id = chan_id;

    p = mpp_calloc(KmppVencCtx, 1);
    if (!p)
        return MPP_NOK;
    cfg_impl->shared_buf = &chan->shared_buf;
    ret = mpp_enc_init(&enc, cfg_impl);
    if (ret) {
        kmpp_loge_f("mpp_enc_init failed ret %d\n", ret);
        mpp_free(p);
        return ret;
    }

    mpp_enc_register_chl(enc, mpp_vcodec_enc_int_handle, cfg_impl->chan_id);

    mpp_vcodec_chan_entry_init(chan, MPP_CTX_ENC, cfg_impl->coding, (void *)enc);

    p->chan_id = chan_id;
    p->enc = enc;
    *ctx = p;

    return ret;
}

rk_s32 kmpp_venc_deinit(KmppCtx ctx, KmppVencDeinitCfg cfg)
{
    KmppVencCtx *p = NULL;
    struct mpp_chan *chan = NULL;
    RK_U8 wait = 1;
    RK_U32 chan_id;

    if (!ctx) {
        kmpp_loge_f("ctx is null\n");
        return MPP_NOK;
    }

    p = (KmppVencCtx *)ctx;
    chan_id = p->chan_id;
    chan = mpp_vcodec_get_chan_entry(chan_id, MPP_CTX_ENC);

    kmpp_logi_f("destroy chan %d hnd %p online %d combo %d mst %d\n",
                chan_id, chan->handle, chan->cfg.online,
                chan->binder_chan_id, chan->master_chan_id);

    kmpp_venc_stop(ctx, cfg);
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

    mpp_free(p);
    kmpp_logi_f("destroy chan %d done\n", chan_id);

    return 0;
}

rk_s32 kmpp_venc_start(KmppCtx ctx, KmppVencStartCfg cfg)
{
    unsigned long lock_flag;
    struct mpp_chan *chan = NULL;
    KmppVencCtx *p = NULL;

    if (!ctx) {
        kmpp_loge_f("ctx is null\n");
        return MPP_NOK;
    }

    p = (KmppVencCtx *)ctx;

    chan = mpp_vcodec_get_chan_entry(p->chan_id, MPP_CTX_ENC);
    kmpp_assert(chan->handle != NULL);
    if (!chan->handle)
        return MPP_NOK;

    mpp_enc_start(chan->handle);
    kmpp_spin_lock_irqsave(chan->chan_lock, &lock_flag);
    chan->state = CHAN_STATE_RUN;
    kmpp_spin_unlock_irqrestore(chan->chan_lock, &lock_flag);
    enc_chan_update_chan_prior_tab();

    return 0;
}

rk_s32 kmpp_venc_stop(KmppCtx ctx, KmppVencStopCfg cfg)
{
    unsigned long lock_flag;
    KmppVencCtx *p = NULL;
    struct mpp_chan *chan = NULL;

    if (!ctx) {
        kmpp_loge_f("ctx is null\n");
        return MPP_NOK;
    }

    p = (KmppVencCtx *)ctx;

    chan = mpp_vcodec_get_chan_entry(p->chan_id, MPP_CTX_ENC);
    kmpp_assert(chan->handle != NULL);
    if (!chan->handle)
        return MPP_NOK;

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
rk_s32 kmpp_venc_ctrl(KmppCtx ctx, rk_s32 cmd, void *arg)
{
    KmppVencCtx *p = NULL;
    struct mpp_chan *chan = NULL;

    if (!ctx) {
        kmpp_loge_f("ctx is null\n");
        return MPP_NOK;
    }

    p = (KmppVencCtx *)ctx;

    chan = mpp_vcodec_get_chan_entry(p->chan_id, MPP_CTX_ENC);
    kmpp_assert(chan->handle != NULL);
    if (!chan->handle)
        return MPP_NOK;

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
                kmpp_wait_timeout_uninterruptible(chan->stop_wait, check_chan_idle, chan->runing, 200);
            }
        }
    } else {
        mpp_enc_control(chan->handle, cmd, arg);
    }

    return 0;
}

/* static config */
rk_s32 kmpp_venc_get_cfg(KmppCtx ctx, KmppVencStCfg cfg)
{
    return 0;
}

rk_s32 kmpp_venc_set_cfg(KmppCtx ctx, KmppVencStCfg cfg)
{
    return 0;
}

/* runtime config */
rk_s32 kmpp_venc_get_rt_cfg(KmppCtx ctx, KmppVencRtCfg cfg)
{
    return 0;
}

rk_s32 kmpp_venc_set_rt_cfg(KmppCtx ctx, KmppVencRtCfg cfg)
{
    return 0;
}

rk_s32 kmpp_venc_encode(KmppCtx ctx, KmppFrame frm, KmppPacket *pkt)
{
    return 0;
}

rk_s32 kmpp_venc_put_frm(KmppCtx ctx, KmppFrame frm)
{
    KmppVencCtx *p = NULL;
    struct mpp_chan *chan = NULL;
    struct venc_module *venc = NULL;
    struct vcodec_threads *thd;
    unsigned long flags;
    rk_u32 chan_id;
    rk_s32 ret = MPP_OK;

    if (!ctx) {
        kmpp_loge_f("ctx is null\n");
        return MPP_NOK;
    }

    p = (KmppVencCtx *)ctx;
    chan_id = p->chan_id;
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
    }
    kmpp_spin_unlock_irqrestore(chan->chan_lock, &flags);
    vcodec_thread_trigger(thd);

    return ret;
}

static rk_s32 check_chan_stream(const void *param)
{
    return kmpp_atomic_read((KmppAtomic *)param);
}

rk_s32 kmpp_venc_get_pkt(KmppCtx ctx, KmppPacket *pkt)
{
    KmppVencCtx *p = NULL;
    struct mpp_chan *chan = NULL;
    KmppPacketImpl *pkt_impl = NULL;
    rk_ul flags;

    if (!ctx) {
        kmpp_loge_f("ctx is null\n");
        return MPP_NOK;
    }

    p = (KmppVencCtx *)ctx;
    chan = mpp_vcodec_get_chan_entry(p->chan_id, MPP_CTX_ENC);
    kmpp_assert(chan->handle != NULL);
    if (!chan->handle)
        return MPP_NOK;

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
        kmpp_loge_f("no packet\n");
        return MPP_NOK;
    }
    pkt_impl = (KmppPacketImpl *)kmpp_obj_to_entry(*pkt);

    /* flush cache before get packet*/
    if (pkt_impl->buf.buf)
        ring_buf_flush(&pkt_impl->buf, 1);
    kmpp_atomic_dec(chan->stream_count);
    chan->seq_user_get = pkt_impl->dts;
    // kmpp_atomic_inc(chan->str_out_cnt);
    // kmpp_atomic_inc(chan->pkt_user_get);

    return 0;
}

#include <linux/export.h>

EXPORT_SYMBOL(kmpp_venc_init);
EXPORT_SYMBOL(kmpp_venc_deinit);
EXPORT_SYMBOL(kmpp_venc_start);
EXPORT_SYMBOL(kmpp_venc_stop);
EXPORT_SYMBOL(kmpp_venc_ctrl);
EXPORT_SYMBOL(kmpp_venc_get_cfg);
EXPORT_SYMBOL(kmpp_venc_set_cfg);
EXPORT_SYMBOL(kmpp_venc_get_rt_cfg);
EXPORT_SYMBOL(kmpp_venc_set_rt_cfg);
EXPORT_SYMBOL(kmpp_venc_encode);
EXPORT_SYMBOL(kmpp_venc_put_frm);
EXPORT_SYMBOL(kmpp_venc_get_pkt);

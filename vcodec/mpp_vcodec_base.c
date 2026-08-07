/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define MODULE_TAG "mpp_vcodec"

#include <linux/module.h>

#include "kmpp_log.h"
#include "kmpp_module.h"

#include "mpp_vcodec_base.h"
#include "mpp_vcodec_debug.h"
#include "mpp_vcodec_flow.h"
#include "mpp_vcodec_thread.h"
#include "mpp_buffer_impl.h"
#include "kmpp_packet_impl.h"
#include "kmpp_frame.h"
#include "rc_api.h"
#include "hal_bufs.h"
#include "mpp_maths.h"

#include "kmpp_obj.h"
#include "kmpp_mem.h"
#include "kmpp_meta_impl.h"
#include "kmpp_buffer_impl.h"
#include "kmpp_obj.h"
#include "kmpp_venc_objs_impl.h"

RK_U32 mpp_vcodec_debug = 0;
module_param(mpp_vcodec_debug, int, 0644);

#define CHAN_MAX_YUV_POOL_SIZE  1
#define CHAN_MAX_STREAM_POOL_SIZE  2

int max_stream_cnt = 0;
module_param(max_stream_cnt, int, 0644);

static struct vcodec_entry g_vcodec_entry;

RK_S32 mpp_vcodec_get_free_chan(MppCtxType type, RK_S32 chan_id)
{
    RK_S32 new_id = -1;
    RK_S32 i;

    switch (type) {
    case MPP_CTX_ENC: {
        unsigned long lock_flag;
        struct venc_module *venc = &g_vcodec_entry.venc;
        struct mpp_chan *chan = NULL;

        kmpp_spin_lock_irqsave(venc->enc_lock, &lock_flag);
        chan = mpp_vcodec_get_chan_entry(chan_id, type);
        if (chan && chan->state == CHAN_STATE_NULL) {
            chan->state = CHAN_STATE_INIT;
            new_id = chan_id;
            venc->num_enc++;
            kmpp_spin_unlock_irqrestore(venc->enc_lock, &lock_flag);
            break;
        }

        for (i = 0; i < MAX_ENC_NUM; i++) {
            if (venc->mpp_enc_chan_entry[i].state == CHAN_STATE_NULL) {
                venc->mpp_enc_chan_entry[i].state = CHAN_STATE_INIT;
                new_id = i;
                venc->num_enc++;
                break;
            }
        }
        kmpp_spin_unlock_irqrestore(venc->enc_lock, &lock_flag);
    } break;
    default: {
        kmpp_loge_f("MppCtxType error %d", type);
    } break;
    }

    return new_id;
}

struct mpp_chan *mpp_vcodec_get_chan_entry(RK_S32 chan_id, MppCtxType type)
{
    struct mpp_chan *chan = NULL;

    switch (type) {
    case MPP_CTX_ENC: {
        struct venc_module *venc = &g_vcodec_entry.venc;

        if (chan_id < 0)
            return NULL;

        if (chan_id > MAX_ENC_NUM) {
            kmpp_loge_f("chan id %d is larger than max %d\n", chan_id, MAX_ENC_NUM);
            return NULL;
        }
        chan = &venc->mpp_enc_chan_entry[chan_id];
    } break;
    default: {
        kmpp_loge_f("MppCtxType error %d", type);
    } break;
    }

    return chan;
}

RK_U32 mpp_vcodec_get_chan_num(MppCtxType type)
{
    RK_U32 num_chan = 0;

    switch (type) {
    case MPP_CTX_ENC: {
        unsigned long lock_flag;
        struct venc_module *venc = &g_vcodec_entry.venc;
        kmpp_spin_lock_irqsave(venc->enc_lock, &lock_flag);
        num_chan = venc->num_enc;
        kmpp_spin_unlock_irqrestore(venc->enc_lock, &lock_flag);
    } break;
    default: {
        kmpp_loge_f("MppCtxType error %d", type);
    } break;
    }
    return num_chan;
}

void mpp_vcodec_put_chan(MppCtxType type)
{
    switch (type) {
    case MPP_CTX_ENC: {
        unsigned long lock_flag;
        struct venc_module *venc = &g_vcodec_entry.venc;

        kmpp_spin_lock_irqsave(venc->enc_lock, &lock_flag);
        venc->num_enc--;
        kmpp_spin_unlock_irqrestore(venc->enc_lock, &lock_flag);

    } break;
    default: {
        kmpp_loge_f("MppCtxType error %d", type);
    } break;
    }

    return;
}

#define REF_BODY_SIZE(w, h)             MPP_ALIGN((((w) * (h) * 3 / 2) + 48 * MPP_MAX(w, h)), SZ_4K)
#define REF_WRAP_BODY_EXT_SIZE(w, h)    MPP_ALIGN((240 * MPP_MAX(w, h)), SZ_4K)
#define REF_HEADER_SIZE(w, h)           MPP_ALIGN((((w) * (h) / 64) + MPP_MAX(w, h) / 2), SZ_4K)
#define REF_WRAP_HEADER_EXT_SIZE(w, h)  MPP_ALIGN((3 * MPP_MAX(w, h)), SZ_4K)

static MPP_RET get_wrap_buf(struct hal_shared_buf *ctx, KmppVencInitCfg cfg, RK_S32 max_lt_cnt)
{
    KmppVencInitCfgImpl *cfg_impl = (KmppVencInitCfgImpl *)kmpp_obj_to_entry(cfg);
    RK_S32 alignment = 64;
    RK_S32 aligned_w = MPP_ALIGN(cfg_impl->max_width, alignment);
    RK_S32 aligned_h = MPP_ALIGN(cfg_impl->max_height, alignment);
    RK_U32 total_wrap_size;
    RK_U32 body_size, body_total_size, hdr_size, hdr_total_size;

    body_size = REF_BODY_SIZE(aligned_w, aligned_h);
    body_total_size = body_size + REF_WRAP_BODY_EXT_SIZE(aligned_w, aligned_h);
    hdr_size = REF_HEADER_SIZE(aligned_w, aligned_h);
    hdr_total_size = hdr_size + REF_WRAP_HEADER_EXT_SIZE(aligned_w, aligned_h);
    total_wrap_size = body_total_size + hdr_total_size;
    if (max_lt_cnt > 0) {
        if (cfg_impl->only_smartp) {
            total_wrap_size += body_size;
            total_wrap_size += hdr_size;
        } else
            total_wrap_size += total_wrap_size;
    }
    if (ctx->recn_ref_buf)
        mpp_buffer_put(ctx->recn_ref_buf);

    return mpp_buffer_get(NULL, &ctx->recn_ref_buf, total_wrap_size);
}

static void mpp_chan_clear_buf_resource(struct mpp_chan *entry)
{
    struct hal_shared_buf *ctx = &entry->shared_buf;
    void *buf = NULL;

    buf = cmpxchg(&ctx->dpb_bufs, ctx->dpb_bufs, NULL);
    if (buf)
        hal_bufs_deinit(buf);

    buf = cmpxchg(&ctx->recn_ref_buf, ctx->recn_ref_buf, NULL);
    if (buf)
        mpp_buffer_put(buf);

    buf = cmpxchg(&ctx->stream_buf, ctx->stream_buf, NULL);
    if (buf)
        mpp_buffer_put(buf);

    buf = cmpxchg(&ctx->ext_line_buf, ctx->ext_line_buf, NULL);
    if (buf)
        mpp_buffer_put(buf);

    entry->max_width = 0;
    entry->max_height = 0;
    entry->max_lt_cnt = 0;
    entry->ring_buf_size = 0;
    entry->shared_buf_release = 0;
}

MPP_RET mpp_vcodec_chan_setup_hal_bufs(struct mpp_chan *entry, KmppVencInitCfg cfg)
{
    KmppVencInitCfgImpl *cfg_impl = (KmppVencInitCfgImpl *)kmpp_obj_to_entry(cfg);

    if (((cfg_impl->max_width * cfg_impl->max_height > entry->max_width * entry->max_height) ||
         (cfg_impl->max_lt_cnt > entry->max_lt_cnt)) && (cfg_impl->coding != MPP_VIDEO_CodingMJPEG)) {
        RK_S32 alignment = 64;
        RK_S32 aligned_w = MPP_ALIGN(cfg_impl->max_width, alignment);
        RK_S32 aligned_h = MPP_ALIGN(cfg_impl->max_height, alignment);

        RK_S32 mb_wd64 = aligned_w / 64;
        RK_S32 mb_h64 = aligned_h / 64;

        RK_S32 pixel_buf_fbc_hdr_size = MPP_ALIGN(aligned_w * (aligned_h + 32) / 64, SZ_8K);
        RK_S32 pixel_buf_fbc_bdy_size = aligned_w * (aligned_h + 32) * 3 / 2;
        RK_S32 pixel_buf_size = pixel_buf_fbc_hdr_size + pixel_buf_fbc_bdy_size;

        RK_S32 thumb_buf_size = MPP_ALIGN(aligned_w / 64 * aligned_h / 64 * 256, SZ_8K);
        RK_S32 max_buf_cnt = 2 + cfg_impl->max_lt_cnt;
        RK_U32 smear_size = 0;
        RK_U32 smear_r_size = 0;
        RK_U32 smear_size_t = 0;
        RK_U32 cmv_size = 0 ;
        RK_S32 max_lt_cnt = cfg_impl->max_lt_cnt;
        struct hal_shared_buf *ctx = &entry->shared_buf;
        RK_S32 pixel_buf_size_265 = MPP_ALIGN(((mb_wd64 * mb_h64) << 6), SZ_8K) +
                                    (aligned_w * aligned_h) * 3 / 2;
        RK_U32 i = 0;

        kmpp_logi_f("max_width %d max_height %d\n", cfg_impl->max_width, cfg_impl->max_height);
        pixel_buf_size = MPP_MAX(pixel_buf_size, pixel_buf_size_265);

        if (cfg_impl->tmvp_enable)
            cmv_size = MPP_ALIGN(mb_wd64 * mb_h64 * 4, 256) * 16;
        if (1) {
            smear_size_t = MPP_ALIGN(aligned_w, 1024) / 1024 * MPP_ALIGN(aligned_h, 16) / 16 * 16;
            smear_r_size = MPP_ALIGN(aligned_h, 1024) / 1024 * MPP_ALIGN(aligned_w, 16) / 16 * 16;
        } else {
            smear_size_t = MPP_ALIGN(aligned_w, 256) / 256 * MPP_ALIGN(aligned_h, 32) / 32;
            smear_r_size = MPP_ALIGN(aligned_h, 256) / 256 * MPP_ALIGN(aligned_w, 32) / 32;;
        }
        smear_size = MPP_MAX(smear_size_t, smear_r_size);
        if (1) {
            smear_size_t = MPP_ALIGN(aligned_w, 512) / 512 * MPP_ALIGN(aligned_h, 32) / 32 * 16;
            smear_r_size = MPP_ALIGN(aligned_h, 512) / 512 * MPP_ALIGN(aligned_w, 32) / 32 * 16;

        } else {
            smear_size_t = MPP_ALIGN(aligned_w, 256) / 256 * MPP_ALIGN(aligned_h, 32) / 32;
            smear_r_size = MPP_ALIGN(aligned_h, 256) / 256 * MPP_ALIGN(aligned_w, 32) / 32;
        }

        smear_size_t = MPP_MAX(smear_size_t, smear_r_size);
        smear_size = MPP_MAX(smear_size, smear_size_t);

        if (ctx->dpb_bufs) {
            hal_bufs_deinit(ctx->dpb_bufs);
            ctx->dpb_bufs = NULL;
        }

        hal_bufs_init(&ctx->dpb_bufs);

        if (cfg_impl->shared_buf_en) {
            size_t sizes[4] = {thumb_buf_size, cmv_size, smear_size, 0};
            hal_bufs_setup(ctx->dpb_bufs, max_buf_cnt, MPP_ARRAY_ELEMS(sizes), sizes);
            if (get_wrap_buf(ctx, cfg, max_lt_cnt))
                goto fail;
        } else {
            size_t sizes[4] = {thumb_buf_size, cmv_size, smear_size, pixel_buf_size};
            hal_bufs_setup(ctx->dpb_bufs, max_buf_cnt, MPP_ARRAY_ELEMS(sizes), sizes);
        }

        for (i = 0; i < max_buf_cnt; i++) {
            if (hal_bufs_get_buf(ctx->dpb_bufs, i) == NULL)
                goto fail;
        }

        if (cfg_impl->max_width < cfg_impl->max_height)
            MPP_SWAP(RK_U32, cfg_impl->max_width,  cfg_impl->max_height);

        if (cfg_impl->max_width  > entry->max_width && cfg_impl->max_width > 3072) {
            RK_S32 aligned_w = MPP_ALIGN(cfg_impl->max_width, 64);
            RK_S32 h265_buf_size = (aligned_w / 32 - 91) * 26 * 16;
            RK_S32 h264_buf_size = (aligned_w / 64 - 36) * 56 * 16;
            RK_S32 extern_line_size = MPP_MAX(h265_buf_size, h264_buf_size);

            if (ctx->ext_line_buf) {
                mpp_buffer_put(ctx->ext_line_buf);
                ctx->ext_line_buf = NULL;
            }

            if (mpp_buffer_get(NULL, &ctx->ext_line_buf, extern_line_size))
                goto fail;
        }

        entry->max_width = cfg_impl->max_width;
        entry->max_height = cfg_impl->max_height;
        entry->max_lt_cnt = cfg_impl->max_lt_cnt;

    }
    if (cfg_impl->buf_size > entry->ring_buf_size) {
        struct hal_shared_buf *ctx = &entry->shared_buf;
        RK_U32 buf_size = MPP_MAX(cfg_impl->buf_size, SZ_16K);
        if (ctx->stream_buf) {
            mpp_buffer_put(ctx->stream_buf);
            ctx->stream_buf = NULL;
        }
        if (mpp_ring_buffer_get(NULL, &ctx->stream_buf, MPP_ALIGN(buf_size, SZ_4K)))
            goto fail;

        entry->ring_buf_size = buf_size;
    }

    return MPP_OK;
fail:
    mpp_chan_clear_buf_resource(entry);

    return MPP_NOK;
}


int mpp_vcodec_chan_entry_init(struct mpp_chan *entry, MppCtxType type,
                               MppCodingType coding, void *handle)
{
    unsigned long lock_flag;

    kmpp_atomic_init(&entry->stream_count);
    kmpp_atomic_init(&entry->pkt_total_num);
    kmpp_atomic_init(&entry->pkt_user_get);
    kmpp_atomic_init(&entry->runing);
    kmpp_atomic_init(&entry->cfg.comb_runing);

    kmpp_spin_lock_irqsave(entry->chan_lock, &lock_flag);
    entry->handle = handle;
    entry->coding_type = coding;
    entry->type = type;
    entry->binder_chan_id = -1;
    entry->master_chan_id = -1;

    kmpp_atomic_set(entry->stream_count, 0);
    kmpp_atomic_set(entry->pkt_total_num, 0);
    kmpp_atomic_set(entry->pkt_user_get, 0);
    kmpp_atomic_set(entry->runing, 0);
    kmpp_atomic_set(entry->cfg.comb_runing, 0);

    KMPP_INIT_LIST_HEAD(&entry->pkt_list);
    KMPP_INIT_LIST_HEAD(&entry->frm_list);
    entry->state = CHAN_STATE_SUSPEND_PENDING;
    kmpp_spin_unlock_irqrestore(entry->chan_lock, &lock_flag);

    return 0;
}

int mpp_vcodec_chan_entry_deinit(struct mpp_chan *entry)
{
    unsigned long lock_flag;

    mpp_vcodec_put_chan(entry->type);
    kmpp_spin_lock_irqsave(entry->chan_lock, &lock_flag);
    if (entry->pskip_frame) {
        kmpp_frame_put(entry->pskip_frame);
        entry->pskip_frame = NULL;
    }
    entry->handle = NULL;
    entry->state = CHAN_STATE_NULL;
    entry->reenc = 0;
    entry->binder_chan_id = -1;
    entry->master_chan_id = -1;
    entry->frame_cnt = 0;
    kmpp_spin_unlock_irqrestore(entry->chan_lock, &lock_flag);

    kmpp_atomic_deinit(&entry->stream_count);
    kmpp_atomic_deinit(&entry->pkt_total_num);
    kmpp_atomic_deinit(&entry->pkt_user_get);
    kmpp_atomic_deinit(&entry->runing);
    kmpp_atomic_deinit(&entry->cfg.comb_runing);

    if (IS_ENABLED(CHAN_RELEASE_BUF) || entry->shared_buf_release)
        mpp_chan_clear_buf_resource(entry);

    return 0;
}

void mpp_vcodec_stream_clear(struct mpp_chan *entry)
{
    KmppPacket packet = NULL;
    unsigned long flags;

    kmpp_spin_lock_irqsave(entry->pkt_lock, &flags);
    while (!kmpp_list_empty(&entry->pkt_list)) {
        kmpp_list_head *next = entry->pkt_list.next;

        packet = kmpp_obj_from_list_head(next);
        kmpp_list_del_init(next);
        kmpp_packet_put(packet);
    }
    kmpp_spin_unlock_irqrestore(entry->pkt_lock, &flags);

    return;
}

struct venc_module *mpp_vcodec_get_enc_module_entry(void)
{
    return &g_vcodec_entry.venc;
}

void enc_chan_update_chan_prior_tab(void)
{
    struct venc_module *venc = &g_vcodec_entry.venc;
    struct mpp_chan *chan = NULL;
    unsigned long lock_flags;
    RK_S32 soft_flag = 0;
    RK_S32 i, j, tmp_pri_chan[2];

    kmpp_spin_lock_irqsave(venc->enc_lock, &lock_flags);
    venc->chan_pri_tab_index = 0;
    /* snap current prior status */
    venc->started_chan_num = 0;
    for (i = 0; i < MAX_ENC_NUM; i++) {
        chan = &venc->mpp_enc_chan_entry[i];

        if (chan->state == CHAN_STATE_RUN) {
            venc->chan_pri_tab[venc->started_chan_num][1] = 1 + chan->cfg.priority; /* 0 is invalid */
            venc->chan_pri_tab[venc->started_chan_num][0] = i;
            venc->started_chan_num++;
        }
    }

    for (i = venc->started_chan_num; i < MAX_ENC_NUM; i++) {
        venc->chan_pri_tab[i][1] = 0;
        venc->chan_pri_tab[i][0] = 0;
    }

    /* sort prio */
    for (i = 0; i < MAX_ENC_NUM - 1; i++) {
        soft_flag = MPP_OK;
        for (j = i + 1; j < MAX_ENC_NUM; j++) {
            if (venc->chan_pri_tab[i][1] < venc->chan_pri_tab[j][1]) {
                tmp_pri_chan[0] = venc->chan_pri_tab[i][0];
                tmp_pri_chan[1] = venc->chan_pri_tab[i][1];

                venc->chan_pri_tab[i][0] = venc->chan_pri_tab[j][0];
                venc->chan_pri_tab[i][1] = venc->chan_pri_tab[j][1];

                venc->chan_pri_tab[j][0] = tmp_pri_chan[0];
                venc->chan_pri_tab[j][1] = tmp_pri_chan[1];
                soft_flag = MPP_NOK;
            }
        }
        if (soft_flag == MPP_OK)
            break;
    }
    kmpp_spin_unlock_irqrestore(venc->enc_lock, &lock_flags);
}

MPP_RET enc_chan_update_tab_after_enc(RK_U32 curr_chan)
{
    struct venc_module *venc = &g_vcodec_entry.venc;
    RK_U32 tmp_index = 0;
    unsigned long lock_flags;
    RK_U32 i;

    kmpp_spin_lock_irqsave(venc->enc_lock, &lock_flags);
    /* snap current prior status */
    for (i = 0; i < venc->started_chan_num; i++) {
        if (venc->chan_pri_tab[i][0] == (RK_S32) curr_chan) {
            tmp_index = i;
            break;
        }
    }

    if (venc->started_chan_num) {
        for (i = tmp_index; i < venc->started_chan_num - 1; i++)
            venc->chan_pri_tab[i][0] = venc->chan_pri_tab[i + 1][0];
        venc->chan_pri_tab[venc->started_chan_num - 1][0] = curr_chan;
    }
    venc->chan_pri_tab_index = tmp_index;
    kmpp_spin_unlock_irqrestore(venc->enc_lock, &lock_flags);

    return MPP_OK;
}

void enc_chan_get_high_prior_chan(void)
{
    struct venc_module *venc = &g_vcodec_entry.venc;
    unsigned long lock_flags;

    kmpp_spin_lock_irqsave(venc->enc_lock, &lock_flags);
    venc->curr_high_prior_chan = venc->chan_pri_tab[venc->chan_pri_tab_index][0];
    venc->chan_pri_tab_index++;
    kmpp_spin_unlock_irqrestore(venc->enc_lock, &lock_flags);

    return;
}

int mpp_vcodec_init(void)
{
    static const char *name = "rkv_enc";
    struct venc_module *venc = &g_vcodec_entry.venc;
    struct vcodec_threads *thds = NULL;
    RK_U32 i = 0;

    kmpp_memset(venc, 0, sizeof(*venc));
    venc->name = name;

    for (i = 0; i < MAX_ENC_NUM; i++) {
        struct mpp_chan *curr_entry = &venc->mpp_enc_chan_entry[i];

        curr_entry->chan_id = i;
        curr_entry->coding_type = -1;
        curr_entry->handle = NULL;
        kmpp_spinlock_init(&curr_entry->chan_lock);
        kmpp_mutex_init(&curr_entry->chan_mutex);
        kmpp_wait_init(&curr_entry->wait);
        kmpp_wait_init(&curr_entry->stop_wait);
        kmpp_spinlock_init(&curr_entry->pkt_lock);
    }
    venc->num_enc = 0;
    thds = vcodec_thread_create((struct vcodec_module*)venc);

    vcodec_thread_set_count(thds, 1);
    vcodec_thread_set_callback(thds, mpp_vcodec_enc_routine, (void*)venc);
    mpp_buffer_pool_init(max_stream_cnt);
    vcodec_thread_start(thds);

    venc->check = &venc;
    venc->thd = thds;
    kmpp_spinlock_init(&venc->enc_lock);

    return 0;
}

int mpp_vcodec_deinit(void)
{
    struct venc_module *venc = &g_vcodec_entry.venc;
    int i = 0;

    for (i = 0; i < MAX_ENC_NUM; i++) {
        struct mpp_chan *entry = &venc->mpp_enc_chan_entry[i];

        kmpp_mutex_deinit(&entry->chan_mutex);
        kmpp_spinlock_deinit(&entry->chan_lock);
        kmpp_wait_deinit(&entry->wait);
        kmpp_wait_deinit(&entry->stop_wait);
        kmpp_spinlock_deinit(&entry->pkt_lock);
        mpp_chan_clear_buf_resource(entry);
    }

    if (venc->thd) {
        vcodec_thread_stop(venc->thd);
        vcodec_thread_destroy(venc->thd);
        venc->thd = NULL;
    }

    kmpp_spinlock_deinit(&venc->enc_lock);

    mpp_buffer_pool_deinit();

    return 0;
}

int mpp_vcodec_clear_buf_resource(void)
{
    struct venc_module *venc = &g_vcodec_entry.venc;
    int i = 0;

    if (IS_ENABLED(CHAN_RELEASE_BUF))
        return 0;

    for (i = 0; i < MAX_ENC_NUM; i++) {
        unsigned long lock_flag;
        struct mpp_chan *entry = &venc->mpp_enc_chan_entry[i];
        RK_U32 can_clear = 1;

        kmpp_spin_lock_irqsave(entry->chan_lock, &lock_flag);
        /* if entry still alive, do not clear resource here */
        if (entry->state != CHAN_STATE_NULL)
            can_clear = 0;
        entry->shared_buf_release = !can_clear;
        kmpp_spin_unlock_irqrestore(entry->chan_lock, &lock_flag);
        if (can_clear)
            mpp_chan_clear_buf_resource(entry);
    }

    return 0;
}
EXPORT_SYMBOL(mpp_vcodec_clear_buf_resource);

int mpp_vcodec_run_task(RK_U32 chan_id, RK_S64 pts, RK_S64 dts)
{
    return mpp_vcodec_enc_run_task(chan_id, pts, dts);
}
EXPORT_SYMBOL(mpp_vcodec_run_task);

int mpp_vcodec_set_online_mode(RK_U32 chan_id, RK_U32 mode)
{
    return mpp_vcodec_enc_set_online_mode(chan_id, mode);
}
EXPORT_SYMBOL(mpp_vcodec_set_online_mode);

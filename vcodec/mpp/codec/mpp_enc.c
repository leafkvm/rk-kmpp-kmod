/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#define  MODULE_TAG "mpp_enc"

#include "kmpp_string.h"
#include <linux/module.h>
#include "kmpp_uaccess.h"

#include "kmpp_version.h"
#include "mpp_mem.h"
#include "mpp_maths.h"
#include "kmpp_packet.h"
#include "mpp_enc_debug.h"
#include "mpp_enc_impl.h"
#include "mpp_enc.h"
#include "mpp_service.h"
#include "mpp_buffer_impl.h"
#include "mpp_vcodec_flow.h"
#include "kmpp_obj.h"
#include "kmpp_venc_objs_impl.h"

RK_U32 mpp_enc_debug = 0;
module_param(mpp_enc_debug, uint, 0644);
MODULE_PARM_DESC(mpp_enc_debug, "bits mpp_enc debug information");

static void copy_enc_cfg(KmppObj dst, KmppObj src)
{
    MppEncCfgSet *_dst = kmpp_obj_to_entry(dst);
    MppEncCfgSet *_src = kmpp_obj_to_entry(src);

    kmpp_memcpy(_dst, _src, sizeof(*_dst));
    _dst->ref_cfg = &_dst->ref_param;

    /* cleanup output change flag to avoid extra change flag bit when user resend the cfg */
    {
        void *p = kmpp_obj_to_flags(dst);
        rk_s32 size = kmpp_obj_to_flags_size(dst);

        if (p && size)
            kmpp_memset(p, 0, size);
    }
}

static MPP_RET mpp_enc_init_cfg(MppEncImpl *enc, KmppVencInitCfgImpl *init_cfg)
{
    const rk_u8 *str_scene[MPP_ENC_SCENE_MODE_BUTT] = {"scene_default", "scene_ipc", "scene_ptz"};
    MppEncCfgSet *enc_cfg = enc->cfg;
    rk_u8 preset_str[256]; /* preset string */
    rk_u32 len = 0;
    MPP_RET ret;

    if (init_cfg->smart_en) {
        if (init_cfg->smart_en != 1 && init_cfg->smart_en != 3) {
            kmpp_logw_f("invalid smart_en %d, default set to 0\n", init_cfg->smart_en);
            init_cfg->smart_en = 0;
        }

        if (init_cfg->smart_en)
            len += kmpp_snprintf(preset_str + len, sizeof(preset_str) - len - 1, "smart:");
    }

    if (init_cfg->scene_mode < 0 || init_cfg->scene_mode >= 3) {
        kmpp_logw_f("invalid scene_mode %d, default set to 0\n", init_cfg->scene_mode);
        init_cfg->scene_mode = 0;
    }

    len += kmpp_snprintf(preset_str + len, sizeof(preset_str) - len - 1,
                         "%s:", str_scene[init_cfg->scene_mode]);

    ret = mpp_enc_hal_init_cfg(enc->enc_hal, enc_cfg, preset_str);
    if (ret)
        return ret;

    ret = enc_impl_init_cfg(enc->impl, enc_cfg, NULL);
    if (ret)
        return ret;

    /* NOTE: setup configure coding for check */
    enc_cfg->base.coding = enc->coding;
    if (init_cfg->smart_en == 1)
        enc_cfg->base.smt1_en = 1;
    else if (init_cfg->smart_en == 3)
        enc_cfg->base.smt3_en = 1;
    enc_cfg->prep.max_width = init_cfg->max_width;
    enc_cfg->prep.max_height = init_cfg->max_height;
    enc_cfg->tune.deblur_en = init_cfg->deblur_en;
    enc_cfg->tune.qpmap_en = init_cfg->qpmap_en;

    return ret;
}

MPP_RET mpp_enc_init(MppEnc * enc, KmppVencInitCfgImpl *cfg)
{
    EncImpl impl = NULL;
    MppEncImpl *p = NULL;
    MppEncHal enc_hal = NULL;
    MppEncHalCfg enc_hal_cfg;
    EncImplCfg ctrl_cfg;
    const char *str_smart = "smart";
    MPP_RET ret;

    if (!enc || !cfg) {
        kmpp_loge_f("found NULL input enc %p cfg %p\n", enc, cfg);
        return MPP_ERR_NULL_PTR;
    }

    *enc = NULL;

    p = mpp_calloc(MppEncImpl, 1);
    if (!p) {
        kmpp_loge_f("failed to malloc context\n");
        return MPP_ERR_MALLOC;
    }

    ret = kmpp_venc_st_cfg_get(&p->set_obj);
    ret |= kmpp_venc_st_cfg_get(&p->cfg_obj);
    if (ret) {
        kmpp_loge_f("failed to get venc st cfg\n");
        goto ERR_RET;
    }

    p->cfg = (MppEncCfgSet *)kmpp_obj_to_entry(p->cfg_obj);
    p->set = (MppEncCfgSet *)kmpp_obj_to_entry(p->set_obj);

    ret = mpp_enc_refs_init(&p->refs);
    if (ret) {
        kmpp_loge_f("could not init enc refs\n");
        goto ERR_RET;
    }

    /* reate hal first */
    MppCodingType coding = cfg->coding;
    enc_hal_cfg.coding = coding;
    enc_hal_cfg.cfg = p->cfg;
    enc_hal_cfg.type = VPU_CLIENT_BUTT;
    enc_hal_cfg.dev = NULL;
    enc_hal_cfg.chan_id = cfg->chan_id;
    enc_hal_cfg.online = cfg->online;
    enc_hal_cfg.hook = mpp_vcodec_enc_int_handle;
    enc_hal_cfg.ref_buf_shared = cfg->shared_buf_en;
    enc_hal_cfg.shared_buf = cfg->shared_buf;
    enc_hal_cfg.only_smartp = cfg->only_smartp;
    p->ring_buf_size = cfg->buf_size;
    p->max_strm_cnt = cfg->max_strm_cnt;
    p->ntfy_mode = cfg->ntfy_mode;
    ctrl_cfg.coding = coding;
    ctrl_cfg.type = VPU_CLIENT_BUTT;
    ctrl_cfg.cfg = p->cfg;
    ctrl_cfg.refs = p->refs;
    ctrl_cfg.task_count = 2;

    kmpp_sema_init(&p->enc_sem, 1);
    ret = mpp_enc_hal_init(&enc_hal, &enc_hal_cfg);
    if (ret) {
        kmpp_loge_f("could not init enc hal\n");
        goto ERR_RET;
    }

    ctrl_cfg.type = enc_hal_cfg.type;
    ctrl_cfg.task_count = -1;
    if (coding == MPP_VIDEO_CodingHEVC)
        p->cfg->h265.tmvp_enable = cfg->tmvp_enable;
    ret = enc_impl_init(&impl, &ctrl_cfg);
    if (ret) {
        kmpp_loge_f("could not init impl\n");
        goto ERR_RET;
    }
    mpp_enc_impl_alloc_task(p);

    p->coding = coding;
    p->impl = impl;
    p->enc_hal = enc_hal;
    p->dev = enc_hal_cfg.dev;
    p->sei_mode = MPP_ENC_SEI_MODE_DISABLE;
    p->version_info = KMPP_VERSION;
    p->version_length = kmpp_strlen(p->version_info);
    p->rc_cfg_size = SZ_1K;
    p->rc_cfg_info = mpp_calloc_size(char, p->rc_cfg_size);

    {
        /* create header packet storage */
        size_t size = SZ_1K;

        p->hdr_buf = mpp_calloc_size(void, size);
        ret = kmpp_packet_init_with_data(&p->hdr_pkt, p->hdr_buf, size);
        if (ret)
            goto ERR_RET;
        kmpp_packet_set_length(p->hdr_pkt, 0);
    }

    ret = mpp_enc_init_cfg(p, cfg);
    if (ret)
        goto ERR_RET;

    copy_enc_cfg(p->set_obj, p->cfg_obj);

    p->rc_brief.name = cfg->smart_en ? str_smart : NULL;
    p->rc_brief.type = coding;
    mpp_enc_proc_rc_update(p);

    if ((ret = mpp_enc_ref_cfg_init(&p->cfg->ref_cfg)))
        goto ERR_RET;

    if ((ret = mpp_enc_ref_cfg_copy(p->cfg->ref_cfg, mpp_enc_ref_default())))
        goto ERR_RET;

    if ((ret = mpp_enc_ref_cfg_init(&p->set->ref_cfg)))
        goto ERR_RET;

    if ((ret = mpp_enc_ref_cfg_copy(p->set->ref_cfg, mpp_enc_ref_default())))
        goto ERR_RET;

    if ((ret = mpp_enc_refs_set_cfg(p->refs, mpp_enc_ref_default())))
        goto ERR_RET;

    if ((ret = mpp_enc_refs_set_rc_igop(p->refs, p->cfg->rc.gop)))
        goto ERR_RET;

    p->stop_flag = 1;
    kmpp_atomic_init(&p->hw_run);
    kmpp_atomic_set(p->hw_run, 0);
    p->rb_userdata.free_cnt = MAX_USRDATA_CNT;

    mpp_enc_ringbuf_pool_init(p, p->ring_buf_size);
    p->online = cfg->online;
    p->shared_buf = cfg->shared_buf;
    p->chan_id = cfg->chan_id;
    p->ref_buf_shared = cfg->shared_buf_en;
    if (p->ntfy_mode)
        kmpp_venc_ntfy_get(&p->venc_notify);

    *enc = p;

    return ret;
ERR_RET:
    mpp_enc_deinit(p);
    return ret;
}

void mpp_enc_deinit_frame(MppEnc ctx)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;
    KmppVencNtfy ntfy;
    KmppVencNtfyImpl* ntfy_impl;

    if (!enc)
        return;

    kmpp_atomic_set(enc->hw_run, 0);

    if (enc->packet) {
        kmpp_packet_ring_buf_put_used(enc->packet);
        kmpp_packet_put(enc->packet);
        enc->packet = NULL;
    }

    if (enc->frame) {
        ntfy = mpp_enc_get_notify(enc);
        if (ntfy) {
            ntfy_impl = (KmppVencNtfyImpl*)kmpp_obj_to_entry(ntfy);

            ntfy_impl->chan_id = enc->chan_id;
            ntfy_impl->frame = enc->frame;

            ntfy_impl->cmd = KMPP_NOTIFY_VENC_TASK_DROP;
            ntfy_impl->drop_type = KMPP_VENC_DROP_CFG_FAILED;
            kmpp_venc_notify(ntfy);
        }
        enc->frame = NULL;
    }
}

MPP_RET mpp_enc_deinit(MppEnc ctx)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;
    MPP_RET ret = MPP_OK;

    kmpp_down(enc->enc_sem);
    if (!enc) {
        kmpp_loge_f("found NULL input\n");
        return MPP_ERR_NULL_PTR;
    }
    mpp_enc_deinit_frame(enc);
    mpp_enc_impl_free_task(enc);
    if (enc->impl) {
        enc_impl_deinit(enc->impl);
        enc->impl = NULL;
    }
    if (enc->enc_hal) {
        mpp_enc_hal_deinit(enc->enc_hal);
        enc->enc_hal = NULL;
    }

    if (enc->hdr_pkt) {
        kmpp_packet_put(enc->hdr_pkt);
        enc->hdr_pkt = NULL;
    }

    MPP_FREE(enc->hdr_buf);

    if (enc->cfg_obj) {
        KmppVencStCfg obj = enc->cfg_obj;
        MppEncCfgSet *cfg = kmpp_obj_to_entry(obj);

        if (cfg->ref_cfg) {
            mpp_enc_ref_cfg_deinit(&cfg->ref_cfg);
            cfg->ref_cfg = NULL;
        }

        kmpp_venc_st_cfg_put(obj);
        enc->cfg_obj = NULL;
        enc->cfg = NULL;
    }

    if (enc->set_obj) {
        KmppVencStCfg obj = enc->set_obj;
        MppEncCfgSet *cfg = kmpp_obj_to_entry(obj);

        if (cfg->ref_cfg) {
            mpp_enc_ref_cfg_deinit(&cfg->ref_cfg);
            cfg->ref_cfg = NULL;
        }

        kmpp_venc_st_cfg_put(obj);
        enc->set_obj = NULL;
        enc->set = NULL;
    }

    if (enc->refs) {
        mpp_enc_refs_deinit(&enc->refs);
        enc->refs = NULL;
    }

    if (enc->rc_ctx) {
        rc_deinit(enc->rc_ctx);
        enc->rc_ctx = NULL;
    }

    if (enc->ring_pool) {
        if (!enc->shared_buf || !enc->shared_buf->stream_buf) {
            if (enc->ring_pool->buf)
                mpp_buffer_put(enc->ring_pool->buf);
        }
        ring_buf_pool_put(enc->ring_pool);
        enc->ring_pool = NULL;
    }
    if (enc->venc_notify) {
        kmpp_venc_ntfy_put(enc->venc_notify);
        enc->venc_notify = NULL;
    }
    MPP_FREE(enc->rc_cfg_info);
    enc->rc_cfg_size = 0;
    enc->rc_cfg_length = 0;
    kmpp_up(enc->enc_sem);
    kmpp_sema_deinit(&enc->enc_sem);
    kmpp_atomic_deinit(&enc->hw_run);
    mpp_free(enc);

    return ret;
}

MPP_RET mpp_enc_start(MppEnc ctx)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;

    enc_dbg_func("%p in\n", enc);

    kmpp_down(enc->enc_sem);
    // snprintf(name, sizeof(name) - 1, "mpp_%se_%d",
    //   strof_coding_type(enc->coding), getpid());
    enc->stop_flag = 0;
    kmpp_up(enc->enc_sem);

    enc_dbg_func("%p out\n", enc);

    return MPP_OK;
}

MPP_RET mpp_enc_stop(MppEnc ctx)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;
    MPP_RET ret = MPP_OK;

    enc_dbg_func("%p in\n", enc);

    kmpp_down(enc->enc_sem);
    enc->stop_flag = 1;
    ret = kmpp_atomic_read(enc->hw_run);
    kmpp_up(enc->enc_sem);

    enc_dbg_func("%p out\n", enc);

    return ret;

}

MPP_RET mpp_enc_reset(MppEnc ctx)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;

    enc_dbg_func("%p in\n", enc);

    if (!enc) {
        kmpp_loge_f("found NULL input enc\n");
        return MPP_ERR_NULL_PTR;
    }

    return MPP_OK;
}

MPP_RET mpp_enc_oneframe(MppEnc ctx, KmppFrame frame, KmppPacket *packet)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;
    MPP_RET ret = MPP_OK;

    if (!enc) {
        kmpp_loge_f("found NULL input enc\n");
        return MPP_ERR_NULL_PTR;
    }

    enc_dbg_func("%p in\n", enc);
    //ret = mpp_enc_impl_oneframe(ctx, frame, packet);
    enc_dbg_func("%p out\n", enc);

    return ret;
}

MPP_RET mpp_enc_online_task_failed(MppEnc ctx)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;
    RK_U32 connect = 0;
    MPP_RET ret = MPP_OK;

    kmpp_down(enc->enc_sem);
    if (enc->stop_flag) {
        kmpp_up(enc->enc_sem);
        return MPP_NOK;
    }
    ret = mpp_dev_chnl_control(enc->dev, MPP_CMD_VEPU_CONNECT_DVBM, &connect);
    kmpp_up(enc->enc_sem);

    return ret;
}

MPP_RET mpp_enc_force_pskip(MppEnc ctx, KmppFrame frame, KmppPacket *packet)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;
    MPP_RET ret = MPP_OK;

    if (!enc) {
        kmpp_loge_f("found NULL input enc\n");
        return MPP_ERR_NULL_PTR;
    }

    enc_dbg_func("%p in\n", enc);

    kmpp_down(enc->enc_sem);
    if (enc->stop_flag) {
        kmpp_up(enc->enc_sem);
        return MPP_NOK;
    }
    mpp_enc_proc_rc_update(enc);
    ret = mpp_enc_impl_force_pskip(enc, frame, packet);
    kmpp_up(enc->enc_sem);

    enc_dbg_func("%p out\n", enc);

    return ret;
}

MPP_RET mpp_enc_cfg_reg(MppEnc ctx, KmppFrame frame)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;
    MPP_RET ret = MPP_OK;

    if (!enc) {
        kmpp_loge_f("found NULL input enc\n");
        return MPP_ERR_NULL_PTR;
    }

    enc_dbg_func("%p in\n", enc);

    kmpp_down(enc->enc_sem);
    if (enc->stop_flag) {
        kmpp_up(enc->enc_sem);
        return MPP_NOK;
    }
    mpp_enc_proc_rc_update(enc);
    enc->enc_status = ENC_STATUS_CFG_IN;
    ret = mpp_enc_impl_reg_cfg(ctx, frame);
    enc->enc_status = ENC_STATUS_CFG_DONE;
    kmpp_up(enc->enc_sem);

    enc_dbg_func("%p out\n", enc);

    return ret;
}

MPP_RET mpp_enc_hw_start(MppEnc ctx, MppEnc jpeg_ctx)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;
    MppEncImpl *jpeg_enc = (MppEncImpl *)jpeg_ctx;
    MPP_RET ret = MPP_OK;

    if (!enc) {
        kmpp_loge_f("found NULL input enc\n");
        return MPP_ERR_NULL_PTR;
    }

    enc_dbg_func("%p in\n", enc);

    kmpp_down(enc->enc_sem);
    if (enc->stop_flag) {
        enc->frame = NULL;
        kmpp_up(enc->enc_sem);
        return MPP_NOK;
    }
    if (jpeg_enc) {
        kmpp_down(jpeg_enc->enc_sem);
        if (jpeg_enc->stop_flag) {
            jpeg_enc->frame = NULL;
            kmpp_up(jpeg_enc->enc_sem);
            return MPP_NOK;
        }
        jpeg_enc->enc_status = ENC_STATUS_START_IN;
    }
    enc->enc_status = ENC_STATUS_START_IN;
    ret = mpp_enc_impl_hw_start(ctx, jpeg_ctx);
    enc->enc_status = ENC_STATUS_START_DONE;

    if (MPP_OK == ret) {
        if (enc->online) {
            KmppVencNtfy ntfy = mpp_enc_get_notify(enc);

            if (ntfy) {
                KmppVencNtfyImpl* ntfy_impl = kmpp_obj_to_entry(ntfy);

                ntfy_impl->cmd = KMPP_NOTIFY_VENC_WRAP_TASK_READY;
                ntfy_impl->chan_id = enc->chan_id;
                ntfy_impl->frame = enc->frame;
                kmpp_venc_notify(ntfy);
            }
        }
        kmpp_atomic_set(enc->hw_run, 1);
    }
    if (jpeg_enc) {
        jpeg_enc->enc_status = ENC_STATUS_START_DONE;
        kmpp_up(jpeg_enc->enc_sem);
    }
    kmpp_up(enc->enc_sem);

    enc_dbg_func("%p out\n", enc);

    return ret;
}

RK_S32 mpp_enc_run_task(MppEnc ctx, RK_S64 pts, RK_S64 dts)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;
    MPP_RET ret = MPP_OK;
    MppTaskInfo info;
    RK_U32 align = enc->coding == MPP_VIDEO_CodingHEVC ? 8 : 16;

    if (!enc) {
        kmpp_loge_f("found NULL input enc\n");
        return MPP_ERR_NULL_PTR;
    }

    enc_dbg_func("%p in\n", enc);

    if (enc->enc_status != ENC_STATUS_START_DONE) {
        ret = MPP_NOK;
        goto done;
    }

    enc->enc_status = ENC_STATUS_RUN_TASK_IN;
    /*
     * The frame configuration is gennerated in advance,
     * so the pts/dts info may not match to per frame.
     * So, re-update the pts/dts here.
     */
    if (enc->packet) {
        kmpp_packet_set_pts(enc->packet, pts);
        kmpp_packet_set_dts(enc->packet, dts);
    }
    if (enc->frame) {
        kmpp_frame_set_pts(enc->frame, pts);
        kmpp_frame_set_dts(enc->frame, dts);
    }

    {
        KmppVencNtfy ntfy = mpp_enc_get_notify(enc);

        if (ntfy) {
            KmppVencNtfyImpl* ntfy_impl = kmpp_obj_to_entry(ntfy);

            ntfy_impl->cmd = KMPP_NOTIFY_VENC_GET_WRAP_TASK_ID;
            ntfy_impl->chan_id = enc->chan_id;
            ntfy_impl->frame = enc->frame;
            kmpp_venc_notify(ntfy);

            kmpp_venc_ntfy_get_pipe_id(ntfy, &info.pipe_id);
            kmpp_venc_ntfy_get_frame_id(ntfy, &info.frame_id);
        }
    }

    info.width = MPP_ALIGN(enc->cfg->prep.width, align);
    info.height = MPP_ALIGN(enc->cfg->prep.height, align);

    ret = mpp_dev_ioctl(enc->dev, MPP_DEV_CMD_RUN_TASK, &info);

    enc->enc_status = ret ? ENC_STATUS_START_DONE : ENC_STATUS_RUN_TASK_DONE;

    enc_dbg_func("%p out\n", enc);
done:
    if (ret) {
        RK_U32 connect = 1;
        mpp_dev_chnl_control(enc->dev, MPP_CMD_VEPU_CONNECT_DVBM, &connect);
    }

    return ret;
}

MPP_RET mpp_enc_set_online_mode(MppEnc ctx, RK_U32 mode)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;
    MPP_RET ret = MPP_OK;

    if (!enc) {
        kmpp_loge_f("found NULL input enc\n");
        return MPP_ERR_NULL_PTR;
    }

    enc_dbg_func("%p in\n", enc);

    enc->online = mode;
    mpp_dev_chnl_control(enc->dev, MPP_CMD_VEPU_SET_ONLINE_MODE, &mode);

    enc_dbg_func("%p out\n", enc);

    return ret;
}

RK_S32 mpp_enc_check_hw_running(MppEnc ctx)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;

    if (!enc) {
        kmpp_loge_f("found NULL input enc\n");
        return MPP_ERR_NULL_PTR;
    }

    return mpp_dev_chnl_check_running(enc->dev);
}

RK_S32 mpp_enc_unbind_jpeg_task(MppEnc ctx)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;

    if (!enc) {
        kmpp_loge_f("found NULL input enc\n");
        return MPP_ERR_NULL_PTR;
    }

    return mpp_dev_chnl_unbind_jpeg_task(enc->dev);
}

rk_bool mpp_enc_check_is_int_process(MppEnc ctx)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;
    rk_bool ret = rk_false;

    if (!enc) {
        kmpp_loge_f("found NULL input enc\n");
        return rk_false;
    }

    kmpp_down(enc->enc_sem);
    if (enc->enc_status == ENC_STATUS_INT_IN)
        ret = rk_true;
    kmpp_up(enc->enc_sem);

    return ret;
}

MPP_RET mpp_enc_int_process(MppEnc ctx, MppEnc jpeg_ctx, KmppPacket *packet,
                            KmppPacket *jpeg_packet)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;
    MppEncImpl *jpeg_enc = (MppEncImpl *)jpeg_ctx;
    MPP_RET ret = MPP_OK;

    if (!enc) {
        kmpp_loge_f("found NULL input enc\n");
        return MPP_ERR_NULL_PTR;
    }

    enc_dbg_func("%p in\n", enc);

    enc->enc_status = ENC_STATUS_INT_IN;
    if (jpeg_enc)
        jpeg_enc->enc_status = ENC_STATUS_INT_IN;
    ret = mpp_enc_impl_int(ctx, jpeg_ctx, packet, jpeg_packet);
    enc->enc_status = ENC_STATUS_INT_DONE;
    if (jpeg_enc)
        jpeg_enc->enc_status = ENC_STATUS_INT_DONE;
    kmpp_atomic_set(enc->hw_run, 0);

    enc_dbg_func("%p out\n", enc);

    return ret;
}

void mpp_enc_slice_info(MppEnc ctx, void *param, KmppPacket *packet)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;

    enc->enc_status = ENC_STATUS_SLICE_INFO_IN;
    mpp_enc_impl_slice_info(ctx, param, packet);
    enc->enc_status = ENC_STATUS_SLICE_INFO_DONE;

    return;
}

void mpp_enc_proc_debug(KmppEnvNode node, MppEnc ctx, RK_U32 chl_id)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;

    if (!enc) {
        kmpp_loge_f("found NULL input enc\n");
        return;
    }

    enc_dbg_func("%p in\n", enc);

    mpp_enc_impl_poc_debug_info(node, ctx, chl_id);

    enc_dbg_func("%p out\n", enc);

    return;
}


MPP_RET mpp_enc_register_chl(MppEnc ctx, void *func, RK_S32 chan_id)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;
    MPP_RET ret = MPP_OK;

    if (!enc) {
        kmpp_loge_f("found NULL input enc\n");
        return MPP_ERR_NULL_PTR;
    }

    return ret;
}

MPP_RET mpp_enc_notify(MppEnc ctx, RK_U32 flag)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;

    enc_dbg_func("%p in flag %08x\n", enc, flag);

    enc_dbg_func("%p out\n", enc);

    return MPP_OK;
}

/*
 * preprocess config and rate-control config is common config then they will
 * be done in mpp_enc layer
 *
 * codec related config will be set in each hal component
 */
MPP_RET mpp_enc_control(MppEnc ctx, MpiCmd cmd, void *param)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;
    MPP_RET ret = MPP_OK;

    if (!enc) {
        kmpp_loge_f("found NULL enc\n");
        return MPP_ERR_NULL_PTR;
    }

    if (!param && cmd != MPP_ENC_SET_IDR_FRAME) {
        kmpp_loge_f("found NULL param enc %p cmd %x\n", enc, cmd);
        return MPP_ERR_NULL_PTR;
    }

    enc_dbg_ctrl("sending cmd %d param %p\n", cmd, param);

    switch (cmd) {
    case MPP_ENC_GET_CFG: {
        enc_dbg_ctrl("get enc config start\n");
        copy_enc_cfg(param, enc->cfg_obj);
        enc_dbg_ctrl("get enc config done\n");
    } break;
    case MPP_ENC_SET_PREP_CFG :
    case MPP_ENC_GET_PREP_CFG :
    case MPP_ENC_SET_RC_CFG :
    case MPP_ENC_GET_RC_CFG :
    case MPP_ENC_SET_CODEC_CFG :
    case MPP_ENC_GET_CODEC_CFG :
    case MPP_ENC_SET_SPLIT :
    case MPP_ENC_GET_SPLIT : {
        kmpp_loge("deprecated %s control use MPP_ENC_GET_CFG / MPP_ENC_SET_CFG instead\n",
                  cmd == MPP_ENC_SET_PREP_CFG ? "MPP_ENC_SET_PREP_CFG" :
                  cmd == MPP_ENC_GET_PREP_CFG ? "MPP_ENC_GET_PREP_CFG" :
                  cmd == MPP_ENC_SET_RC_CFG ? "MPP_ENC_SET_RC_CFG" :
                  cmd == MPP_ENC_GET_RC_CFG ? "MPP_ENC_GET_RC_CFG" :
                  cmd == MPP_ENC_SET_CODEC_CFG ? "MPP_ENC_SET_CODEC_CFG" :
                  cmd == MPP_ENC_GET_CODEC_CFG ? "MPP_ENC_GET_CODEC_CFG" :
                  cmd == MPP_ENC_SET_SPLIT ? "MPP_ENC_SET_SPLIT" :
                  cmd == MPP_ENC_GET_SPLIT ? "MPP_ENC_GET_SPLIT" : "unknown");
        ret = MPP_NOK;
    } break;
    case MPP_ENC_GET_HEADER_MODE: {
        enc_dbg_ctrl("get header mode\n");
        kmpp_memcpy(param, &enc->hdr_mode, sizeof(enc->hdr_mode));
    } break;
    case MPP_ENC_GET_REF_CFG: {
        MppEncCfgSet *cfg = enc->cfg;

        enc_dbg_ctrl("get ref config\n");
        kmpp_memcpy(param, &cfg->ref_param, sizeof(cfg->ref_param));
    } break;
    case MPP_ENC_GET_ROI_CFG: {
        MppEncCfgSet *cfg = enc->cfg;

        enc_dbg_ctrl("get roi config\n");
        kmpp_memcpy(param, &cfg->roi, sizeof(cfg->roi));
    } break;
    case MPP_ENC_GET_JPEG_ROI_CFG: {
        MppEncCfgSet *cfg = enc->cfg;

        enc_dbg_ctrl("get jpeg roi config\n");
        kmpp_memcpy(param, &cfg->jpeg_roi, sizeof(cfg->jpeg_roi));
    } break;
    default : {
        enc_dbg_ctrl("sending cmd %d start\n", cmd);

        kmpp_down(enc->enc_sem);
        mpp_enc_proc_cfg(enc, cmd, param);
        mpp_enc_hal_prepare(enc->enc_hal);
        kmpp_up(enc->enc_sem);

        enc_dbg_ctrl("sending cmd %d done\n", cmd);
    } break;
    }

    return ret;
}

void mpp_enc_pkt_full_inc(MppEnc ctx)
{
    mpp_enc_impl_pkt_full_inc(ctx);
}

RK_S32 mpp_enc_get_fps_out(MppEnc ctx)
{
    MppEncCfgSet *cfg = NULL;
    MppEncImpl *enc = (MppEncImpl *)ctx;
    RK_S32 fps_out = -1;

    if (!enc) {
        kmpp_loge_f("found NULL input enc\n");
        return MPP_ERR_NULL_PTR;
    }

    cfg = enc->cfg;
    if (cfg->rc.fps_out_num && cfg->rc.fps_out_denom)
        fps_out = cfg->rc.fps_out_num / cfg->rc.fps_out_denom;

    return fps_out;
}

KmppVencNtfy mpp_enc_get_notify(MppEnc ctx)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;

    if (!enc) {
        kmpp_loge_f("found NULL input enc\n");
        return NULL;
    }

    if (!enc->ntfy_mode)
        return NULL;

    return enc->venc_notify;
}

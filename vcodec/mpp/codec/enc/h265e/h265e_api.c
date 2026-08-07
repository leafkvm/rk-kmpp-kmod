/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "h265e_api"

#include "kmpp_string.h"

#include "mpp_mem.h"
#include  "mpp_2str.h"
#include "rc.h"
#include "h265e_api.h"
#include "h265e_slice.h"
#include "h265e_codec.h"
#include "h265e_syntax_new.h"
#include "h265e_ps.h"
#include "h265e_header_gen.h"
#include "kmpp_packet.h"

RK_U32 h265e_debug = 0;

static MPP_RET h265e_init(void *ctx, EncImplCfg * ctrlCfg)
{
    H265eCtx *p = (H265eCtx *) ctx;
    MPP_RET ret = MPP_OK;

    if (ctx == NULL) {
        kmpp_loge_f("invalid NULL ctx\n");
        return MPP_ERR_NULL_PTR;
    }
//   mpp_env_get_u32("h265e_debug", &h265e_debug, 0);
    h265e_dbg_func("enter ctx %p\n", ctx);

    kmpp_assert(ctrlCfg->coding == MPP_VIDEO_CodingHEVC);
    p->cfg = ctrlCfg->cfg;

    kmpp_memset(&p->syntax, 0, sizeof(p->syntax));
    ctrlCfg->task_count = 1;

    p->extra_info = mpp_calloc(H265eExtraInfo, 1);

    h265e_init_extra_info(p->extra_info);

    h265e_dbg_func("leave ctx %p\n", ctx);

    return ret;
}

static MPP_RET h265e_deinit(void *ctx)
{
    H265eCtx *p = (H265eCtx *) ctx;

    h265e_dbg_func("enter ctx %p\n", ctx);

    if (ctx == NULL) {
        kmpp_loge_f("invalid NULL ctx\n");
        return MPP_ERR_NULL_PTR;
    }

    h265e_deinit_extra_info(p->extra_info);

    MPP_FREE(p->extra_info);

    h265e_dpb_deinit(p->dpb);

    MPP_FREE(p->pskip_buf);

    h265e_dbg_func("leave ctx %p\n", ctx);

    return MPP_OK;
}

static MPP_RET h265e_gen_hdr(void *ctx, KmppPacket pkt)
{
    H265eCtx *p = (H265eCtx *) ctx;

    h265e_dbg_func("enter ctx %p\n", ctx);

    h265e_set_extra_info(p);
    h265e_get_extra_info(p, pkt);

    if (NULL == p->dpb)
        h265e_dpb_init(&p->dpb);

    h265e_dbg_func("leave ctx %p\n", ctx);

    return MPP_OK;
}

static MPP_RET h265e_start(void *ctx, HalEncTask *task)
{
    H265eCtx *p = (H265eCtx *) ctx;
    (void)p;

    h265e_dbg_func("enter\n");

    /*
     * Step 2: Fps conversion
     *
     * Determine current frame which should be encoded or not according to
     * input and output frame rate.
     */

    if (!task->valid)
        kmpp_logi_f("drop one frame\n");

    /*
     * Step 3: Backup dpb for reencode
     */
    //h265e_dpb_copy(&p->dpb_bak, p->dpb);
    h265e_dbg_func("leave\n");

    return MPP_OK;
}

static MPP_RET h265e_proc_dpb(void *ctx, HalEncTask *task)
{
    H265eCtx *p = (H265eCtx *) ctx;
    EncRcTask *rc_task = task->rc_task;
    EncCpbStatus *cpb = &task->rc_task->cpb;

    h265e_dbg_func("enter\n");

    h265e_dpb_proc_cpb(p->dpb, cpb);
    h265e_dpb_get_curr(p->dpb);
    h265e_slice_init(ctx, cpb->curr);
    if (h265e_dpb_build_list(p->dpb, cpb))
        return MPP_NOK;

    rc_task->frm = p->dpb->curr->status;

    h265e_dbg_func("leave\n");

    return MPP_OK;
}

static MPP_RET h265e_proc_hal(void *ctx, HalEncTask *task)
{
    H265eCtx *p = (H265eCtx *) ctx;
    H265eSyntax_new *syntax = NULL;
//   EncFrmStatus *frm = &task->rc_task->frm;
//   KmppPacket packet = task->packet;
//    MppMeta meta = mpp_packet_get_meta(packet);

    if (ctx == NULL) {
        kmpp_loge_f("invalid NULL ctx\n");
        return MPP_ERR_NULL_PTR;
    }
//    mpp_meta_set_s32(meta, KEY_TEMPORAL_ID, frm->temporal_id);
    h265e_dbg_func("enter ctx %p\n", ctx);

    syntax = &p->syntax;
    h265e_syntax_fill(ctx);
    task->valid = 1;
    task->syntax.data = syntax;

    h265e_dbg_func("leave ctx %p\n", ctx);

    return MPP_OK;
}

static MPP_RET h265e_proc_enc_skip(void *ctx, HalEncTask *task)
{
    H265eCtx *p = (H265eCtx *) ctx;
    KmppPacket pkt = task->packet;
    RK_U8 *ptr = NULL;
    KmppShmPtr pos;
    RK_U32 offset;
    RK_S32 size;
    RK_U32 len;
    RK_U32 new_length = 0;

    h265e_dbg_func("enter\n");

    kmpp_packet_get_pos(pkt, &pos);
    ptr = pos.kptr;
    kmpp_packet_get_length(pkt, &offset);
    kmpp_packet_get_size(pkt, &size);
    len = size - offset;

    ptr += offset;
    p->slice->slice_qp = task->rc_task->info.quality_target;
    new_length = h265e_code_slice_skip_frame(ctx, p->slice, ptr, len);
    task->length = new_length;
    task->rc_task->info.bit_real = 8 * new_length;
    ///mpp_packet_set_length(pkt, offset + new_length);

    h265e_dbg_func("leave\n");

    return MPP_OK;
}

static MPP_RET h265e_add_sei(KmppPacket pkt, RK_S32 * length, RK_U8 uuid[16],
                             const void *data, RK_S32 size)
{
    KmppShmPtr pos;
    RK_U8 *ptr = NULL;
    RK_U32 offset;
    RK_U32 new_length = 0;

    kmpp_packet_get_pos(pkt, &pos);
    ptr = pos.kptr;
    kmpp_packet_get_length(pkt, &offset);

    ptr += offset;
    if (uuid == uuid_refresh_cfg) {
        RK_U32 recovery_frame_cnt = ((RK_U32 *)data)[0] - 1;
        new_length = h265e_sei_recovery_point(ptr, uuid, &recovery_frame_cnt, 0);
    } else
        new_length = h265e_data_to_sei(ptr, uuid, data, size);
    *length = new_length;

    kmpp_packet_set_length(pkt, offset + new_length);

    return MPP_OK;
}

static MPP_RET h265e_proc_cfg(void *ctx, MpiCmd cmd, void *param)
{
    H265eCtx *p = (H265eCtx *) ctx;
    MPP_RET ret = MPP_OK;

    h265e_dbg_func("enter ctx %p cmd %08x\n", ctx, cmd);

    switch (cmd) {
    case MPP_ENC_SET_CFG: {
    }
    break;
    case MPP_ENC_GET_EXTRA_INFO: {
        KmppPacket pkt_out = (KmppPacket)param;
        h265e_set_extra_info(p);
        h265e_get_extra_info(p, pkt_out);
    }
    break;
    case MPP_ENC_SET_SEI_CFG: {
    }
    break;
    default:
        kmpp_loge_f("No correspond %08x found, and can not config!\n", cmd);
        ret = MPP_NOK;
        break;
    }

    h265e_dbg_func("leave ctx %p\n", ctx);

    return ret;
}

static MPP_RET h265e_init_cfg(MppEncCfgSet *cfg, const rk_u8 *preset)
{
    MppEncRcCfg *rc_cfg = &cfg->rc;
    MppEncPrepCfg *prep = &cfg->prep;
    MppEncH265Cfg *h265 = &cfg->h265;
    MppEncFineTuneCfg *tune = &cfg->tune;

    (void)preset;
    /* set defualt value of codec */
    h265->qpmap_mode = 1;

    h265->profile = MPP_PROFILE_HEVC_MAIN;
    h265->level = 120;

    h265->ctu_size = 32;
    h265->max_cu_size = 32;
    h265->amp_enable = 0;
    h265->num_ref = 1;

    h265->slice_cfg.split_enable = 0;
    h265->entropy_cfg.cabac_init_flag = 0;
    h265->sao_cfg.slice_sao_chroma_disable = 1;
    h265->sao_cfg.slice_sao_luma_disable = 1;
    h265->sao_cfg.sao_bit_ratio = 5;
    h265->dblk_cfg.slice_deblocking_filter_disabled_flag = 0;
    h265->cu_cfg.strong_intra_smoothing_enabled_flag = 1;
    h265->merge_cfg.max_mrg_cnd = 2;
    h265->merge_cfg.merge_left_flag = 1;
    h265->merge_cfg.merge_up_flag = 1;
    h265->trans_cfg.scaling_list_mode = 0;
    h265->trans_cfg.diff_cu_qp_delta_depth = 1;

    tune->scene_mode = MPP_ENC_SCENE_MODE_IPC;
    tune->motion_static_switch_enable = 0;
    tune->lgt_chg_lvl = 0;
    tune->static_frm_num = 0;
    tune->madp16_th = 15;
    tune->skip16_wgt = 6;
    tune->skip32_wgt = 6;
    tune->speed = 1;
    tune->bg_delta_qp_i = -5;
    tune->bg_delta_qp_p = -5;
    tune->fg_delta_qp_i = 4;
    tune->fg_delta_qp_p = 4;
    tune->bmap_qpmin_i = 20;
    tune->bmap_qpmin_p = 20;
    tune->bmap_qpmax_i = 51;
    tune->bmap_qpmax_p = 51;
    tune->min_bg_fqp =  35;
    tune->max_bg_fqp =  40;
    tune->min_fg_fqp =  30;
    tune->max_fg_fqp =  35;
    tune->fg_area = -1;
    tune->se_mode = MPP_ENC_SE_MODE_QF;

    /*
     * default prep:
     * 720p
     * YUV420SP
     */
    prep->format = MPP_FMT_YUV420SP;
    prep->color = MPP_FRAME_SPC_UNSPECIFIED;
    prep->colorprim = MPP_FRAME_PRI_UNSPECIFIED;
    prep->colortrc = MPP_FRAME_TRC_UNSPECIFIED;
    prep->range = MPP_FRAME_RANGE_UNSPECIFIED;
    prep->rotation = MPP_ENC_ROT_0;
    prep->rotation_ext = MPP_ENC_ROT_0;
    prep->mirroring = 0;
    prep->mirroring_ext = 0;
    prep->denoise = 0;
    prep->flip = 0;

    /*
     * default rc_cfg:
     * CBR
     * 2Mbps +-25%
     * 30fps
     * gop 60
     */
    rc_cfg->quality = MPP_ENC_RC_QUALITY_MEDIUM;
    rc_cfg->bps_target = 2000 * 1000;
    rc_cfg->bps_max = rc_cfg->bps_target * 5 / 4;
    rc_cfg->bps_min = rc_cfg->bps_target * 3 / 4;
    rc_cfg->fps_in_flex = 0;
    rc_cfg->fps_in_num = 30;
    rc_cfg->fps_in_denom = 1;
    rc_cfg->fps_out_flex = 0;
    rc_cfg->fps_out_num = 30;
    rc_cfg->fps_out_denom = 1;
    rc_cfg->gop = 60;
    rc_cfg->max_reenc_times = 1;
    rc_cfg->max_i_prop = 30;
    rc_cfg->min_i_prop = 10;
    rc_cfg->init_ip_ratio = 160;
    rc_cfg->qp_init = 26;
    rc_cfg->qp_max = 51;
    rc_cfg->qp_min = 10;
    rc_cfg->qp_max_i = 51;
    rc_cfg->qp_min_i = 15;
    rc_cfg->qp_delta_ip = 4;
    rc_cfg->qp_delta_vi = 2;
    rc_cfg->fqp_min_i = 26;
    rc_cfg->fqp_min_p = 28;
    rc_cfg->fqp_max_i = 51;
    rc_cfg->fqp_max_p = 51;
    rc_cfg->mt_st_swth_frm_qp = 32;

    return MPP_OK;
}

static void h265e_proc_show(KmppEnvNode node, void *ctx, RK_S32 chl_id)
{
    H265eCtx *p = (H265eCtx *) ctx;
    MppEncCfgSet *cfg = p->cfg;
    MppEncPrepCfg *prep = &cfg->prep;
    MppEncH265Cfg *h265 = &cfg->h265;

    kmpp_env_log(node, "\n--------h265e chn attr-----------------------------------------------------------------------------------------------------------\n");
    kmpp_env_log(node, "%7s|%10s|%10s|%10s\n", "ID", "Width", "Height", "profile");
    kmpp_env_log(node, "%7d|%10u|%10u|%10s\n", chl_id, prep->width, prep->height,
                 strof_profle(MPP_VIDEO_CodingHEVC, h265->profile));

    kmpp_env_log(node, "\n--------Syntax INFO1-------------------------------------------------------------------------------------------------------------\n");
    kmpp_env_log(node, "%7s|%10s|%10s|%10s|%15s|%15s|%15s\n",
                 "ID", "SlcspltEn", "SplitMode", "Slcsize",
                 "IntraRefresh", "RefreshMode", "RefreshNum");
    kmpp_env_log(node, "%7d|%10s|%10u|%10u|%15s|%15u|%15u\n",
                 chl_id, strof_bool(cfg->split.split_mode), cfg->split.split_mode, cfg->split.split_arg,
                 strof_bool(cfg->rc.refresh_en), cfg->rc.refresh_mode, cfg->rc.refresh_num);

    kmpp_env_log(node, "--------Syntax INFO2-------------------------------------------------------------------------------------------------------------\n");
    kmpp_env_log(node, "%7s|%8s|%6s|%6s|%10s|%10s|%15s|%12s|%8s|%6s|%10s|%10s\n",
                 "ID", "DblkEn",
                 "Tc", "Beta",
                 "Saoluma", "Saochroma",
                 "IntraSmoothing", "ScalingMode", "TmvpEn", "CuDqp", "sao_str_i", "sao_str_p");
    kmpp_env_log(node, "%7d|%8s|%6d|%6d|%10d|%10d|%15d|%12d|%8d|%6d|%10d|%10d\n",
                 chl_id, strof_bool(1 - h265->dblk_cfg.slice_deblocking_filter_disabled_flag),
                 h265->dblk_cfg.slice_tc_offset_div2, h265->dblk_cfg.slice_beta_offset_div2,
                 !h265->sao_cfg.slice_sao_luma_disable, !h265->sao_cfg.slice_sao_chroma_disable,
                 !h265->cu_cfg.strong_intra_smoothing_enabled_flag, h265->trans_cfg.scaling_list_mode,
                 h265->tmvp_enable, h265->trans_cfg.diff_cu_qp_delta_depth, cfg->tune.sao_str_i,
                 cfg->tune.sao_str_p);

    kmpp_env_log(node, "------Trans INFO-----------------------------------------------------------------------------------------------------------------\n");
    kmpp_env_log(node, "%7s|%12s|%12s\n", "ID", "CbQpOffset", "CrQpOffset");
    kmpp_env_log(node, "%7d|%12d|%12d\n", chl_id, h265->trans_cfg.cb_qp_offset,
                 h265->trans_cfg.cr_qp_offset);
}

const EncImplApi api_h265e = {
    "h265e_control",
    MPP_VIDEO_CodingHEVC,
    sizeof(H265eCtx),
    0,
    h265e_init,
    h265e_deinit,
    h265e_proc_cfg,
    h265e_gen_hdr,
    h265e_start,
    h265e_proc_dpb,
    h265e_proc_hal,
    h265e_add_sei,
    h265e_proc_enc_skip,
    h265e_init_cfg,
    h265e_proc_show,
};

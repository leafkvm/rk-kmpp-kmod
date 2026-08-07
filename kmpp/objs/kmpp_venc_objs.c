/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_venc_objs"

#include "kmpp_log.h"
#include "kmpp_macro.h"
#include "kmpp_module.h"

#include "kmpp_mod_id.h"
#include "kmpp_obj.h"
#include "kmpp_venc_objs_impl.h"

static rk_s32 kmpp_venc_init_cfg_impl_init(void *entry, KmppObj obj, KmppFsDev *file, const rk_u8 *caller)
{
    if (entry) {
        KmppVencInitCfgImpl *impl = (KmppVencInitCfgImpl*)entry;

        impl->type = MPP_CTX_BUTT;
        impl->coding = MPP_VIDEO_CodingUnused;
        impl->chan_id = -1;
        impl->chan_fd = -1;
        impl->online = 0;
        impl->buf_size = 0;
        impl->max_strm_cnt = 0;
        impl->shared_buf_en = 0;
        impl->shared_buf = NULL;
        impl->smart_en = 0;
        impl->max_width = 0;
        impl->max_height = 0;
        impl->max_lt_cnt = 0;
        impl->qpmap_en = 0;
        impl->chan_dup = 0;
        impl->tmvp_enable = 0;
        impl->only_smartp = 0;
        /* ntfy_mode set 1 default, notify info to rockit */
        impl->ntfy_mode = 1;
        impl->input_timeout = MPP_POLL_NON_BLOCK;
        impl->output_timeout = MPP_POLL_BLOCK;
    }

    return rk_ok;
}

static rk_s32 kmpp_venc_init_cfg_impl_dump(void *entry)
{
    KmppVencInitCfgImpl *impl = (KmppVencInitCfgImpl*)entry;

    if (!impl) {
        kmpp_loge_f("invalid param frame NULL\n");
        return rk_nok;
    }

    kmpp_logi("type             %d\n", impl->type);
    kmpp_logi("coding           %d\n", impl->coding);
    kmpp_logi("chan_id          %d\n", impl->chan_id);
    kmpp_logi("online           %d\n", impl->online);
    kmpp_logi("buf_size         %d\n", impl->buf_size);
    kmpp_logi("max_strm_cnt     %d\n", impl->max_strm_cnt);
    kmpp_logi("shared_buf_en    %d\n", impl->shared_buf_en);
    kmpp_logi("smart_en         %d\n", impl->smart_en);
    kmpp_logi("max_width        %d\n", impl->max_width);
    kmpp_logi("max_height       %d\n", impl->max_height);
    kmpp_logi("max_lt_cnt       %d\n", impl->max_lt_cnt);
    kmpp_logi("qpmap_en         %d\n", impl->qpmap_en);
    kmpp_logi("chan_dup         %d\n", impl->chan_dup);
    kmpp_logi("tmvp_enable      %d\n", impl->tmvp_enable);
    kmpp_logi("only_smartp      %d\n", impl->only_smartp);

    return rk_ok;
}

#define KMPP_OBJ_NAME               kmpp_venc_init_cfg
#define KMPP_OBJ_INTF_TYPE          KmppVencInitCfg
#define KMPP_OBJ_IMPL_TYPE          KmppVencInitCfgImpl
#define KMPP_OBJ_ENTRY_TABLE        KMPP_VENC_INIT_CFG_ENTRY_TABLE
#define KMPP_OBJ_FUNC_INIT          kmpp_venc_init_cfg_impl_init
#define KMPP_OBJ_FUNC_DUMP          kmpp_venc_init_cfg_impl_dump
#include "kmpp_obj_helper.h"

/* ------------------------ kmpp notify infos ------------------------- */
static rk_s32 kmpp_venc_notify_impl_init(void *entry, KmppObj obj, KmppFsDev *file, const rk_u8 *caller)
{
    if (entry) {
        KmppVencNtfyImpl *impl = (KmppVencNtfyImpl*)entry;

        impl->chan_id = -1;

        if (!impl->venc_syms)
            kmpp_syms_get(&impl->venc_syms, "venc");

        if (impl->venc_syms && !impl->ntfy_sym)
            kmpp_sym_get(&impl->ntfy_sym, impl->venc_syms, "venc_ntfy");
    }

    return rk_ok;
}

static rk_s32 kmpp_venc_notify_impl_deinit(void *entry, KmppObj obj, const rk_u8 *caller)
{
    (void)obj;

    if (entry) {
        KmppVencNtfyImpl *impl = (KmppVencNtfyImpl*)entry;

        impl->chan_id = -1;

        if (impl->ntfy_sym) {
            kmpp_sym_put(impl->ntfy_sym);
            impl->ntfy_sym = NULL;
        }

        if (impl->venc_syms) {
            kmpp_syms_put(impl->venc_syms);
            impl->venc_syms = NULL;
        }
    }

    return rk_ok;
}

static rk_s32 kmpp_venc_notify_impl_dump(void *entry)
{
    KmppVencNtfyImpl *impl = (KmppVencNtfyImpl*)entry;

    if (!impl) {
        kmpp_loge_f("invalid param frame NULL\n");
        return rk_nok;
    }

    kmpp_logi("chan_id          %d\n", impl->chan_id);
    kmpp_logi("cmd              %d\n", impl->cmd);
    kmpp_logi("type             %d\n", impl->drop_type);
    kmpp_logi("pipe_id          %d\n", impl->pipe_id);
    kmpp_logi("frame_id         %d\n", impl->frame_id);
    kmpp_logi("frame            %p\n", impl->frame);
    kmpp_logi("is_intra         %d\n", impl->is_intra);
    kmpp_logi("luma_pix_sum_od  %d\n", impl->luma_pix_sum_od);
    kmpp_logi("md_index         %d\n", impl->md_index);
    kmpp_logi("od_index         %d\n", impl->od_index);

    return rk_ok;
}

rk_s32 kmpp_venc_notify(KmppVencNtfy ntfy)
{
    rk_s32 ret = rk_ok;
    KmppVencNtfyImpl *impl = kmpp_obj_to_entry(ntfy);

    if (!impl->ntfy_sym) {
        kmpp_err_f("the kmpp venc notify symbol is NULL\n");
        return rk_nok;
    }

    if (kmpp_sym_run(impl->ntfy_sym, ntfy, &ret))
        return rk_nok;

    return ret;
}

#define KMPP_OBJ_NAME               kmpp_venc_ntfy
#define KMPP_OBJ_INTF_TYPE          KmppVencNtfy
#define KMPP_OBJ_IMPL_TYPE          KmppVencNtfyImpl
#define KMPP_OBJ_ENTRY_TABLE        KMPP_NOTIFY_CFG_ENTRY_TABLE
#define KMPP_OBJ_FUNC_INIT          kmpp_venc_notify_impl_init
#define KMPP_OBJ_FUNC_DEINIT        kmpp_venc_notify_impl_deinit
#define KMPP_OBJ_FUNC_DUMP          kmpp_venc_notify_impl_dump
#define KMPP_OBJ_SHARE_DISABLE
#include "kmpp_obj_helper.h"

/* ------------------------ kmpp venc static config ------------------------- */
#include "mpp_enc_cfg.h"

#define KMPP_VENC_ST_CFG_TABLE(prefix, ENTRY, STRCT, EHOOK, SHOOK, ALIAS) \
    CFG_DEF_START() \
    /*    prefix ElemType base type trie name               update flag type                    element address */ \
    STRUCT_START(codec); \
    ENTRY(prefix, s32,  rk_s32,     type,                   FLAG_BASE(0),                       base, coding); \
    STRUCT_END(codec); \
    STRUCT_START(base) \
    ENTRY(prefix, s32,  rk_s32,     low_delay,              FLAG_INCR,                          base, low_delay) \
    ENTRY(prefix, s32,  rk_s32,     smt1_en,                FLAG_INCR,                          base, smt1_en) \
    ENTRY(prefix, s32,  rk_s32,     smt3_en,                FLAG_INCR,                          base, smt3_en) \
    STRUCT_END(base) \
    STRUCT_START(rc) \
    ENTRY(prefix, s32,  rk_s32,     mode,                   FLAG_BASE(0),                       rc, rc_mode) \
    ENTRY(prefix, s32,  rk_s32,     quality,                FLAG_INCR,                          rc, quality); \
    ENTRY(prefix, s32,  rk_s32,     bps_target,             FLAG_INCR,                          rc, bps_target) \
    ENTRY(prefix, s32,  rk_s32,     bps_max,                FLAG_PREV,                          rc, bps_max) \
    ENTRY(prefix, s32,  rk_s32,     bps_min,                FLAG_PREV,                          rc, bps_min) \
    ENTRY(prefix, s32,  rk_s32,     fps_in_flex,            FLAG_INCR,                          rc, fps_in_flex) \
    ENTRY(prefix, s32,  rk_s32,     fps_in_num,             FLAG_PREV,                          rc, fps_in_num) \
    ENTRY(prefix, s32,  rk_s32,     fps_in_denorm,          FLAG_PREV,                          rc, fps_in_denom) \
    ALIAS(prefix, s32,  rk_s32,     fps_in_denom,           FLAG_PREV,                          rc, fps_in_denom) \
    ENTRY(prefix, s32,  rk_s32,     fps_out_flex,           FLAG_INCR,                          rc, fps_out_flex) \
    ENTRY(prefix, s32,  rk_s32,     fps_out_num,            FLAG_PREV,                          rc, fps_out_num) \
    ENTRY(prefix, s32,  rk_s32,     fps_out_denorm,         FLAG_PREV,                          rc, fps_out_denom) \
    ALIAS(prefix, s32,  rk_s32,     fps_out_denom,          FLAG_PREV,                          rc, fps_out_denom) \
    ENTRY(prefix, s32,  rk_s32,     fps_chg_no_idr,         FLAG_PREV,                          rc, fps_chg_no_idr) \
    ENTRY(prefix, s32,  rk_s32,     gop,                    FLAG_INCR,                          rc, gop) \
    ENTRY(prefix, kptr, void *,     ref_cfg,                FLAG_INCR,                          rc, ref_cfg) \
    ENTRY(prefix, u32,  rk_u32,     max_reenc_times,        FLAG_INCR,                          rc, max_reenc_times) \
    ENTRY(prefix, u32,  rk_u32,     priority,               FLAG_INCR,                          rc, rc_priority) \
    ENTRY(prefix, u32,  rk_u32,     drop_mode,              FLAG_INCR,                          rc, drop_mode) \
    ENTRY(prefix, u32,  rk_u32,     drop_thd,               FLAG_PREV,                          rc, drop_threshold) \
    ENTRY(prefix, u32,  rk_u32,     drop_gap,               FLAG_PREV,                          rc, drop_gap) \
    ENTRY(prefix, s32,  rk_s32,     max_i_prop,             FLAG_INCR,                          rc, max_i_prop) \
    ENTRY(prefix, s32,  rk_s32,     min_i_prop,             FLAG_INCR,                          rc, min_i_prop) \
    ENTRY(prefix, s32,  rk_s32,     init_ip_ratio,          FLAG_INCR,                          rc, init_ip_ratio) \
    ENTRY(prefix, u32,  rk_u32,     super_mode,             FLAG_INCR,                          rc, super_mode) \
    ENTRY(prefix, u32,  rk_u32,     super_i_thd,            FLAG_PREV,                          rc, super_i_thd) \
    ENTRY(prefix, u32,  rk_u32,     super_p_thd,            FLAG_PREV,                          rc, super_p_thd) \
    ENTRY(prefix, u32,  rk_u32,     debreath_en,            FLAG_INCR,                          rc, debreath_en) \
    ENTRY(prefix, u32,  rk_u32,     debreath_strength,      FLAG_PREV,                          rc, debre_strength) \
    ENTRY(prefix, s32,  rk_s32,     qp_init,                FLAG_REC_INC(0),                    rc, qp_init) \
    ENTRY(prefix, s32,  rk_s32,     qp_min,                 FLAG_REC_INC(1),                    rc, qp_min) \
    ENTRY(prefix, s32,  rk_s32,     qp_max,                 FLAG_REPLAY(1),                     rc, qp_max) \
    ENTRY(prefix, s32,  rk_s32,     qp_min_i,               FLAG_REC_INC(2),                    rc, qp_min_i) \
    ENTRY(prefix, s32,  rk_s32,     qp_max_i,               FLAG_REPLAY(2),                     rc, qp_max_i) \
    ENTRY(prefix, s32,  rk_s32,     qp_step,                FLAG_REC_INC(3),                    rc, qp_max_step) \
    ENTRY(prefix, s32,  rk_s32,     qp_ip,                  FLAG_REC_INC(4),                    rc, qp_delta_ip) \
    ENTRY(prefix, s32,  rk_s32,     qp_vi,                  FLAG_INCR,                          rc, qp_delta_vi) \
    ENTRY(prefix, s32,  rk_s32,     hier_qp_en,             FLAG_INCR,                          rc, hier_qp_en) \
    STRCT(prefix, st,   void *,     hier_qp_delta,          FLAG_PREV,                          rc, hier_qp_delta); \
    STRCT(prefix, st,   void *,     hier_frame_num,         FLAG_PREV,                          rc, hier_frame_num); \
    ENTRY(prefix, s32,  rk_s32,     stats_time,             FLAG_INCR,                          rc, stats_time) \
    ENTRY(prefix, u32,  rk_u32,     refresh_en,             FLAG_INCR,                          rc, refresh_en) \
    ENTRY(prefix, u32,  rk_u32,     refresh_mode,           FLAG_PREV,                          rc, refresh_mode) \
    ENTRY(prefix, u32,  rk_u32,     refresh_num,            FLAG_PREV,                          rc, refresh_num) \
    /* refresh_length: computed by proc_h26xe_cfg as ceil(mb_rows / refresh_num), clamped to rc.gop */ \
    /* exposed for MPP_ENC_GET_CFG readback so the caller can check its rc.gop cleared the sweep; */ \
    /* writing it is a no-op, the codec cfg pass recomputes it before enc_refs consumes it */ \
    ENTRY(prefix, s32,  rk_s32,     refresh_length,         FLAG_PREV,                          rc, refresh_length) \
    ENTRY(prefix, s32,  rk_s32,     fqp_min_i,              FLAG_INCR,                          rc, fqp_min_i) \
    ENTRY(prefix, s32,  rk_s32,     fqp_min_p,              FLAG_PREV,                          rc, fqp_min_p) \
    ENTRY(prefix, s32,  rk_s32,     fqp_max_i,              FLAG_PREV,                          rc, fqp_max_i) \
    ENTRY(prefix, s32,  rk_s32,     fqp_max_p,              FLAG_PREV,                          rc, fqp_max_p) \
    ENTRY(prefix, s32,  rk_s32,     mt_st_swth_frm_qp,      FLAG_PREV,                          rc, mt_st_swth_frm_qp) \
    ENTRY(prefix, s32,  rk_s32,     inst_br_lvl,            FLAG_INCR,                          rc, inst_br_lvl) \
    STRUCT_END(rc) \
    STRUCT_START(prep); \
    ENTRY(prefix, s32,  rk_s32,     width,                  FLAG_BASE(0),                       prep, width_set); \
    ENTRY(prefix, s32,  rk_s32,     height,                 FLAG_PREV,                          prep, height_set); \
    ENTRY(prefix, s32,  rk_s32,     max_width,              FLAG_PREV,                          prep, max_width); \
    ENTRY(prefix, s32,  rk_s32,     max_height,             FLAG_PREV,                          prep, max_height); \
    ENTRY(prefix, s32,  rk_s32,     hor_stride,             FLAG_PREV,                          prep, hor_stride); \
    ENTRY(prefix, s32,  rk_s32,     ver_stride,             FLAG_PREV,                          prep, ver_stride); \
    ENTRY(prefix, s32,  rk_s32,     format,                 FLAG_INCR,                          prep, format); \
    ENTRY(prefix, s32,  rk_s32,     format_out,             FLAG_PREV,                          prep, format_out); \
    ENTRY(prefix, s32,  rk_s32,     chroma_ds_mode,         FLAG_PREV,                          prep, chroma_ds_mode); \
    ENTRY(prefix, s32,  rk_s32,     fix_chroma_en,          FLAG_PREV,                          prep, fix_chroma_en); \
    ENTRY(prefix, s32,  rk_s32,     fix_chroma_u,           FLAG_PREV,                          prep, fix_chroma_u); \
    ENTRY(prefix, s32,  rk_s32,     fix_chroma_v,           FLAG_PREV,                          prep, fix_chroma_v); \
    ENTRY(prefix, s32,  rk_s32,     colorspace,             FLAG_INCR,                          prep, color); \
    ENTRY(prefix, s32,  rk_s32,     colorprim,              FLAG_INCR,                          prep, colorprim); \
    ENTRY(prefix, s32,  rk_s32,     colortrc,               FLAG_INCR,                          prep, colortrc); \
    ENTRY(prefix, s32,  rk_s32,     colorrange,             FLAG_INCR,                          prep, range); \
    ALIAS(prefix, s32,  rk_s32,     range,                  FLAG_PREV,                          prep, range); \
    ENTRY(prefix, s32,  rk_s32,     range_out,              FLAG_PREV,                          prep, range_out); \
    ENTRY(prefix, s32,  rk_s32,     rotation,               FLAG_INCR,                          prep, rotation_ext); \
    ENTRY(prefix, s32,  rk_s32,     mirroring,              FLAG_INCR,                          prep, mirroring_ext); \
    ENTRY(prefix, s32,  rk_s32,     flip,                   FLAG_INCR,                          prep, flip); \
    STRUCT_END(prep); \
    STRUCT_START(h264); \
    /* h264 config */ \
    ENTRY(prefix, s32,  rk_s32,     stream_type,            FLAG_BASE(0),                       h264, stream_type); \
    ENTRY(prefix, s32,  rk_s32,     profile,                FLAG_INCR,                          h264, profile); \
    ENTRY(prefix, s32,  rk_s32,     level,                  FLAG_PREV,                          h264, level); \
    ENTRY(prefix, u32,  rk_u32,     poc_type,               FLAG_INCR,                          h264, poc_type); \
    ENTRY(prefix, u32,  rk_u32,     log2_max_poc_lsb,       FLAG_INCR,                          h264, log2_max_poc_lsb); \
    ENTRY(prefix, u32,  rk_u32,     log2_max_frm_num,       FLAG_INCR,                          h264, log2_max_frame_num); \
    ENTRY(prefix, u32,  rk_u32,     gaps_not_allowed,       FLAG_INCR,                          h264, gaps_not_allowed); \
    ENTRY(prefix, s32,  rk_s32,     cabac_en,               FLAG_INCR,                          h264, entropy_coding_mode_ex); \
    ENTRY(prefix, s32,  rk_s32,     cabac_idc,              FLAG_PREV,                          h264, cabac_init_idc_ex); \
    ENTRY(prefix, s32,  rk_s32,     trans8x8,               FLAG_INCR,                          h264, transform8x8_mode_ex); \
    ENTRY(prefix, s32,  rk_s32,     const_intra,            FLAG_INCR,                          h264, constrained_intra_pred_mode); \
    ENTRY(prefix, s32,  rk_s32,     scaling_list,           FLAG_INCR,                          h264, scaling_list_mode); \
    ENTRY(prefix, s32,  rk_s32,     cb_qp_offset,           FLAG_INCR,                          h264, chroma_cb_qp_offset); \
    ENTRY(prefix, s32,  rk_s32,     cr_qp_offset,           FLAG_PREV,                          h264, chroma_cr_qp_offset); \
    ENTRY(prefix, s32,  rk_s32,     dblk_disable,           FLAG_INCR,                          h264, deblock_disable); \
    ENTRY(prefix, s32,  rk_s32,     dblk_alpha,             FLAG_PREV,                          h264, deblock_offset_alpha); \
    ENTRY(prefix, s32,  rk_s32,     dblk_beta,              FLAG_PREV,                          h264, deblock_offset_beta); \
    ALIAS(prefix, s32,  rk_s32,     qp_init,                FLAG_REPLAY(0),                     rc, qp_init); \
    ALIAS(prefix, s32,  rk_s32,     qp_min,                 FLAG_REPLAY(1),                     rc, qp_min); \
    ALIAS(prefix, s32,  rk_s32,     qp_max,                 FLAG_REPLAY(1),                     rc, qp_max); \
    ALIAS(prefix, s32,  rk_s32,     qp_min_i,               FLAG_REPLAY(2),                     rc, qp_min_i); \
    ALIAS(prefix, s32,  rk_s32,     qp_max_i,               FLAG_REPLAY(2),                     rc, qp_max_i); \
    ALIAS(prefix, s32,  rk_s32,     qp_step,                FLAG_REPLAY(3),                     rc, qp_max_step); \
    ALIAS(prefix, s32,  rk_s32,     qp_delta_ip,            FLAG_REPLAY(4),                     rc, qp_delta_ip); \
    ENTRY(prefix, s32,  rk_s32,     max_tid,                FLAG_INCR,                          h264, max_tid); \
    ENTRY(prefix, s32,  rk_s32,     max_ltr,                FLAG_INCR,                          h264, max_ltr_frames); \
    ENTRY(prefix, s32,  rk_s32,     prefix_mode,            FLAG_INCR,                          h264, prefix_mode); \
    ENTRY(prefix, s32,  rk_s32,     base_layer_pid,         FLAG_INCR,                          h264, base_layer_pid); \
    ENTRY(prefix, u32,  rk_u32,     constraint_set,         FLAG_INCR,                          h264, constraint_set); \
    ENTRY(prefix, u32,  rk_u32,     vui_en,                 FLAG_INCR,                          h264, vui, vui_en); \
    STRUCT_END(h264); \
    /* h265 config*/ \
    STRUCT_START(h265); \
    ENTRY(prefix, s32,  rk_s32,     profile,                FLAG_BASE(0),                       h265, profile); \
    ENTRY(prefix, s32,  rk_s32,     tier,                   FLAG_PREV,                          h265, tier); \
    ENTRY(prefix, s32,  rk_s32,     level,                  FLAG_PREV,                          h265, level); \
    ENTRY(prefix, u32,  rk_u32,     scaling_list,           FLAG_INCR,                          h265, trans_cfg, scaling_list_mode); \
    ENTRY(prefix, s32,  rk_s32,     cb_qp_offset,           FLAG_PREV,                          h265, trans_cfg, cb_qp_offset); \
    ENTRY(prefix, s32,  rk_s32,     cr_qp_offset,           FLAG_PREV,                          h265, trans_cfg, cr_qp_offset); \
    ENTRY(prefix, s32,  rk_s32,     diff_cu_qp_delta_depth, FLAG_PREV,                          h265, trans_cfg, diff_cu_qp_delta_depth); \
    ENTRY(prefix, u32,  rk_u32,     dblk_disable,           FLAG_INCR,                          h265, dblk_cfg, slice_deblocking_filter_disabled_flag); \
    ENTRY(prefix, s32,  rk_s32,     dblk_alpha,             FLAG_PREV,                          h265, dblk_cfg, slice_beta_offset_div2); \
    ENTRY(prefix, s32,  rk_s32,     dblk_beta,              FLAG_PREV,                          h265, dblk_cfg, slice_tc_offset_div2); \
    ALIAS(prefix, s32,  rk_s32,     qp_init,                FLAG_REPLAY(0),                     rc, qp_init); \
    ALIAS(prefix, s32,  rk_s32,     qp_min,                 FLAG_REPLAY(1),                     rc, qp_min); \
    ALIAS(prefix, s32,  rk_s32,     qp_max,                 FLAG_REPLAY(1),                     rc, qp_max); \
    ALIAS(prefix, s32,  rk_s32,     qp_min_i,               FLAG_REPLAY(2),                     rc, qp_min_i); \
    ALIAS(prefix, s32,  rk_s32,     qp_max_i,               FLAG_REPLAY(2),                     rc, qp_max_i); \
    ALIAS(prefix, s32,  rk_s32,     qp_step,                FLAG_REPLAY(3),                     rc, qp_max_step); \
    ALIAS(prefix, s32,  rk_s32,     qp_delta_ip,            FLAG_REPLAY(4),                     rc, qp_delta_ip); \
    ENTRY(prefix, s32,  rk_s32,     sao_luma_disable,       FLAG_INCR,                          h265, sao_cfg, slice_sao_luma_disable); \
    ENTRY(prefix, s32,  rk_s32,     sao_chroma_disable,     FLAG_PREV,                          h265, sao_cfg, slice_sao_chroma_disable); \
    ENTRY(prefix, s32,  rk_s32,     sao_bit_ratio,          FLAG_PREV,                          h265, sao_cfg, sao_bit_ratio); \
    ENTRY(prefix, u32,  rk_u32,     lpf_acs_sli_en,         FLAG_INCR,                          h265, lpf_acs_sli_en); \
    ENTRY(prefix, u32,  rk_u32,     lpf_acs_tile_disable,   FLAG_INCR,                          h265, lpf_acs_tile_disable); \
    ENTRY(prefix, s32,  rk_s32,     auto_tile,              FLAG_INCR,                          h265, auto_tile); \
    ENTRY(prefix, s32,  rk_s32,     max_tid,                FLAG_INCR,                          h265, max_tid); \
    ENTRY(prefix, s32,  rk_s32,     max_ltr,                FLAG_INCR,                          h265, max_ltr_frames); \
    ENTRY(prefix, s32,  rk_s32,     base_layer_pid,         FLAG_INCR,                          h265, base_layer_pid); \
    ENTRY(prefix, s32,  rk_s32,     const_intra,            FLAG_INCR,                          h265, const_intra_pred); \
    ENTRY(prefix, s32,  rk_s32,     lcu_size,               FLAG_INCR,                          h265, max_cu_size); \
    ENTRY(prefix, s32,  rk_s32,     vui_en,                 FLAG_INCR,                          h265, vui, vui_en); \
    STRUCT_END(h265); \
    /* vp8 config */ \
    STRUCT_START(vp8); \
    ENTRY(prefix, s32,  rk_s32,     disable_ivf,            FLAG_BASE(0),                       vp8, disable_ivf); \
    ALIAS(prefix, s32,  rk_s32,     qp_init,                FLAG_REPLAY(0),                     rc, qp_init); \
    ALIAS(prefix, s32,  rk_s32,     qp_min,                 FLAG_REPLAY(1),                     rc, qp_min); \
    ALIAS(prefix, s32,  rk_s32,     qp_max,                 FLAG_REPLAY(1),                     rc, qp_max); \
    ALIAS(prefix, s32,  rk_s32,     qp_min_i,               FLAG_REPLAY(2),                     rc, qp_min_i); \
    ALIAS(prefix, s32,  rk_s32,     qp_max_i,               FLAG_REPLAY(2),                     rc, qp_max_i); \
    ALIAS(prefix, s32,  rk_s32,     qp_step,                FLAG_REPLAY(3),                     rc, qp_max_step); \
    ALIAS(prefix, s32,  rk_s32,     qp_delta_ip,            FLAG_REPLAY(4),                     rc, qp_delta_ip); \
    STRUCT_END(vp8); \
    /* jpeg config */ \
    STRUCT_START(jpeg); \
    ENTRY(prefix, s32,  rk_s32,     quant,                  FLAG_BASE(0),                       jpeg, quant); \
    ENTRY(prefix, s32,  rk_s32,     q_factor,               FLAG_INCR,                          jpeg, q_factor); \
    ENTRY(prefix, s32,  rk_s32,     qf_max,                 FLAG_PREV,                          jpeg, qf_max); \
    ENTRY(prefix, s32,  rk_s32,     qf_min,                 FLAG_PREV,                          jpeg, qf_min); \
    ENTRY(prefix, st,   void *,     qtable_y,               FLAG_INCR,                          jpeg, qtable_y); \
    ENTRY(prefix, st,   void *,     qtable_u,               FLAG_PREV,                          jpeg, qtable_u); \
    ENTRY(prefix, st,   void *,     qtable_v,               FLAG_PREV,                          jpeg, qtable_v); \
    STRUCT_END(jpeg); \
    /* split config */ \
    STRUCT_START(split); \
    ENTRY(prefix, u32,  rk_u32,     mode,                   FLAG_BASE(0),                       split, split_mode) \
    ENTRY(prefix, u32,  rk_u32,     arg,                    FLAG_INCR,                          split, split_arg) \
    ENTRY(prefix, u32,  rk_u32,     out,                    FLAG_INCR,                          split, split_out) \
    STRUCT_END(split); \
    /* hardware detail config */ \
    STRUCT_START(hw); \
    ENTRY(prefix, s32,  rk_s32,     qp_row,                 FLAG_BASE(0),                       hw, qp_delta_row) \
    ENTRY(prefix, s32,  rk_s32,     qp_row_i,               FLAG_INCR,                          hw, qp_delta_row_i) \
    STRCT(prefix, st,   void *,     aq_thrd_i,              FLAG_INCR,                          hw, aq_thrd_i) \
    STRCT(prefix, st,   void *,     aq_thrd_p,              FLAG_INCR,                          hw, aq_thrd_p) \
    STRCT(prefix, st,   void *,     aq_step_i,              FLAG_INCR,                          hw, aq_step_i) \
    STRCT(prefix, st,   void *,     aq_step_p,              FLAG_INCR,                          hw, aq_step_p) \
    ENTRY(prefix, s32,  rk_s32,     mb_rc_disable,          FLAG_INCR,                          hw, mb_rc_disable) \
    STRCT(prefix, st,   void *,     aq_rnge_arr,            FLAG_INCR,                          hw, aq_rnge_arr) \
    STRCT(prefix, st,   void *,     mode_bias,              FLAG_INCR,                          hw, mode_bias) \
    ENTRY(prefix, s32,  rk_s32,     skip_bias_en,           FLAG_INCR,                          hw, skip_bias_en) \
    ENTRY(prefix, s32,  rk_s32,     skip_sad,               FLAG_PREV,                          hw, skip_sad) \
    ENTRY(prefix, s32,  rk_s32,     skip_bias,              FLAG_PREV,                          hw, skip_bias) \
    ENTRY(prefix, s32,  rk_s32,     qbias_i,                FLAG_INCR,                          hw, qbias_i) \
    ENTRY(prefix, s32,  rk_s32,     qbias_p,                FLAG_INCR,                          hw, qbias_p) \
    ENTRY(prefix, s32,  rk_s32,     qbias_en,               FLAG_INCR,                          hw, qbias_en) \
    STRCT(prefix, st,   void *,     qbias_arr,              FLAG_INCR,                          hw, qbias_arr) \
    ENTRY(prefix, s32,  rk_s32,     flt_str_i,              FLAG_INCR,                          hw, flt_str_i) \
    ENTRY(prefix, s32,  rk_s32,     flt_str_p,              FLAG_INCR,                          hw, flt_str_p) \
    STRUCT_END(hw); \
    /* quality fine tuning config */ \
    STRUCT_START(tune); \
    ENTRY(prefix, s32,  rk_s32,     scene_mode,             FLAG_BASE(0),                       tune, scene_mode); \
    ENTRY(prefix, s32,  rk_s32,     se_mode,                FLAG_INCR,                          tune, se_mode); \
    ENTRY(prefix, s32,  rk_s32,     deblur_en,              FLAG_INCR,                          tune, deblur_en); \
    ENTRY(prefix, s32,  rk_s32,     deblur_str,             FLAG_INCR,                          tune, deblur_str); \
    ENTRY(prefix, s32,  rk_s32,     anti_flicker_str,       FLAG_INCR,                          tune, anti_flicker_str); \
    ENTRY(prefix, s32,  rk_s32,     lambda_idx_i,           FLAG_INCR,                          tune, lambda_idx_i); \
    ENTRY(prefix, s32,  rk_s32,     lambda_idx_p,           FLAG_INCR,                          tune, lambda_idx_p); \
    ENTRY(prefix, s32,  rk_s32,     atr_str_i,              FLAG_INCR,                          tune, atr_str_i); \
    ENTRY(prefix, s32,  rk_s32,     atr_str_p,              FLAG_INCR,                          tune, atr_str_p); \
    ENTRY(prefix, s32,  rk_s32,     atl_str,                FLAG_INCR,                          tune, atl_str); \
    ENTRY(prefix, s32,  rk_s32,     sao_str_i,              FLAG_INCR,                          tune, sao_str_i); \
    ENTRY(prefix, s32,  rk_s32,     sao_str_p,              FLAG_INCR,                          tune, sao_str_p); \
    ENTRY(prefix, s32,  rk_s32,     rc_container,           FLAG_INCR,                          tune, rc_container); \
    ENTRY(prefix, s32,  rk_s32,     vmaf_opt,               FLAG_INCR,                          tune, vmaf_opt); \
    ENTRY(prefix, s32,  rk_s32,     motion_static_switch_enable, FLAG_INCR,                     tune, motion_static_switch_enable); \
    ENTRY(prefix, s32,  rk_s32,     atf_str,                FLAG_INCR,                          tune, atf_str); \
    ENTRY(prefix, s32,  rk_s32,     lgt_chg_lvl,            FLAG_INCR,                          tune, lgt_chg_lvl); \
    ENTRY(prefix, s32,  rk_s32,     static_frm_num,         FLAG_INCR,                          tune, static_frm_num); \
    ENTRY(prefix, s32,  rk_s32,     madp16_th,              FLAG_INCR,                          tune, madp16_th); \
    ENTRY(prefix, s32,  rk_s32,     skip16_wgt,             FLAG_INCR,                          tune, skip16_wgt); \
    ENTRY(prefix, s32,  rk_s32,     skip32_wgt,             FLAG_INCR,                          tune, skip32_wgt); \
    ENTRY(prefix, s32,  rk_s32,     speed,                  FLAG_INCR,                          tune, speed); \
    ENTRY(prefix, s32,  rk_s32,     bg_delta_qp_i,          FLAG_INCR,                          tune, bg_delta_qp_i); \
    ENTRY(prefix, s32,  rk_s32,     bg_delta_qp_p,          FLAG_PREV,                          tune, bg_delta_qp_p); \
    ENTRY(prefix, s32,  rk_s32,     fg_delta_qp_i,          FLAG_PREV,                          tune, fg_delta_qp_i); \
    ENTRY(prefix, s32,  rk_s32,     fg_delta_qp_p,          FLAG_PREV,                          tune, fg_delta_qp_p); \
    ENTRY(prefix, s32,  rk_s32,     bmap_qpmin_i,           FLAG_PREV,                          tune, bmap_qpmin_i); \
    ENTRY(prefix, s32,  rk_s32,     bmap_qpmin_p,           FLAG_PREV,                          tune, bmap_qpmin_p); \
    ENTRY(prefix, s32,  rk_s32,     bmap_qpmax_i,           FLAG_PREV,                          tune, bmap_qpmax_i); \
    ENTRY(prefix, s32,  rk_s32,     bmap_qpmax_p,           FLAG_PREV,                          tune, bmap_qpmax_p); \
    ENTRY(prefix, s32,  rk_s32,     min_bg_fqp,             FLAG_PREV,                          tune, min_bg_fqp); \
    ENTRY(prefix, s32,  rk_s32,     max_bg_fqp,             FLAG_PREV,                          tune, max_bg_fqp); \
    ENTRY(prefix, s32,  rk_s32,     min_fg_fqp,             FLAG_PREV,                          tune, min_fg_fqp); \
    ENTRY(prefix, s32,  rk_s32,     max_fg_fqp,             FLAG_PREV,                          tune, max_fg_fqp); \
    ENTRY(prefix, s32,  rk_s32,     fg_area,                FLAG_PREV,                          tune, fg_area); \
    ENTRY(prefix, s32,  rk_s32,     qpmap_en,               FLAG_INCR,                          tune, qpmap_en); \
    STRUCT_END(tune); \
    STRUCT_START(ro); \
    ENTRY(prefix, s32,  rk_s32,     pos_rc,                 FLAG_NONE,                          ro, pos_rc); \
    ENTRY(prefix, s32,  rk_s32,     pos_prep,               FLAG_NONE,                          ro, pos_prep); \
    ENTRY(prefix, s32,  rk_s32,     pos_h264,               FLAG_NONE,                          ro, pos_h264); \
    ENTRY(prefix, s32,  rk_s32,     pos_h265,               FLAG_NONE,                          ro, pos_h265); \
    ENTRY(prefix, s32,  rk_s32,     pos_vp8,                FLAG_NONE,                          ro, pos_vp8); \
    ENTRY(prefix, s32,  rk_s32,     pos_jpeg,               FLAG_NONE,                          ro, pos_jpeg); \
    ENTRY(prefix, s32,  rk_s32,     pos_tune,               FLAG_NONE,                          ro, pos_tune); \
    STRUCT_END(ro); \
    CFG_DEF_END()

static rk_s32 kmpp_venc_st_cfg_impl_init(void *entry, KmppObj obj, KmppFsDev *file, const rk_u8 *caller);

static rk_s32 kmpp_venc_st_cfg_impl_deinit(void *entry, KmppObj obj, const rk_u8 *caller)
{
    (void)obj;

    if (entry) {
        MppEncCfgSet *cfg = (MppEncCfgSet *)entry;

        kmpp_memset(cfg, 0, sizeof(MppEncCfgSet));
    }

    return rk_ok;
}

static rk_s32 kmpp_venc_st_cfg_impl_dump(void *entry)
{
    MppEncCfgSet *impl = (MppEncCfgSet*)entry;

    if (!impl) {
        kmpp_loge_f("invalid param frame NULL\n");
        return rk_nok;
    }

    /* rc config */
    kmpp_logi("rc mode      %d\n", impl->rc.rc_mode);
    kmpp_logi("bps target   %d\n", impl->rc.bps_target);
    kmpp_logi("bps max      %d\n", impl->rc.bps_max);
    kmpp_logi("bps min      %d\n", impl->rc.bps_min);
    kmpp_logi("fps in       %d / %d : %s\n", impl->rc.fps_in_num,
              impl->rc.fps_in_denom, impl->rc.fps_in_flex ? "flex" : "fixed");
    kmpp_logi("fps out      %d / %d : %s\n", impl->rc.fps_out_num,
              impl->rc.fps_out_denom, impl->rc.fps_out_flex ? "flex" : "fixed");
    kmpp_logi("gop          %d\n", impl->rc.gop);
    kmpp_logi("refresh      %s mode %d num %d length %d\n",
              impl->rc.refresh_en ? "on" : "off", impl->rc.refresh_mode,
              impl->rc.refresh_num, impl->rc.refresh_length);
    kmpp_logi("qp init      %d\n", impl->rc.qp_init);
    kmpp_logi("qp i min/max %d : %d\n", impl->rc.qp_min_i, impl->rc.qp_max_i);
    kmpp_logi("qp p min/max %d : %d\n", impl->rc.qp_min, impl->rc.qp_max);

    kmpp_logi("width        %d\n", impl->prep.width);
    kmpp_logi("height       %d\n", impl->prep.height);
    kmpp_logi("hor_stride   %d\n", impl->prep.hor_stride);
    kmpp_logi("ver_stride   %d\n", impl->prep.ver_stride);
    kmpp_logi("format in    %d\n", impl->prep.format);
    kmpp_logi("format out   %d\n", impl->prep.format_out);
    kmpp_logi("range  in    %s\n", impl->prep.range == MPP_FRAME_RANGE_JPEG ? "full" : "limited");
    kmpp_logi("range  out   %s\n", impl->prep.range_out == MPP_FRAME_RANGE_JPEG ? "full" : "limited");
    kmpp_logi("mirroring    %d\n", impl->prep.mirroring);
    kmpp_logi("rotation     %d\n", impl->prep.rotation);
    kmpp_logi("flip         %d\n", impl->prep.flip);

    {
        rk_u8 *base = (rk_u8 *)(impl + 1);
        rk_u32 *flag;
        rk_s32 size = sizeof(*impl);

        flag = (rk_u32 *)(base + (impl->ro.pos_rc - size));
        kmpp_logi("rc   change  %px - %#x\n", flag, *flag);
        flag = (rk_u32 *)(base + (impl->ro.pos_prep - size));
        kmpp_logi("prep change  %px - %#x\n", flag, *flag);
    }

    return rk_ok;
}

#define KMPP_OBJ_NAME               kmpp_venc_st_cfg
#define KMPP_OBJ_INTF_TYPE          KmppVencStCfg
#define KMPP_OBJ_IMPL_TYPE          MppEncCfgSet
#define KMPP_OBJ_FUNC_INIT          kmpp_venc_st_cfg_impl_init
#define KMPP_OBJ_FUNC_DEINIT        kmpp_venc_st_cfg_impl_deinit
#define KMPP_OBJ_FUNC_DUMP          kmpp_venc_st_cfg_impl_dump
#define KMPP_OBJ_ENTRY_TABLE        KMPP_VENC_ST_CFG_TABLE
#define KMPP_OBJ_ACCESS_DISABLE
#define KMPP_OBJ_HIERARCHY_ENABLE
#include "kmpp_obj_helper.h"

#define TO_CHANGE_POS(name, ...) \
    static rk_s32 to_change_pos_##name(void) \
    { \
        static rk_s32 CONCAT_US(name, flag, pos) = -1; \
        if (CONCAT_US(name, flag, pos) < 0) { \
            KmppEntry *tbl = NULL; \
            kmpp_objdef_get_entry(kmpp_venc_st_cfg_def, CONCAT_STR(name, __VA_ARGS__), &tbl); \
            CONCAT_US(name, flag, pos) = tbl ? (tbl->tbl.flag_offset / 8) : 0; \
        } \
        return CONCAT_US(name, flag, pos); \
    }

/* vcodec type -> base.change */
TO_CHANGE_POS(rc, mode)
TO_CHANGE_POS(prep, width)
TO_CHANGE_POS(h264, stream_type)
TO_CHANGE_POS(h265, profile)
TO_CHANGE_POS(vp8, disable_ivf)
TO_CHANGE_POS(jpeg, quant)
TO_CHANGE_POS(hw, qp_row)
TO_CHANGE_POS(tune, scene_mode)

KmppEntry *query_mpp_enc_cfg_entry(const rk_u8 *name)
{
    KmppEntry *tbl = NULL;

    kmpp_objdef_get_entry(kmpp_venc_st_cfg_def, name, &tbl);

    return tbl;
}

static rk_s32 kmpp_venc_st_cfg_impl_init(void *entry, KmppObj obj, KmppFsDev *file, const rk_u8 *caller)
{
    if (entry) {
        rk_u32 i;
        MppEncCfgSet *cfg = (MppEncCfgSet *)entry;

        cfg->rc.max_reenc_times = 1;

        cfg->prep.color = MPP_FRAME_SPC_UNSPECIFIED;
        cfg->prep.colorprim = MPP_FRAME_PRI_UNSPECIFIED;
        cfg->prep.colortrc = MPP_FRAME_TRC_UNSPECIFIED;
        cfg->prep.format_out = MPP_CHROMA_UNSPECIFIED;
        cfg->prep.chroma_ds_mode = MPP_FRAME_CHROMA_DOWN_SAMPLE_MODE_NONE;
        cfg->prep.fix_chroma_en = 0;
        cfg->prep.range_out = MPP_FRAME_RANGE_UNSPECIFIED;

        for (i = 0; i < KMPP_ARRAY_ELEMS(cfg->hw.mode_bias); i++)
            cfg->hw.mode_bias[i] = 8;
        cfg->hw.skip_sad  = 8;
        cfg->hw.skip_bias = 8;

        /* tune */
        cfg->tune.deblur_str = 5;

        /* generate ro data */
        cfg->ro.pos_rc      = to_change_pos_rc();
        cfg->ro.pos_prep    = to_change_pos_prep();
        cfg->ro.pos_h264    = to_change_pos_h264();
        cfg->ro.pos_h265    = to_change_pos_h265();
        cfg->ro.pos_vp8     = to_change_pos_vp8();
        cfg->ro.pos_jpeg    = to_change_pos_jpeg();
        cfg->ro.pos_hw      = to_change_pos_hw();
        cfg->ro.pos_tune    = to_change_pos_tune();
    }

    return rk_ok;
}

/* --------------------- kmpp venc reference config ------------------------- */
static rk_s32 kmpp_ref_st_cfg_impl_dump(void *entry)
{
    KmppRefStCfgImpl *impl = (KmppRefStCfgImpl*)entry;

    if (!impl) {
        kmpp_loge_f("invalid param ref_st_cfg NULL\n");
        return rk_nok;
    }

    kmpp_logi("is_non_ref       %d\n", impl->is_non_ref);
    kmpp_logi("temporal_id      %d\n", impl->temporal_id);
    kmpp_logi("ref_mode         0x%x\n", impl->ref_mode);
    kmpp_logi("ref_arg          %d\n", impl->ref_arg);
    kmpp_logi("repeat           %d\n", impl->repeat);

    return rk_ok;
}

#define KMPP_OBJ_NAME               kmpp_ref_st_cfg
#define KMPP_OBJ_INTF_TYPE          KmppRefStCfg
#define KMPP_OBJ_IMPL_TYPE          KmppRefStCfgImpl
#define KMPP_OBJ_ENTRY_TABLE        KMPP_REF_ST_CFG_ENTRY_TABLE
#define KMPP_OBJ_FUNC_DUMP          kmpp_ref_st_cfg_impl_dump
#include "kmpp_obj_helper.h"

static rk_s32 kmpp_ref_lt_cfg_impl_dump(void *entry)
{
    KmppRefLtCfgImpl *impl = (KmppRefLtCfgImpl*)entry;

    if (!impl) {
        kmpp_loge_f("invalid param ref_lt_cfg NULL\n");
        return rk_nok;
    }

    kmpp_logi("lt_idx           %d\n", impl->lt_idx);
    kmpp_logi("temporal_id      %d\n", impl->temporal_id);
    kmpp_logi("ref_mode         0x%x\n", impl->ref_mode);
    kmpp_logi("ref_arg          %d\n", impl->ref_arg);
    kmpp_logi("lt_gap           %d\n", impl->lt_gap);
    kmpp_logi("lt_delay         %d\n", impl->lt_delay);

    return rk_ok;
}

#define KMPP_OBJ_NAME               kmpp_ref_lt_cfg
#define KMPP_OBJ_INTF_TYPE          KmppRefLtCfg
#define KMPP_OBJ_IMPL_TYPE          KmppRefLtCfgImpl
#define KMPP_OBJ_ENTRY_TABLE        KMPP_REF_LT_CFG_ENTRY_TABLE
#define KMPP_OBJ_FUNC_DUMP          kmpp_ref_lt_cfg_impl_dump
#include "kmpp_obj_helper.h"

static rk_s32 kmpp_ref_cfg_impl_dump(void *entry)
{
    KmppRefCfgImpl *impl = (KmppRefCfgImpl*)entry;

    if (!impl) {
        kmpp_loge_f("invalid param ref_cfg NULL\n");
        return rk_nok;
    }

    kmpp_logi("keep_cpb         %d\n", impl->keep_cpb);
    kmpp_logi("lt_cfg_cnt       %d\n", impl->lt_cfg_cnt);
    kmpp_logi("st_cfg_cnt       %d\n", impl->st_cfg_cnt);
    kmpp_logi("lt_cfg_offset    %u\n", impl->lt_cfg_offset);
    kmpp_logi("st_cfg_offset    %u\n", impl->st_cfg_offset);

    return rk_ok;
}

#define KMPP_OBJ_NAME               kmpp_ref_cfg
#define KMPP_OBJ_INTF_TYPE          KmppRefCfg
#define KMPP_OBJ_IMPL_TYPE          KmppRefCfgImpl
#define KMPP_OBJ_ENTRY_TABLE        KMPP_REF_CFG_ENTRY_TABLE
#define KMPP_OBJ_FUNC_DUMP          kmpp_ref_cfg_impl_dump
#include "kmpp_obj_helper.h"

static rk_s32 kmpp_venc_objs_init(void)
{
    kmpp_venc_init_cfg_objdef_get();
    kmpp_venc_st_cfg_objdef_get();
    kmpp_venc_ntfy_objdef_get();
    kmpp_ref_st_cfg_objdef_get();
    kmpp_ref_lt_cfg_objdef_get();
    kmpp_ref_cfg_objdef_get();

    return rk_ok;
}

static rk_s32 kmpp_venc_objs_deinit(void)
{
    kmpp_venc_ntfy_objdef_put();
    kmpp_venc_st_cfg_objdef_put();
    kmpp_venc_init_cfg_objdef_put();
    kmpp_ref_cfg_objdef_put();
    kmpp_ref_lt_cfg_objdef_put();
    kmpp_ref_st_cfg_objdef_put();

    return rk_ok;
}

KMPP_MODULE_ADD(kmpp, KMPP_MOD_VENC_OBJS, venc_objs, kmpp_venc_objs_init, kmpp_venc_objs_deinit)
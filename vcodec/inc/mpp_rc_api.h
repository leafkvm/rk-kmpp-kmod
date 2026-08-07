/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#ifndef __MPP_RC_API_H__
#define __MPP_RC_API_H__

#include "kmpp_env.h"

#include "rk_venc_rc.h"
#include "rk_venc_cfg.h"
#include "mpp_rc_defs.h"

/*
 * Mpp rate control has three parts:
 *
 * 1. MPI user config module
 *    MppEncRcCfg structure is provided to user for overall rate control config
 *    Mpp will receive MppEncRcCfg from user, check parameter and set it to
 *    encoder.
 *
 * 2. Encoder rate control module
 *    Encoder will implement the rate control strategy required by users
 *    including CBR, VBR, AVBR and so on.
 *    This module only implement the target bit calculation behavior and
 *    quality restriction. And the quality level will be controlled by hal.
 *
 * 3. Hal rate control module
 *    Hal will implement the rate control on hardware. Hal will calculate the
 *    QP parameter for hardware according to the frame level target bit
 *    specified by the encoder. And the report the real bitrate and quality to
 *    encoder.
 *
 * The header defines the communication interfaces and structures used between
 * MPI, encoder and hal.
 */

typedef enum RcMode_e {
    RC_VBR,
    RC_CBR,
    RC_FIXQP,
    RC_AVBR,
    RC_SMT,
    RC_CVBR,
    RC_QVBR,
    RC_LEARNING,
    RC_MODE_BUTT,
} RcMode;

typedef enum GopMode_e {
    NORMAL_P,
    SMART_P,
} GopMode;

typedef enum EncFrameType_e {
    INTER_P_FRAME       = 0,
    INTER_B_FRAME       = 1,
    INTRA_FRAME         = 2,
    INTER_VI_FRAME      = 3,
} EncFrameType;

/*
 * frame rate parameters have great effect on rate control
 *
 * fps_in_flex
 * 0 - fix input frame rate
 * 1 - variable input frame rate
 *
 * fps_in_num
 * input frame rate numerator, if 0 then default 30
 *
 * fps_in_denom
 * input frame rate denorminator, if 0 then default 1
 *
 * fps_out_flex
 * 0 - fix output frame rate
 * 1 - variable output frame rate
 *
 * fps_out_num
 * output frame rate numerator, if 0 then default 30
 *
 * fps_out_denom
 * output frame rate denorminator, if 0 then default 1
 */
typedef struct RcFpsCfg_t {
    RK_S32      fps_in_flex;
    RK_S32      fps_in_num;
    RK_S32      fps_in_denom;
    RK_S32      fps_out_flex;
    RK_S32      fps_out_num;
    RK_S32      fps_out_denom;
} RcFpsCfg;

typedef struct RcSuperframeCfg_t {
    MppEncRcSuperFrameMode  super_mode;
    RK_U32                  super_i_thd;
    RK_U32                  super_p_thd;
    MppEncRcPriority        rc_priority;
} RcSuperframeCfg;

typedef struct RcDebreathCfg_t {
    RK_U32      enable;
    RK_U32      strength;
} RcDebreathCfg;

typedef struct RcHierQPCfg_t {
    RK_S32      hier_qp_en;
    RK_S32      hier_qp_delta[4];
    RK_S32      hier_frame_num[4];
} RcHierQPCfg;

/*
 * Control parameter from external config
 *
 * It will be updated on rc/prep/gopref config changed.
 */
typedef struct RcCfg_s {
    MppEncCfg   cfg;

    /* encode image size */
    RK_S32      width;
    RK_S32      height;

    /* Use rc_mode to find different api */
    RcMode      mode;

    RcFpsCfg    fps;

    GopMode     gop_mode;
    /* I frame gop len */
    RK_S32      igop;
    /* visual gop len */
    RK_S32      vgop;

    /* bitrate parameter */
    RK_S32      bps_min;
    RK_S32      bps_target;
    RK_S32      bps_max;
    RK_S32      stats_time;

    /* max I frame bit ratio to P frame bit */
    RK_S32      max_i_bit_prop;
    RK_S32      min_i_bit_prop;
    RK_S32      init_ip_ratio;
    /* layer bitrate proportion */
    RK_S32      layer_bit_prop[4];

    /* quality parameter */
    RK_S32      init_quality;
    RK_S32      max_quality;
    RK_S32      min_quality;
    RK_S32      max_i_quality;
    RK_S32      min_i_quality;
    RK_S32      i_quality_delta;
    RK_S32      vi_quality_delta;
    RK_S32      fm_lv_min_quality;
    RK_S32      fm_lv_min_i_quality;
    RK_S32      fm_lv_max_quality;
    RK_S32      fm_lv_max_i_quality;
    RK_S32      mt_st_swth_frm_qp;
    /* layer quality proportion */
    RK_S32      layer_quality_delta[4];

    /* reencode parameter */
    RK_S32      max_reencode_times;

    /* still / motion desision parameter */
    RK_S32      min_still_prop;
    RK_S32      max_still_quality;

    /*
     * vbr parameter
     *
     * vbr_hi_prop  - high proportion bitrate for reduce quality
     * vbr_lo_prop  - low proportion bitrate for increase quality
     */
    RK_S32      vbr_hi_prop;
    RK_S32      vbr_lo_prop;

    MppEncRcDropFrmMode drop_mode;
    RK_U32      drop_thd;
    RK_U32      drop_gap;

    RcSuperframeCfg super_cfg;
    RcDebreathCfg   debreath_cfg;
    RcHierQPCfg     hier_qp_cfg;
    RK_U32          shared_buf_en;
    RK_S32          scene_mode;
    RK_S32          motion_static_switch_enable;
    RK_S32          deblur_str;
    RK_S32          atr_str_i;
    RK_S32          atr_str_p;
    RK_S32          atl_str;
    RK_S32          lambda_idx_p;
    RK_S32          lambda_idx_i;
    RK_S32          atf_str;
    RK_U32          lgt_chg_lvl; /* light change level, [0, 3] */
    RK_U32          refresh_len;
} RcCfg;

typedef void *RcCtx;

/*
 * Different rate control strategy will be implemented by different API config
 */
typedef struct RcApi_t {
    char            *name;
    MppCodingType   type;

    /* ctx_size   - get context size */
    RK_U32          (*ctx_size)(void);

    MPP_RET         (*init)(RcCtx ctx, MppEncCfg cfg);
    MPP_RET         (*deinit)(RcCtx ctx);

    MPP_RET         (*frm_check_drop)(RcCtx ctx, EncRcTask *task);
    MPP_RET         (*frm_check_reenc)(RcCtx ctx, EncRcTask *task);

    /*
     * frm_start -  frame level rate control frm_start.
     *              The EncRcTaskInfo will be output to hal for hardware to implement.
     * frm_end   -  frame level rate control frm_end.
     *              The EncRcTaskInfo is returned for real quality and bitrate.
     */
    MPP_RET         (*frm_start)(RcCtx ctx, EncRcTask *task);
    MPP_RET         (*frm_end)(RcCtx ctx, EncRcTask *task);

    /*
     * hal_start -  hardware level rate control start.
     *              The EncRcTaskInfo will be output to hal for hardware to implement.
     * hal_end   -  hardware level rate control end.
     *              The EncRcTaskInfo is returned for real quality and bitrate.
     */
    MPP_RET         (*hal_start)(RcCtx ctx, EncRcTask *task);
    MPP_RET         (*hal_end)(RcCtx ctx, EncRcTask *task);
    /*
     * proc_show - rc debug info show
     *
     */
    void            (*proc_show)(KmppEnvNode node, RcCtx ctx, RK_S32 chl_id);
} RcApi;

/*
 * structures for RC API register and query
 */
typedef struct RcApiBrief_t {
    const char      *name;
    MppCodingType   type;
} RcApiBrief;

typedef struct RcApiQueryAll_t {
    /* input param for query */
    RcApiBrief      *brief;
    RK_S32          max_count;

    /* output query count */
    RK_S32          count;
} RcApiQueryAll;

typedef struct RcApiQueryType_t {
    /* input param for query */
    RcApiBrief      *brief;
    RK_S32          max_count;
    MppCodingType   type;

    /* output query count */
    RK_S32          count;
} RcApiQueryType;

#ifdef __cplusplus
extern "C" {
#endif

rk_s32 rc_api_register(const RcApi *api);
rk_s32 rc_api_unregister(const RcApi *api);

rk_s32 rc_brief_get_all(RcApiQueryAll *query);
rk_s32 rc_brief_get_by_type(RcApiQueryType *query);

#ifdef __cplusplus
}
#endif

#endif /* __MPP_RC_API_H__ */

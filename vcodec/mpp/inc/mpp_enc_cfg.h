/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#ifndef __MPP_ENC_CFG_H__
#define __MPP_ENC_CFG_H__

#include "rk_venc_cmd.h"
#include "rk_venc_ref.h"

extern RK_U8 uuid_refresh_cfg[16];

/*
 * MppEncCfgSet shows the relationship between different configuration
 * Due to the huge amount of configurable parameters we need to setup
 * only minimum amount of necessary parameters.
 *
 * For normal user rc and prep config are enough.
 */

#define POS_TO_FLAG(p, pos) ((rk_u8*)(p) + ((rk_u32)(pos) & 0xffff))
#define POS_TO_ELEM(p, pos) ((rk_u8*)(p) + ((rk_u32)(pos) >> 16))

typedef struct MppEncCfgRoInfo_t {
    rk_s32                 pos_rc;
    rk_s32                 pos_prep;
    rk_s32                 pos_h264;
    rk_s32                 pos_h265;
    rk_s32                 pos_vp8;
    rk_s32                 pos_jpeg;
    rk_s32                 pos_hw;
    rk_s32                 pos_tune;
} MppEncCfgRoInfo;

typedef struct MppEncCfgSet_t {
    MppEncBaseCfg       base;

    // esential config
    MppEncPrepCfg       prep;
    MppEncRcCfg         rc;

    // hardware related config
    MppEncHwCfg         hw;

    // codec detail config
    MppEncH264Cfg       h264;
    MppEncH265Cfg       h265;
    MppEncJpegCfg       jpeg;
    MppEncVp8Cfg        vp8;

    MppEncSliceSplit    split;
    MppEncRefCfg        ref_cfg;
    MppEncROICfg        roi;
    MppJpegROICfg       jpeg_roi;
    MppEncOSDData3      osd;
    MppEncOSDPltCfg     plt_cfg;
    MppEncOSDPlt        plt_data;
    MppEncRefParam      ref_param;

    // quality fine tuning config
    MppEncFineTuneCfg   tune;

    // readonly information
    MppEncCfgRoInfo     ro;
} MppEncCfgSet;

KmppEntry *query_mpp_enc_cfg_entry(const rk_u8 *name);

rk_u32 *mpp_enc_cfg_prep_change(MppEncCfgSet *cfg);
rk_u32 *mpp_enc_cfg_rc_change(MppEncCfgSet *cfg);
rk_u32 *mpp_enc_cfg_hw_change(MppEncCfgSet *cfg);
rk_u32 *mpp_enc_cfg_tune_change(MppEncCfgSet *cfg);
rk_u32 *mpp_enc_cfg_h264_change(MppEncCfgSet *cfg);
rk_u32 *mpp_enc_cfg_h265_change(MppEncCfgSet *cfg);
rk_u32 *mpp_enc_cfg_jpeg_change(MppEncCfgSet *cfg);

#endif /*__MPP_ENC_CFG_H__*/
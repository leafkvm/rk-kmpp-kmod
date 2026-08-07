/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_VSP_OBJS_IMPL_H__
#define __KMPP_VSP_OBJS_IMPL_H__

#include "kmpp_vsp_objs.h"

typedef struct KmppVspInitCfgImpl_t {
    KmppShmPtr      name;
    rk_s32          chan_id;
    rk_s32          max_width;
    rk_s32          max_height;
} KmppVspInitCfgImpl;

typedef struct KmppVspPpMdCfg_t {
    rk_s32          enable;
    rk_s32          switch_sad;
    rk_s32          thres_sad;
    rk_s32          thres_move;
    rk_s32          night_mode;
    rk_s32          filter_switch;
    rk_s32          thres_dust_move;
    rk_s32          thres_dust_blk;
    rk_s32          thres_dust_chng;
} KmppVspPpMdCfg;

typedef struct KmppVspPpOdCfg_t {
    rk_s32          enable;
    rk_s32          is_background;
    rk_s32          thres_complex;
    rk_s32          thres_area_complex;
    rk_s32          thres_complex_cnt;
    rk_s32          thres_sad;
} KmppVspPpOdCfg;

typedef struct KmppVspPpOut_t {
    struct {
        rk_s32      flag;
        rk_s32      pix_sum;
    } od;
} KmppVspPpOut;

typedef struct KmppVspPpWpOut_t {
    rk_s32          wp_out_par_y;
    rk_s32          wp_out_par_u;
    rk_s32          wp_out_par_v;
    rk_s32          wp_out_pic_mean;
} KmppVspPpWpOut;

typedef struct KmppVspPpCfg_t {
    rk_s32          one_shot;
    rk_s32          width;
    rk_s32          height;
    rk_s32          down_scale_en;
    KmppVspPpMdCfg  md;
    KmppVspPpOdCfg  od;
} KmppVspPpCfg;

#endif /* __KMPP_VSP_OBJS_IMPL_H__ */

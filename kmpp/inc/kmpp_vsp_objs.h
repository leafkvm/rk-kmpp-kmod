/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_VSP_OBJS_H__
#define __KMPP_VSP_OBJS_H__

#include "kmpi_vsp.h"

/* vsp pp init config */
#define KMPP_VSP_INIT_CFG_ENTRY_TABLE(prefix, ENTRY, STRCT, EHOOK, SHOOK, ALIAS) \
    ENTRY(prefix, s32, rk_s32,      chan_id,            FLAG_NONE,  chan_id) \
    ENTRY(prefix, s32, rk_s32,      max_width,          FLAG_NONE,  max_width) \
    ENTRY(prefix, s32, rk_s32,      max_height,         FLAG_NONE,  max_height) \
    STRCT(prefix, shm, KmppShmPtr,  name,               FLAG_NONE,  name) \

#define KMPP_OBJ_NAME           kmpp_vsp_init_cfg
#define KMPP_OBJ_INTF_TYPE      KmppVspInitCfg
#define KMPP_OBJ_ENTRY_TABLE    KMPP_VSP_INIT_CFG_ENTRY_TABLE
#include "kmpp_obj_func.h"

/* vsp pp runtime config */
#define KMPP_VSP_PP_CFG_ENTRY_TABLE(prefix, ENTRY, STRCT, EHOOK, SHOOK, ALIAS) \
    CFG_DEF_START() \
    ENTRY(prefix, s32, rk_s32,      width,              FLAG_NONE,  width) \
    ENTRY(prefix, s32, rk_s32,      height,             FLAG_NONE,  height) \
    ENTRY(prefix, s32, rk_s32,      down_scale_en,      FLAG_NONE,  down_scale_en) \
    STRUCT_START(md) \
    ENTRY(prefix, s32, rk_s32,      enable,             FLAG_NONE,  md, enable) \
    ENTRY(prefix, s32, rk_s32,      switch_sad,         FLAG_NONE,  md, switch_sad) \
    ENTRY(prefix, s32, rk_s32,      thres_sad,          FLAG_NONE,  md, thres_sad) \
    ENTRY(prefix, s32, rk_s32,      thres_move,         FLAG_NONE,  md, thres_move) \
    ENTRY(prefix, s32, rk_s32,      night_mode,         FLAG_NONE,  md, night_mode) \
    ENTRY(prefix, s32, rk_s32,      filter_switch,      FLAG_NONE,  md, filter_switch) \
    ENTRY(prefix, s32, rk_s32,      thres_dust_move,    FLAG_NONE,  md, thres_dust_move) \
    ENTRY(prefix, s32, rk_s32,      thres_dust_blk,     FLAG_NONE,  md, thres_dust_blk) \
    ENTRY(prefix, s32, rk_s32,      thres_dust_chng,    FLAG_NONE,  md, thres_dust_chng) \
    STRUCT_END(md) \
    STRUCT_START(od) \
    ENTRY(prefix, s32, rk_s32,      enable,             FLAG_NONE,  od, enable) \
    ENTRY(prefix, s32, rk_s32,      is_background,      FLAG_NONE,  od, is_background) \
    ENTRY(prefix, s32, rk_s32,      thres_complex,      FLAG_NONE,  od, thres_complex) \
    ENTRY(prefix, s32, rk_s32,      thres_area_complex, FLAG_NONE,  od, thres_area_complex) \
    ENTRY(prefix, s32, rk_s32,      thres_complex_cnt,  FLAG_NONE,  od, thres_complex_cnt) \
    ENTRY(prefix, s32, rk_s32,      thres_sad,          FLAG_NONE,  od, thres_sad) \
    STRUCT_END(od) \
    CFG_DEF_END()

#define KMPP_OBJ_NAME               kmpp_vsp_pp_rt_cfg
#define KMPP_OBJ_INTF_TYPE          KmppVspRtCfg
#define KMPP_OBJ_ENTRY_TABLE        KMPP_VSP_PP_CFG_ENTRY_TABLE
#include "kmpp_obj_func.h"

/* vsp pp md / od output data */
#define KMPP_VSP_PP_OUT_ENTRY_TABLE(prefix, ENTRY, STRCT, EHOOK, SHOOK, ALIAS) \
    CFG_DEF_START() \
    STRUCT_START(od) \
    ENTRY(prefix, s32, rk_s32,      flag,               FLAG_NONE,  od, flag) \
    ENTRY(prefix, s32, rk_s32,      pix_sum,            FLAG_NONE,  od, pix_sum) \
    STRUCT_END(od) \
    CFG_DEF_END()

#define KMPP_OBJ_NAME               kmpp_vsp_pp_rt_out
#define KMPP_OBJ_INTF_TYPE          KmppVspRtOut
#define KMPP_OBJ_ENTRY_TABLE        KMPP_VSP_PP_OUT_ENTRY_TABLE
#include "kmpp_obj_func.h"

#endif /*__KMPP_VSP_OBJS_H__*/

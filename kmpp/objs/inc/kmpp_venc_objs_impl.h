/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_VENC_OBJS_IMPL_H__
#define __KMPP_VENC_OBJS_IMPL_H__

#include "kmpp_sym.h"
#include "kmpp_venc_objs.h"

typedef enum {
    MPP_POLL_BUTT       = -2,
    MPP_POLL_BLOCK      = -1,
    MPP_POLL_NON_BLOCK  = 0,
    MPP_POLL_MAX        = 8000,
} KmppPollType;

typedef struct KmppVencInitCfgImpl_t {
    MppCtxType      type;
    MppCodingType   coding;
    rk_s32          chan_id;
    rk_s32          chan_fd;
    rk_s32          online;
    rk_u32          buf_size;
    rk_u32          max_strm_cnt;
    rk_u32          shared_buf_en;
    void            *shared_buf;
    rk_u32          smart_en;
    rk_u32          max_width;
    rk_u32          max_height;
    rk_u32          max_lt_cnt;
    rk_u32          deblur_en;
    rk_u32          qpmap_en;
    rk_u32          chan_dup;
    rk_u32          tmvp_enable;
    rk_u32          only_smartp;
    rk_u32          ntfy_mode;
    rk_s32          input_timeout;
    rk_s32          output_timeout;
    rk_s32          scene_mode;
} KmppVencInitCfgImpl;

typedef struct KmppVencNotifyImpl_t {
    KmppSyms            venc_syms;
    KmppSym             ntfy_sym;
    rk_s32              chan_id;
    KmppNotifyCmd       cmd;
    KmppVencDropFrmType drop_type;
    rk_u32              pipe_id;
    rk_u32              frame_id;
    KmppFrame           frame;
    KmppPacket          packet;
    rk_u32              is_intra;
    /* md/od infos */
    rk_u32              luma_pix_sum_od;
    rk_u32              md_index;
    rk_u32              od_index;
} KmppVencNtfyImpl;

typedef struct KmppRefStCfgImpl_t {
    RK_S32 is_non_ref;
    RK_S32 temporal_id;
    MppEncRefMode ref_mode;
    RK_S32 ref_arg;
    RK_S32 repeat;
} KmppRefStCfgImpl;

typedef struct KmppRefLtCfgImpl_t {
    RK_S32 lt_idx;
    RK_S32 temporal_id;
    MppEncRefMode ref_mode;
    RK_S32 ref_arg;
    RK_S32 lt_gap;
    RK_S32 lt_delay;
} KmppRefLtCfgImpl;

typedef struct KmppRefCfgImpl_t {
    /* ref cfg parameters */
    RK_S32 keep_cpb;
    RK_S32 lt_cfg_cnt;
    RK_S32 st_cfg_cnt;

    /* offset information for SHM data */
    RK_U32 lt_cfg_offset;
    RK_U32 st_cfg_offset;
} KmppRefCfgImpl;

rk_s32 kmpp_venc_notify(KmppVencNtfy ntfy);

#endif /* __KMPP_VENC_OBJS_IMPL_H__ */

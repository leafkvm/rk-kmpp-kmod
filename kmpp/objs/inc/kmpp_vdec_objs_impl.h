/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_VDEC_OBJS_IMPL_H__
#define __KMPP_VDEC_OBJS_IMPL_H__

#include "kmpp_sym.h"
#include "kmpp_frame.h"
#include "kmpp_vdec_objs.h"

typedef struct KmppVdecBaseCfg_t {
    MppCtxType          type;
    MppCodingType       coding;
    rk_s32              hw_type;
    rk_u32              batch_mode;

    MppFrameFormat      out_fmt;
    rk_u32              fast_out;
    rk_u32              fast_parse;
    rk_u32              split_parse;
    rk_u32              internal_pts;
    rk_u32              sort_pts;
    rk_u32              disable_error;
    rk_u32              enable_vproc;
    rk_u32              enable_fast_play;
    rk_u32              enable_hdr_meta;
    rk_u32              enable_thumbnail;
    rk_u32              enable_mvc;
    rk_u32              disable_dpb_chk;
    rk_u32              disable_thread;
    rk_u32              codec_mode;
    rk_u32              dis_err_clr_mark;

    /* new kmpp features */
    /*
     * Setup decoder input packet buffer size for valid packet querying
     * When zero decoder will accept input packet with any size but limit by input queue size
     * When non-zero decoder need to query valid packet size before filling data to packet
     */
    rk_u32              pkt_buf_size;
    /*
     * Request decoder to run in ring buffer input mode
     * When ring buffer mode is enabled user should call get_pkt to get valid packet
     * from decoder first and check valid length for data filling. Then after data
     * filling, user should call put_pkt to put the packet back to decoder.
     * NOTE: the ring buffer is dmabuf shared memory, so user can write to it directly
     */
    rk_u32              enable_ringbuf;
} KmppVdecBaseCfg;

typedef struct KmppVdecIoCfg_t {
    rk_s32              timeout;
    rk_s32              queue_size;
} KmppVdecIoCfg;

typedef struct KmppVdecCfgSet_t {
    rk_u32              width;
    rk_u32              height;
    MppFrameFormat      format;

    /* decoded buffer count from parser */
    rk_s32              dpb_cnt;
    /* hardware buffer count */
    rk_u32              hal_task_cnt;
    /* extra buffer count for display / vproc */
    rk_u32              extra_buf_cnt;
    /* externl buffer group mode */
    rk_s32              buf_mode;

    /* frame buffer group */
    KmppShmPtr          frm_grp;
} KmppVdecCfgSet;

typedef struct KmppVdecCfgImpl_t {
    /* base init config */
    KmppVdecBaseCfg     base;

    /* input / output queue config */
    KmppVdecIoCfg       input;
    KmppVdecIoCfg       output;

    /* internal value for reading */
    KmppVdecCfgSet      cfg;
} KmppVdecCfgImpl;

#endif /* __KMPP_VDEC_OBJS_IMPL_H__ */

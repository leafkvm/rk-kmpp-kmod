/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __LEGACY_ROCKIT_H__
#define __LEGACY_ROCKIT_H__

#include "kmpp_frame.h"

typedef void* MppEnc;
typedef void* MppRoi;
typedef void* MppOsd;
typedef void* MpiBuf;
typedef void *HalBufs;
typedef void* MppPpInfo;

struct hal_shared_buf {
    HalBufs    dpb_bufs;
    MppBuffer  recn_ref_buf;
    MppBuffer  ext_line_buf;
    MppBuffer  stream_buf;
};

/* pp legacy defines */
struct pp_chn_attr {
    int width;
    int height;
    int smear_en;
    int weightp_en;
    int md_en;
    int od_en;
    int down_scale_en;
    int max_width;
    int max_height;
    int reserved[6];
};

struct pp_com_cfg {
    int frm_cnt;
    MppFrameFormat fmt;

    int gop;
    int md_interval;
    int od_interval;

    struct mpi_buf *src_buf;
    int stride0;
    int stride1;
    int reserved[8];
};

struct pp_md_cfg {
    struct mpi_buf *mdw_buf;
    int switch_sad;
    int thres_sad;
    int thres_move;
    int night_mode;
    int filter_switch;
    int thres_dust_move;
    int thres_dust_blk;
    int thres_dust_chng;
    int reserved[5];
};

struct pp_od_cfg {
    int is_background;
    int thres_complex;
    int thres_area_complex;
    struct mpi_buf *odw_buf;
    int thres_complex_cnt;
    int thres_sad;
    int reserved[5];
};

struct pp_smear_cfg {
    struct mpi_buf *smrw_buf; /* 0x200: smear write */
    int reserved[32];
};

struct pp_weightp_cfg {
    int dummy;
    int reserved[16];
};

struct pp_od_out {
    int od_flg;
    int pix_sum;
    int reserved[8];
};

struct pp_weightp_out {
    int wp_out_par_y;
    int wp_out_par_u;
    int wp_out_par_v;
    int wp_out_pic_mean;
    int reserved[8];
};

enum pp_cmd {
    PP_CMD_BASE = 0,
    PP_CMD_SET_COMMON_CFG = 0x10, /* struct pp_com_cfg */
    PP_CMD_SET_CHANNEL_INFO = 0x11, /* struct pp_chn_attr */
    PP_CMD_SET_MD_CFG = 0x20, /* struct pp_md_cfg */
    PP_CMD_SET_OD_CFG = 0x30, /* struct pp_od_cfg */
    PP_CMD_SET_SMEAR_CFG = 0x40, /* struct pp_smear_cfg */
    PP_CMD_SET_WEIGHTP_CFG = 0x50, /* struct pp_weightp_cfg */

    PP_CMD_RUN_SYNC = 0x60,

    PP_CMD_GET_OD_OUTPUT = 0x70, /* struct pp_od_out */
    PP_CMD_GET_WEIGHTP_OUTPUT = 0x80, /* struct pp_weightp_out */
};

int vepu_pp_create_chn(int chn, struct pp_chn_attr *attr);
int vepu_pp_destroy_chn(int chn);
int vepu_pp_control(int chn, enum pp_cmd cmd, void *param);

#endif /* __LEGACY_ROCKIT_H__ */

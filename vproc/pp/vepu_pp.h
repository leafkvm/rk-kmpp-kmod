/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2022 Rockchip Electronics Co., Ltd.
 */

#ifndef __VEPU_PP_H__
#define __VEPU_PP_H__

#include <linux/types.h>

#include "mpp_device.h"
#include "vepu_pp_common.h"

#define MAX_CHN_NUM  (8)

struct pp_buffer_t {
    struct mpi_buf *buf;
    struct dma_buf *buf_dma;
    dma_addr_t iova;
};

struct pp_param_t {
    /* 0x0034 - ENC_PIC_FMT */
    struct {
        RK_U32 src_from_isp : 1;
        RK_U32 ref_pic0_updt_en : 1;
        RK_U32 ref_pic1_updt_en : 1;
        RK_U32 reserved : 29;
    } enc_pic_fmt;

    /* 0x0038 - ENC_PIC_RSL */
    struct {
        RK_U32 pic_wd8_m1 : 11;
        RK_U32 reserved1 : 5;
        RK_U32 pic_hd8_m1 : 11;
        RK_U32 reserved2 : 5;
    } enc_pic_rsl;

    /* 0x0100 - VSP_PIC_CON */
    struct {
        RK_U32 src_cfmt : 4;
        RK_U32 out_fmt_cfg : 1;
        RK_U32 alpha_swap : 1;
        RK_U32 rbuv_swap : 1;
        RK_U32 src_rot : 2;
        RK_U32 src_mirr : 1;
        RK_U32 src_range_trns_en : 1;
        RK_U32 src_range_trns_sel : 1;
        RK_U32 src_chroma_ds_mode : 1;
        RK_U32 reserved1 : 3;
        RK_U32 vepu_pp_internal_filter_strength : 2;
        RK_U32 reserved2 : 2;
        RK_U32 vepu_pp_edge_filter_strength : 2;
        RK_U32 reserved3 : 2;
        RK_U32 vepu_pp_corner_filter_strength : 2;
        RK_U32 reserved4 : 6;
    } vsp_pic_con;

    /* 0x0104 - VSP_PIC_FILL */
    struct {
        RK_U32 pic_wfill : 6;
        RK_U32 reserved1 : 10;
        RK_U32 pic_hfill : 6;
        RK_U32 reserved2 : 10;
    } vsp_pic_fill;

    /* 0x0108 - VSP_PIC_OFST */
    struct {
        RK_U32 pic_ofst_x : 14;
        RK_U32 reserved1 : 2;
        RK_U32 pic_ofst_y : 14;
        RK_U32 reserved2 : 2;
    } vsp_pic_ofst;

    /* 0x0118 - VSP_PIC_STRD0 */
    struct {
        RK_U32 src_strd0 : 17;
        RK_U32 reserved : 15;
    } vsp_pic_strd0;

    /* 0x011c - VSP_PIC_STRD1 */
    struct {
        RK_U32 src_strd1 : 16;
        RK_U32 reserved : 16;
    } vsp_pic_strd1;

    /* 0x0120 - VSP_PIC_UDFY */
    struct {
        RK_U32 csc_wgt_b2y : 9;
        RK_U32 csc_wgt_g2y : 9;
        RK_U32 csc_wgt_r2y : 9;
        RK_U32 reserved : 5;
    } vsp_pic_udfy;

    /* 0x0124 - VSP_PIC_UDFU */
    struct {
        RK_U32 csc_wgt_b2u : 9;
        RK_U32 csc_wgt_g2u : 9;
        RK_U32 csc_wgt_r2u : 9;
        RK_U32 reserved : 5;
    } vsp_pic_udfu;

    /* 0x0128 - VSP_PIC_UDFV */
    struct {
        RK_U32 csc_wgt_b2v : 9;
        RK_U32 csc_wgt_g2v : 9;
        RK_U32 csc_wgt_r2v : 9;
        RK_U32 reserved : 5;
    } vsp_pic_udfv;

    /* 0x012c - VSP_PIC_UDFO */
    struct {
        RK_U32 csc_ofst_v : 8;
        RK_U32 csc_ofst_u : 8;
        RK_U32 csc_ofst_y : 5;
    } vsp_pic_udfo;

    /* 0x0204 - SMR_CON_BASE */
    struct {
        RK_U32 smear_cur_frm_en : 1;
        RK_U32 smear_ref_frm_en : 1;
        RK_U32 reserved : 30;
    } smr_con_base;

    RK_U32 smr_resi_thd0; /* 0x208 */
    RK_U32 smr_resi_thd1;
    RK_U32 smr_resi_thd2;
    RK_U32 smr_resi_thd3;
    RK_U32 smr_madp_thd0; /* 0x218 */
    RK_U32 smr_madp_thd1;
    RK_U32 smr_madp_thd2;
    RK_U32 smr_madp_thd3;
    RK_U32 smr_madp_thd4;
    RK_U32 smr_madp_thd5;
    RK_U32 smr_cnt_thd0; /* 0x230 */
    RK_U32 smr_cnt_thd1;
    RK_U32 smr_sto_strd; /* 0x238 */

    RK_U32 wp_con_comb0; /* 0x300 */
    RK_U32 wp_con_comb1;
    RK_U32 wp_con_comb2;
    RK_U32 wp_con_comb3;
    RK_U32 wp_con_comb4;
    RK_U32 wp_con_comb5;
    RK_U32 wp_con_comb6;

    /* 0x0404 - MD_CON_BASE */
    struct {
        RK_U32 cur_frm_en : 1;
        RK_U32 ref_frm_en : 1;
        RK_U32 switch_sad : 2;
        RK_U32 thres_sad  : 12;
        RK_U32 thres_move : 3;
        RK_U32 reserved1  : 13;
    } md_con_base;

    /* 0x0408 - MD_CON_FLY_CHECK */
    struct {
        RK_U32 night_mode_en : 1;
        RK_U32 flycatkin_flt_en : 1;
        RK_U32 thres_dust_move : 4;
        RK_U32 thres_dust_blk : 3;
        RK_U32 thres_dust_chng : 8;
        RK_U32 reserved1 : 15;
    } md_fly_chk;

    RK_U32 md_sto_strd;

    RK_U32 od_con_base; /* 0x500 */

    struct {
        RK_U32 od_thres_complex : 12;
        RK_U32 od_thres_area_complex : 18;
        RK_U32 reserved1 : 2;
    } od_con_cmplx;

    /* 0x0508 - OD_CON_SAD */
    struct {
        RK_U32 od_thres_sad : 12;
        RK_U32 od_thres_area_sad : 18;
        RK_U32 reserved1 : 2;
    } od_con_sad;

    /* 0x600 ~ 0x734 */
    RK_U32 osd_en[8];
    RK_U32 osd_cfgs[78];

    /* buffer address */
    RK_U32 adr_rfpw; /* 0x3C */
    RK_U32 adr_rfpr0; /* 0x40 */
    RK_U32 adr_rfpr1; /* 0x44 */
    RK_U32 adr_rfsw; /* 0x48 */
    RK_U32 adr_rfsr; /* 0x4C */
    RK_U32 adr_rfmw; /* 0x50 */
    RK_U32 adr_rfmr; /* 0x54 */
    RK_U32 adr_src0; /* 0x10C */
    RK_U32 adr_src1; /* 0x110 */
    RK_U32 adr_src2; /* 0x114 */
    RK_U32 adr_smr_base; /* 0x200 */
    RK_U32 adr_md_base; /* 0x400 */
};

struct pp_output_t {
    RK_U32 wp_out_par_y; /* 0x800 */
    RK_U32 wp_out_par_u;
    RK_U32 wp_out_par_v;
    RK_U32 wp_out_pic_mean;
    RK_U32 od_out_flag;
    RK_U32 od_out_pix_sum; /* 0x814 */
};

struct pp_chn_info_t {
    RK_U32 chn; /* channel ID */
    RK_U32 width;
    RK_U32 height;
    RK_U32 max_width;
    RK_U32 max_height;
    int smear_en;
    int weightp_en;
    int md_en;
    int od_en;
    int down_scale_en;
    int frm_accum_interval;
    int frm_accum_gop;
    int smear_stride;

    struct pp_buffer_t *buf_rfpw;
    struct pp_buffer_t *buf_rfpr;
    struct pp_buffer_t *buf_rfmwr; /* MD */
    struct pp_buffer_t *buf_rfswr; /* smear */

    struct pp_param_t param;
    struct pp_output_t output;

    MppDev dev;
    void *device;   /* struct device* */
};

struct vepu_pp_ctx_t {
    struct pp_chn_info_t chn_info[MAX_CHN_NUM];
};

#endif

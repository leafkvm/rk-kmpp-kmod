/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __VEPU511_PP_H__
#define __VEPU511_PP_H__

#include "mpp_device.h"
#include "kmpp_dmaheap.h"

#include "kmpp_vsp_objs_impl.h"
#include "vepu_pp_common.h"

#define MAX_CHN_NUM  (8)

typedef struct vepu511_pp_param_t {
    struct {
        rk_u32 enc_done_en          : 1;
        rk_u32 lkt_node_done_en     : 1;
        rk_u32 sclr_done_en         : 1;
        rk_u32 vslc_done_en         : 1;
        rk_u32 vbsf_oflw_en         : 1;
        rk_u32 vbuf_lens_en         : 1;
        rk_u32 enc_err_en           : 1;
        rk_u32 vsrc_err_en          : 1;
        rk_u32 wdg_en               : 1;
        rk_u32 lkt_err_int_en       : 1;
        rk_u32 lkt_err_stop_en      : 1;
        rk_u32 lkt_force_stop_en    : 1;
        rk_u32 jslc_done_en         : 1;
        rk_u32 jbsf_oflw_en         : 1;
        rk_u32 jbuf_lens_en         : 1;
        rk_u32 dvbm_err_en          : 1;
        rk_u32 reserved             : 16;
    } int_en;

    /* 0x24 */
    rk_u32 int_msk;

    /* 0x00000280 reg160 */
    rk_u32 adr_src0;

    /* 0x00000284 reg161 */
    rk_u32 adr_src1;

    /* 0x00000288 reg162 */
    rk_u32 adr_src2;

    /* 0x000002a4 reg169 */
    rk_u32 dspw_addr;

    /* 0x000002a8 reg170 */
    rk_u32 dspr_addr;

    /* 0x00000300 reg192 */
    struct {
        rk_u32 enc_stnd                : 2;
        rk_u32 cur_frm_ref             : 1;
        rk_u32 mei_stor                : 1;
        rk_u32 bs_scp                  : 1;
        rk_u32 reserved                : 3;
        rk_u32 pic_qp                  : 6;
        rk_u32 num_pic_tot_cur_hevc    : 5;
        rk_u32 log2_ctu_num_hevc       : 5;
        rk_u32 reserved1               : 3;
        rk_u32 eslf_out_e_jpeg         : 1;
        rk_u32 jpeg_slen_fifo          : 1;
        rk_u32 eslf_out_e              : 1;
        rk_u32 slen_fifo               : 1;
        rk_u32 rec_fbc_dis             : 1;
    } enc_pic;

    /* 0x00000308 reg194 */
    struct {
        rk_u32 frame_id        : 8;
        rk_u32 frm_id_match    : 1;
        rk_u32 reserved        : 3;
        rk_u32 source_id       : 1;
        rk_u32 src_id_match    : 1;
        rk_u32 reserved1       : 2;
        rk_u32 ch_id           : 2;
        rk_u32 vrsp_rtn_en     : 1;
        rk_u32 vinf_req_en     : 1;
        rk_u32 reserved2       : 12;
    } enc_id;

    /* 0x00000310 reg196 */
    struct {
        rk_u32 pic_wd8_m1    : 11;
        rk_u32 reserved      : 5;
        rk_u32 pic_hd8_m1    : 11;
        rk_u32 reserved1     : 5;
    } enc_rsl;

    /* 0x00000314 reg197 */
    struct {
        rk_u32 pic_wfill    : 6;
        rk_u32 reserved     : 10;
        rk_u32 pic_hfill    : 6;
        rk_u32 reserved1    : 10;
    } src_fill;

    /* 0x00000318 reg198 */
    struct {
        rk_u32 alpha_swap            : 1;
        rk_u32 rbuv_swap             : 1;
        rk_u32 src_cfmt              : 4;
        rk_u32 src_rcne              : 1;
        rk_u32 out_fmt               : 1;
        rk_u32 src_range_trns_en     : 1;
        rk_u32 src_range_trns_sel    : 1;
        rk_u32 chroma_ds_mode        : 1;
        rk_u32 reserved              : 21;
    } src_fmt;

    /* 0x0000031c reg199 */
    struct {
        RK_U32 csc_wgt_b2y    : 9;
        RK_U32 csc_wgt_g2y    : 9;
        RK_U32 csc_wgt_r2y    : 9;
        RK_U32 reserved       : 5;
    } src_udfy;

    /* 0x00000320 reg200 */
    struct {
        RK_U32 csc_wgt_b2u    : 9;
        RK_U32 csc_wgt_g2u    : 9;
        RK_U32 csc_wgt_r2u    : 9;
        RK_U32 reserved       : 5;
    } src_udfu;

    /* 0x00000324 reg201 */
    struct {
        RK_U32 csc_wgt_b2v    : 9;
        RK_U32 csc_wgt_g2v    : 9;
        RK_U32 csc_wgt_r2v    : 9;
        RK_U32 reserved       : 5;
    } src_udfv;

    /* 0x00000328 reg202 */
    struct {
        RK_U32 csc_ofst_v    : 8;
        RK_U32 csc_ofst_u    : 8;
        RK_U32 csc_ofst_y    : 5;
        RK_U32 reserved      : 11;
    } src_udfo;

    /* 0x00000330 reg204 */
    struct {
        rk_u32 pic_ofst_x    : 14;
        rk_u32 reserved      : 2;
        rk_u32 pic_ofst_y    : 14;
        rk_u32 reserved1     : 2;
    } pic_ofst;

    /* 0x00000334 reg205 */
    struct {
        rk_u32 src_strd0    : 21;
        rk_u32 reserved     : 11;
    } src_strd0;

    /* 0x00000338 reg206 */
    struct {
        rk_u32 src_strd1    : 16;
        rk_u32 reserved     : 16;
    } src_strd1;

    /* 0x0000033c reg207 */
    struct {
        rk_u32 pp_corner_filter_strength      : 2;
        rk_u32 reserved                       : 2;
        rk_u32 pp_edge_filter_strength        : 2;
        rk_u32 reserved1                      : 2;
        rk_u32 pp_internal_filter_strength    : 2;
        rk_u32 reserved2                      : 22;
    } src_flt_cfg;

    /* 0x00000370 reg220 */
    struct {
        rk_u32 cime_srch_dwnh    : 4;
        rk_u32 cime_srch_uph     : 4;
        rk_u32 cime_srch_rgtw    : 4;
        rk_u32 cime_srch_lftw    : 4;
        rk_u32 dlt_frm_num       : 16;
    } me_rnge;

    /* 0x00000374 reg221 */
    struct {
        rk_u32 srgn_max_num      : 7;
        rk_u32 cime_dist_thre    : 13;
        rk_u32 rme_srch_h        : 2;
        rk_u32 rme_srch_v        : 2;
        rk_u32 rme_dis           : 3;
        rk_u32 reserved1         : 1;
        rk_u32 fme_dis           : 3;
        rk_u32 reserved2         : 1;
    } me_cfg;

    /* 0x00000378 reg222 */
    struct {
        rk_u32 cime_zero_thre     : 13;
        rk_u32 reserved           : 15;
        rk_u32 fme_prefsu_en      : 2;
        rk_u32 colmv_stor         : 1;
        rk_u32 colmv_load         : 1;
    } me_cach;

    /* 0x000003bc reg239 */
    struct {
        rk_u32 cbc_init_flg           : 1;
        rk_u32 mvd_l1_zero_flg        : 1;
        rk_u32 mrg_up_flg             : 1;
        rk_u32 mrg_lft_flg            : 1;
        rk_u32 reserved               : 1;
        rk_u32 ref_pic_lst_mdf_l0     : 1;
        rk_u32 num_refidx_l1_act      : 2;
        rk_u32 num_refidx_l0_act      : 2;
        rk_u32 num_refidx_act_ovrd    : 1;
        rk_u32 sli_sao_chrm_flg       : 1;
        rk_u32 sli_sao_luma_flg       : 1;
        rk_u32 sli_tmprl_mvp_e        : 1;
        rk_u32 pic_out_flg            : 1;
        rk_u32 sli_type               : 2;
        rk_u32 sli_rsrv_flg           : 7;
        rk_u32 dpdnt_sli_seg_flg      : 1;
        rk_u32 sli_pps_id             : 6;
        rk_u32 no_out_pri_pic         : 1;
    } synt_sli0;

    /* 0x0000047c reg287 */
    struct {
        rk_u32 reserved             : 31;
        rk_u32 jpeg_stnd            : 1;
    } jpeg_base_cfg;

    /* 0x00000520 reg328 */
    rk_u32 adr_md_vpp;

    /* 0x00000524 reg329 */
    rk_u32 adr_od_vpp;

    /* 0x00000528 reg330 */
    rk_u32 adr_ref_mdw;

    /* 0x0000052c reg331 */
    rk_u32 adr_ref_mdr;

    /* 0x00000530 reg332 */
    struct {
        rk_u32 sto_stride_md          : 8;
        rk_u32 sto_stride_od          : 8;
        rk_u32 cur_frm_en_md          : 1;
        rk_u32 ref_frm_en_md          : 1;
        rk_u32 switch_sad_md          : 2;
        rk_u32 night_mode_en_md       : 1;
        rk_u32 flycatkin_flt_en_md    : 1;
        rk_u32 en_od                  : 1;
        rk_u32 background_en_od       : 1;
        rk_u32 sad_comp_en_od         : 1;
        rk_u32 reserved               : 6;
        rk_u32 vepu_pp_en             : 1;
    } vpp_base_cfg;

    /* 0x00000534 reg333 */
    struct {
        rk_u32 thres_sad_md          : 12;
        rk_u32 thres_move_md         : 3;
        rk_u32 reserved              : 1;
        rk_u32 thres_dust_move_md    : 4;
        rk_u32 thres_dust_blk_md     : 3;
        rk_u32 reserved1             : 1;
        rk_u32 thres_dust_chng_md    : 8;
    } thd_md_vpp;

    /* 0x00000538 reg334 */
    struct {
        rk_u32 thres_complex_od        : 12;
        rk_u32 thres_complex_cnt_od    : 3;
        rk_u32 thres_sad_od            : 14;
        rk_u32 reserved                : 3;
    } thd_od_vpp;
} vepu511_pp_param;

typedef struct vepu511_pp_chn_info_t {
    KmppVspPpCfg cfg;
    KmppFrame frame;
    rk_u32 frm_cnt;
    rk_u32 frm_num;
    KmppDmaBuf in_buf;
    KmppDmaBuf md_buf;
    KmppDmaBuf od_buf;

    rk_u32 chn; /* channel ID */
    rk_u32 used;
    rk_u32 width;
    rk_u32 height;
    rk_u32 max_width;
    rk_u32 max_height;
    rk_s32 md_en;
    rk_s32 od_en;
    rk_s32 flycatkin_en;
    rk_s32 frm_accum_interval;
    rk_s32 frm_accum_gop;
    rk_s32 mdw_len;
    rk_s32 gop;
    rk_s32 md_interval;
    rk_s32 od_interval;

    struct pp_buffer_t *buf_rfpw;
    struct pp_buffer_t *buf_rfpr;
    /* MD rw*/
    struct pp_buffer_t *buf_rfmrd;
    struct pp_buffer_t *buf_rfmwr;

    vepu511_pp_param param;
    struct pp_output_t output;

    MppDev dev;
    void *device;   /* struct device* */
} vepu511_pp_chn_info;

struct vepu511_pp_ctx_t {
    vepu511_pp_chn_info chn_info[MAX_CHN_NUM];
};

extern KmppVspApi vepu511_pp_api;

#endif

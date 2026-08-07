/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __HAL_JPEGE_VEPU500_REG_H__
#define __HAL_JPEGE_VEPU500_REG_H__

#include "rk_type.h"
#include "vepu500_common.h"

#define VEPU500_JPEGE_FRAME_OFFSET      (0x300)
#define VEPU500_JPEGE_TABLE_OFFSET      (0x2590)
#define VEPU500_JPEGE_OSD_OFFSET    (0x3138)
#define VEPU500_JPEGE_STATUS_OFFSET     (0x4000)
#define VEPU500_JPEGE_INT_OFFSET        (0x2c)

typedef struct JpegVepu500Frame_t {
    /* 0x00000300 reg192 */
    struct {
        RK_U32 enc_stnd                : 2;
        RK_U32 cur_frm_ref             : 1;
        RK_U32 mei_stor                : 1;
        RK_U32 bs_scp                  : 1;
        RK_U32 reserved                : 3;
        RK_U32 pic_qp                  : 6;
        RK_U32 num_pic_tot_cur_hevc    : 5;
        RK_U32 log2_ctu_num_hevc       : 5;
        RK_U32 reserved1               : 3;
        RK_U32 eslf_out_e_jpeg         : 1;
        RK_U32 jpeg_slen_fifo          : 1;
        RK_U32 eslf_out_e              : 1;
        RK_U32 slen_fifo               : 1;
        RK_U32 rec_fbc_dis             : 1;
    } video_enc_pic;

    RK_U32 reserved_reg304;

    /* 0x00000308 reg194 */
    struct {
        RK_U32 frame_id        : 8;
        RK_U32 frm_id_match    : 1;
        RK_U32 reserved        : 3;
        RK_U32 source_id       : 1;
        RK_U32 src_id_match    : 1;
        RK_U32 reserved1       : 2;
        RK_U32 ch_id           : 2;
        RK_U32 vrsp_rtn_en     : 1;
        RK_U32 vinf_req_en     : 1;
        RK_U32 reserved2       : 12;
    } enc_id;

    RK_U32 reserved[61];

    /* 0x400 - 0x50c */
    Vepu500JpegFrame jpeg_frame;
} JpegVepu500Frame;

typedef struct JpegV500RegSet_t {
    Vepu500ControlCfg   reg_ctl;
    /* 0x400 - 0x50c */
    JpegVepu500Frame    reg_frm;
    /* 0x2590 - 0x270c */
    Vepu500JpegTable    reg_table;
    /* 0x3138 - 0x3264 */
    Vepu500Osd      reg_osd;
    Vepu500Dbg      reg_dbg;
} JpegV500RegSet;

typedef struct JpegV500Status_t {
    RK_U32 hw_status;
    Vepu500Status st;
} JpegV500Status;

#endif

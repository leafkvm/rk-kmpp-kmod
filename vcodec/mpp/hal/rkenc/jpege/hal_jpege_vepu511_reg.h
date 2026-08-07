/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __HAL_JPEGE_VEPU511_REG_H__
#define __HAL_JPEGE_VEPU511_REG_H__

#include "rk_type.h"
#include "vepu511_common.h"
#include "hal_h264e_vepu511_reg.h"

#define VEPU511_JPEGE_FRAME_OFFSET      (0x300)
#define VEPU511_JPEGE_TABLE_OFFSET      (0x2ca0)
#define VEPU511_JPEGE_OSD_OFFSET        (0x3138)
#define VEPU511_JPEGE_STATUS_OFFSET     (0x4000)
#define VEPU511_JPEGE_INT_OFFSET        (0x2c)

typedef struct JpegVepu511Frame_t {
    /* 0x00000300 reg192 */
    struct {
        RK_U32 enc_stnd                : 2;
        RK_U32 cur_frm_ref             : 1;
        RK_U32 mei_stor                : 1;
        RK_U32 bs_scp                  : 1;
        RK_U32 meiw_mode               : 1;
        RK_U32 reserved                : 2;
        RK_U32 pic_qp                  : 6;
        RK_U32 num_pic_tot_cur_hevc    : 5;
        RK_U32 log2_ctu_num_hevc       : 5;
        RK_U32 rfpr_compress_mode      : 1;
        RK_U32 reserved1               : 2;
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

    /* 0x0000030c reg195 - 0x000003fc reg255 */
    RK_U32 reserved195_255[61];

    /* 0x400 - 0x50c */
    Vepu511JpegFrame jpeg_frame;
} JpegVepu511Frame;

typedef struct JpegV511RegSet_t {
    Vepu511ControlCfg   reg_ctl;
    Vepu511FrameCfg     reg_frm;
    Vepu511Param        reg_param;
    /* 0x2ca0 - 0x2e1c */
    Vepu511JpegTable    reg_table;
    /* 0x00003138 reg3150 - 0x00003264 reg3225 */
    Vepu511Osd      reg_osd;
    /* 0x00005000 reg5120 - 0x00005230 reg5260*/
    Vepu511Dbg      reg_dbg;
} JpegV511RegSet;

typedef struct JpegV511Status_t {
    RK_U32 hw_status;
    Vepu511Status st;
} JpegV511Status;

#endif

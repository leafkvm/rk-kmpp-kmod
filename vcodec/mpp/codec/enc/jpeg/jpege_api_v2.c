/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "jpege_api_v2"

#include <uapi/asm-generic/errno-base.h>

#include "kmpp_string.h"
#include "kmpp_uaccess.h"

#include "kmpp_log.h"
#include "kmpp_log.h"
#include "mpp_maths.h"
#include "mpp_mem.h"
//#include "mpp_2str.h"
#include "kmpp_packet.h"
#include "jpege_debug.h"
#include "jpege_api_v2.h"
#include "jpege_syntax.h"
#include "mpp_bitwrite.h"
#include "kmpp_frame.h"

typedef struct {
    MppEncCfgSet    *cfg;
    JpegeSyntax     syntax;
} JpegeCtx;

const RK_U8 jpege_luma_quantizer[QUANTIZE_TABLE_SIZE] = {
    16, 11, 10, 16, 24, 40, 51, 61,
    12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56,
    14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77,
    24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103, 99
};

const RK_U8 jpege_chroma_quantizer[QUANTIZE_TABLE_SIZE] = {
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
};

RK_U32 jpege_debug = 0;

static MPP_RET jpege_init_v2(void *ctx, EncImplCfg *cfg)
{
    JpegeCtx *p = (JpegeCtx *)ctx;

//    mpp_env_get_u32("jpege_debug", &jpege_debug, 0);
    jpege_dbg_func("enter ctx %p\n", ctx);

    p->cfg = cfg->cfg;

    kmpp_assert(cfg->coding = MPP_VIDEO_CodingMJPEG);
    cfg->task_count = 1;

    jpege_dbg_func("leave ctx %p\n", ctx);

    return MPP_OK;
}

static MPP_RET jpege_deinit_v2(void *ctx)
{
    jpege_dbg_func("enter ctx %p\n", ctx);

    jpege_dbg_func("leave ctx %p\n", ctx);

    return MPP_OK;
}

static MPP_RET jpege_proc_cfg(void *ctx, MpiCmd cmd, void *param)
{
    MPP_RET ret = MPP_OK;

    jpege_dbg_func("enter ctx %p cmd %x param %p\n", ctx, cmd, param);

    switch (cmd) {
    case MPP_ENC_SET_CFG : {
    } break;
    case MPP_ENC_SET_IDR_FRAME :
    case MPP_ENC_SET_OSD_PLT_CFG :
    case MPP_ENC_SET_OSD_DATA_CFG :
    case MPP_ENC_GET_SEI_DATA :
    case MPP_ENC_SET_SEI_CFG : {
    } break;
    default:
        kmpp_loge_f("No correspond cmd(%08x) found, and can not config!", cmd);
        ret = MPP_NOK;
        break;
    }

    jpege_dbg_func("leave ret %d\n", ret);

    return ret;
}

static MPP_RET jpege_start(void *ctx, HalEncTask *task)
{
    JpegeCtx *p = (JpegeCtx *)ctx;
    JpegeSyntax syntax = p->syntax;
    KmppPacket pkt = task->packet;
    KmppShmPtr pos;
    RK_U8 *ptr = NULL;
    RK_S32 buf_size;
    RK_S32 size = 0;
    MppWriteCtx bit_ctx;
    MppWriteCtx *bits = &bit_ctx;

    kmpp_packet_get_pos(pkt, &pos);
    ptr = pos.kptr;
    kmpp_packet_get_size(pkt, &buf_size);

    mpp_writer_init(bits, ptr, buf_size);

    /* add SOI and APP0 data */
    /* SOI */
    mpp_writer_put_raw_bits(bits, 0xFFD8, 16);
    /* APP0 */
    mpp_writer_put_raw_bits(bits, 0xFFE0, 16);
    /* length */
    mpp_writer_put_raw_bits(bits, 0x0010, 16);
    /* "JFIF" ID */
    /* Ident1 */
    mpp_writer_put_raw_bits(bits, 0x4A46, 16);
    /* Ident2 */
    mpp_writer_put_raw_bits(bits, 0x4946, 16);
    /* Ident3 */
    mpp_writer_put_raw_bits(bits, 0x00, 8);
    /* Version */
    mpp_writer_put_raw_bits(bits, 0x0102, 16);

    if (syntax.density_x && syntax.density_y) {
        /* Units */
        mpp_writer_put_raw_bits(bits, syntax.units_type, 8);
        /* Xdensity */
        mpp_writer_put_raw_bits(bits, syntax.density_x, 16);
        /* Ydensity */
        mpp_writer_put_raw_bits(bits, syntax.density_y, 16);
    } else {
        /* Units */
        mpp_writer_put_raw_bits(bits, 0, 8);
        /* Xdensity */
        mpp_writer_put_raw_bits(bits, 0, 8);
        mpp_writer_put_raw_bits(bits, 1, 8);
        /* Ydensity */
        mpp_writer_put_raw_bits(bits, 1, 16);
    }
    /* XThumbnail */
    mpp_writer_put_raw_bits(bits, 0x00, 8);
    /* YThumbnail */
    mpp_writer_put_raw_bits(bits, 0x00, 8);
    /* Do NOT write thumbnail */
    size = mpp_writer_bytes(bits);
    kmpp_packet_set_length(pkt, size);
    task->length += size;

    return MPP_OK;
}

static MPP_RET jpege_proc_hal(void *ctx, HalEncTask *task)
{
    JpegeCtx *p = (JpegeCtx *)ctx;
    KmppFrame frame = task->frame;
    JpegeSyntax *syntax = &p->syntax;
    MppEncCfgSet *cfg = p->cfg;
    MppEncPrepCfg *prep = &cfg->prep;
    MppEncJpegCfg *jpeg = &cfg->jpeg;
    MppEncSliceSplit *split = &cfg->split;

    jpege_dbg_func("enter ctx %p\n", ctx);

    syntax->width       = prep->width;
    syntax->height      = prep->height;
    syntax->hor_stride  = prep->hor_stride;
    syntax->ver_stride  = prep->ver_stride;
    syntax->mcu_w       = MPP_ALIGN(prep->width, 16) / 16;
    syntax->mcu_h       = MPP_ALIGN(prep->height, 16) / 16;
    syntax->format      = prep->format;
    syntax->color       = prep->color;
    syntax->range       = prep->range;
    syntax->range_out   = prep->range_out;
    syntax->rotation    = prep->rotation;
    syntax->mirroring   = prep->mirroring;
    syntax->q_mode      = jpeg->q_mode;
    syntax->quant       = jpeg->quant;
    syntax->q_factor    = jpeg->q_factor;
    syntax->qf_min      = jpeg->qf_min;
    syntax->qf_max      = jpeg->qf_max;
    syntax->qtable_y    = jpeg->qtable_y;
    syntax->qtable_u    = jpeg->qtable_u;
    syntax->qtable_v    = jpeg->qtable_v;
    syntax->part_rows   = 0;
    syntax->restart_ri  = 0;
    syntax->low_delay   = 0;
    kmpp_frame_get_offset_y(frame, &syntax->offset_y);
    kmpp_frame_get_offset_x(frame, &syntax->offset_x);

    if (split->split_mode) {
        RK_U32 mb_h = MPP_ALIGN(prep->height, 16) / 16;
        RK_U32 part_rows = 0;

        if (split->split_mode == MPP_ENC_SPLIT_BY_CTU) {
            RK_U32 part_mbs = split->split_arg;
            RK_U32 mb_w = MPP_ALIGN(prep->width, 16) / 16;
            RK_U32 mb_all = mb_w * mb_h;

            if (part_mbs > 0 && part_mbs <= mb_all) {
                part_rows = (part_mbs + mb_w - 1) / mb_w;
                if (part_rows >= mb_h)
                    part_rows = 0;
            } else {
                kmpp_loge_f("warning: invalid split arg %d > max %d\n",
                            part_mbs, mb_all);
            }
        } else
            kmpp_loge_f("warning: only mcu split is supported\n");

        if (part_rows) {
            syntax->part_rows   = part_rows;
            syntax->restart_ri  = syntax->mcu_w * part_rows;
            syntax->low_delay   = cfg->base.low_delay && part_rows;
        }
    }

    task->valid = 1;
    task->syntax.data = syntax;
    task->syntax.number = 1;

    jpege_dbg_func("leave ctx %p\n", ctx);

    return MPP_OK;
}

static MPP_RET jpege_add_Prefix(KmppPacket pkt, RK_S32 *len, RK_U8 uuid[16],
                                const void *data, RK_S32 size)
{
    KmppShmPtr pos;
    RK_U8 *ptr = NULL;
    RK_S32 length;
    RK_S32 buf_size;
    MppWriteCtx bit_ctx;
    MppWriteCtx *bits = &bit_ctx;
    const RK_U8 *user_data = data;
    RK_S32 i = 0, app_size = 0;

    kmpp_packet_get_pos(pkt, &pos);
    ptr = pos.kptr;
    kmpp_packet_get_size(pkt, &buf_size);
    kmpp_packet_get_length(pkt, &length);

    mpp_writer_init(bits, ptr + length, buf_size - length);

    if ((size > 8) && user_data[0] == 0xFF && user_data[1] == 0xE1 &&
        user_data[4] == 0x45 && user_data[5] == 0x78 &&
        user_data[6] == 0x69 && user_data[7] == 0x66)
        jpege_dbg_func("write EXIF data, total length %d\n", size);
    else if ((size > 8) && user_data[0] == 0xFF && user_data[1] == 0xE2 &&
             user_data[4] == 0x4D && user_data[5] == 0x50 &&
             user_data[6] == 0x46 && user_data[7] == 0x00)
        jpege_dbg_func("write MPF data, total length %d\n", size);
    else {
        /* add user data to APP7 */
        mpp_writer_put_raw_bits(bits, 0xFFE7, 16);
        /* length */
        mpp_writer_put_raw_bits(bits, size + 2, 16);
    }

    /* data */
    for (i = 0; i < size; i++)
        mpp_writer_put_raw_bits(bits, user_data[i], 8);

    app_size = mpp_writer_bytes(bits);
    *len = app_size;
    length += app_size;
    kmpp_packet_set_length(pkt, length);
    (void)uuid;

    return MPP_OK;
}

static MPP_RET jpeg_init_cfg(MppEncCfgSet *cfg, const rk_u8 *preset)
{
    MppEncRcCfg *rc = &cfg->rc;

    (void)preset;
    /* init default rc config */
    rc->fps_in_flex = 0;
    rc->fps_in_num = 30;
    rc->fps_in_denom = 1;
    rc->fps_out_flex = 0;
    rc->fps_out_num = 30;
    rc->fps_out_denom = 1;
    rc->rc_mode = MPP_ENC_RC_MODE_FIXQP;

    /* set quant to invalid value */
    cfg->jpeg.quant = -1;

    return MPP_OK;
}

static void jpege_proc_show(KmppEnvNode node, void *ctx, RK_S32 chl_id)
{
    JpegeCtx *p = (JpegeCtx *)ctx;
    MppEncCfgSet *cfg = p->cfg;
    MppEncPrepCfg *prep = &cfg->prep;
    MppEncJpegCfg  *jpege = &cfg->jpeg;

    kmpp_env_log(node, "\n--------jpege chn attr----------------------------------------------------------------------------\n");
    kmpp_env_log(node, "%7s|%10s|%10s|%10s|%10s|%10s|%10s\n", "ID",
                 "Width", "Height", "quant", "qfactor", "qfmax", "qfmin");
    kmpp_env_log(node, "%7d|%10u|%10u|%10u|%10u|%10u|%10u\n", chl_id,
                 prep->width, prep->height, jpege->quant, jpege->q_factor, jpege->qf_max, jpege->qf_min);
}

const EncImplApi api_jpege = {
    .name       = "jpege_control",
    .coding     = MPP_VIDEO_CodingMJPEG,
    .ctx_size   = sizeof(JpegeCtx),
    .flag       = 0,
    .init       = jpege_init_v2,
    .deinit     = jpege_deinit_v2,
    .proc_cfg   = jpege_proc_cfg,
    .gen_hdr    = NULL,
    .start      = jpege_start,
    .proc_dpb   = NULL,
    .proc_hal   = jpege_proc_hal,
    .add_prefix = jpege_add_Prefix,
    .sw_enc     = NULL,
    .init_cfg   = jpeg_init_cfg,
    .proc_debug = jpege_proc_show,
};

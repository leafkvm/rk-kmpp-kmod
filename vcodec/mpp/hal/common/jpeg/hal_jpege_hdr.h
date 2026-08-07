/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#ifndef __HAL_JPEGE_HDR_H__
#define __HAL_JPEGE_HDR_H__

#include "mpp_enc_cfg.h"
#include "jpege_syntax.h"

typedef void *JpegeBits;

typedef struct HalJpegeRc_t {
    MppEncCfgSet    *cfg;

    RK_S32          q_mode;

    RK_S32          quant;

    RK_S32          q_factor;
    RK_S32          q_factor_prev;

    RK_U8           qtable_y[QUANTIZE_TABLE_SIZE];
    RK_U8           qtable_u[QUANTIZE_TABLE_SIZE];
    RK_U8           qtable_v[QUANTIZE_TABLE_SIZE];

    const RK_U8     *qtables[3];
} HalJpegeRc;

#ifdef __cplusplus
extern "C" {
#endif

void jpege_bits_init(JpegeBits *ctx);
void jpege_bits_deinit(JpegeBits ctx);

void jpege_bits_setup(JpegeBits ctx, RK_U8 *buf, RK_S32 size);
void jpege_bits_put(JpegeBits ctx, RK_U32 val, RK_S32 len);
void jpege_bits_align_byte(JpegeBits ctx);
void jpege_seek_bits(JpegeBits ctx, RK_S32 len);
RK_U8 *jpege_bits_get_buf(JpegeBits ctx);
RK_S32 jpege_bits_get_bitpos(JpegeBits ctx);
RK_S32 jpege_bits_get_bytepos(JpegeBits ctx);

MPP_RET write_jpeg_header(JpegeBits *bits, JpegeSyntax *syntax, HalJpegeRc *rc);

void jpege_rc_init(HalJpegeRc *rc, MppEncCfgSet *cfg);
void jpege_rc_update(HalJpegeRc *rc, JpegeSyntax *syntax);

#ifdef __cplusplus
}
#endif
#endif /*__HAL_JPEGE_HDR_H__*/

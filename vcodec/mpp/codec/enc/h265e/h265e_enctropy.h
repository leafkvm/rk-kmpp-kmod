/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 */

#ifndef __H265E_ENCTROPY_H__
#define __H265E_ENCTROPY_H__

#include "mpp_bitwrite.h"
typedef struct H265eContexModel_e {
    RK_U8 m_state;      ///< internal state variable
    RK_U8 bBinsCoded;
} H265eContexModel;

typedef struct H265eCabacCtx_t {
    MppWriteCtx * m_bitIf;
    RK_U32 m_low;
    RK_U32 m_range;
    RK_U32 m_bufferedByte;
    RK_S32 m_numBufferedBytes;
    RK_S32 m_bitsLeft;
    RK_U64 m_fracBits;
    RK_U8 m_bIsCounter;
} H265eCabacCtx;

#ifdef __cplusplus
extern "C" {

#endif  /*  */
void h265e_cabac_init(H265eCabacCtx *cabac_ctx, MppWriteCtx * bitIf);
void h265e_reset_enctropy(void *slice_ctx);
void h265e_cabac_resetBits(H265eCabacCtx *cabac_ctx);
void h265e_cabac_encodeBin(H265eCabacCtx *cabac_ctx, H265eContexModel * ctxModel,
                           RK_U32 binValue);
void h265e_cabac_encodeBinTrm(H265eCabacCtx *cabac_ctx, RK_U32 binValue);
void h265e_cabac_start(H265eCabacCtx *cabac_ctx);
void h265e_cabac_finish(H265eCabacCtx *cabac_ctx);
void h265e_cabac_flush(H265eCabacCtx *cabac_ctx);

#ifdef __cplusplus
}
#endif  /*  */

#endif  /*  */

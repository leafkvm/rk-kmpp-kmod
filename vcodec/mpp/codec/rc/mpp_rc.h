/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2016 Rockchip Electronics Co., Ltd.
 */

#ifndef __MPP_RC__
#define __MPP_RC__

#include "rk_venc_cmd.h"

#include "kmpp_log.h"

/*
 * mpp rate control contain common caculation methd
 *
 * 1. MppData - data statistic struct
 *    size - max valid data number
 *    len  - valid data number
 *    pos  - current load/store position
 *    val  - buffer array pointer
 */
typedef struct {
    RK_S32 size;
    RK_S32 len;
    RK_S32 pos;
    RK_S32 *val;
} MppData;

/*
 * 2. Proportion Integration Differentiation (PID) control
 */
typedef struct {
    RK_S32 p;
    RK_S32 i;
    RK_S32 d;
    RK_S32 coef_p;
    RK_S32 coef_i;
    RK_S32 coef_d;
    RK_S32 div;
    RK_S32 len;
    RK_S32 count;
} MppPIDCtx;

/*
 * MppRateControl has three steps work:
 *
 * 1. translate user requirement to bit rate parameters
 * 2. calculate target bit from bit parameters
 * 3. calculate qstep from target bit
 *
 * That is user setting -> target bit -> qstep.
 *
 * This struct will be used in both controller and hal.
 * EncImpl provide step 1 and step 2. Hal provide step 3.
 *
 */
typedef enum MppEncGopMode_e {
    /* gop == 0 */
    MPP_GOP_ALL_INTER,
    /* gop == 1 */
    MPP_GOP_ALL_INTRA,
    /* gop < fps */
    MPP_GOP_SMALL,
    /* gop >= fps */
    MPP_GOP_LARGE,
    MPP_GOP_MODE_BUTT,
} MppEncGopMode;

typedef enum RC_PARAM_OPS {
    RC_RECORD_REAL_BITS,
    RC_RECORD_QP_SUM,
    RC_RECORD_QP_MIN,
    RC_RECORD_QP_MAX,
    RC_RECORD_SET_QP,
    RC_RECORD_REAL_QP,
    RC_RECORD_SSE_SUM,
    RC_RECORD_WIN_LEN
} RC_PARAM_OPS;

#ifdef __cplusplus
extern "C" {
#endif

MPP_RET mpp_data_init(MppData ** p, RK_S32 len);
void mpp_data_deinit(MppData * p);
void mpp_data_update(MppData * p, RK_S32 val);
RK_S32 mpp_data_avg(MppData * p, RK_S32 len, RK_S32 num, RK_S32 denorm);

void mpp_pid_reset(MppPIDCtx * p);
void mpp_pid_set_param(MppPIDCtx * p, RK_S32 coef_p, RK_S32 coef_i,
                       RK_S32 coef_d, RK_S32 div, RK_S32 len);
void mpp_pid_update(MppPIDCtx * p, RK_S32 val, RK_S32 reset);
RK_S32 mpp_pid_calc(MppPIDCtx * ctx);

#ifdef __cplusplus
}
#endif

#endif /* __MPP_RC__ */

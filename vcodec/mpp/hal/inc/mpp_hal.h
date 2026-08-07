/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2015 Rockchip Electronics Co., Ltd.
 */

#ifndef __MPP_HAL_H__
#define __MPP_HAL_H__

#include "rk_mpi_cmd.h"
#include "hal_task.h"
#include "mpp_enc_cfg.h"
#include "mpp_device.h"

typedef enum VpuHwMode_e {
    MODE_NULL = 0,
    RKVDEC_MODE = 0x01,
    VDPU1_MODE = 0x02,
    VDPU2_MODE = 0x04,
    RKVENC_MODE = 0x05,
    MODE_BUTT,
} VpuHwMode;

typedef struct MppHalCfg_t {
    // input
    MppCtxType type;
    MppCodingType coding;
    MppDev dev;
} MppHalCfg;

typedef struct MppHalApi_t {
    char *name;
    MppCtxType type;
    MppCodingType coding;
    RK_U32 ctx_size;
    RK_U32 flag;

    MPP_RET(*init) (void *ctx, MppHalCfg * cfg);
    MPP_RET(*deinit) (void *ctx);

    // task preprocess function
    MPP_RET(*reg_gen) (void *ctx, HalTaskInfo * syn);

    // hw operation function
    MPP_RET(*start) (void *ctx, HalTaskInfo * task);
    MPP_RET(*wait) (void *ctx, HalTaskInfo * task);

    MPP_RET(*reset) (void *ctx);
    MPP_RET(*flush) (void *ctx);
    MPP_RET(*control) (void *ctx, MpiCmd cmd, void *param);
} MppHalApi;

typedef void *MppHal;

#ifdef __cplusplus
extern "C" {
#endif

MPP_RET mpp_hal_init(MppHal * ctx, MppHalCfg * cfg);
MPP_RET mpp_hal_deinit(MppHal ctx);

MPP_RET mpp_hal_reg_gen(MppHal ctx, HalTaskInfo * task);
MPP_RET mpp_hal_hw_start(MppHal ctx, HalTaskInfo * task);
MPP_RET mpp_hal_hw_wait(MppHal ctx, HalTaskInfo * task);

MPP_RET mpp_hal_reset(MppHal ctx);
MPP_RET mpp_hal_flush(MppHal ctx);
MPP_RET mpp_hal_control(MppHal ctx, MpiCmd cmd, void *param);

#ifdef __cplusplus
}
#endif

#endif /*__MPP_HAL_H__*/

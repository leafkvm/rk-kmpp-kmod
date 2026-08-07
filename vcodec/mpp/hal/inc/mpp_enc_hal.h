/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#ifndef __MPP_ENC_HAL_H__
#define __MPP_ENC_HAL_H__

#include "hal_task.h"
#include "mpp_enc_cfg.h"
#include "mpp_device.h"
#include "kmpp_session.h"

typedef struct MppEncHalCfg_t {
    // input for encoder
    MppCodingType coding;
    MppEncCfgSet *cfg;
    RK_S32 chan_id;
    RK_S32 online;
    KmppSessionHook hook;
    RK_U32 ref_buf_shared;
    RK_U32 only_smartp;
    MppClientType type;
    MppDev dev;

    struct hal_shared_buf *shared_buf;
} MppEncHalCfg;

typedef struct MppEncHalApi_t {
    char *name;
    MppCodingType coding;
    MppClientType type;
    RK_U32 ctx_size;
    RK_U32 flag;

    MPP_RET(*init) (void *ctx, MppEncHalCfg *cfg);
    MPP_RET(*deinit) (void *ctx);

    MPP_RET(*check_cfg) (void *ctx, MppEncCfgSet *cfg);

    // prepare function after encoder config is set
    MPP_RET(*prepare) (void *ctx);

    // configure function
    MPP_RET(*get_task) (void *ctx, HalEncTask *task);
    MPP_RET(*gen_regs) (void *ctx, HalEncTask *task);

    // hw operation function
    MPP_RET(*start) (void *ctx, HalEncTask *task);
    MPP_RET(*wait) (void *ctx, HalEncTask *task);
    MPP_RET(*part_start) (void *ctx, HalEncTask *task);
    MPP_RET(*part_wait) (void *ctx, HalEncTask *task);

    // return function
    MPP_RET(*ret_task) (void *ctx, HalEncTask *task);

    MPP_RET(*comb_start) (void *ctx, HalEncTask *task, HalEncTask *jpeg_task);
    MPP_RET(*comb_ret_task) (void *ctx, HalEncTask *task, HalEncTask *jpeg_task);
    MPP_RET(*init_cfg)(void *ctx, MppEncCfgSet *cfg, const rk_u8 *preset);
} MppEncHalApi;

typedef void *MppEncHal;

#ifdef __cplusplus
extern "C" {
#endif

MPP_RET mpp_enc_hal_init(MppEncHal * ctx, MppEncHalCfg *cfg);
MPP_RET mpp_enc_hal_deinit(MppEncHal ctx);

/* prepare after cfg */
MPP_RET mpp_enc_hal_prepare(MppEncHal ctx);

MPP_RET mpp_enc_hal_get_task(MppEncHal ctx, HalEncTask *task);
MPP_RET mpp_enc_hal_gen_regs(MppEncHal ctx, HalEncTask *task);

// start / wait hardware
MPP_RET mpp_enc_hal_start(MppEncHal ctx, HalEncTask *task, HalEncTask *jpeg_task);
MPP_RET mpp_enc_hal_wait(MppEncHal ctx, HalEncTask *task);
MPP_RET mpp_enc_hal_part_start(MppEncHal ctx, HalEncTask *task);
MPP_RET mpp_enc_hal_part_wait(MppEncHal ctx, HalEncTask *task);

MPP_RET mpp_enc_hal_ret_task(MppEncHal ctx, HalEncTask *task, HalEncTask *jpeg_task);
MPP_RET mpp_enc_hal_init_cfg(void *hal, MppEncCfgSet *cfg, const rk_u8 *preset);
MPP_RET mpp_enc_hal_check_part_mode(MppEncHal ctx);

#ifdef __cplusplus
}
#endif
#endif /*__MPP_ENC_HAL_H__*/

/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */


#define MODULE_TAG "hal_jpege_api_v2"

#include "kmpp_string.h"
#include "mpp_mem.h"
//#include "mpp_platform.h"

#include "mpp_enc_hal.h"
#include "vepu5xx_common.h"
#include "hal_jpege_debug.h"
#include "hal_jpege_api_v2.h"

#ifdef RKVEPU540C_JPEG
#include "hal_jpege_vepu540c.h"
#endif

#ifdef RKVEPU500_H264
#include "hal_jpege_vepu500.h"
#endif

#ifdef RKVEPU510_JPEG
#include "hal_jpege_vpu720.h"
#endif

#ifdef RKVEPU511_JPEG
#include "hal_jpege_vepu511.h"
#endif
typedef struct HaljpegeCtx_t {
    const MppEncHalApi *api;
    void *hw_ctx;
} HaljpegeCtx;

RK_U32 hal_jpege_debug = 0;

static MPP_RET hal_jpege_init(void *hal, MppEncHalCfg *cfg)
{
    HaljpegeCtx *ctx = (HaljpegeCtx *) hal;
    const MppEncHalApi *api = NULL;
    MppDevCfg dev_cfg = {0};
    void *hw_ctx = NULL;
    MPP_RET ret = MPP_OK;

#ifdef RKVEPU540C_JPEG
    api = &hal_jpege_vepu540c;
#endif
#ifdef RKVEPU500_JPEG
    api = &hal_jpege_vepu500;
#endif
#ifdef RKVEPU510_JPEG
    api = &hal_jpege_vpu720;
#endif
#ifdef RKVEPU511_JPEG
    api = &hal_jpege_vepu511;
#endif

    kmpp_assert(api);

    hw_ctx = mpp_calloc_size(void, api->ctx_size);
    if (!hw_ctx)
        return MPP_ERR_MALLOC;

    ctx->api = api;
    ctx->hw_ctx = hw_ctx;
    cfg->type = api->type;

    dev_cfg.type = cfg->type;
    dev_cfg.chan_id = cfg->chan_id;
    dev_cfg.online = cfg->online;
    dev_cfg.hook = cfg->hook;

    ret = mpp_dev_init(&cfg->dev, &dev_cfg);
    if (ret) {
        kmpp_loge_f("mpp_dev_init type %d failed. ret: %d\n", cfg->type, ret);
        return ret;
    }

    return api->init(hw_ctx, cfg);
}

static MPP_RET hal_jpege_deinit(void *hal)
{
    HaljpegeCtx *ctx = (HaljpegeCtx *) hal;
    const MppEncHalApi *api = ctx->api;
    void *hw_ctx = ctx->hw_ctx;
    MPP_RET ret = MPP_OK;

    if (!hw_ctx || !api || !api->deinit)
        return MPP_OK;

    ret = api->deinit(hw_ctx);
    MPP_FREE(hw_ctx);

    return ret;
}

static MPP_RET hal_jpege_check_cfg(void *hal, MppEncCfgSet *cfg)
{
    HaljpegeCtx *ctx = (HaljpegeCtx *) hal;
    const MppEncHalApi *api = ctx->api;
    void *hw_ctx = ctx->hw_ctx;

    if (!hw_ctx || !api || !api->check_cfg)
        return MPP_OK;

    return api->check_cfg(hw_ctx, cfg);
}

#define HAL_JPEGE_FUNC(func) \
    static MPP_RET hal_jpege_##func(void *hal)                      \
    {                                                               \
        HaljpegeCtx *ctx = (HaljpegeCtx *)hal;                      \
        const MppEncHalApi *api = ctx->api;                         \
        void *hw_ctx = ctx->hw_ctx;                                 \
                                                                    \
        if (!hw_ctx || !api || !api->func)                          \
            return MPP_OK;                                          \
                                                                    \
        return api->func(hw_ctx);                                   \
    }

#define HAL_JPEGE_TASK_FUNC(func) \
    static MPP_RET hal_jpege_##func(void *hal, HalEncTask *task)    \
    {                                                               \
        HaljpegeCtx *ctx = (HaljpegeCtx *)hal;                      \
        const MppEncHalApi *api = ctx->api;                         \
        void *hw_ctx = ctx->hw_ctx;                                 \
                                                                    \
        if (!hw_ctx || !api || !api->func)                          \
            return MPP_OK;                                          \
                                                                    \
        return api->func(hw_ctx, task);                             \
    }

HAL_JPEGE_FUNC(prepare)
HAL_JPEGE_TASK_FUNC(get_task)
HAL_JPEGE_TASK_FUNC(gen_regs)
HAL_JPEGE_TASK_FUNC(start)
HAL_JPEGE_TASK_FUNC(wait)
HAL_JPEGE_TASK_FUNC(part_start)
HAL_JPEGE_TASK_FUNC(part_wait)
HAL_JPEGE_TASK_FUNC(ret_task)

const MppEncHalApi hal_api_jpege_v2 = {
    .name = "hal_jpege",
    .coding = MPP_VIDEO_CodingMJPEG,
    .type = VPU_CLIENT_BUTT,
    .ctx_size = sizeof(HaljpegeCtx),
    .flag = 0,
    .init = hal_jpege_init,
    .deinit = hal_jpege_deinit,
    .check_cfg = hal_jpege_check_cfg,
    .prepare = hal_jpege_prepare,
    .get_task = hal_jpege_get_task,
    .gen_regs = hal_jpege_gen_regs,
    .start = hal_jpege_start,
    .wait = hal_jpege_wait,
    .part_start = hal_jpege_part_start,
    .part_wait = hal_jpege_part_wait,
    .ret_task = hal_jpege_ret_task,
    .init_cfg = NULL,
};

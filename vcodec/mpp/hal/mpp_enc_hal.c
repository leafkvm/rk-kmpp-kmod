/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#define  MODULE_TAG "mpp_enc_hal"

#include "mpp_mem.h"
#include "kmpp_log.h"
#include "mpp_maths.h"

#include "mpp_enc_hal.h"
#include "hal_h265e_api_v2.h"
#include "hal_h264e_api_v2.h"
#include "hal_jpege_api_v2.h"

static const MppEncHalApi *hw_enc_apis[] = {
#ifdef HAVE_H265E
    &hal_api_h265e_v2,
#endif
#ifdef HAVE_H264E
    &hal_api_h264e_v2,
#endif
#ifdef HAVE_JPEGE
    &hal_api_jpege_v2,
#endif
};

typedef struct MppEncHalImpl_t {
    MppCodingType coding;

    void *ctx;
    const MppEncHalApi *api;

    HalTaskGroup tasks;
    RK_S32 task_count;
} MppEncHalImpl;

MPP_RET mpp_enc_hal_init(MppEncHal * ctx, MppEncHalCfg *cfg)
{
    MppEncHalImpl *p = NULL;
    RK_U32 i = 0;

    if (!ctx || !cfg) {
        kmpp_loge_f("found NULL input ctx %p cfg %p\n", ctx, cfg);
        return MPP_ERR_NULL_PTR;
    }
    *ctx = NULL;

    p = mpp_calloc(MppEncHalImpl, 1);
    if (!p) {
        kmpp_loge_f("malloc failed\n");
        return MPP_ERR_MALLOC;
    }

    for (i = 0; i < MPP_ARRAY_ELEMS(hw_enc_apis); i++) {
        if (cfg->coding == hw_enc_apis[i]->coding) {
            MPP_RET ret = MPP_OK;

            p->coding = cfg->coding;
            p->api = hw_enc_apis[i];
            p->ctx = mpp_calloc_size(void, p->api->ctx_size);

            ret = p->api->init(p->ctx, cfg);
            if (ret) {
                kmpp_loge_f("hal %s init failed ret %d\n",
                            hw_enc_apis[i]->name, ret);
                break;
            }
            *ctx = p;
            return MPP_OK;
        }
    }

    kmpp_loge_f("could not found coding type %d\n", cfg->coding);
    mpp_free(p->ctx);
    mpp_free(p);

    return MPP_NOK;
}

MPP_RET mpp_enc_hal_deinit(MppEncHal ctx)
{
    MppEncHalImpl *p = (MppEncHalImpl *)ctx;

    if (!ctx) {
        kmpp_loge_f("found NULL input\n");
        return MPP_ERR_NULL_PTR;
    }

    p->api->deinit(p->ctx);
    mpp_free(p->ctx);
#if 0
    if (p->tasks)
        hal_task_group_deinit(p->tasks);
#endif
    mpp_free(p);
    return MPP_OK;
}

MPP_RET mpp_enc_hal_prepare(void *hal)
{
    MppEncHalImpl *p = (MppEncHalImpl *)hal;

    if (!hal) {
        kmpp_loge_f("found NULL input ctx %p\n", hal);
        return MPP_ERR_NULL_PTR;
    }

    if (!p->api || !p->api->prepare)
        return MPP_OK;

    return p->api->prepare(p->ctx);
}

MPP_RET mpp_enc_hal_check_part_mode(MppEncHal ctx)
{
    MppEncHalImpl *p = (MppEncHalImpl *) ctx;

    if (p && p->api && p->api->part_start && p->api->part_wait)
        return MPP_OK;

    return MPP_NOK;
}

MPP_RET mpp_enc_hal_start(void *hal, HalEncTask *task, HalEncTask *jpeg_task)
{
    MppEncHalImpl *p = (MppEncHalImpl *)hal;

    if (!p || !task) {
        kmpp_loge_f("found NULL input ctx %p task %p\n", hal, task);
        return MPP_ERR_NULL_PTR;
    }

    if (!p->api || !p->api->start)
        return MPP_OK;

    if (jpeg_task && p->api->comb_start)
        return p->api->comb_start(p->ctx, task, jpeg_task);

    return p->api->start(p->ctx, task);
}

MPP_RET mpp_enc_hal_ret_task(void *hal, HalEncTask *task, HalEncTask *jpeg_task)
{
    MppEncHalImpl *p = (MppEncHalImpl *)hal;

    if (!p || !task) {
        kmpp_loge_f("found NULL input ctx %p task %p\n", hal, task);
        return MPP_ERR_NULL_PTR;
    }

    if (!p->api || !p->api->ret_task)
        return MPP_OK;

    if (jpeg_task && p->api->comb_ret_task)
        return p->api->comb_ret_task(p->ctx, task, jpeg_task);

    return p->api->ret_task(p->ctx, task);
}

MPP_RET mpp_enc_hal_init_cfg(void *hal, MppEncCfgSet *cfg, const rk_u8 *preset)
{
    MppEncHalImpl *p = (MppEncHalImpl *)hal;

    if (!p) {
        kmpp_loge_f("found NULL input ctx %p\n", hal);
        return MPP_ERR_NULL_PTR;
    }

    if (!p->api || !p->api->init_cfg)
        return MPP_OK;

    return p->api->init_cfg(p->ctx, cfg, preset);
}

#define MPP_ENC_HAL_TASK_FUNC(func) \
    MPP_RET mpp_enc_hal_##func(void *hal, HalEncTask *task)                 \
    {                                                                       \
        MppEncHalImpl *p = (MppEncHalImpl*)hal;                             \
                                                                            \
        if (!hal || !task) {                                                \
            kmpp_loge_f("found NULL input ctx %p task %p\n", hal, task);    \
            return MPP_ERR_NULL_PTR;                                        \
        }                                                                   \
                                                                            \
        if (!p->api || !p->api->func)                                       \
            return MPP_OK;                                                  \
                                                                            \
        return p->api->func(p->ctx, task);                                  \
    }

MPP_ENC_HAL_TASK_FUNC(get_task)
MPP_ENC_HAL_TASK_FUNC(gen_regs)
MPP_ENC_HAL_TASK_FUNC(wait)
MPP_ENC_HAL_TASK_FUNC(part_start)
MPP_ENC_HAL_TASK_FUNC(part_wait)

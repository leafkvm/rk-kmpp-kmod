/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __VDEC_HAL_API_H__
#define __VDEC_HAL_API_H__

#include "kmpp_list.h"

#include "vdec_hal.h"

typedef struct VdecHalApi_t {
    kmpp_list_head      list;

    const rk_u8         *name;
    MppCodingType       coding;
    rk_u32              ctx_size;
    rk_u64              flag;

    rk_s32 (*init)(void *ctx, VdecHalCfg *cfg);
    rk_s32 (*deinit)(void *ctx);
    rk_s32 (*reset)(void *ctx);
    rk_s32 (*start)(void *ctx);
    rk_s32 (*stop)(void *ctx);

    rk_s32 (*get_task)(void *ctx, VdecHalTask **task);
    rk_s32 (*put_task)(void *ctx, VdecHalTask *task);

    rk_s32 (*update_task)(void *ctx, VdecHalTask *task);
    rk_s32 (*poll_task)(void *ctx, VdecHalTask *out);
} VdecHalApi;

rk_s32 vdec_hal_add(VdecHalApi *api);
rk_s32 vdec_hal_del(VdecHalApi *api);

#endif /* __VDEC_HAL_API_H__ */

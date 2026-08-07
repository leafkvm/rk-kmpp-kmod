/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_hal"

#include "kmpp_osal.h"
#include "kmpp_module.h"
#include "kmpp_mod_id.h"

#include "kmpp_service.h"
#include "kmpp_session.h"

static rk_s32 kmpp_hal_init(void)
{
    rk_s32 ret = rk_ok;

    ret |= kmpp_session_objdef_get();
    ret |= kmpp_service_init();

    return ret;
}

static rk_s32 kmpp_hal_deinit(void)
{
    rk_s32 ret = rk_ok;

    ret |= kmpp_service_deinit();
    ret |= kmpp_session_objdef_put();

    return ret;
}

KMPP_MODULE_ADD(kmpp, KMPP_MOD_HAL, hal, kmpp_hal_init, kmpp_hal_deinit)

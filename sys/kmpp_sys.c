/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_sys"

#include "kmpp_version.h"

#include "kmpp_osal.h"

#include "kmpp_sys.h"
#include "kmpp_sys_mod.h"

KMPP_MODULE(kmpp_sys);

int kmpp_sys_init(void)
{
    kmpp_logi("sys init\n");

    KMPP_MODULE_INIT(kmpp_sys);

    return 0;
}

void kmpp_sys_exit(void)
{
    KMPP_MODULE_DEINIT(kmpp_sys);

    kmpp_logi("sys exit\n");
}

#ifdef BUILD_MULTI_KO
#include <linux/module.h>

module_init(kmpp_sys_init);
module_exit(kmpp_sys_exit);

MODULE_AUTHOR("rockchip");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_VERSION("1.0");
#endif

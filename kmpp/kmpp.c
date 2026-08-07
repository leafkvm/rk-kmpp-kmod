/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp"

#include "kmpp_version.h"

#include "kmpp_osal.h"
#include "kmpp_sys.h"
#include "kmpp_mod.h"

KMPP_MODULE(kmpp);

extern int mpp_service_register(void);
extern void mpp_service_unregister(void);
extern int vcodec_init(void);
extern void vcodec_exit(void);

int kmpp_init(void)
{
#ifdef BUILD_ONE_KO
    kmpp_osal_init();
    kmpp_sys_init();
#endif
    kmpp_logi("kmpp init\n");

    KMPP_MODULE_INIT(kmpp);

#ifndef CONFIG_ROCKCHIP_MPP_SERVICE
    mpp_service_register();
#endif
    vcodec_init();

    return 0;
}

void kmpp_exit(void)
{
#ifndef CONFIG_ROCKCHIP_MPP_SERVICE
    mpp_service_unregister();
#endif
    vcodec_exit();

    KMPP_MODULE_DEINIT(kmpp);

    kmpp_logi("kmpp exit\n");

#ifdef BUILD_ONE_KO
    kmpp_sys_exit();
    kmpp_osal_exit();
#endif
}

#include <linux/module.h>

module_init(kmpp_init);
module_exit(kmpp_exit);

MODULE_AUTHOR("rockchip");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_VERSION("1.0");

MODULE_IMPORT_NS(DMA_BUF);
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);

/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_osal"

#include "kmpp_log.h"
#include "kmpp_osal_mod.h"

KMPP_MODULE(kmpp_osal);

int kmpp_osal_init(void)
{
    kmpp_logi("osal init\n");

    KMPP_MODULE_INIT(kmpp_osal);

    return 0;
}

void kmpp_osal_exit(void)
{
    KMPP_MODULE_DEINIT(kmpp_osal);

    kmpp_logi("osal exit\n");
}

#ifdef BUILD_MULTI_KO
#include <linux/module.h>

module_init(kmpp_osal_init);
module_exit(kmpp_osal_exit);

MODULE_AUTHOR("rockchip");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_VERSION("1.0");
MODULE_IMPORT_NS(DMA_BUF);
#endif

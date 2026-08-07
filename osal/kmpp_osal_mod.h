/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_OSAL_MOD_H__
#define __KMPP_OSAL_MOD_H__

#include "kmpp_module.h"

#define KMPP_OSAL_ENTRY_TABLE(mod, ENTRY) \
    ENTRY(mod, mem) \
    ENTRY(mod, env) \
    ENTRY(mod, version) \
    ENTRY(mod, class) \
    ENTRY(mod, dmaheap) \
    ENTRY(mod, log) \
    ENTRY(mod, plat)

KMPP_MODULE_ARRAYS(kmpp_osal, KMPP_OSAL_ENTRY_TABLE)

#else
#error "Multiple inclusion of kmpp_osal_mod.h"
#endif /* __KMPP_OSAL_MOD_H__ */

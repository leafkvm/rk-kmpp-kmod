/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_SYS_MOD_H__
#define __KMPP_SYS_MOD_H__

#include "kmpp_module.h"

#define KMPP_SYS_ENTRY_TABLE(mod, ENTRY) \
    ENTRY(mod, env) \
    ENTRY(mod, mempool) \
    ENTRY(mod, obj) \
    ENTRY(mod, ioctl) \
    ENTRY(mod, sym) \
    ENTRY(mod, shm)

KMPP_MODULE_ARRAYS(kmpp_sys, KMPP_SYS_ENTRY_TABLE)

#else
#error "Multiple inclusion of kmpp_sys_mod.h"
#endif /* __KMPP_SYS_MOD_H__ */

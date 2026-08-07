/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_MOD_H__
#define __KMPP_MOD_H__

#include "kmpp_module.h"

#define KMPP_ENTRY_TABLE(mod, ENTRY) \
    ENTRY(mod, buffer) \
    ENTRY(mod, meta) \
    ENTRY(mod, frame) \
    ENTRY(mod, packet) \
    ENTRY(mod, venc_objs) \
    ENTRY(mod, venc) \
    ENTRY(mod, vdec_objs) \
    ENTRY(mod, vdec) \
    ENTRY(mod, vsp_objs) \
    ENTRY(mod, rc_api) \
    ENTRY(mod, hal)

KMPP_MODULE_ARRAYS(kmpp, KMPP_ENTRY_TABLE)

#else
#error "Multiple inclusion of kmpp_mod.h"
#endif /* __KMPP_MOD_H__ */

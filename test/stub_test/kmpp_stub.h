/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_STUB_H__
#define __KMPP_STUB_H__

#include <linux/export.h>

#define STUB_FUNC_RET_ZERO(name) \
    { return 0; } \
    EXPORT_SYMBOL(name)

#define STUB_FUNC_RET_NULL(name) \
    { return NULL; } \
    EXPORT_SYMBOL(name)

#define STUB_FUNC_RET_VOID(name) \
    { } \
    EXPORT_SYMBOL(name)

#endif /* __KMPP_STUB_H__ */
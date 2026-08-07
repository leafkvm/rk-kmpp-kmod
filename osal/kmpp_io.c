/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/io.h>

#include "kmpp_io.h"

void kmpp_writel(rk_u32 val, void *p)
{
    writel(val, p);
}
EXPORT_SYMBOL(kmpp_writel);

rk_u32 kmpp_readl(void *p)
{
    return readl(p);
}
EXPORT_SYMBOL(kmpp_readl);

void kmpp_writel_relaxed(rk_u32 val, void *p)
{
    writel_relaxed(val, p);
}
EXPORT_SYMBOL(kmpp_writel_relaxed);

rk_u32 kmpp_readl_relaxed(void *p)
{
    return readl_relaxed(p);
}
EXPORT_SYMBOL(kmpp_readl_relaxed);
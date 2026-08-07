/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/delay.h>

#include "kmpp_delay.h"

void kmpp_udelay(rk_u32 us)
{
    udelay(us);
}
EXPORT_SYMBOL(kmpp_udelay);

void kmpp_mdelay(rk_u32 ms)
{
    mdelay(ms);
}
EXPORT_SYMBOL(kmpp_mdelay);

void kmpp_msleep(rk_u32 ms)
{
    msleep(ms);
}
EXPORT_SYMBOL(kmpp_msleep);

void kmpp_ssleep(rk_u32 sec)
{
    ssleep(sec);
}
EXPORT_SYMBOL(kmpp_ssleep);

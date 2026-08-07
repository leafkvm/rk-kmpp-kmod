/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_time"

#include <linux/ktime.h>

#include "kmpp_log.h"
#include "kmpp_time.h"

rk_s64 kmpp_time(void)
{
    return ktime_to_ns(ktime_get());
}
EXPORT_SYMBOL(kmpp_time);

rk_s64 kmpp_time_us(void)
{
    return ktime_to_us(ktime_get());
}
EXPORT_SYMBOL(kmpp_time_us);

rk_s64 kmpp_time_ms(void)
{
    return ktime_to_ms(ktime_get());
}
EXPORT_SYMBOL(kmpp_time_ms);

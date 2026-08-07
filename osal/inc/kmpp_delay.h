/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_DELAY_H__
#define __KMPP_DELAY_H__

#include "rk_type.h"

void kmpp_udelay(rk_u32 us);
void kmpp_mdelay(rk_u32 ms);
void kmpp_msleep(rk_u32 ms);
void kmpp_ssleep(rk_u32 sec);

#endif /* __KMPP_STRING_H__ */
/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_WAIT_IMPL_H__
#define __KMPP_WAIT_IMPL_H__

#include "rk_type.h"

struct KmppWait_t {
    rk_u32 need_free;
    void *wait;
};

#endif /* __KMPP_WAIT_IMPL_H__ */

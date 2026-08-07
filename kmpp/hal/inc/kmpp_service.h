/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_SERVICE_H__
#define __KMPP_SERVICE_H__

#include "rk_type.h"

typedef struct KmppSrvProp_t {
    rk_u32 taskqueue_cnt;
    rk_u32 reset_group_cnt;
} KmppSrvProp;

typedef struct KmppDevProp_t {
    rk_u32 task_capacity;
    rk_u32 taskqueue_id;
    rk_u32 reset_group_id;
} KmppDevProp;

rk_s32 kmpp_service_init(void);
rk_s32 kmpp_service_deinit(void);

#endif /* __KMPP_SERVICE_H__ */

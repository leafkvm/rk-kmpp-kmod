/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_SYS_H__
#define __KMPP_SYS_H__

#include "kmpp_env.h"
#include "kmpp_mem_pool.h"
#include "kmpp_obj.h"
#include "kmpp_sym.h"
#include "kmpp_shm.h"

rk_s32 kmpp_sys_init(void);
void kmpp_sys_exit(void);
KmppEnvGrp sys_get_env(void);

#endif /* __KMPP_SYS_H__ */

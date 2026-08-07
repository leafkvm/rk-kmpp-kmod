/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_SEMA_H__
#define __KMPP_SEMA_H__

#include "rk_type.h"

typedef struct KmppSemaphore_t KmppSemaphore;

rk_s32 kmpp_sema_init(KmppSemaphore **sem, rk_s32 val);
void   kmpp_sema_deinit(KmppSemaphore **sem);

rk_s32 kmpp_sema_size(void);
rk_s32 kmpp_sema_assign(KmppSemaphore **sem, rk_s32 val, void *buf, rk_s32 size);

rk_s32 kmpp_down(KmppSemaphore *sem);
rk_s32 kmpp_down_interruptible(KmppSemaphore *sem);
rk_s32 kmpp_down_trylock(KmppSemaphore *sem);
void   kmpp_up(KmppSemaphore *sem);

#endif /* __KMPP_SEMA_H__ */

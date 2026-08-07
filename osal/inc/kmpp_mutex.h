/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_MUTEX_H__
#define __KMPP_MUTEX_H__

#include "rk_type.h"

typedef struct KmppMutex_t KmppMutex;

rk_s32 kmpp_mutex_init(KmppMutex **mutex);
void kmpp_mutex_deinit(KmppMutex **mutex);

rk_s32 kmpp_mutex_size(void);
rk_s32 kmpp_mutex_assign(KmppMutex **mutex, void *buf, rk_s32 size);

rk_s32 kmpp_mutex_lock(KmppMutex *mutex);
rk_s32 kmpp_mutex_lock_interruptible(KmppMutex *mutex);
rk_s32 kmpp_mutex_trylock(KmppMutex *mutex);
void kmpp_mutex_unlock(KmppMutex *mutex);

#endif /* __KMPP_MUTEX_H__ */

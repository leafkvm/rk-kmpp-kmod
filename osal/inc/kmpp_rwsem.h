/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_RWSEM_H__
#define __KMPP_RWSEM_H__

#include "rk_type.h"

typedef struct KmppRwsemaphore_t KmppRwsemaphore;

rk_s32 kmpp_rwsem_init(KmppRwsemaphore **rwsem);
void   kmpp_rwsem_deinit(KmppRwsemaphore **rwsem);
rk_s32 kmpp_rwsem_size(void);
rk_s32 kmpp_rwsem_assign(KmppRwsemaphore **rwsem, void *buf, rk_s32 size);

void   kmpp_down_read(KmppRwsemaphore *rwsem);
rk_s32 kmpp_down_read_interruptible(KmppRwsemaphore *rwsem);
rk_s32 kmpp_down_read_killable(KmppRwsemaphore *rwsem);
rk_s32 kmpp_down_read_trylock(KmppRwsemaphore *rwsem);

void   kmpp_down_write(KmppRwsemaphore *rwsem);
rk_s32 kmpp_down_write_killable(KmppRwsemaphore *rwsem);
rk_s32 kmpp_down_write_trylock(KmppRwsemaphore *rwsem);

void   kmpp_up_read(KmppRwsemaphore *rwsem);
void   kmpp_up_write(KmppRwsemaphore *rwsem);

void   kmpp_downgrade_write(KmppRwsemaphore *rwsem);
rk_s32 kmpp_rwsem_is_locked(KmppRwsemaphore *rwsem);

#endif /* __KMPP_RWSEM_H__ */

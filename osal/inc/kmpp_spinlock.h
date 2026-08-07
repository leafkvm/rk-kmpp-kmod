/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_SPINLOCK_H__
#define __KMPP_SPINLOCK_H__

#include "rk_type.h"

typedef struct KmppSpinlock_t KmppSpinlock;

rk_s32 kmpp_spinlock_init(KmppSpinlock **lock);
void   kmpp_spinlock_deinit(KmppSpinlock **lock);

rk_s32 kmpp_spinlock_size(void);
rk_s32 kmpp_spinlock_assign(KmppSpinlock **lock, void *buf, rk_s32 size);

void   kmpp_spin_lock(KmppSpinlock *lock);
rk_s32 kmpp_spin_trylock(KmppSpinlock *lock);
void   kmpp_spin_unlock(KmppSpinlock *lock);

void   kmpp_spin_lock_bh(KmppSpinlock *lock);
rk_s32 kmpp_spin_trylock_bh(KmppSpinlock *lock);
void   kmpp_spin_unlock_bh(KmppSpinlock *lock);

void   kmpp_spin_lock_irqsave(KmppSpinlock *lock, rk_ul *flags);
rk_s32 kmpp_spin_trylock_irqsave(KmppSpinlock *lock, rk_ul *flags);
void   kmpp_spin_unlock_irqrestore(KmppSpinlock *lock, rk_ul *flags);

#endif /* __KMPP_SPINLOCK_H__ */

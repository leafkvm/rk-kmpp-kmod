/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/spinlock.h>

#include "kmpp_mem.h"
#include "kmpp_atomic.h"
#include "kmpp_spinlock.h"

struct KmppSpinlock_t {
    rk_u32 need_free;
    void *lock;
};

rk_s32 kmpp_spinlock_init(KmppSpinlock **lock)
{
    if (lock) {
        KmppSpinlock *p = NULL;
        spinlock_t *s;

        p = kmpp_malloc(sizeof(KmppSpinlock) + sizeof(spinlock_t));
        if (!p)
            return -1;

        s = (spinlock_t *)(p + 1);

        spin_lock_init(s);
        p->lock = (void *)s;
        p->need_free = 1;
        *lock = p;

        return 0;
    }

    return rk_nok;
}
EXPORT_SYMBOL(kmpp_spinlock_init);

void kmpp_spinlock_deinit(KmppSpinlock **lock)
{
    if (lock) {
        KmppSpinlock *p = KMPP_FETCH_AND(lock, NULL);

        if (p && p->need_free)
            kmpp_free(p);
    }
}
EXPORT_SYMBOL(kmpp_spinlock_deinit);

rk_s32 kmpp_spinlock_size(void)
{
    return sizeof(KmppSpinlock) + sizeof(spinlock_t);
}
EXPORT_SYMBOL(kmpp_spinlock_size);

rk_s32 kmpp_spinlock_assign(KmppSpinlock **lock, void *buf, rk_s32 size)
{
    if (size >= sizeof(KmppSpinlock) + sizeof(spinlock_t)) {
        KmppSpinlock *p = (KmppSpinlock *)buf;
        spinlock_t *s = (spinlock_t *)(p + 1);

        spin_lock_init(s);
        p->lock = s;
        p->need_free = 0;
        *lock = p;
        return rk_ok;
    }

    if (lock)
        *lock = NULL;

    return rk_nok;
}
EXPORT_SYMBOL(kmpp_spinlock_assign);

void kmpp_spin_lock(KmppSpinlock *lock)
{
    if (lock && lock->lock)
        spin_lock((spinlock_t *)lock->lock);
}
EXPORT_SYMBOL(kmpp_spin_lock);

rk_s32 kmpp_spin_trylock(KmppSpinlock *lock)
{
    if (lock && lock->lock)
        return (spin_trylock((spinlock_t *)lock->lock)) ? rk_ok : rk_nok;

    return rk_nok;
}
EXPORT_SYMBOL(kmpp_spin_trylock);

void kmpp_spin_unlock(KmppSpinlock *lock)
{
    if (lock && lock->lock)
        spin_unlock((spinlock_t *)lock->lock);
}
EXPORT_SYMBOL(kmpp_spin_unlock);

void kmpp_spin_lock_bh(KmppSpinlock *lock)
{
    if (lock && lock->lock)
        spin_lock_bh((spinlock_t *)lock->lock);
}
EXPORT_SYMBOL(kmpp_spin_lock_bh);

rk_s32 kmpp_spin_trylock_bh(KmppSpinlock *lock)
{
    if (lock && lock->lock)
        return (spin_trylock_bh((spinlock_t *)lock->lock)) ? rk_ok : rk_nok;

    return rk_nok;
}
EXPORT_SYMBOL(kmpp_spin_trylock_bh);

void kmpp_spin_unlock_bh(KmppSpinlock *lock)
{
    if (lock && lock->lock)
        spin_unlock_bh((spinlock_t *)lock->lock);
}
EXPORT_SYMBOL(kmpp_spin_unlock_bh);

void kmpp_spin_lock_irqsave(KmppSpinlock *lock, rk_ul *flags)
{
    if (lock && lock->lock) {
        unsigned long v = *flags;

        spin_lock_irqsave((spinlock_t *)lock->lock, v);
        *flags = v;
    }
}
EXPORT_SYMBOL(kmpp_spin_lock_irqsave);

rk_s32 kmpp_spin_trylock_irqsave(KmppSpinlock *lock, rk_ul *flags)
{
    rk_s32 ret = rk_nok;

    if (lock && lock->lock) {
        unsigned long v = *flags;

        ret = (spin_trylock_irqsave((spinlock_t *)lock->lock, v)) ? rk_ok : rk_nok;
        *flags = v;
    }

    return ret;
}
EXPORT_SYMBOL(kmpp_spin_trylock_irqsave);

void kmpp_spin_unlock_irqrestore(KmppSpinlock *lock, rk_ul *flags)
{
    if (lock && lock->lock) {
        unsigned long v = *flags;

        spin_unlock_irqrestore((spinlock_t *)lock->lock, v);
    }
}
EXPORT_SYMBOL(kmpp_spin_unlock_irqrestore);
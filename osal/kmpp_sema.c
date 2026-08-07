/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/semaphore.h>

#include "kmpp_mem.h"
#include "kmpp_atomic.h"
#include "kmpp_sema.h"

struct KmppSemaphore_t {
    rk_u32 need_free;
    void *sem;
};

rk_s32 kmpp_sema_init(KmppSemaphore **sem, rk_s32 val)
{
    if (sem) {
        KmppSemaphore *p = kmpp_calloc(kmpp_sema_size());
        struct semaphore *s;

        *sem = NULL;
        if (!p)
            return -ENOMEM;

        s = (struct semaphore *)(p + 1);
        sema_init(s, val);
        p->sem = s;
        p->need_free = 1;
        *sem = p;
        return 0;
    }

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_sema_init);

void kmpp_sema_deinit(KmppSemaphore **sem)
{
    if (sem && *sem) {
        KmppSemaphore *p = KMPP_FETCH_AND(sem, NULL);

        if (p && p->need_free)
            kmpp_free(p);
    }
}
EXPORT_SYMBOL(kmpp_sema_deinit);

rk_s32 kmpp_sema_size(void)
{
    return sizeof(KmppSemaphore) + sizeof(struct semaphore);
}
EXPORT_SYMBOL(kmpp_sema_size);

rk_s32 kmpp_sema_assign(KmppSemaphore **sem, rk_s32 val, void *buf, rk_s32 size)
{
    if (sem && buf && size >= kmpp_sema_size()) {
        KmppSemaphore *p = (KmppSemaphore *)buf;
        struct semaphore *s = (struct semaphore *)(p + 1);

        sema_init(s, val);
        p->sem = s;
        p->need_free = 0;
        *sem = p;
        return 0;
    }

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_sema_assign);

rk_s32 kmpp_down(KmppSemaphore *sem)
{
    if (sem && sem->sem) {
        down((struct semaphore *)sem->sem);
        return 0;
    }

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_down);

rk_s32 kmpp_down_interruptible(KmppSemaphore *sem)
{
    if (sem && sem->sem)
        return down_interruptible((struct semaphore *)sem->sem);

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_down_interruptible);

rk_s32 kmpp_down_trylock(KmppSemaphore *sem)
{
    if (sem && sem->sem)
        return down_trylock((struct semaphore *)sem->sem);

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_down_trylock);

void kmpp_up(KmppSemaphore *sem)
{
    if (sem && sem->sem)
        up((struct semaphore *)sem->sem);
}
EXPORT_SYMBOL(kmpp_up);
/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/rwsem.h>

#include "kmpp_mem.h"
#include "kmpp_atomic.h"
#include "kmpp_rwsem.h"

struct KmppRwsemaphore_t {
    rk_u32 need_free;
    void *rwsem;
};

rk_s32 kmpp_rwsem_init(KmppRwsemaphore **rwsem)
{
    if (rwsem) {
        KmppRwsemaphore *p = kmpp_calloc(kmpp_rwsem_size());
        struct rw_semaphore *s;

        *rwsem = NULL;
        if (!p)
            return -ENOMEM;

        s = (struct rw_semaphore *)(p + 1);
        init_rwsem(s);
        p->rwsem = s;
        p->need_free = 1;
        *rwsem = p;

        return rk_ok;
    }

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_rwsem_init);

void kmpp_rwsem_deinit(KmppRwsemaphore **rwsem)
{
    if (rwsem && *rwsem) {
        KmppRwsemaphore *p = KMPP_FETCH_AND(rwsem, NULL);

        if (p->need_free)
            kmpp_free(p);
    }
}
EXPORT_SYMBOL(kmpp_rwsem_deinit);

rk_s32 kmpp_rwsem_size(void)
{
    return sizeof(KmppRwsemaphore) + sizeof(struct rw_semaphore);
}
EXPORT_SYMBOL(kmpp_rwsem_size);

rk_s32 kmpp_rwsem_assign(KmppRwsemaphore **rwsem, void *buf, rk_s32 size)
{
    if (size >= kmpp_rwsem_size()) {
        KmppRwsemaphore *p = (KmppRwsemaphore *)buf;
        struct rw_semaphore *s = (struct rw_semaphore *)(p + 1);;

        init_rwsem(s);
        p->rwsem = s;
        p->need_free = 0;
        *rwsem = p;

        return rk_ok;
    }

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_rwsem_assign);

void kmpp_down_read(KmppRwsemaphore *rwsem)
{
    if (rwsem && rwsem->rwsem)
        down_read((struct rw_semaphore *)rwsem->rwsem);
}
EXPORT_SYMBOL(kmpp_down_read);

rk_s32 kmpp_down_read_interruptible(KmppRwsemaphore *rwsem)
{
    if (rwsem && rwsem->rwsem)
        return down_read_interruptible((struct rw_semaphore *)rwsem->rwsem);

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_down_read_interruptible);

rk_s32 kmpp_down_read_killable(KmppRwsemaphore *rwsem)
{
    if (rwsem && rwsem->rwsem)
        return down_read_killable((struct rw_semaphore *)rwsem->rwsem);

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_down_read_killable);

rk_s32 kmpp_down_read_trylock(KmppRwsemaphore *rwsem)
{
    if (rwsem && rwsem->rwsem)
        return down_read_trylock((struct rw_semaphore *)rwsem->rwsem);

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_down_read_trylock);

void kmpp_down_write(KmppRwsemaphore *rwsem)
{
    if (rwsem && rwsem->rwsem)
        down_write((struct rw_semaphore *)rwsem->rwsem);
}
EXPORT_SYMBOL(kmpp_down_write);

rk_s32 kmpp_down_write_killable(KmppRwsemaphore *rwsem)
{
    if (rwsem && rwsem->rwsem)
        return down_write_killable((struct rw_semaphore *)rwsem->rwsem);

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_down_write_killable);

rk_s32 kmpp_down_write_trylock(KmppRwsemaphore *rwsem)
{
    if (rwsem && rwsem->rwsem)
        return down_write_trylock((struct rw_semaphore *)rwsem->rwsem);

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_down_write_trylock);

void kmpp_up_read(KmppRwsemaphore *rwsem)
{
    if (rwsem && rwsem->rwsem)
        up_read((struct rw_semaphore *)rwsem->rwsem);
}
EXPORT_SYMBOL(kmpp_up_read);

void kmpp_up_write(KmppRwsemaphore *rwsem)
{
    if (rwsem && rwsem->rwsem)
        up_write((struct rw_semaphore *)rwsem->rwsem);
}
EXPORT_SYMBOL(kmpp_up_write);

void kmpp_downgrade_write(KmppRwsemaphore *rwsem)
{
    if (rwsem && rwsem->rwsem)
        downgrade_write((struct rw_semaphore *)rwsem->rwsem);
}
EXPORT_SYMBOL(kmpp_downgrade_write);

rk_s32 kmpp_rwsem_is_locked(KmppRwsemaphore *rwsem)
{
    if (rwsem && rwsem->rwsem)
        return rwsem_is_locked((struct rw_semaphore *)rwsem->rwsem);

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_rwsem_is_locked);

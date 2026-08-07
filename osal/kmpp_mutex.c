/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/mutex.h>

#include "kmpp_mem.h"
#include "kmpp_atomic.h"
#include "kmpp_mutex.h"

typedef struct KmppMutex_t {
    rk_u32 need_free;
    void *mutex;
} KmppMutex;

rk_s32 kmpp_mutex_init(KmppMutex **mutex)
{
    if (mutex) {
        KmppMutex *p = kmpp_calloc(sizeof(KmppMutex) + sizeof(struct mutex));
        struct mutex *s;

        *mutex = NULL;
        if (!p)
            return -ENOMEM;

        s = (struct mutex *)(p + 1);

        mutex_init(s);
        p->mutex = (void *)s;
        p->need_free = 1;
        *mutex = p;

        return 0;
    }

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_mutex_init);

void kmpp_mutex_deinit(KmppMutex **mutex)
{
    if (mutex) {
        KmppMutex *p = KMPP_FETCH_AND(mutex, NULL);

        if (p) {
            if (p->mutex)
                mutex_destroy((struct mutex *)(p->mutex));

            if (p->need_free)
                kmpp_free(p);
        }
    }
}
EXPORT_SYMBOL(kmpp_mutex_deinit);

rk_s32 kmpp_mutex_size(void)
{
    return sizeof(KmppMutex) + sizeof(struct mutex);
}
EXPORT_SYMBOL(kmpp_mutex_size);

rk_s32 kmpp_mutex_assign(KmppMutex **mutex, void *buf, rk_s32 size)
{
    if (mutex && buf && size >= kmpp_mutex_size()) {
        KmppMutex *p = (KmppMutex *)buf;
        struct mutex *s = (struct mutex *)(p + 1);

        mutex_init(s);
        p->mutex = (void *)s;
        p->need_free = 0;
        *mutex = p;

        return rk_ok;
    }

    return rk_nok;
}
EXPORT_SYMBOL(kmpp_mutex_assign);

rk_s32 kmpp_mutex_lock(KmppMutex *mutex)
{
    if (mutex && mutex->mutex) {
        mutex_lock((struct mutex *)(mutex->mutex));
        return 0;
    }

    return -1;
}
EXPORT_SYMBOL(kmpp_mutex_lock);

rk_s32 kmpp_mutex_lock_interruptible(KmppMutex *mutex)
{
    if (mutex && mutex->mutex)
        return mutex_lock_interruptible((struct mutex *)(mutex->mutex));

    return -1;
}
EXPORT_SYMBOL(kmpp_mutex_lock_interruptible);

rk_s32 kmpp_mutex_trylock(KmppMutex *mutex)
{
    if (mutex && mutex->mutex)
        return mutex_trylock((struct mutex *)(mutex->mutex));

    return -1;
}
EXPORT_SYMBOL(kmpp_mutex_trylock);

void kmpp_mutex_unlock(KmppMutex *mutex)
{
    if (mutex && mutex->mutex)
        mutex_unlock((struct mutex *)(mutex->mutex));
}
EXPORT_SYMBOL(kmpp_mutex_unlock);
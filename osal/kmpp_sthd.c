/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/printk.h>

#include "kmpp_mem.h"
#include "kmpp_log.h"
#include "kmpp_atomic.h"
#include "kmpp_mutex.h"
#include "kmpp_sthd.h"
#include "kmpp_thread.h"

#define KTHREAD_NAME_LEN 32

typedef struct KmppThd_t {
    KmppThds        base;
    KmppWorker      *worker;
    KmppWork        *work;
} KmppThd;

typedef struct KmppThdsImpl_t {
    KmppThds        base;
    char            name[KTHREAD_NAME_LEN];
    KmppMutex       *lock;
    KmppFunc        func;
    rk_s32          count;
    KmppThd         thds[];
} KmppThdsImpl;

rk_s32 kmpp_thds_init(KmppThds **thds, const char *name, rk_s32 count)
{
    KmppThdsImpl *impl = NULL;
    rk_s32 total_size;
    rk_s32 elem_size;

    if (!thds || count <= 0)
        return -EINVAL;

    elem_size = ALIGN(sizeof(KmppThd), 8);
    total_size = ALIGN(sizeof(KmppThdsImpl), 8) + count * elem_size;

    impl = kmpp_calloc(total_size);
    if (impl) {
        rk_s32 i;

        if (!name)
            name = "kmpp_thds";

        impl->base.self = &impl->base;
        impl->base.idx = -1;
        impl->count = count;

        kmpp_mutex_init(&impl->lock);

        for (i = 0; i < count; i++) {
            KmppThd *thd = &impl->thds[i];

            snprintf(impl->name, KTHREAD_NAME_LEN - 1, "%s-%d", name, i);

            kmpp_worker_init(&thd->worker, impl->name);
            thd->base.self = &impl->base;
            thd->base.idx = i;
        }

        snprintf(impl->name, KTHREAD_NAME_LEN - 1, "%s", name);
    }

    *thds = &impl->base;
    return rk_ok;
}
EXPORT_SYMBOL(kmpp_thds_init);

void kmpp_thds_deinit(KmppThds **thds)
{
    if (thds && *thds) {
        KmppThdsImpl *impl = (KmppThdsImpl *)kmpp_cmpxchg(thds, *thds, NULL);
        rk_s32 i;

        kmpp_mutex_lock(impl->lock);

        for (i = 0; i < impl->count; i++) {
            KmppThd *thd = &impl->thds[i];

            kmpp_worker_deinit(&thd->worker);
            kmpp_work_deinit(&thd->work);
            thd->base.idx = -1;
        }

        kmpp_mutex_unlock(impl->lock);

        kmpp_mutex_deinit(&impl->lock);
        kmpp_free(impl);
    }
}
EXPORT_SYMBOL(kmpp_thds_deinit);

void kmpp_thds_setup(KmppThds *thds, KmppFunc func, void *ctx)
{
    KmppThdsImpl *impl = (KmppThdsImpl *)thds;
    rk_s32 i;

    kmpp_assert(impl);

    kmpp_mutex_lock(impl->lock);
    impl->base.ctx = ctx;
    impl->func = func;

    for (i = 0; i < impl->count; i++) {
        KmppThd *thd = &impl->thds[i];

        thd->base.ctx = ctx;
        kmpp_work_init(&thd->work, func, &thd->base);
    }

    kmpp_mutex_unlock(impl->lock);
}
EXPORT_SYMBOL(kmpp_thds_setup);

const char *kmpp_thds_get_name(KmppThds *thds)
{
    KmppThdsImpl *impl = (KmppThdsImpl *)thds->self;

    return impl->name;
}
EXPORT_SYMBOL(kmpp_thds_get_name);

KmppThds *kmpp_thds_get_entry(KmppThds *thds, rk_s32 idx)
{
    KmppThdsImpl *impl = (KmppThdsImpl *)thds->self;

    if (idx >= 0 && idx < impl->count)
        return &impl->thds[idx].base;

    return NULL;
}
EXPORT_SYMBOL(kmpp_thds_get_entry);

void kmpp_thds_lock(KmppThds *thds)
{
    KmppThdsImpl *impl = (KmppThdsImpl *)(thds->self);

    kmpp_mutex_lock(impl->lock);
}
EXPORT_SYMBOL(kmpp_thds_lock);

void kmpp_thds_unlock(KmppThds *thds)
{
    KmppThdsImpl *impl = (KmppThdsImpl *)(thds->self);

    kmpp_mutex_unlock(impl->lock);
}
EXPORT_SYMBOL(kmpp_thds_unlock);

rk_s32 kmpp_thds_run_nl(KmppThds *thds)
{
    KmppThdsImpl *impl = (KmppThdsImpl *)(thds->self);
    KmppThd *thd = NULL;
    rk_s32 idx = thds->idx;
    rk_s32 ret = rk_nok;
    rk_s32 i;

    kmpp_assert(impl);

    if (idx < 0) {
        for (i = 0; i < impl->count; i++) {
            thd = &impl->thds[i];
            ret = kmpp_work_queue(thd->worker, thd->work);
            if (ret > 0)
                return i;
        }
    } else {
        thd = &impl->thds[idx];
        ret = kmpp_work_queue(thd->worker, thd->work);
        if (ret > 0)
            return idx;
    }

    return -1;
}
EXPORT_SYMBOL(kmpp_thds_run_nl);
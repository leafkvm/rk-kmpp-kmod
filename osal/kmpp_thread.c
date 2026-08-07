/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>

#include "kmpp_mem.h"
#include "kmpp_log.h"
#include "kmpp_atomic.h"
#include "kmpp_thread.h"

typedef struct KmppWorkImpl_t {
    KmppWork            work;
    struct kthread_work work_s;
} KmppWorkImpl;

static void mpp_work_run(struct kthread_work *work_s)
{
    KmppWorkImpl *work = container_of(work_s, KmppWorkImpl, work_s);

    if (work->work.func)
        work->work.ret = work->work.func(work->work.data);
}

static rk_s32 get_sched_policy(rk_s32 sched_policy)
{
    switch (sched_policy) {
    case KMPP_SCHED_NORMAL:
        return SCHED_NORMAL;
    case KMPP_SCHED_FIFO:
        return SCHED_FIFO;
    case KMPP_SCHED_RR:
        return SCHED_RR;
    default:
        kmpp_loge_f("invalid sched policy %d\n", sched_policy);
        return SCHED_NORMAL;
    }
}

void kmpp_worker_init(KmppWorker **worker, const char *name)
{
    if (worker) {
        KmppWorker *p = kmpp_calloc(sizeof(KmppWorker));

        if (p)
            p->worker = kthread_create_worker(0, name);

        *worker = p;
    }
}
EXPORT_SYMBOL(kmpp_worker_init);

void kmpp_worker_deinit(KmppWorker **worker)
{
    if (worker) {
        KmppWorker *p = kmpp_cmpxchg(worker, *worker, NULL);

        if (p && p->worker) {
            kthread_destroy_worker((struct kthread_worker *)(p->worker));
            kmpp_free(p);
        }
    }
}
EXPORT_SYMBOL(kmpp_worker_deinit);

void kmpp_worker_flush(KmppWorker *worker)
{
    if (IS_ERR_OR_NULL(worker) || IS_ERR_OR_NULL(worker->worker)) {
        kmpp_loge("can not flush invalid worker\n");
        return;
    }
    kthread_flush_worker((struct kthread_worker *)(worker->worker));
}
EXPORT_SYMBOL(kmpp_worker_flush);

rk_s32 kmpp_worker_set_priority(KmppWorker *worker, rk_s32 sched_policy,
                                rk_s32 priority, rk_s32 nice)
{
    struct kthread_worker *kworker = NULL;
    struct sched_attr attr;
    rk_s32 ret;

    if (!worker || !worker->worker) {
        kmpp_loge("can not set priority for invalid worker %px\n", worker);
        return -EINVAL;
    }

    kworker = (struct kthread_worker *)worker->worker;

    attr.sched_policy = get_sched_policy(sched_policy);
    attr.sched_priority = priority;
    attr.sched_nice = nice;

    ret = sched_setattr_nocheck(kworker->task, &attr);
    if (ret)
        kmpp_loge("failed to set worker priority %d policy %d nice %d\n",
                  priority, sched_policy, nice);

    return ret;
}
EXPORT_SYMBOL(kmpp_worker_set_priority);

rk_s32 kmpp_work_init(KmppWork **work, KmppWorkFunc func, void *data)
{
    if (work) {
        KmppWorkImpl *p = kmpp_malloc(sizeof(*p));

        *work = NULL;
        if (!p)
            return -ENOMEM;

        kthread_init_work(&p->work_s, mpp_work_run);
        p->work.func = func;
        p->work.data = data;
        p->work.ret = rk_ok;
        p->work.delay_ms = 0;
        p->work.work = (void *)&p->work_s;
        *work = &p->work;

        return 0;
    }

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_work_init);

void kmpp_work_deinit(KmppWork **work)
{
    if (work) {
        void *p = kmpp_cmpxchg(work, *work, NULL);

        if (p)
            kmpp_free(p);
    }
}
EXPORT_SYMBOL(kmpp_work_deinit);

rk_s32 kmpp_work_queue(KmppWorker *worker, KmppWork *work)
{
    if (IS_ERR_OR_NULL(worker) || IS_ERR_OR_NULL(worker->worker)) {
        kmpp_loge("can not queue invalid worker %px\n", worker);
        return -EINVAL;
    }

    if (IS_ERR_OR_NULL(work) || IS_ERR_OR_NULL(work->work)) {
        kmpp_loge("can not queue invalid work %px\n", work);
        return -EINVAL;
    }

    return kthread_queue_work((struct kthread_worker *)worker->worker,
                              &((KmppWorkImpl *)work)->work_s);
}
EXPORT_SYMBOL(kmpp_work_queue);

rk_s32 kmpp_delayed_work_init(KmppWork **work, KmppWorkFunc func, void *data)
{
    if (work) {
        KmppWork *p = kmpp_malloc(sizeof(KmppWork) + sizeof(struct kthread_delayed_work));
        struct kthread_delayed_work *s;

        *work = NULL;
        if (!p)
            return -ENOMEM;

        s = (struct kthread_delayed_work *)(p + 1);
        kthread_init_delayed_work(s, mpp_work_run);
        p->func = func;
        p->data = data;
        p->ret = rk_ok;
        p->delay_ms = 0;
        p->work = (void *)s;
        *work = p;

        return 0;
    }

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_delayed_work_init);

void kmpp_delayed_work_deinit(KmppWork **work)
{
    if (work) {
        KmppWork *p = kmpp_cmpxchg(work, *work, NULL);

        kmpp_free(p);
    }
}
EXPORT_SYMBOL(kmpp_delayed_work_deinit);

rk_s32 kmpp_delayed_work_queue(KmppWorker *worker, KmppWork *work, rk_s32 delay_ms)
{
    if (IS_ERR_OR_NULL(worker) || IS_ERR_OR_NULL(worker->worker) ||
        IS_ERR_OR_NULL(work) || IS_ERR_OR_NULL(work->work)) {
        kmpp_loge("can not queue invalid delayed work\n");
        return -EINVAL;
    }

    return kthread_queue_delayed_work((struct kthread_worker *)worker->worker,
                                      (struct kthread_delayed_work *)work->work,
                                      delay_ms);
}
EXPORT_SYMBOL(kmpp_delayed_work_queue);
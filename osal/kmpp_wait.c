/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>

#include "kmpp_atomic.h"
#include "kmpp_mem.h"
#include "kmpp_wait.h"
#include "kmpp_wait_impl.h"

rk_s32 kmpp_wait_init(KmppWait **wait)
{
    KmppWait *p = NULL;
    wait_queue_head_t *wq = NULL;

    if (!wait)
        return -1;

    p = kmpp_malloc(sizeof(KmppWait) + sizeof(wait_queue_head_t));
    if (!p)
        return -1;

    wq = (wait_queue_head_t *)(p + 1);

    init_waitqueue_head(wq);
    p->wait = (void *)wq;
    p->need_free = 1;

    *wait = p;
    return 0;
}
EXPORT_SYMBOL(kmpp_wait_init);

void kmpp_wait_deinit(KmppWait **wait)
{
    if (wait) {
        KmppWait *p = KMPP_FETCH_AND(wait, NULL);

        if (p && p->need_free)
            kmpp_free(p);
    }
}
EXPORT_SYMBOL(kmpp_wait_deinit);

rk_s32 kmpp_wait_size(void)
{
    return sizeof(KmppWait) + sizeof(wait_queue_head_t);
}
EXPORT_SYMBOL(kmpp_wait_size);

rk_s32 kmpp_wait_assign(KmppWait **wait, void *buf, rk_s32 size)
{
    if (wait && buf && size >= sizeof(wait_queue_head_t)) {
        KmppWait *p = (KmppWait *)buf;
        wait_queue_head_t *wq = (wait_queue_head_t *)(buf + sizeof(KmppWait));

        init_waitqueue_head(wq);
        p->wait = (void *)wq;
        p->need_free = 0;

        *wait = p;
        return rk_ok;
    }

    return rk_nok;
}
EXPORT_SYMBOL(kmpp_wait_assign);

rk_s32 kmpp_wait_interruptible(KmppWait *wait, KmppWaitCondFunc func, void *param)
{
    if (wait && wait->wait) {
        DEFINE_WAIT(__wait);
        rk_s32 cond = 0;
        rk_s32 ret = 0;

        wait_queue_head_t *wq = (wait_queue_head_t *)(wait->wait);

        prepare_to_wait(wq, &__wait, TASK_INTERRUPTIBLE);
        if (func)
            cond = func(param);

        if (!cond) {
            if (!signal_pending(current)) {
                schedule();
            }
            if (signal_pending(current)) {
                ret = -ERESTARTSYS;
            }
        }

        finish_wait(wq, &__wait);
        return ret;
    }

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_wait_interruptible);

rk_s32 kmpp_wait_uninterruptible(KmppWait *wait, KmppWaitCondFunc func, void *param)
{
    if (wait && wait->wait) {
        DEFINE_WAIT(__wait);
        rk_s32 cond = 0;
        rk_s32 ret = 0;

        wait_queue_head_t *wq = (wait_queue_head_t *)(wait->wait);

        prepare_to_wait(wq, &__wait, TASK_UNINTERRUPTIBLE);
        if (func)
            cond = func(param);

        if (!cond)
            schedule();

        finish_wait(wq, &__wait);
        return ret;
    }

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_wait_uninterruptible);

rk_s32 kmpp_wait_timeout_interruptible(KmppWait *wait, KmppWaitCondFunc func, void *param, rk_u32 ms)
{
    if (wait && wait->wait) {
        DEFINE_WAIT(__wait);
        rk_s32 cond = 0;
        rk_s32 ret = ms;

        wait_queue_head_t *wq = (wait_queue_head_t *)(wait->wait);

        prepare_to_wait(wq, &__wait, TASK_INTERRUPTIBLE);
        if (func)
            cond = func(param);

        if (!cond) {
            if (!signal_pending(current)) {
                ret = schedule_timeout(msecs_to_jiffies(ret));
                ret = jiffies_to_msecs(ret);
            }
            if (signal_pending(current)) {
                ret = -ERESTARTSYS;
            }
        }

        finish_wait(wq, &__wait);
        return ret;
    }

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_wait_timeout_interruptible);

rk_s32 kmpp_wait_timeout_uninterruptible(KmppWait *wait, KmppWaitCondFunc func, void *param, rk_u32 ms)
{
    if (wait && wait->wait) {
        DEFINE_WAIT(__wait);
        rk_s32 cond = 0;
        rk_s32 ret = ms;

        wait_queue_head_t *wq = (wait_queue_head_t *)(wait->wait);

        prepare_to_wait(wq, &__wait, TASK_UNINTERRUPTIBLE);
        if (func)
            cond = func(param);

        if (!cond) {
            ret = schedule_timeout(msecs_to_jiffies(ret));
            ret = jiffies_to_msecs(ret);
        }

        finish_wait(wq, &__wait);
        return ret;
    }

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_wait_timeout_uninterruptible);

void kmpp_wait_wakeup(KmppWait *wait)
{
    if (wait && wait->wait)
        wake_up_all((wait_queue_head_t *)(wait->wait));
}
EXPORT_SYMBOL(kmpp_wait_wakeup);
/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 */

#define pr_fmt(fmt) "vcodec_thread: " fmt

#include <linux/module.h>

#include "kmpp_mem.h"

#include "mpp_vcodec_thread.h"

#define THREAD_DBG_FLOW     (0x00000001)

#define thread_dbg(flag, fmt, args...) \
    do { \
        if (unlikely(thread_debug & flag)) \
            pr_info(fmt, ##args); \
    } while (0)

#define thread_dbg_flow(fmt, args...)   thread_dbg(THREAD_DBG_FLOW, fmt, ##args)

unsigned int thread_debug;
module_param(thread_debug, uint, 0644);
MODULE_PARM_DESC(thread_debug, "bit switch for vcodec thread debug information");

static const char *const str_thd_state[] = {
    "invalid",
    "ready",
    "running",
    "paused",
};

static const char *state_to_str(enum vcodec_threads_status status)
{
    return (status < VCODEC_THREADS_BUTT) ? str_thd_state[status] : NULL;
}

static int check_vcodec_threads(struct vcodec_threads *thds, const char *fmt)
{
    if (!thds) {
        pr_err("NULL invalid vcodec threads on %s\n", fmt);
        return -EINVAL;
    }

    if (thds->check != thds || !thds->module) {
        pr_err("%p invalid vcodec threads check %p module %p at %s\n",
               thds, thds->check, thds->module, fmt);
        return -EINVAL;
    }

    return 0;
}

static rk_s32 vcodec_thread_worker(void *data)
{
    struct vcodec_thread *thd = data;
    struct vcodec_threads *thds = thd->thds;
    RK_U64 curr_us = ktime_get_raw_ns();

    thread_dbg_flow("enter worker\n");

    do {
        thd->sum_gap += curr_us - thd->last_us;
        thd->last_us = curr_us;

        if (thds->cfg.callback) {
            set_bit(thd->thd_id, &thds->run_state);
            thd->ret = thds->cfg.callback(thds->cfg.param);
            clear_bit(thd->thd_id, &thds->run_state);
        } else
            thd->ret = 0;

        curr_us = ktime_get_raw_ns();
        thd->sum_run += curr_us - thd->last_us;
        thd->last_us = curr_us;

        thd->run_cnt++;
    } while (thd->ret > 0);

    thread_dbg_flow("leave worker\n");

    return 0;
}

static int vcodec_thread_prepare(struct vcodec_threads *thds)
{
    vcodec_work_func_t callback = thds->cfg.callback;
    int count = thds->cfg.count;
    int i;

    if (!count || count > VCODEC_MAX_WORK_THREAD || !callback) {
        pr_err("invalid count %d callback %p", count, callback);
        return -EINVAL;
    }

    thread_dbg_flow("enter prepare\n");
    WARN_ON(thds->worker);

    thds->worker = kmpp_calloc(count * sizeof(*thds->worker));
    if (!thds->worker)
        return -ENOMEM;

    thread_dbg_flow("prepare count %d\n", count);
    for (i = 0; i < count; i++) {
        struct vcodec_thread *thd = &thds->worker[i];
        rk_u8 name[32];

        thd->thds = thds;
        thd->thd_id = i;
        thd->last_us = ktime_get_raw_ns();

        kmpp_sprintf(name, "vcodec_thread_%d", i);
        kmpp_worker_init(&thd->worker, name);
        kmpp_worker_set_priority(thd->worker, KMPP_SCHED_FIFO, 50, 0);
        kmpp_work_init(&thd->work, vcodec_thread_worker, thd);
        thread_dbg_flow("prepare thread %d done\n", i);
    }
    thread_dbg_flow("leave prepare\n");

    return 0;
}

static void vcodec_thread_reset(struct vcodec_threads *thds)
{
    thread_dbg_flow("enter reset\n");
    if (thds->worker) {
        if (thds->cfg.count) {
            int i;

            for (i = 0; i < thds->cfg.count; i++) {
                struct vcodec_thread *thd = &thds->worker[i];

                thread_dbg_flow("stop thread %d\n", i);
                if (thd->worker) {
                    kmpp_worker_flush(thd->worker);
                    kmpp_worker_deinit(&thd->worker);
                    thd->worker = NULL;
                }
                if (thd->work) {
                    kmpp_work_deinit(&thd->work);
                    thd->work = NULL;
                }
            }
        }
        kmpp_free(thds->worker);
    }
    thread_dbg_flow("leave reset\n");
}

struct vcodec_threads *vcodec_thread_create(struct vcodec_module *module)
{
    struct vcodec_threads *thds = kmpp_calloc(sizeof(*thds));

    if (thds) {
        /* setup init single thread pool without callback */
        thds->set.count = 1;
        thds->change = VCODEC_THREADS_CHANGE_COUNT | VCODEC_THREADS_CHANGE_CALLBACK;
        thds->check = thds;
        thds->module = module;
        thds->status = VCODEC_THREADS_INVALID;
        kmpp_spinlock_init(&thds->lock);
    }

    return thds;
}

int vcodec_thread_destroy(struct vcodec_threads *thds)
{
    unsigned long lock_flags = 0;

    if (check_vcodec_threads(thds, "destroy"))
        return -EINVAL;

    kmpp_spin_lock_irqsave(thds->lock, &lock_flags);
    thds->status = VCODEC_THREADS_INVALID;
    kmpp_spin_unlock_irqrestore(thds->lock, &lock_flags);

    vcodec_thread_reset(thds);

    kmpp_spinlock_deinit(&thds->lock);
    thds->check = NULL;
    thds->module = NULL;

    kmpp_free(thds);

    return 0;
}

int vcodec_thread_set_count(struct vcodec_threads *thds, int count)
{
    unsigned long lock_flags = 0;

    if (check_vcodec_threads(thds, "set count"))
        return -EINVAL;

    thread_dbg_flow("enter set count %d\n", count);

    kmpp_spin_lock_irqsave(thds->lock, &lock_flags);
    if (thds->set.count != count) {
        thds->set.count = count;
        thds->change |= VCODEC_THREADS_CHANGE_COUNT;
    }

    kmpp_spin_unlock_irqrestore(thds->lock, &lock_flags);
    thread_dbg_flow("leave set count\n");

    return 0;
}

int vcodec_thread_set_callback(struct vcodec_threads *thds,
                               vcodec_work_func_t callback, void *param)
{
    unsigned long lock_flags = 0;

    if (check_vcodec_threads(thds, "set callback"))
        return -EINVAL;

    thread_dbg_flow("enter set callback\n");

    kmpp_spin_lock_irqsave(thds->lock, &lock_flags);
    if (thds->set.callback != callback || thds->set.param != param) {
        thds->set.callback = callback;
        thds->set.param = param;
        thds->change |= VCODEC_THREADS_CHANGE_CALLBACK;
    }

    kmpp_spin_unlock_irqrestore(thds->lock, &lock_flags);
    thread_dbg_flow("leave set callback\n");

    return 0;
}

int vcodec_thread_start(struct vcodec_threads *thds)
{
    int ret = 0;

    if (check_vcodec_threads(thds, "start"))
        return -EINVAL;

    thread_dbg_flow("enter start with change %x\n", thds->change);

    if (thds->status != VCODEC_THREADS_INVALID && thds->status != VCODEC_THREADS_READY) {
        pr_err("%p can not start at status %s", thds, state_to_str(thds->status));
        return -EINVAL;
    }

    if (thds->change) {
        int change = thds->change;

        vcodec_thread_reset(thds);

        if (change & VCODEC_THREADS_CHANGE_COUNT)
            thds->cfg.count = thds->set.count;

        if (change & VCODEC_THREADS_CHANGE_CALLBACK) {
            thds->cfg.callback = thds->set.callback;
            thds->cfg.param = thds->set.param;
        }
        thds->change = 0;

        ret = vcodec_thread_prepare(thds);
    }

    thds->status = VCODEC_THREADS_RUNNING;

    thread_dbg_flow("leave start\n");

    return ret;
}

int vcodec_thread_stop(struct vcodec_threads *thds)
{
    unsigned long lock_flags = 0;

    if (check_vcodec_threads(thds, "stop"))
        return -EINVAL;

    thread_dbg_flow("enter stop\n");

    if (thds->status != VCODEC_THREADS_RUNNING && thds->status != VCODEC_THREADS_PAUSED) {
        pr_err("%p can not stop at status %s", thds, state_to_str(thds->status));
        return -EINVAL;
    }

    if (thds->cfg.count && thds->worker) {
        int i;

        for (i = 0; i < thds->cfg.count; i++) {
            struct vcodec_thread *thd = &thds->worker[i];

            kmpp_worker_flush(thd->worker);
        }
    }

    kmpp_spin_lock_irqsave(thds->lock, &lock_flags);
    thds->status = VCODEC_THREADS_READY;
    kmpp_spin_unlock_irqrestore(thds->lock, &lock_flags);
    thread_dbg_flow("leave stop\n");

    return 0;
}

int vcodec_thread_pause(struct vcodec_threads *thds)
{
    unsigned long lock_flags = 0;

    if (check_vcodec_threads(thds, "pause"))
        return -EINVAL;

    kmpp_spin_lock_irqsave(thds->lock, &lock_flags);
    if (thds->status != VCODEC_THREADS_RUNNING) {
        pr_err("%p can not pause at status %s", thds, state_to_str(thds->status));
        kmpp_spin_unlock_irqrestore(thds->lock, &lock_flags);
        return -EINVAL;
    }

    thds->status = VCODEC_THREADS_PAUSED;

    kmpp_spin_unlock_irqrestore(thds->lock, &lock_flags);
    return 0;
}

int vcodec_thread_resume(struct vcodec_threads *thds)
{
    unsigned long lock_flags = 0;

    if (check_vcodec_threads(thds, "resume"))
        return -EINVAL;


    kmpp_spin_lock_irqsave(thds->lock, &lock_flags);
    if (thds->status != VCODEC_THREADS_PAUSED) {
        pr_err("%p can not resume at status %s", thds, state_to_str(thds->status));
        kmpp_spin_unlock_irqrestore(thds->lock, &lock_flags);
        return -EINVAL;
    }

    thds->status = VCODEC_THREADS_RUNNING;

    kmpp_spin_unlock_irqrestore(thds->lock, &lock_flags);

    return 0;
}

int vcodec_thread_trigger(struct vcodec_threads *thds)
{
    unsigned long lock_flags = 0;

    if (check_vcodec_threads(thds, "stop"))
        return -EINVAL;

    kmpp_spin_lock_irqsave(thds->lock, &lock_flags);
    thread_dbg_flow("enter trigger %llu\n", thds->queue_cnt);

    if (thds->status != VCODEC_THREADS_RUNNING) {
        pr_err("%p can not trigger at status %s", thds, state_to_str(thds->status));
        kmpp_spin_unlock_irqrestore(thds->lock, &lock_flags);
        return -EINVAL;
    }

    if (thds->cfg.count && thds->worker) {
        bool cnt = 0;
        int i;

        if (thds->cfg.count > 1) {
            for (i = 0; i < thds->cfg.count; i++) {
                if (!test_bit(i, &thds->run_state)) {
                    struct vcodec_thread *thd = &thds->worker[i];

                    cnt = kmpp_work_queue(thd->worker, thd->work);
                    thread_dbg_flow ("queue work %d ret %d\n", i, cnt);
                    thd->queue_cnt += cnt;
                    break;
                }
            }
        } else {
            struct vcodec_thread *thd = &thds->worker[0];

            cnt = kmpp_work_queue(thd->worker, thd->work);
            thread_dbg_flow("queue work %d ret %d\n", 0, cnt);
            thd->queue_cnt += cnt;
        }

        if (cnt) {
            thds->queue_cnt++;
            thds->miss_cnt = 0;
        } else
            thds->miss_cnt++;
    }

    thread_dbg_flow("leave trigger %llu:%llu\n", thds->queue_cnt, thds->miss_cnt);

    kmpp_spin_unlock_irqrestore(thds->lock, &lock_flags);

    return 0;
}

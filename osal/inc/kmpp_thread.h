/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_THREAD_H__
#define __KMPP_THREAD_H__

#include "rk_type.h"

typedef rk_s32 (*KmppWorkFunc)(void *data);

typedef struct KmppWork_t {
    KmppWorkFunc func;
    void *work;
    void *data;
    rk_s32 ret;
    rk_s32 delay_ms;
} KmppWork;

typedef struct KmppWorker_t {
    void *worker;
} KmppWorker;

/* sched nice */
#define KMPP_MAX_NICE   19
#define KMPP_MIN_NICE   -20

#define KMPP_NICE_WIDTH (KMPP_MAX_NICE - KMPP_MIN_NICE + 1)

/*
 * priority of a process goes from 0..max_prio-1, valid rt
 * priority is 0..max_rt_prio-1, and sched_normal/sched_batch
 * tasks are in the range max_rt_prio..max_prio-1. priority
 * values are inverted: lower p->prio value means higher priority.
 */

#define KMPP_MAX_RT_PRIO    100

#define KMPP_MAX_PRIO       (KMPP_MAX_RT_PRIO + KMPP_NICE_WIDTH)
#define KMPP_DEFAULT_PRIO   (KMPP_MAX_RT_PRIO + KMPP_NICE_WIDTH / 2)

/* sched policy */
#define KMPP_SCHED_NORMAL   0
#define KMPP_SCHED_FIFO     1
#define KMPP_SCHED_RR       2


void   kmpp_worker_init(KmppWorker **worker, const char *name);
void   kmpp_worker_deinit(KmppWorker **worker);

void   kmpp_worker_flush(KmppWorker *worker);
rk_s32 kmpp_worker_set_priority(KmppWorker *worker, rk_s32 sched_policy,
                                rk_s32 priority, rk_s32 nice);

rk_s32 kmpp_work_init(KmppWork **work, KmppWorkFunc func, void *data);
void   kmpp_work_deinit(KmppWork **work);
rk_s32 kmpp_work_queue(KmppWorker *worker, KmppWork *work);

rk_s32 kmpp_delayed_work_init(KmppWork **work, KmppWorkFunc func, void *data);
void   kmpp_delayed_work_deinit(KmppWork **work);
rk_s32 kmpp_delayed_work_queue(KmppWorker *worker, KmppWork *work, rk_s32 delay_ms);

#endif /* __KMPP_THREAD_H__ */
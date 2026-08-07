/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 */

#ifndef __ROCKCHIP_MPP_VCODEC_THREAD_H__
#define __ROCKCHIP_MPP_VCODEC_THREAD_H__

#include "kmpp_thread.h"
#include "mpp_vcodec_base.h"

#define VCODEC_MAX_WORK_THREAD  32

enum vcodec_threads_status {
    VCODEC_THREADS_INVALID  = 0,
    VCODEC_THREADS_READY    = 1,
    VCODEC_THREADS_RUNNING  = 2,
    VCODEC_THREADS_PAUSED   = 3,

    VCODEC_THREADS_BUTT,
};

enum {
    VCODEC_THREADS_CHANGE_COUNT     = (1 << 0),
    VCODEC_THREADS_CHANGE_CALLBACK  = (1 << 1),

    VCODEC_THREADS_CHANGE_ALL       = (0xffffffff),
};

struct vcodec_thread;
struct vcodec_threads_config;
struct vcodec_threads;
typedef int (*vcodec_work_func_t) (void *param);

/* Thread pool global config */
struct vcodec_threads_config {
    int count;

    vcodec_work_func_t callback;
    void *param;
};

/*
 * Thread pool for each function module
 * NOTE: The max thread count is 32
 */
struct vcodec_thread {
    /* kworkers for thread pool */
    KmppWork *work;

    struct vcodec_threads *thds;
    int thd_id;
    int ret;

    RK_U64 last_us;
    RK_U64 queue_cnt;
    RK_U64 run_cnt;
    RK_U64 sum_run;
    RK_U64 sum_gap;

    KmppWorker *worker;
};

struct vcodec_threads {
    void *check;
    struct vcodec_module *module;

    /* lock for global config and state change */
    KmppSpinlock *lock;

    enum vcodec_threads_status status;
    int change;
    struct vcodec_threads_config cfg;
    struct vcodec_threads_config set;
    unsigned long run_state;
    RK_U64 queue_cnt;
    RK_U64 miss_cnt;

    /* unique work context for one thread pool */
    struct vcodec_thread *worker;
};

struct vcodec_threads *vcodec_thread_create(struct vcodec_module *module);
int vcodec_thread_destroy(struct vcodec_threads *thds);

int vcodec_thread_set_count(struct vcodec_threads *thds, int count);
int vcodec_thread_set_callback(struct vcodec_threads *thds,
                               vcodec_work_func_t callback, void *param);

int vcodec_thread_start(struct vcodec_threads *thds);
int vcodec_thread_stop(struct vcodec_threads *thds);
int vcodec_thread_pause(struct vcodec_threads *thds);
int vcodec_thread_resume(struct vcodec_threads *thds);

int vcodec_thread_trigger(struct vcodec_threads *thds);

#endif

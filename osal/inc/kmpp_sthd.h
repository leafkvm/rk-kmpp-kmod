/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_STHD_H__
#define __KMPP_STHD_H__

#include "rk_type.h"

typedef rk_s32 (*KmppFunc)(void *);

/*
 * self for thread group pointer
 * idx -1 for thread group
 * idx not negtive for thread in thread group
 * ctx for thread context for each thread
 */
typedef struct KmppThds_t {
    void        *self;
    void        *ctx;
    rk_s32      idx;
} KmppThds;

/* multi-thread group with same callback and context */
rk_s32 kmpp_thds_init(KmppThds **thds, const char *name, RK_S32 count);
void kmpp_thds_deinit(KmppThds **thds);

void kmpp_thds_setup(KmppThds *thds, KmppFunc func, void *ctx);
KmppThds *kmpp_thds_get_entry(KmppThds *thds, rk_s32 idx);
const char *kmpp_thds_get_name(KmppThds *thds);

/* single thread function */
void kmpp_thds_lock(KmppThds *thds);
void kmpp_thds_unlock(KmppThds *thds);
rk_s32 kmpp_thds_run_nl(KmppThds *thds);

#endif /* __KMPP_STHD_H__ */
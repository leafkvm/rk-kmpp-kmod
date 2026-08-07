/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_DQ_H__
#define __KMPP_DQ_H__

#include "rk_type.h"

typedef struct KmppDq_t  KmppDq;
typedef struct KmppDqData_t {
    rk_s32 state;
    rk_s32 idx;
    void *data;
} KmppDqData;

typedef rk_s32 (*KmppDqFilter)(KmppDqData *data, void *ctx);

typedef struct KmppDqCfg_t {
    /* data queue node state count */
    rk_s32 state_cnt;
    /* data size for each queue node */
    rk_s32 node_size;
    /* data node count in the data queue */
    rk_s32 node_cnt;
    /* lock or nolock for each operation */
    rk_s32 no_lock;

    /* preallocted buffer address for data queue nodes */
    void *buf;
    /* preallocted buffer total size */
    rk_s32 size;

    /* the names of each data queue node state, total state_cnt */
    const rk_u8 **names;
    /* context for init / deinit callback */
    void *ctx;
    rk_s32 (*init)(void *ctx, KmppDqData *data);
    rk_s32 (*deinit)(void *ctx, KmppDqData *data);
} KmppDqCfg;

rk_s32 kmpp_dq_calc_size(KmppDqCfg *cfg);
rk_s32 kmpp_dq_init(KmppDq **dq, KmppDqCfg *cfg);
rk_s32 kmpp_dq_deinit(KmppDq *dq);

rk_s32 kmpp_dq_count(KmppDq *dq, rk_s32 state);
const rk_u8 *kmpp_dq_counts(KmppDq *dq);

rk_s32 kmpp_dq_get(KmppDq *dq, KmppDqData **data, rk_s32 state);
rk_s32 kmpp_dq_get2(KmppDq *dq, KmppDqData **data, rk_s32 state, rk_s32 state2);
rk_s32 kmpp_dq_move(KmppDq *dq, KmppDqData *data, rk_s32 next);
rk_s32 kmpp_dq_get_and_move(KmppDq *dq, KmppDqData **data, rk_s32 state, rk_s32 next);
rk_s32 kmpp_dq_filter_and_get(KmppDq *dq, KmppDqData **data, rk_s32 state, KmppDqFilter filter, void *ctx);

#endif /* __KMPP_DQ_H__ */
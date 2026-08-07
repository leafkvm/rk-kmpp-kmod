/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define  MODULE_TAG "kmpp_dq"

#include "kmpp_list.h"

#include "kmpp_log.h"
#include "kmpp_mem.h"
#include "kmpp_string.h"
#include "kmpp_spinlock.h"

#include "kmpp_dq.h"

typedef struct KmppDqList_t {
    kmpp_list_head list;
    const rk_u8 *name;
    rk_u32 state;
    rk_s32 count;
} KmppDqState;

typedef struct KmppDqNode_t {
    kmpp_list_head list;
    KmppDq *queue;
    KmppDqState *state;

    KmppDqData data;
} KmppDqNode;

struct KmppDq_t {
    KmppDqCfg cfg;
    rk_u8 counts[64];
    KmppSpinlock *lock;
    KmppDqState *state;
    KmppDqNode *nodes;
    void *data;
};

rk_s32 kmpp_dq_calc_size(KmppDqCfg *cfg)
{
    rk_s32 state_cnt = cfg->state_cnt;
    rk_s32 node_size = cfg->node_size;
    rk_s32 node_cnt = cfg->node_cnt;
    rk_s32 no_lock = cfg->no_lock;
    rk_s32 impl_size = sizeof(KmppDq);
    rk_s32 lock_size = no_lock ? 0 : kmpp_spinlock_size();
    rk_s32 state_size = sizeof(KmppDqState) * state_cnt;
    rk_s32 node_all_size = sizeof(KmppDqNode) + node_size;
    rk_s32 node_all = node_all_size * node_cnt;

    return impl_size + lock_size + state_size + node_all;
}

rk_s32 kmpp_dq_init(KmppDq **dq, KmppDqCfg *cfg)
{
    KmppDq *queue = NULL;
    rk_s32 calc_size;
    rk_s32 lock_size;
    rk_s32 i;

    if (!dq || !cfg || !cfg->buf) {
        kmpp_loge_f("invalid param dq %px cfg %px buf %px\n", dq, cfg, cfg ? cfg->buf : NULL);
        return rk_nok;
    }

    calc_size = kmpp_dq_calc_size(cfg);
    if (cfg->size < calc_size) {
        kmpp_loge_f("size %d is too small need %d\n", cfg->size, calc_size);
        *dq = NULL;
        return rk_nok;
    }

    lock_size = cfg->no_lock ? 0 : kmpp_spinlock_size();

    queue = (KmppDq *)cfg->buf;
    *dq = queue;

    queue->cfg = *cfg;
    queue->state = (KmppDqState *)(cfg->buf + sizeof(KmppDq) + lock_size);
    queue->nodes = (KmppDqNode *)(queue->state + cfg->state_cnt);
    queue->data = (void *)(queue->nodes + cfg->node_cnt);

    if (lock_size)
        kmpp_spinlock_assign(&queue->lock, (cfg->buf + sizeof(KmppDq)), lock_size);
    else
        queue->lock = NULL;

    for (i = 0; i < cfg->state_cnt; i++) {
        KmppDqState *state = &queue->state[i];

        KMPP_INIT_LIST_HEAD(&state->list);
        state->name = cfg->names ? cfg->names[i] : NULL;
        state->state = i;
        state->count = 0;
    }

    for (i = 0; i < cfg->node_cnt; i++) {
        KmppDqNode *node = &queue->nodes[i];

        KMPP_INIT_LIST_HEAD(&node->list);
        node->queue = queue;
        node->state = &queue->state[0];
        node->data.state = 0;
        node->data.idx = i;
        node->data.data = queue->data + i * cfg->node_size;
        kmpp_list_add_tail(&node->list, &queue->state[0].list);
        queue->state[0].count++;
    }

    if (cfg->init && cfg->ctx) {
        KmppDqNode *node, *n;

        kmpp_spin_lock(queue->lock);
        kmpp_list_for_each_entry_safe(node, n, &queue->state[0].list, KmppDqNode, list) {
            cfg->init(cfg->ctx, &node->data);
        }
        kmpp_spin_unlock(queue->lock);
    }

    return rk_ok;
}

rk_s32 kmpp_dq_deinit(KmppDq *dq)
{
    if (dq) {
        if (dq->cfg.deinit) {
            KmppDqNode *node, *n;

            kmpp_spin_lock(dq->lock);
            kmpp_list_for_each_entry_safe(node, n, &dq->state[0].list, KmppDqNode, list) {
                dq->cfg.deinit(dq->cfg.ctx, &node->data);
            }
            kmpp_spin_unlock(dq->lock);
        }

        kmpp_spinlock_deinit(&dq->lock);
    }

    return rk_ok;
}

rk_s32 kmpp_dq_count(KmppDq *dq, rk_s32 state)
{
    rk_s32 count;

    kmpp_spin_lock(dq->lock);
    count = dq->state[state].count;
    kmpp_spin_unlock(dq->lock);

    return count;
}

const rk_u8 *kmpp_dq_counts(KmppDq *dq)
{
    rk_u8 *buf = dq->counts;
    rk_s32 state_cnt = dq->cfg.state_cnt;
    rk_s32 left = sizeof(dq->counts) - 1;
    rk_s32 len = 0;
    rk_s32 i;

    kmpp_spin_lock(dq->lock);
    for (i = 0; i < state_cnt; i++) {
        len = kmpp_snprintf(buf, left, "%d:", dq->state[i].count);
        left -= len;
        buf += len;
        if (i == state_cnt - 1)
            buf[-1] = '\0';
    }
    kmpp_spin_unlock(dq->lock);

    return dq->counts;
}

rk_s32 kmpp_dq_get(KmppDq *dq, KmppDqData **data, rk_s32 state)
{
    KmppDqNode *node;

    if (!dq || !data || state >= dq->cfg.state_cnt) {
        kmpp_loge_f("invalid param dq %px data %px state %d\n", dq, data, state);
        return rk_nok;
    }

    kmpp_spin_lock(dq->lock);
    node = kmpp_list_first_entry_or_null(&dq->state[state].list, KmppDqNode, list);
    kmpp_spin_unlock(dq->lock);

    *data = node ? &node->data : NULL;
    return node ? rk_ok : rk_nok;
}

rk_s32 kmpp_dq_get2(KmppDq *dq, KmppDqData **data, rk_s32 state, rk_s32 state2)
{
    KmppDqNode *node;

    if (!dq || !data || state >= dq->cfg.state_cnt || state2 >= dq->cfg.state_cnt) {
        kmpp_loge_f("invalid param dq %px data %px state %d %d\n", dq, data, state, state2);
        return rk_nok;
    }

    kmpp_spin_lock(dq->lock);
    node = kmpp_list_first_entry_or_null(&dq->state[state].list, KmppDqNode, list);
    if (!node)
        node = kmpp_list_first_entry_or_null(&dq->state[state2].list, KmppDqNode, list);
    kmpp_spin_unlock(dq->lock);

    *data = node ? &node->data : NULL;
    return node ? rk_ok : rk_nok;
}

rk_s32 kmpp_dq_move(KmppDq *dq, KmppDqData *data, rk_s32 next)
{
    KmppDqNode *node;

    if (!dq || !data || data->idx >= dq->cfg.node_cnt || next >= dq->cfg.state_cnt) {
        kmpp_loge_f("invalid param dq %px data %px\n", dq, data);
        return rk_nok;
    }

    node = dq->nodes + data->idx;

    kmpp_spin_lock(dq->lock);
    if (node->data.state == node->state->state) {
        rk_u32 state = node->data.state;

        dq->state[state].count--;
        dq->state[next].count++;
        kmpp_list_move_tail(&node->list, &dq->state[next].list);
        node->data.state = next;
        node->state = &dq->state[next];
    } else {
        kmpp_loge_f("data %d state can not change %d:%s -> %d:%s\n", node->data.idx,
                    node->data.state, dq->state[node->data.state].name,
                    next, dq->state[next].name);
    }
    kmpp_spin_unlock(dq->lock);

    return rk_ok;
}

rk_s32 kmpp_dq_get_and_move(KmppDq *dq, KmppDqData **data, rk_s32 state, rk_s32 next)
{
    KmppDqNode *node;

    if (!dq || !data || state >= dq->cfg.state_cnt || next >= dq->cfg.state_cnt) {
        kmpp_loge_f("invalid param dq %px state %d next %d data %px\n", dq, state, next, data);
        return rk_nok;
    }

    kmpp_spin_lock(dq->lock);
    node = kmpp_list_first_entry_or_null(&dq->state[state].list, KmppDqNode, list);
    if (node) {
        dq->state[state].count--;
        dq->state[next].count++;
        kmpp_list_move_tail(&node->list, &dq->state[next].list);
        node->data.state = next;
        node->state = &dq->state[next];
    }
    kmpp_spin_unlock(dq->lock);

    *data = node ? &node->data : NULL;

    return node ? rk_ok : rk_nok;
}

rk_s32 kmpp_dq_filter_and_get(KmppDq *dq, KmppDqData **data, rk_s32 state, KmppDqFilter filter, void *ctx)
{
    KmppDqNode *node, *n;
    KmppDqData *result = NULL;

    if (!dq || !data || state >= dq->cfg.state_cnt) {
        kmpp_loge_f("invalid param dq %px state %d data %px\n", dq, state, data);
        return rk_nok;
    }

    kmpp_spin_lock(dq->lock);
    if (filter) {
        kmpp_list_for_each_entry_safe(node, n, &dq->state[state].list, KmppDqNode, list) {
            if (!filter(&node->data, ctx)) {
                result = &node->data;
                break;
            }
        }
    } else {
        node = kmpp_list_first_entry_or_null(&dq->state[state].list, KmppDqNode, list);
        if (node)
            result = &node->data;
    }
    kmpp_spin_unlock(dq->lock);

    *data = result;

    return result ? rk_ok : rk_nok;
}

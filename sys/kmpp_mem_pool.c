/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_mem_pool"

#include "kmpp_env.h"
#include "kmpp_log.h"
#include "kmpp_mem.h"
#include "kmpp_macro.h"
#include "kmpp_module.h"
#include "kmpp_atomic.h"
#include "kmpp_string.h"
#include "kmpp_spinlock.h"

#include "kmpp_sys.h"
#include "kmpp_sys_id.h"
#include "kmpp_mem_pool.h"

#define POOL_DEG_FLOW               (0x00000001)
#define POOL_DEG_POOL               (0x00000002)
#define POOL_DEG_BUF                (0x00000004)

#define pool_dbg(flag, fmt, ...)    kmpp_dbg(kmpp_mem_pool_debug, flag, fmt, ## __VA_ARGS__)

#define pool_dbg_flow(fmt, ...)     pool_dbg(POOL_DEG_FLOW, fmt, ## __VA_ARGS__)
#define pool_dbg_pool(fmt, ...)     pool_dbg(POOL_DEG_POOL, fmt, ## __VA_ARGS__)
#define pool_dbg_buf(fmt, ...)      pool_dbg(POOL_DEG_BUF, fmt, ## __VA_ARGS__)

static rk_u32 kmpp_mem_pool_debug = 0;

typedef struct KmppMemPoolSrv_t {
    /* list for list_srv in KmppMemPoolImpl */
    kmpp_list_head      list_pool;
    KmppSpinlock        *lock;
    KmppEnvNode         env;
    rk_s32              pool_id;
    rk_s32              pool_cnt;
} KmppMemPoolSrv;

typedef struct MppMemPoolNode_t {
    void                *check;
    /* list to used and unused in KmppMemPoolImpl */
    kmpp_list_head      list;
    void                *ptr;
    rk_s32              size;
    rk_u32              id;
} KmppMemPoolNode;

typedef struct MppMemPoolImpl_t {
    void                *check;
    /* list to list_pool in KmppMemPoolSrv */
    kmpp_list_head      list_srv;
    /* list for list in KmppMemPoolNode */
    kmpp_list_head      used;
    /* list for list in KmppMemPoolNode */
    kmpp_list_head      unused;
    rk_s32              used_count;
    rk_s32              unused_count;
    rk_u32              max_cnt;
    rk_s32              pool_id;
    rk_s32              node_id;
    rk_s32              size;
    const rk_u8         name[0];
} KmppMemPoolImpl;

static KmppMemPoolSrv *srv_mempool = NULL;

#define get_pool_srv(caller) \
    ({ \
        KmppMemPoolSrv *__tmp; \
        if (srv_mempool) { \
            __tmp = srv_mempool; \
        } else { \
            kmpp_err_f("mem pool srv not init at %s : %s\n", __FUNCTION__, caller); \
            __tmp = NULL; \
        } \
        __tmp; \
    })

#define get_pool_srv_f()            get_pool_srv(__FUNCTION__)
#define put_mem_pool_f(srv, pool)   put_mem_pool(srv, pool, __FUNCTION__)

static const rk_u8 space_12[13] = "            ";

static void kmpp_mem_pool_show(KmppEnvNode env, void *data)
{
    KmppMemPoolSrv *srv = get_pool_srv_f();
    KmppMemPoolImpl *pool, *m;

    kmpp_env_log(env, "\n------------------------------------- total %d mem pool -------------------------------------\n", srv->pool_cnt);

    if (!srv->pool_cnt)
        return;


    kmpp_spin_lock(srv->lock);

    kmpp_env_log(env, "|%-4s|%-3s|%-16s|%-12s|%-12s|%-12s|%-12s|%-12s|\n",
                 "pool", "mem", "name", "size", "unused", "used", "total", "max");

    kmpp_list_for_each_entry_safe(pool, m, &srv->list_pool, KmppMemPoolImpl, list_srv) {
        KmppMemPoolNode *node, *n;

        kmpp_env_log(env, "--------------------------------------------------------------------------------------------\n");
        kmpp_env_log(env, "|%-4d|   |%-16s|%-12d|%-12d|%-12d|%-12d|%-12d|\n",
                     pool->pool_id, pool->name, pool->size, pool->unused_count, pool->used_count,
                     pool->unused_count + pool->used_count, pool->max_cnt);

        if (pool->used_count) {
            kmpp_list_for_each_entry_safe(node, n, &pool->used, KmppMemPoolNode, list) {
                kmpp_env_log(env, "|    |%-3d|%-16px|%-12d|%-12s|%-12s|%-12s|%-12s|\n",
                             node->id, node->ptr, node->size, space_12, "used", space_12, space_12);
            }
        }

        if (pool->unused_count) {
            kmpp_list_for_each_entry_safe(node, n, &pool->unused, KmppMemPoolNode, list) {
                kmpp_env_log(env, "|    |%-3d|%-16px|%-12d|%-12s|%-12s|%-12s|%-12s|\n",
                             node->id, node->ptr, node->size, "unused", space_12, space_12, space_12);
            }
        }
    }

    kmpp_spin_unlock(srv->lock);
}


static void put_mem_pool(KmppMemPoolSrv *srv, KmppMemPoolImpl *pool, const rk_u8 *caller)
{
    KmppMemPoolNode *node, *m;
    rk_ul flags;

    if (pool != pool->check) {
        kmpp_err_f("invalid mem pool %2d - %-16s %px check %px at %s\n",
                   pool->pool_id, pool->name, pool, pool->check, caller);
        return;
    }

    kmpp_spin_lock_irqsave(srv->lock, &flags);
    if (!kmpp_list_empty(&pool->list_srv)) {
        kmpp_list_del_init(&pool->list_srv);
        srv->pool_cnt--;
    }
    kmpp_spin_unlock_irqrestore(srv->lock, &flags);

    pool_dbg_flow("pool %2d - %-16s put %4d max:used:unused [%d:%d:%d] at %s\n",
                  pool->pool_id, pool->name, pool->size, pool->max_cnt,
                  pool->used_count, pool->unused_count, caller);

    if (!kmpp_list_empty(&pool->used)) {
        kmpp_err_f("pool %2d - %-16s size %d found %d used buffer at %s\n",
                   pool->pool_id, pool->name, pool->size, pool->used_count, caller);

        kmpp_list_for_each_entry_safe(node, m, &pool->used, KmppMemPoolNode, list) {
            kmpp_list_move_tail(&node->list, &pool->unused);
            pool->used_count--;
            pool->unused_count++;
        }
    }

    kmpp_list_for_each_entry_safe(node, m, &pool->unused, KmppMemPoolNode, list) {
        kmpp_list_del_init(&node->list);
        pool->unused_count--;
        kmpp_free(node);
    }

    if (pool->used_count || pool->unused_count)
        kmpp_err_f("pool %2d - %-16s size %d max %d found leaked buffer used:unused [%d:%d]\n",
                   pool->pool_id, pool->name, pool->size,
                   pool->max_cnt, pool->used_count, pool->unused_count);

    kmpp_free(pool);
}

static rk_s32 kmpp_mem_pool_init(void)
{
    KmppMemPoolSrv *srv;
    rk_s32 lock_size = kmpp_spinlock_size();
    rk_s32 size;

    if (srv_mempool)
        return rk_ok;

    size = sizeof(KmppMemPoolSrv) + lock_size;
    srv = kmpp_calloc(size);
    if (!srv) {
        kmpp_err_f("failed to alloc mem pool srv size %d\n", size);
        return rk_nok;
    }

    KMPP_INIT_LIST_HEAD(&srv->list_pool);
    kmpp_spinlock_assign(&srv->lock, (KmppSpinlock *)(srv + 1), lock_size);

    {
        KmppEnvInfo info;

        info.name = "mem_pool_info";
        info.readonly = 1;
        info.type = KmppEnv_user;
        info.val = NULL;
        info.env_show = kmpp_mem_pool_show;

        kmpp_env_add(sys_get_env(), &srv->env, &info);
    }

    srv_mempool = srv;

    return rk_ok;
}

static rk_s32 kmpp_mem_pool_deinit(void)
{
    KmppMemPoolSrv *srv = get_pool_srv_f();
    KmppMemPoolImpl *pool, *m;

    if (!srv)
        return rk_ok;

    kmpp_list_for_each_entry_safe(pool, m, &srv->list_pool, KmppMemPoolImpl, list_srv) {
        put_mem_pool_f(srv, pool);
    }

    kmpp_free(srv);
    srv_mempool = NULL;

    return rk_ok;
}

KMPP_MODULE_ADD(kmpp_sys, KMPP_SYS_MEM_POOL, mempool, kmpp_mem_pool_init, kmpp_mem_pool_deinit);

KmppMemPool kmpp_mem_get_pool(const rk_u8 *name, rk_s32 size, rk_u32 max_cnt, rk_u32 flag, const rk_u8 *caller)
{
    KmppMemPoolSrv *srv = get_pool_srv(caller);
    KmppMemPoolImpl *pool;
    rk_ul flags;
    rk_s32 str_len;

    if (!srv)
        return NULL;

    if (!name || !size) {
        kmpp_err_f("invalid mem pool name %s size %d\n", name, size);
        return NULL;
    }

    str_len = kmpp_strlen(name) + 1;
    pool = kmpp_calloc_atomic(sizeof(KmppMemPoolImpl) + KMPP_ALIGN(str_len, 4));
    if (!pool) {
        kmpp_err_f("failed to alloc mem pool %2d - %-16s\n", name);
        return NULL;
    }

    pool->check = pool;
    pool->max_cnt = max_cnt;
    pool->size = size;

    kmpp_memcpy((void*)pool->name, name, str_len);

    KMPP_INIT_LIST_HEAD(&pool->list_srv);
    KMPP_INIT_LIST_HEAD(&pool->used);
    KMPP_INIT_LIST_HEAD(&pool->unused);

    kmpp_spin_lock_irqsave(srv->lock, &flags);
    kmpp_list_add_tail(&pool->list_srv, &srv->list_pool);
    pool->pool_id = srv->pool_id++;
    srv->pool_cnt++;
    kmpp_spin_unlock_irqrestore(srv->lock, &flags);

    pool_dbg_flow("pool %2d - %-16s get size %4d max %3d at %s\n",
                  pool->pool_id, name, pool->size, pool->max_cnt, caller);

    return pool;
}

void kmpp_mem_put_pool(KmppMemPool pool, const rk_u8 *caller)
{
    KmppMemPoolSrv *srv = get_pool_srv(caller);
    KmppMemPoolImpl *impl = (KmppMemPoolImpl *)pool;

    if (!srv)
        return;

    if (!impl) {
        kmpp_err_f("invalid mem pool %px\n", impl);
        return;
    }

    put_mem_pool(srv, impl, caller);
}

void *kmpp_mem_pool_get(KmppMemPool pool, const rk_u8 *caller)
{
    KmppMemPoolSrv *srv = get_pool_srv(caller);
    KmppMemPoolImpl *impl = (KmppMemPoolImpl *)pool;
    KmppMemPoolNode *node = NULL;
    void* ptr = NULL;
    rk_ul flags;

    if (!srv)
        return NULL;

    kmpp_spin_lock_irqsave(srv->lock, &flags);

    node = kmpp_list_first_entry_or_null(&impl->unused, KmppMemPoolNode, list);
    if (node) {
        kmpp_list_move_tail(&node->list, &impl->used);
        impl->unused_count--;
        impl->used_count++;
        ptr = node->ptr;
        node->check = node;
        goto DONE;
    }

    if (impl->max_cnt > 0 && (impl->unused_count + impl->used_count) >= impl->max_cnt) {
        kmpp_log("pool %2d - %-16s size %d reach max cnt %d\n",
                 impl->pool_id, impl->name, impl->size, impl->max_cnt);
        goto DONE;
    }

    node = kmpp_calloc_atomic(sizeof(KmppMemPoolNode) + KMPP_ALIGN(impl->size, 4));
    if (!node) {
        kmpp_err_f("pool %2d - %-16s failed to create node at %s\n",
                   impl->pool_id, impl->name, caller);
    } else {
        node->check = node;
        node->ptr = (void *)(node + 1);
        node->size = impl->size;
        node->id = impl->node_id++;
        KMPP_INIT_LIST_HEAD(&node->list);
        kmpp_list_add_tail(&node->list, &impl->used);
        impl->used_count++;
        ptr = node->ptr;
    }

DONE:
    pool_dbg_flow("pool %2d - %-16s size %d node_id %d get used:unused [%d:%d] at %s\n",
                  impl->pool_id, impl->name, impl->size, node->id,
                  impl->used_count, impl->unused_count, caller);
    kmpp_spin_unlock_irqrestore(srv->lock, &flags);

    return ptr;
}

void kmpp_mem_pool_put(KmppMemPool pool, void *p, const rk_u8 *caller)
{
    KmppMemPoolSrv *srv = get_pool_srv(caller);
    KmppMemPoolImpl *impl = (KmppMemPoolImpl *)pool;
    KmppMemPoolNode *node = (KmppMemPoolNode *)((rk_u8 *)p - sizeof(KmppMemPoolNode));
    rk_ul flags;

    if (!srv)
        return;

    if (impl != impl->check) {
        kmpp_err_f("invalid mem pool %2d - %-16s %px check %px\n",
                   impl->pool_id, impl->name, impl, impl->check);
        return;
    }

    if (node != node->check) {
        kmpp_err_f("invalid mem pool %2d - %-16s ptr %px node %px check %px\n",
                   impl->pool_id, impl->name, p, node, node->check);
        return;
    }

    kmpp_memset(node->ptr, 0, node->size);

    kmpp_spin_lock_irqsave(srv->lock, &flags);

    kmpp_list_move_tail(&node->list, &impl->unused);
    impl->used_count--;
    impl->unused_count++;
    node->check = NULL;

    pool_dbg_flow("pool %2d - %-16s size %d node_id %d put used:unused [%d:%d] at %s\n",
                  impl->pool_id, impl->name, impl->size, node->id,
                  impl->used_count, impl->unused_count, caller);

    kmpp_spin_unlock_irqrestore(srv->lock, &flags);
}

void kmpp_mem_pool_dump(KmppMemPool pool, const rk_u8 *caller)
{
    KmppMemPoolImpl *impl = (KmppMemPoolImpl *)pool;
    KmppMemPoolNode *node, *n;

    kmpp_logi_f("dump pool %2d %#px - %s size %4d max:used:unused [%d:%d:%d] at %s\n",
                impl->pool_id, impl, impl->name, impl->size,
                impl->max_cnt, impl->used_count, impl->unused_count, caller);

    if (impl->used_count) {
        kmpp_logi_f("used %d node:\n", impl->used_count);

        kmpp_list_for_each_entry_safe(node, n, &impl->used, KmppMemPoolNode, list) {
            kmpp_logi_f("  %2d [%px:%px] used\n", node->id,
                        node, node->ptr);
        }
    }

    if (impl->unused_count) {
        kmpp_logi_f("unused %d node:\n", impl->unused_count);

        kmpp_list_for_each_entry_safe(node, n, &impl->unused, KmppMemPoolNode, list) {
            kmpp_logi_f("  %2d [%px:%px] unsed\n", node->id,
                        node, node->ptr);
        }
    }
}

void kmpp_mem_pool_dump_all(const rk_u8 *caller)
{
    KmppMemPoolSrv *srv = get_pool_srv(caller);
    KmppMemPoolImpl *pool, *m;
    rk_ul flag;

    if (!srv)
        return;

    kmpp_spin_lock_irqsave(srv->lock, &flag);

    kmpp_logi_f("total %d pool at %s\n", srv->pool_cnt, caller);

    kmpp_list_for_each_entry_safe(pool, m, &srv->list_pool, KmppMemPoolImpl, list_srv) {
        kmpp_mem_pool_dump(pool, caller);
    }

    kmpp_spin_unlock_irqrestore(srv->lock, &flag);
}

#include <linux/export.h>
EXPORT_SYMBOL(kmpp_mem_get_pool);
EXPORT_SYMBOL(kmpp_mem_put_pool);
EXPORT_SYMBOL(kmpp_mem_pool_get);
EXPORT_SYMBOL(kmpp_mem_pool_put);
EXPORT_SYMBOL(kmpp_mem_pool_dump);
EXPORT_SYMBOL(kmpp_mem_pool_dump_all);

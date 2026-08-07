/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/mm.h>
#include <linux/module.h>
#include <linux/vmalloc.h>

#include "kmpp_list.h"
#include "kmpp_env.h"
#include "kmpp_mem.h"
#include "kmpp_log.h"
#include "kmpp_atomic.h"
#include "kmpp_module.h"
#include "kmpp_osal_id.h"

#define MEM_DBG_DEBUG               (0x00000001)
#define MEM_DBG_FLOW                (0x00000002)
#define MEM_DBG_SHOW_DETAIL         (0x00000004)

#define mem_dbg(flag, fmt, ...)     kmpp_dbg(kmpp_mem_debug_rt, flag, fmt, ## __VA_ARGS__)

#define mem_dbg_flow(fmt, ...)      mem_dbg(MEM_DBG_FLOW, fmt, ## __VA_ARGS__)

#define MEM_NODE_FUNC_LEN 32

#define get_mem_srv(caller) \
    ({ \
        KmppMemSrv *__tmp; \
        if (!srv_mem) { \
            kmpp_mem_init(); \
        } \
        if (srv_mem) { \
            __tmp = srv_mem; \
        } else { \
            kmpp_loge_f("kmpp mem srv not init at %s : %s\n", __FUNCTION__, caller); \
            __tmp = NULL; \
        } \
        __tmp; \
    })

typedef struct KmppMemNode_t {
    kmpp_list_head  list;
    void            *ptr;
    rk_u32          size;
    char            func[MEM_NODE_FUNC_LEN];
} KmppMemNode;

typedef struct KmppMemSrv_t {
    spinlock_t      lock;
    /* list for list in KmppMemNode */
    kmpp_list_head  list;
    /* total size of memory */
    rk_s32          mem_size;
} KmppMemSrv;

/* static value on insmod on */
unsigned int kmpp_mem_debug = MEM_DBG_DEBUG;
module_param(kmpp_mem_debug, uint, 0644);
MODULE_PARM_DESC(kmpp_mem_debug, "kmpp memory debug flag on insmod (default: ON)");

/* NOTE: static debug flag update once on insmod */
static rk_s32 kmpp_mem_debug_st;
/* runtime debug flag can be updated by user */
static rk_s32 kmpp_mem_debug_rt;
static KmppMemSrv *srv_mem = NULL;

static rk_s32 kmpp_mem_init(void)
{
    KmppMemSrv *srv = srv_mem;

    if (srv)
        return rk_ok;

    srv = kvzalloc(sizeof(KmppMemSrv), GFP_ATOMIC);
    if (!srv) {
        kmpp_loge_f("kmem srv alloc failed\n");
        return rk_nok;
    }

    srv_mem = srv;

    KMPP_INIT_LIST_HEAD(&srv->list);
    spin_lock_init(&srv->lock);

    kmpp_mem_debug_st = kmpp_mem_debug;
    kmpp_mem_debug_rt = kmpp_mem_debug;

    return rk_ok;
}

static rk_s32 kmpp_mem_deinit(void)
{
    KmppMemSrv *srv = KMPP_FETCH_ADD(&srv_mem, NULL);
    KmppMemNode *node, *next;
    KMPP_LIST_HEAD(list);
    unsigned long flags;

    if (!srv)
        return rk_ok;

    if (srv->mem_size)
        kmpp_loge("%d mem leaked\n", srv->mem_size);

    spin_lock_irqsave(&srv->lock, flags);
    kmpp_list_replace_init(&srv->list, &list);
    spin_unlock_irqrestore(&srv->lock, flags);

    kmpp_list_for_each_entry_safe(node, next, &list, KmppMemNode, list) {
        kmpp_loge("%s: %px, size: %d\n", node->func, node->ptr, node->size);
        kmpp_list_del_init(&node->list);
        kvfree(node);
    }

    kvfree(srv);

    return rk_ok;
}

KMPP_MODULE_ADD(kmpp_osal, KMPP_kmpp_mem, mem, kmpp_mem_init, kmpp_mem_deinit);

static inline void *kmem_add(void *p, const char *func, rk_u32 size)
{
    KmppMemNode *node = (KmppMemNode *)p;
    KmppMemSrv *srv = get_mem_srv(func);

    node->ptr = p + sizeof(KmppMemNode);
    node->size = size;

    snprintf(node->func, sizeof(node->func) - 1, "%s", func);

    if (srv) {
        spin_lock(&srv->lock);
        kmpp_list_add_tail(&node->list, &srv->list);
        KMPP_FETCH_ADD(&srv->mem_size, size);
        spin_unlock(&srv->lock);
    }

    mem_dbg_flow("kmem_add %px size %6d (%8d) at %s\n",
                 p, size, srv ? srv->mem_size : 0, func);

    return node->ptr;
}

static inline void *kmem_del(void *p, const char *func)
{
    KmppMemNode *node = (KmppMemNode *)((rk_u8 *)p - sizeof(KmppMemNode));
    KmppMemSrv *srv = get_mem_srv(func);

    mem_dbg_flow("kmem_del %px size %6d (%8d) at %-20s (%s)\n",
                 node, node->size, srv ? srv->mem_size : 0, node->func, func);

    if (srv) {
        spin_lock(&srv->lock);
        if (node->ptr != p)
            kmpp_loge("kmem_del mismatch: node %px input %px at %s\n",
                      node->ptr, p, func);

        kmpp_list_del_init(&node->list);
        KMPP_FETCH_SUB(&srv->mem_size, node->size);
        spin_unlock(&srv->lock);
    }

    return node;
}

void *osal_kmalloc_f(const char *func, rk_u32 size, rk_u32 gfp)
{
    void *p;

    if (!kmpp_mem_debug_st) {
        p = kvmalloc(size, gfp ? GFP_ATOMIC : GFP_KERNEL);
    } else {
        p = kvmalloc(size + sizeof(KmppMemNode), gfp ? GFP_ATOMIC : GFP_KERNEL);
        if (!IS_ERR_OR_NULL(p)) {
            p = kmem_add(p, func, size);
        }
    }

    return p;
}

void *osal_kcalloc_f(const char *func, rk_u32 size, rk_u32 gfp)
{
    void *p;

    if (!kmpp_mem_debug_st) {
        p = kvzalloc(size, gfp ? GFP_ATOMIC : GFP_KERNEL);
    } else {
        p = kvzalloc(size + sizeof(KmppMemNode), gfp ? GFP_ATOMIC : GFP_KERNEL);
        if (!IS_ERR_OR_NULL(p))
            p = kmem_add(p, func, size);
    }

    return p;
}

void *osal_krealloc_f(const char *func, void *ptr, rk_u32 oldsize, rk_u32 newsize, rk_u32 gfp)
{
    void *p;

    if (!kmpp_mem_debug_st) {
        p = kvrealloc(ptr, oldsize, newsize, gfp ? GFP_ATOMIC : GFP_KERNEL);
    } else {
        if (ptr)
            ptr = kmem_del(ptr, func);

        p = kvrealloc(ptr, oldsize + sizeof(KmppMemNode), newsize + sizeof(KmppMemNode),
                      gfp ? GFP_ATOMIC : GFP_KERNEL);
        if (!IS_ERR_OR_NULL(p))
            p = kmem_add(p, func, newsize);
    }

    return p;
}

void osal_kfree_f(const char *func, void *ptr)
{
    if (!IS_ERR_OR_NULL(ptr)) {
        if (kmpp_mem_debug_st)
            ptr = kmem_del(ptr, func);

        kvfree(ptr);
    } else {
        kmpp_loge("can not kfree %px from %s\n", ptr, func);
    }
}

void *osal_malloc_share_f(const char *func, rk_u32 size)
{
    size = PAGE_ALIGN(size);
    return vmalloc_user(size);
}

void osal_kfree_share_f(const char *func, void *ptr)
{
    if (!IS_ERR_OR_NULL(ptr)) {
        vfree(ptr);
    } else {
        kmpp_loge("can not kfree %px from %s\n", ptr, func);
    }
}

EXPORT_SYMBOL(osal_kmalloc_f);
EXPORT_SYMBOL(osal_kcalloc_f);
EXPORT_SYMBOL(osal_krealloc_f);
EXPORT_SYMBOL(osal_kfree_f);
EXPORT_SYMBOL(osal_malloc_share_f);
EXPORT_SYMBOL(osal_kfree_share_f);

static void kmpp_mem_show(KmppEnvNode env, void *data)
{
    KmppMemSrv *srv = srv_mem;
    KmppMemNode *node, *next;
    unsigned long flags;

    if (!srv)
        return;

    if (!kmpp_mem_debug_st) {
        kmpp_env_log(env, "kmpp memory debug flag is not enabled on insmod\n");
        kmpp_env_log(env, "use insmod kmpp.ko kmpp_mem_debug=1 to enable it\n");
    } else {
        spin_lock_irqsave(&srv->lock, flags);

        if (kmpp_mem_debug_rt & MEM_DBG_SHOW_DETAIL) {
            rk_s32 i = 0;

            kmpp_list_for_each_entry_safe(node, next, &srv->list, KmppMemNode, list) {
                kmpp_env_log(env, "%-4d : %px - %6d by %s\n", i, node->ptr, node->size, node->func);
                i++;
            }
        }

        kmpp_env_log(env, "total mem size: %d\n", srv->mem_size);

        spin_unlock_irqrestore(&srv->lock, flags);
    }
}

void kmpp_mem_env_init(void)
{
    KmppEnvInfo info;

    info.type       = KmppEnv_u32;
    info.readonly   = 0;
    info.name       = "kmpp_mem_debug";
    info.val        = &kmpp_mem_debug_rt;
    info.env_show   = NULL;

    kmpp_env_add(kmpp_env_debug(), NULL, &info);

    info.type       = KmppEnv_user;
    info.readonly   = 1;
    info.name       = "mem_info";
    info.val        = NULL;
    info.env_show   = kmpp_mem_show;

    kmpp_env_add(kmpp_env_root(), NULL, &info);
}

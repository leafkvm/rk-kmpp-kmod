/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define  MODULE_TAG "kmpp_sym"

#include <linux/kernel.h>

#include "kmpp_list.h"
#include "kmpp_atomic.h"
#include "kmpp_env.h"
#include "kmpp_macro.h"
#include "kmpp_mem.h"
#include "kmpp_module.h"
#include "kmpp_log.h"
#include "kmpp_string.h"
#include "kmpp_spinlock.h"

#include "kmpp_obj.h"
#include "kmpp_sym.h"
#include "kmpp_sys_id.h"

#define SYM_DBG_FLOW                (0x00000001)
#define SYM_DBG_DETAIL              (0x00000002)
#define SYM_DBG_RUN                 (0x00000004)
#define SYM_DBG_NODES               (0x00000010)
#define SYM_DBG_NODE                (0x00000020)

#define sym_dbg(flag, fmt, ...)     kmpp_dbg(kmpp_sym_debug, flag, fmt, ## __VA_ARGS__)
#define sym_dbg_f(flag, fmt, ...)   kmpp_dbg_f(kmpp_sym_debug, flag, fmt, ## __VA_ARGS__)

#define sym_dbg_flow(fmt, ...)      sym_dbg_f(SYM_DBG_FLOW, fmt, ## __VA_ARGS__)
#define sym_dbg_detail(fmt, ...)    sym_dbg(SYM_DBG_DETAIL, fmt, ## __VA_ARGS__)
#define sym_dbg_run(fmt, ...)       sym_dbg(SYM_DBG_RUN, fmt, ## __VA_ARGS__)
#define sym_dbg_nodes(fmt, ...)     sym_dbg(SYM_DBG_NODES | SYM_DBG_DETAIL, fmt, ## __VA_ARGS__)
#define sym_dbg_node(fmt, ...)      sym_dbg(SYM_DBG_NODE | SYM_DBG_DETAIL, fmt, ## __VA_ARGS__)

typedef struct KmppSymNodes_t       KmppSymNodes;
typedef struct KmppSymNode_t        KmppSymNode;
typedef struct KmppSymDefImpl_t     KmppSymDefImpl;

typedef struct KmppSymMgr_t {
    KmppSpinlock    *lock;
    /* symbol manager node */
    kmpp_list_head  list_nodes;
    rk_s32          nodes_count;
    rk_s32          nodes_id;

    /* symbol implement node */
    kmpp_list_head  list_symdef;
    rk_s32          symdef_count;
} KmppSymMgr;

/* symbol node is for symbol life time management */
struct KmppSymNode_t {
    /* list to KmppSymNodes list_node and list_idle */
    kmpp_list_head  list_nodes;

    KmppSymNodes    *nodes;
    const rk_u8     *name;
    void            *root;
    rk_s32          buf_size;
    rk_s32          nodes_id;
    rk_s32          id;
    rk_s32          used_count;
};

/* symbol nodes is the link between KmppSymMgr and KmppSymDefImpl */
struct KmppSymNodes_t {
    KmppSpinlock    *lock;
    /* list to KmppSymMgr list_node */
    kmpp_list_head  list_mgr;
    /* list for KmppSymNode list_nodes */
    kmpp_list_head  list_node;
    /* list for KmppSymsImpl list_nodes */
    kmpp_list_head  list_syms;
    rk_u8           *name;
    KmppSymMgr      *mgr;
    /* symbol node which is on effect */
    KmppSymNode     *working;
    rk_s32          id;
    rk_s32          syms_count;

    rk_s32          node_id;
    rk_s32          syms_id;
};

/* symbol provider */
struct KmppSymDefImpl_t {
    kmpp_list_head  list_mgr;

    KmppSymMgr      *mgr;
    KmppSymNodes    *nodes;
    KmppSymNode     *node;

    rk_u8           *name;
    const rk_u8     *caller;
    rk_s32          name_len;
    KmppTrie        trie;
};

/* symbol user */
typedef struct KmppSymsImpl_t {
    KmppSpinlock    *lock;
    kmpp_list_head  list_nodes;
    kmpp_list_head  list_sym;
    kmpp_list_head  list_sym_running;

    KmppSymNodes    *nodes;
    const rk_u8     *name;

    KmppSymNode     *node;
    void            *root;

    rk_s32          id;
    rk_s32          sym_total;
} KmppSymsImpl;

typedef struct KmppSymImpl_t {
    KmppSpinlock    *lock;
    kmpp_list_head  list_syms;

    KmppSymNode     *node;
    KmppSymsImpl    *syms;
    rk_u8           *name;
    void            *root;
    KmppFuncPtr     func;
} KmppSymImpl;

static KmppSymMgr *kmpp_sym_mgr = NULL;
static KmppEnvNode kmpp_sym_env = NULL;
static rk_u32 kmpp_sym_debug = 0;

static rk_s32 get_nodes_id(KmppSymMgr *mgr)
{
    return mgr ? KMPP_FETCH_ADD(&mgr->nodes_id, 1) : -1;
}

static rk_s32 get_node_id(KmppSymNodes *nodes)
{
    return nodes ? KMPP_FETCH_ADD(&nodes->node_id, 1) : -1;
}

static rk_s32 get_syms_id(KmppSymNodes *nodes)
{
    return nodes ? KMPP_FETCH_ADD(&nodes->syms_id, 1) : -1;
}

static KmppSymNode *get_node(KmppSymDefImpl *sym, KmppSymNodes *nodes)
{
    KmppTrie trie = KMPP_FETCH_AND(&sym->trie, NULL);
    KmppSymNode *node;
    rk_s32 node_size;
    rk_s32 buf_size;

    if (!trie) {
        kmpp_loge_f("sym %s can not install twice\n", sym->name);
        return NULL;
    }

    kmpp_trie_add_info(trie, NULL, NULL, 0);
    buf_size = kmpp_trie_get_buf_size(trie);
    node_size = sizeof(KmppSymNode) + buf_size;

    node = kmpp_calloc(node_size);
    if (!node) {
        kmpp_loge_f("sym %s create symbol node %d failed\n", sym->name, buf_size);
        return NULL;
    }

    KMPP_INIT_LIST_HEAD(&node->list_nodes);
    node->nodes = nodes;
    node->name = nodes->name;
    node->root = (void *)(node + 1);
    node->buf_size = buf_size;
    node->nodes_id = nodes->id;
    node->id = get_node_id(nodes);
    kmpp_memcpy(node->root, kmpp_trie_get_node_root(trie), buf_size);
    kmpp_trie_deinit(trie);
    sym->nodes = nodes;
    sym->node = node;

    sym_dbg_node("sym nd %s:%d:n%d created size %d:%d\n",
                 node->name, node->nodes_id, node->id, node_size, buf_size);

    return node;
}

static void put_node_nl(KmppSymNode *node, const rk_u8 *caller)
{
    kmpp_list_del_init(&node->list_nodes);

    sym_dbg_node("sym nd %s:%d:n%d free at %s\n",
                 node->name, node->nodes_id, node->id, caller);

    kmpp_free(node);
}

static void put_node(KmppSymNode *node, const rk_u8 *caller)
{
    KmppSymNodes *nodes = node->nodes;
    rk_ul flags;

    kmpp_spin_lock_irqsave(nodes->lock, &flags);
    kmpp_list_del_init(&node->list_nodes);
    kmpp_spin_unlock_irqrestore(nodes->lock, &flags);

    sym_dbg_node("sym nd %s:%d:n%d free at %s\n",
                 node->name, node->nodes_id, node->id, caller);

    kmpp_free(node);
}

static KmppSymNodes *get_nodes(const rk_u8 *name, const rk_u8 *caller)
{
    rk_s32 name_len = kmpp_strlen(name) + 1;
    rk_s32 lock_size = kmpp_spinlock_size();
    rk_s32 nodes_size = sizeof(KmppSymNodes) + lock_size + name_len;
    KmppSymNodes *nodes = kmpp_calloc(nodes_size);

    if (!nodes) {
        kmpp_loge_f("create symbol node %s failed at %s\n", name, caller);
        return NULL;
    }

    kmpp_spinlock_assign(&nodes->lock, (void *)(nodes + 1), lock_size);
    nodes->name = (rk_u8 *)(nodes + 1) + lock_size;
    nodes->mgr = kmpp_sym_mgr;
    kmpp_memcpy(nodes->name, name, name_len);
    KMPP_INIT_LIST_HEAD(&nodes->list_mgr);
    KMPP_INIT_LIST_HEAD(&nodes->list_node);
    KMPP_INIT_LIST_HEAD(&nodes->list_syms);
    nodes->id = get_nodes_id(nodes->mgr);

    sym_dbg_nodes("sym ns %s:%d created size %d at %s\n",
                  nodes->name, nodes->id, nodes_size, caller);

    return nodes;
}

static void put_nodes(KmppSymNodes *nodes, const rk_u8 *caller)
{
    KmppSymMgr *mgr = nodes->mgr;
    KmppSymNode *node, *n;
    rk_ul flags;

    sym_dbg_nodes("sym ns %s:%d free start at %s\n", nodes->name, nodes->id, caller);

    if (mgr) {
        kmpp_spin_lock(mgr->lock);
        kmpp_list_del_init(&nodes->list_mgr);
        mgr->nodes_count--;
        kmpp_spin_unlock(mgr->lock);
    }

    kmpp_spin_lock_irqsave(nodes->lock, &flags);
    kmpp_list_for_each_entry_safe(node, n, &nodes->list_node, KmppSymNode, list_nodes) {
        kmpp_spin_unlock_irqrestore(nodes->lock, &flags);

        put_node(node, caller);

        kmpp_spin_lock_irqsave(nodes->lock, &flags);
    }
    kmpp_spin_unlock_irqrestore(nodes->lock, &flags);

    sym_dbg_nodes("sym ns %s:%d free finish at %s\n", nodes->name, nodes->id, caller);

    kmpp_spinlock_deinit(&nodes->lock);
    kmpp_free(nodes);
}

static void attach_nodes(KmppSymNodes *nodes, KmppSymMgr *mgr)
{
    kmpp_spin_lock(mgr->lock);
    nodes->mgr = mgr;
    kmpp_list_add_tail(&nodes->list_mgr, &mgr->list_nodes);
    mgr->nodes_count++;
    kmpp_spin_unlock(mgr->lock);

    sym_dbg_nodes("sym ns %s:%d attached %d\n",
                  nodes->name, nodes->id, mgr->nodes_count);
}

static KmppSymImpl *get_sym(KmppSymsImpl *syms, const rk_u8 *name)
{
    rk_s32 name_len = kmpp_strlen(name) + 1;
    rk_s32 sym_size = sizeof(KmppSymImpl) + name_len;
    KmppSymImpl *sym = kmpp_calloc(sym_size);

    if (sym) {
        rk_ul flags;

        sym->lock = syms->lock;
        KMPP_INIT_LIST_HEAD(&sym->list_syms);
        sym->syms = syms;
        sym->name = (rk_u8 *)(sym + 1);
        kmpp_memcpy(sym->name, name, name_len);

        kmpp_spin_lock_irqsave(sym->lock, &flags);
        sym->node = syms->node;
        sym->root = syms->root;

        if (syms->root) {
            void *root = syms->root;
            KmppFuncPtr func = NULL;
            KmppTrieInfo *info = kmpp_trie_get_info_from_root(root, name);

            if (info)
                func = *((KmppFuncPtr *)kmpp_trie_info_ctx(info));

            sym->func = func;
        }

        if (syms->node) {
            syms->node->used_count++;
        }

        kmpp_list_add_tail(&sym->list_syms, &syms->list_sym);
        syms->sym_total++;
        kmpp_spin_unlock_irqrestore(sym->lock, &flags);

        sym_dbg_detail("sym %s:%s size %d get root %px func %px\n",
                       syms->name, sym->name, sym_size, sym->root, sym->func);
    } else {
        kmpp_loge_f("sym %s:%s calloc len %d failed\n", syms->name, name, name_len);
    }

    return sym;
}

static void update_sym(KmppSymImpl *sym, const rk_u8 *caller)
{
    KmppSymsImpl *syms = sym->syms;

    if (sym->node != syms->node) {
        KmppSymNode *old_node = sym->node;
        KmppSymNode *new_node = syms->node;
        rk_s32 old_id = old_node ? old_node->id : -1;
        rk_s32 new_id = new_node ? new_node->id : -1;
        void *root = NULL;
        KmppFuncPtr func = NULL;

        sym_dbg_detail("sym %s:%s update node %d -> %d\n",
                       syms->name, sym->name, old_id, new_id);

        if (old_node) {
            rk_s32 old_used = old_node->used_count--;

            sym_dbg_detail("sym %s:%s node %d used %d -> %d\n",
                           syms->name, sym->name, old_used, old_node->used_count);

            if (old_node->used_count <= 0)
                put_node_nl(old_node, caller);
        }

        if (new_node && new_node->root) {
            rk_s32 old_used = new_node->used_count++;
            KmppTrieInfo *info = kmpp_trie_get_info_from_root(new_node->root, sym->name);

            if (info)
                func = *((KmppFuncPtr *)kmpp_trie_info_ctx(info));

            root = new_node->root;

            sym_dbg_detail("sym %s:%s node %d used %d -> %d\n",
                           syms->name, sym->name, old_used, new_node->used_count);
        }

        sym->node = new_node;
        sym->root = root;
        sym->func = func;
    }
}

static void put_sym(KmppSymImpl *sym)
{
    KmppSymNode *node = sym->node;

    if (node) {
        rk_s32 old_used = node->used_count--;

        sym_dbg_detail("sym %s:%s node %d used %d -> %d\n",
                       node->nodes->name, sym->name, node->id,
                       old_used, node->used_count);
    }

    kmpp_free(sym);
}

static KmppSymsImpl *get_syms(KmppSymNodes *nodes)
{
    KmppSymsImpl *impl = kmpp_calloc(sizeof(KmppSymsImpl));

    if (impl) {
        KmppSymNode *node = nodes->working;
        rk_s32 node_id = node ? node->id : -1;
        void *node_root = node ? node->root : NULL;
        rk_s32 old;
        rk_ul flags;

        KMPP_INIT_LIST_HEAD(&impl->list_nodes);
        KMPP_INIT_LIST_HEAD(&impl->list_sym);
        KMPP_INIT_LIST_HEAD(&impl->list_sym_running);
        impl->lock = nodes->lock;
        impl->nodes = nodes;
        impl->name = nodes->name;
        impl->node = node;
        impl->root = node_root;
        impl->id = get_syms_id(nodes);
        impl->sym_total = 0;

        kmpp_spin_lock_irqsave(nodes->lock, &flags);
        kmpp_list_add_tail(&impl->list_nodes, &nodes->list_syms);
        old = nodes->syms_count++;
        kmpp_spin_unlock_irqrestore(nodes->lock, &flags);

        sym_dbg_detail("sym ss %s:%d:%d from nd %d root %px count %d -> %d\n",
                       nodes->name, nodes->id, impl->id, node_id, node_root,
                       old, nodes->syms_count);
    }

    return impl;
}

static void put_syms(KmppSymsImpl *syms)
{
    KmppSymNodes *nodes = syms->nodes;
    rk_s32 old;
    rk_ul flags;

    kmpp_spin_lock_irqsave(nodes->lock, &flags);
    old = nodes->syms_count--;
    kmpp_list_del_init(&syms->list_nodes);
    kmpp_spin_unlock_irqrestore(nodes->lock, &flags);

    sym_dbg_detail("sym ss %s:%d:%d put count %d -> %d\n",
                   syms->name, nodes->id, syms->id, old, nodes->syms_count);

    kmpp_free(syms);
}

static rk_s32 kmpp_sym_init(void)
{
    KmppEnvInfo env;
    rk_s32 lock_size;
    KmppSymMgr *mgr = kmpp_sym_mgr;

    if (mgr) {
        kmpp_loge_f("mgr already init\n");
        return rk_nok;
    }

    env.type = KmppEnv_u32;
    env.readonly = 0;
    env.name = "kmpp_sym_debug";
    env.val = &kmpp_sym_debug;
    env.env_show = NULL;

    kmpp_env_add(kmpp_env_debug(), &kmpp_sym_env, &env);

    lock_size = kmpp_spinlock_size();
    mgr = kmpp_calloc_atomic(sizeof(KmppSymMgr) + lock_size);
    if (!mgr) {
        kmpp_loge_f("mgr calloc failed\n");
        return rk_nok;
    }
    kmpp_sym_mgr = mgr;

    kmpp_spinlock_assign(&mgr->lock, (void *)(mgr + 1), lock_size);
    KMPP_INIT_LIST_HEAD(&mgr->list_nodes);
    KMPP_INIT_LIST_HEAD(&mgr->list_symdef);

    sym_dbg_flow("mgr %px init done\n", mgr);

    return rk_ok;
}

static rk_s32 kmpp_sym_deinit(void)
{
    KmppSymMgr *mgr = KMPP_FETCH_AND(&kmpp_sym_mgr, NULL);
    KmppSymNodes *nodes, *m;
    KmppSymDefImpl *symdef, *n;

    if (!mgr) {
        kmpp_loge_f("mgr not inited\n");
        return rk_nok;
    }

    sym_dbg_flow("mgr %px deinit start\n", mgr);

    kmpp_spin_lock(mgr->lock);
    kmpp_list_for_each_entry_safe(symdef, n, &mgr->list_symdef, KmppSymDefImpl, list_mgr) {
        kmpp_spin_unlock(mgr->lock);

        kmpp_symdef_put_f(symdef, __FUNCTION__);

        kmpp_spin_lock(mgr->lock);
    }
    kmpp_spin_unlock(mgr->lock);

    if (mgr->symdef_count)
        kmpp_loge_f("mgr %px still has %d symdef not released\n", mgr, mgr->symdef_count);

    kmpp_spin_lock(mgr->lock);
    kmpp_list_for_each_entry_safe(nodes, m, &mgr->list_nodes, KmppSymNodes, list_mgr) {
        kmpp_spin_unlock(mgr->lock);

        put_nodes(nodes, __FUNCTION__);

        kmpp_spin_lock(mgr->lock);
    }
    kmpp_spin_unlock(mgr->lock);

    if (mgr->nodes_count)
        kmpp_loge_f("mgr %px still has %d nodes not released\n", mgr, mgr->nodes_count);

    kmpp_spinlock_deinit(&mgr->lock);

    if (kmpp_sym_env) {
        kmpp_env_del(kmpp_env_debug(), kmpp_sym_env);
        kmpp_sym_env = NULL;
    }

    sym_dbg_flow("kmpp_sym_mgr %px deinit done\n", mgr);

    kmpp_free(mgr);

    return rk_ok;
}

KMPP_MODULE_ADD(kmpp_sys, KMPP_SYS_SYM, sym, kmpp_sym_init, kmpp_sym_deinit);

rk_s32 kmpp_symdef_get_f(KmppSymDef *def, const rk_u8 *name, const rk_u8 *caller)
{
    KmppSymMgr *mgr = kmpp_sym_mgr;
    KmppSymDefImpl *impl;
    rk_s32 name_len;

    if (!def || !name || !mgr) {
        kmpp_loge_f("invalid param sym %px name %px mgr %px\n", def, name, mgr);
        return rk_nok;
    }

    name_len = kmpp_strlen(name) + 1;
    impl = kmpp_calloc(sizeof(KmppSymDefImpl) + name_len);
    if (!impl) {
        kmpp_loge_f("KmppSymDef %s len %d calloc failed\n", name, name_len);
    } else {
        KMPP_INIT_LIST_HEAD(&impl->list_mgr);
        impl->mgr = mgr;
        impl->name = (rk_u8 *)(impl + 1);
        impl->caller = caller;
        kmpp_memcpy(impl->name, name, name_len);
        impl->name_len = name_len;
        kmpp_trie_init(&impl->trie, impl->name);

        kmpp_spin_lock(mgr->lock);
        kmpp_list_add_tail(&impl->list_mgr, &mgr->list_symdef);
        mgr->symdef_count++;
        kmpp_spin_unlock(mgr->lock);

        sym_dbg_detail("symdef %s create at %s\n", impl->name, caller);
    }

    *def = (KmppSymDef)impl;

    return impl ? rk_ok : rk_nok;
}

rk_s32 kmpp_symdef_put_f(KmppSymDef def, const rk_u8 *caller)
{
    KmppSymDefImpl *impl = (KmppSymDefImpl *)def;
    KmppSymMgr *mgr = impl ? impl->mgr : NULL;

    if (!impl || !mgr) {
        kmpp_loge_f("invalid param sym %px mgr %px\n",
                    impl, impl ? impl->mgr : NULL);
        return rk_nok;
    }

    kmpp_spin_lock(mgr->lock);
    kmpp_list_del_init(&impl->list_mgr);
    mgr->symdef_count--;
    kmpp_spin_unlock(mgr->lock);

    kmpp_trie_deinit(impl->trie);

    sym_dbg_detail("symdef %s free at %s\n", impl->name, caller);

    kmpp_free(impl);
    return rk_ok;
}

rk_u32 kmpp_symdef_add(KmppSymDef def, const rk_u8 *name, KmppFuncPtr func)
{
    KmppSymDefImpl *impl = (KmppSymDefImpl *)def;

    if (!impl || !name || !func) {
        kmpp_loge_f("invalid param def %px name %s func %px\n", impl, name, func);
        return rk_nok;
    }

    sym_dbg_detail("symdef %s add func %s %px\n", impl->name, name, func);

    return kmpp_trie_add_info(impl->trie, name, &func, sizeof(func));
}

rk_s32 kmpp_symdef_install(KmppSymDef def)
{
    KmppSymDefImpl *impl = (KmppSymDefImpl *)def;
    KmppSymNodes *nodes = NULL;
    KmppSymNodes *pos, *n;
    KmppSymNode *node = NULL;
    KmppSymNode *working = NULL;
    KmppSymNode *free_node = NULL;
    KmppSymMgr *mgr;
    rk_s32 new_node = 0;
    rk_ul flags;

    if (!impl || !impl->mgr) {
        kmpp_loge_f("invalid param sym %px mgr %px\n", impl, impl ? impl->mgr : NULL);
        return rk_nok;
    }

    mgr = impl->mgr;

    kmpp_spin_lock(mgr->lock);
    kmpp_list_for_each_entry_safe(pos, n, &mgr->list_nodes, KmppSymNodes, list_mgr) {
        if (!kmpp_strncmp(pos->name, impl->name, impl->name_len)) {
            sym_dbg_detail("sym %s node %px reinstall\n", impl->name, nodes);
            nodes = pos;
            break;
        }
    }
    kmpp_spin_unlock(mgr->lock);

    if (!nodes) {
        nodes = get_nodes(impl->name, __FUNCTION__);
        if (!nodes)
            goto failed;

        new_node = 1;
    }

    node = get_node(impl, nodes);
    if (!node)
        goto failed;

    kmpp_spin_lock_irqsave(nodes->lock, &flags);

    working = nodes->working;
    nodes->working = node;

    if (working) {
        KmppSymsImpl *syms, *m;

        kmpp_loge_f("sym %s can not install while previous symbol %px is stll working\n",
                    impl->name, nodes->working);

        kmpp_list_del_init(&working->list_nodes);

        kmpp_list_for_each_entry_safe(syms, m, &nodes->list_syms, KmppSymsImpl, list_nodes) {
            KmppSymImpl *sym, *l;

            syms->root = node->root;
            kmpp_list_for_each_entry_safe(sym, l, &syms->list_sym, KmppSymImpl, list_syms) {
                update_sym(sym, __FUNCTION__);
            }
        }

        if (nodes->syms_count) {
            kmpp_list_add_tail(&working->list_nodes, &nodes->list_node);
        } else {
            free_node = working;
        }
    }

    kmpp_spin_unlock_irqrestore(nodes->lock, &flags);

    if (new_node)
        attach_nodes(nodes, mgr);

    if (free_node)
        put_node(free_node, __FUNCTION__);

    sym_dbg_detail("sym ns %s:%d installed %d\n",
                   impl->name, nodes->id, mgr->nodes_count);

    return rk_ok;

failed:
    kmpp_loge_f("sym %s install failed\n", impl->name);
    kmpp_free(node);
    kmpp_free(nodes);

    return rk_nok;
}

rk_s32 kmpp_symdef_uninstall(KmppSymDef def)
{
    KmppSymDefImpl *impl = (KmppSymDefImpl *)def;
    KmppSymNodes *nodes;
    KmppSymNode *node;
    KmppSymsImpl *syms, *n;
    rk_s32 can_free_node = 0;
    rk_ul flags;

    if (!impl) {
        kmpp_loge_f("invalid param def %px\n", impl);
        return rk_nok;
    }

    nodes = impl->nodes;
    node = impl->node;

    if (!nodes || !node) {
        kmpp_loge_f("sym %s can not be uninstall while it is not install\n", impl->name);
        return rk_nok;
    }

    kmpp_spin_lock_irqsave(nodes->lock, &flags);
    if (node != nodes->working)
        kmpp_loge_f("sym %s can not uninstall while it is not working\n", impl->name);

    nodes->working = NULL;
    kmpp_list_move_tail(&node->list_nodes, &nodes->list_node);
    kmpp_list_for_each_entry_safe(syms, n, &nodes->list_syms, KmppSymsImpl, list_nodes) {
        KmppSymImpl *sym, *m;

        syms->root = NULL;
        syms->node = NULL;
        kmpp_list_for_each_entry_safe(sym, m, &syms->list_sym, KmppSymImpl, list_syms) {
            update_sym(sym, __FUNCTION__);
        }
    }

    if (!node->used_count) {
        can_free_node = 1;
        impl->node = NULL;
    }
    kmpp_spin_unlock_irqrestore(nodes->lock, &flags);

    sym_dbg_detail("symdef %s uninstalled\n", impl->name);

    if (can_free_node)
        put_node(node, __FUNCTION__);

    return rk_ok;
}

rk_u32 kmpp_syms_get_f(KmppSyms *syms, const rk_u8 *name, const rk_u8 *caller)
{
    KmppSymMgr *mgr = kmpp_sym_mgr;
    KmppSymNodes *nodes = NULL;
    KmppSymNodes *pos, *n;

    if (!mgr || !syms || !name) {
        kmpp_loge_f("invalid param sym %px name %px mgr %px\n", syms, name, mgr);
        return rk_nok;
    }

    *syms = NULL;

    kmpp_spin_lock(mgr->lock);
    kmpp_list_for_each_entry_safe(pos, n, &mgr->list_nodes, KmppSymNodes, list_mgr) {
        if (!kmpp_strcmp(pos->name, name)) {
            nodes = pos;
            break;
        }
    }
    kmpp_spin_unlock(mgr->lock);

    sym_dbg_detail("sym nd %s lookup ret nodes %d\n", name, nodes ? nodes->id : -1);

    if (!nodes) {
        nodes = get_nodes(name, caller);
        if (!nodes)
            return rk_nok;

        attach_nodes(nodes, mgr);
    }

    *syms = get_syms(nodes);
    return *syms ? rk_ok : rk_nok;
}

rk_u32 kmpp_syms_put_f(KmppSyms syms, const rk_u8 *caller)
{
    KmppSymsImpl *impl = (KmppSymsImpl *)syms;
    KmppSymImpl *sym, *n;
    KMPP_LIST_HEAD(list);
    rk_ul flags;

    if (!impl) {
        kmpp_loge_f("invalid param sym %px\n", impl);
        return rk_nok;
    }

    kmpp_spin_lock_irqsave(impl->lock, &flags);
    kmpp_list_for_each_entry_safe(sym, n, &impl->list_sym, KmppSymImpl, list_syms) {
        kmpp_list_move_tail(&sym->list_syms, &list);
    }
    kmpp_spin_unlock_irqrestore(impl->lock, &flags);

    kmpp_list_for_each_entry_safe(sym, n, &list, KmppSymImpl, list_syms) {
        put_sym(sym);
    }

    put_syms(syms);

    return rk_ok;
}

rk_u32 kmpp_sym_get(KmppSym *sym, KmppSyms syms, const rk_u8 *name)
{
    if (!sym || !syms || !name) {
        kmpp_loge_f("invalid param sym %px syms %px root %px name %s\n",
                    sym, syms, name);
        return rk_nok;
    }

    *sym = get_sym(syms, name);

    return *sym ? rk_ok : rk_nok;
}

rk_u32 kmpp_sym_put(KmppSym sym)
{
    KmppSymImpl *impl = (KmppSymImpl *)sym;

    if (impl && impl->syms) {
        KmppSymsImpl *syms = impl->syms;
        rk_ul flags;

        kmpp_spin_lock_irqsave(impl->lock, &flags);
        kmpp_list_del_init(&impl->list_syms);
        syms->sym_total--;
        kmpp_spin_unlock_irqrestore(impl->lock, &flags);
    }

    kmpp_free(impl);

    return rk_ok;
}

rk_s32 kmpp_sym_run_f(KmppSym sym, void *arg, rk_s32 *ret, const rk_u8 *caller)
{
    KmppSymImpl *impl = (KmppSymImpl *)sym;
    KmppSymsImpl *syms = impl ? impl->syms : NULL;
    rk_s32 ret_flow = rk_nok;
    rk_ul flags;

    if (!impl || !syms) {
        kmpp_loge_f("invalid param sym %px - %px\n", syms, impl);
        return ret_flow;
    }

    kmpp_spin_lock_irqsave(impl->lock, &flags);
    update_sym(impl, "sym_run before");
    kmpp_list_move_tail(&impl->list_syms, &syms->list_sym_running);
    kmpp_spin_unlock_irqrestore(impl->lock, &flags);

    sym_dbg_run("sym %s:%s run func %px at %s\n",
                syms->name, impl->name, impl->func, caller);

    if (impl->func) {
        rk_s32 ret_func = impl->func(arg, caller);

        if (ret)
            *ret = ret_func;

        ret_flow = rk_ok;
    }

    kmpp_spin_lock_irqsave(impl->lock, &flags);
    update_sym(impl, "sym_run after");
    kmpp_list_move_tail(&impl->list_syms, &syms->list_sym);
    kmpp_spin_unlock_irqrestore(impl->lock, &flags);

    return ret_flow;
}

static void dump_sym_node(KmppSymNode *node)
{
    KmppTrie trie = NULL;
    KmppTrieInfo *info;
    const rk_u8 *name;

    kmpp_trie_init_by_root(&trie, node->root);

    info = kmpp_trie_get_info_first(trie);
    do {
        if (!info)
            break;

        name = kmpp_trie_info_name(info);
        if (!name)
            break;

        if (!kmpp_trie_info_name_is_self(name))
            kmpp_logi("symbol %2d - %s\n", info->index, name);

        info = kmpp_trie_get_info_next(trie, info);
    } while (info);

    kmpp_trie_deinit(trie);
}

void kmpp_syms_dump(KmppSyms syms, const rk_u8 *caller)
{
    KmppSymsImpl *impl = (KmppSymsImpl *)syms;
    KmppSymNodes *nodes = impl ? impl->nodes : NULL;

    if (!impl || !nodes) {
        kmpp_loge_f("invalid param sym %px nodes %px\n", impl, nodes);
        return;
    }

    kmpp_logi("dump %s start at %s\n", impl->name, caller);

    if (nodes && nodes->working)
        dump_sym_node(nodes->working);

    kmpp_logi("dump %s end\n", impl->name);
}

void kmpp_syms_dump_all(const rk_u8 *caller)
{
    KmppSymMgr *mgr = kmpp_sym_mgr;
    KmppSymNodes *nodes, *n;
    rk_s32 i;

    if (!mgr) {
        kmpp_loge_f("invalid param mgr %px at %s\n", mgr, caller);
        return;
    }

    kmpp_logi_f("total %d symbol define at %s\n", mgr->symdef_count, caller);

    i = 0;

    kmpp_spin_lock(mgr->lock);

    kmpp_list_for_each_entry_safe(nodes, n, &mgr->list_nodes, KmppSymNodes, list_mgr) {
        kmpp_spin_unlock(mgr->lock);

        kmpp_logi("symdef %2d - %s total %d using\n",
                  i, nodes->name, nodes->syms_count);
        i++;

        if (nodes->working)
            dump_sym_node(nodes->working);

        kmpp_spin_lock(mgr->lock);
    }
    kmpp_spin_unlock(mgr->lock);

    kmpp_logi_f("end\n");
}

EXPORT_SYMBOL(kmpp_symdef_get_f);
EXPORT_SYMBOL(kmpp_symdef_put_f);
EXPORT_SYMBOL(kmpp_symdef_add);
EXPORT_SYMBOL(kmpp_symdef_install);
EXPORT_SYMBOL(kmpp_symdef_uninstall);
EXPORT_SYMBOL(kmpp_syms_get_f);
EXPORT_SYMBOL(kmpp_syms_put_f);
EXPORT_SYMBOL(kmpp_sym_get);
EXPORT_SYMBOL(kmpp_sym_put);
EXPORT_SYMBOL(kmpp_sym_run_f);
EXPORT_SYMBOL(kmpp_syms_dump);
EXPORT_SYMBOL(kmpp_syms_dump_all);

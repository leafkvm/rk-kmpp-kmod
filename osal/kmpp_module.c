/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_module"

#define pr_fmt(fmt) MODULE_TAG ": " fmt

#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/printk.h>

#include "kmpp_mem.h"
#include "kmpp_string.h"
#include "kmpp_module.h"

#define module_dbg(fmt, ...) \
    do { \
        if (kmpp_module_debug) \
            pr_info(fmt, ##__VA_ARGS__); \
    } while (0)

typedef struct KmppModuleImpl_t {
    rk_s32      count;
    rk_s32      str_len;
    const rk_u8 *name;
    void        *start;
    void        *end;
    KmppModInfo *node[0];
} KmppModuleImpl;

static rk_u32 kmpp_module_debug = 0;

static int cmp_module_id(const void *p0, const void *p1)
{
    KmppModInfo *a = *(KmppModInfo **)p0;
    KmppModInfo *b = *(KmppModInfo **)p1;

    return (a->id == b->id) ? 0 : (a->id < b->id) ? -1 : 1;
}

KmppModule kmpp_module_init(const rk_u8 *name, void *start, void *end)
{
    KmppModuleImpl *impl;
    rk_u32 impl_size;
    rk_u32 count;
    rk_u32 i;

    count = (end - start) / sizeof(KmppModInfo *);

    module_dbg("module %s %px:%px %d init enter\n", name, start, end, count);

    if (!count)
        return NULL;

    impl_size = sizeof(KmppModuleImpl) + count * sizeof(KmppModInfo *);
    impl = kvzalloc(impl_size, GFP_ATOMIC);
    if (!impl) {
        pr_err("kmpp_calloc count %d module %s %px:%px %d failed\n",
               count, name, start, end, impl_size);
        return NULL;
    }

    impl->count = count;
    impl->name = name;
    impl->start = start;
    impl->end = end;
    kmpp_memcpy(impl->node, start, count * sizeof(KmppModInfo *));
    sort(impl->node, count, sizeof(KmppModInfo *), cmp_module_id, NULL);

    /* get sub-module max name length */
    for (i = 0; i < count; i++) {
        KmppModInfo *node = impl->node[i];

        if (node) {
            rk_s32 len = strlen(node->name);

            if (len > impl->str_len)
                impl->str_len = len;
        }
    }

    for (i = 0; i < count; i++) {
        KmppModInfo *node = impl->node[i];

        if (!node) {
            module_dbg("get %-2d - skip NULL node\n", i);
            continue;
        }

        if (node->init) {
            rk_s32 ret = node->init();

            module_dbg("get %-2d - %-2d %-*s -> %s\n",
                       i, node->id, impl->str_len,
                       node->name, !ret ? "success" : "failed");
        } else {
            module_dbg("get %-2d - %-2d %-*s -> skip init\n",
                       i, node->id, impl->str_len, node->name);
        }
    }

    module_dbg("module %s %px:%px %d init leave\n", name, start, end, count);

    return impl;
}

rk_s32 kmpp_module_deinit(KmppModule ctx)
{
    KmppModuleImpl *impl = (KmppModuleImpl *)ctx;

    if (!impl)
        return rk_nok;

    module_dbg("module %s %px:%px %d deinit enter\n",
               impl->name, impl->start, impl->end, impl->count);

    if (impl->count) {
        rk_s32 i;

        for (i = impl->count - 1; i >= 0; i--) {
            KmppModInfo *node = impl->node[i];

            if (!node) {
                module_dbg("put %-2d - skip NULL node\n", i);
                continue;
            }

            if (node->deinit) {
                rk_s32 ret = node->deinit();

                module_dbg("put %-2d - %-2d %-*s -> %s\n",
                           i, node->id, impl->str_len, node->name,
                           !ret ? "success" : "failed");
            } else {
                module_dbg("put %-2d - %-2d %-*s -> skip\n",
                           i, node->id, impl->str_len, node->name);
            }
        }
    }

    module_dbg("module %s %px:%px %d deinit leave\n",
               impl->name, impl->start, impl->end, impl->count);

    kvfree(impl);

    return rk_ok;
}

#include <linux/module.h>

module_param(kmpp_module_debug, uint, 0644);
MODULE_PARM_DESC(kmpp_module_debug, "kmpp module loading process debug information");

EXPORT_SYMBOL(kmpp_module_init);
EXPORT_SYMBOL(kmpp_module_deinit);

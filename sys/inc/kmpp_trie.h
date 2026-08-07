/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_TRIE_H__
#define __KMPP_TRIE_H__

#include "kmpp_string.h"
#include "kmpp_sys_defs.h"

typedef void* KmppTrie;

/*
 * KmppTrie node buffer layout
 * +----------------+
 * |  KmppTrieImpl  |
 * +----------------+
 * |  KmppTrieNodes |
 * +----------------+
 * |  KmppTrieInfos |
 * +----------------+
 *
 * KmppTrieInfo element layout
 * +----------------+
 * |  KmppTrieInfo  |
 * +----------------+
 * |  name string   |
 * +----------------+
 * |  User context  |
 * +----------------+
 */
typedef struct KmppTrieInfo_t {
    rk_u32      index       : 12;
    rk_u32      ctx_len     : 12;
    rk_u32      str_len     : 8;
} KmppTrieInfo;

rk_s32 kmpp_trie_init(KmppTrie *trie, const rk_u8 *name);
rk_s32 kmpp_trie_init_by_root(KmppTrie *trie, void *root);
rk_s32 kmpp_trie_deinit(KmppTrie trie);

rk_s32 kmpp_trie_add_info(KmppTrie trie, const rk_u8 *name, const void *ctx, rk_u32 ctx_len);
rk_s32 kmpp_trie_add_entry(KmppTrie trie, const rk_u8 *name, KmppEntry entry);

rk_s32 kmpp_trie_get_node_count(KmppTrie trie);
rk_s32 kmpp_trie_get_info_count(KmppTrie trie);
rk_s32 kmpp_trie_get_buf_size(KmppTrie trie);
rk_s32 kmpp_trie_get_name_max(KmppTrie trie);
void *kmpp_trie_get_node_root(KmppTrie trie);

/* trie info handle to other elements */
static inline const rk_u8 *kmpp_trie_info_name(KmppTrieInfo *info)
{
    return (info) ? (const rk_u8 *)(info + 1) : NULL;
}

static inline void *kmpp_trie_info_ctx(KmppTrieInfo *info)
{
    return (info) ? (void *)((rk_u8 *)(info + 1) + info->str_len) : NULL;
}

static inline rk_s32 kmpp_trie_info_is_self(KmppTrieInfo *info)
{
    return (info) ? !!kmpp_strstr((const rk_u8 *)(info + 1), "__") : 0;
}

static inline rk_s32 kmpp_trie_info_name_is_self(const rk_u8 *name)
{
    return (name) ? !!kmpp_strstr(name, "__") : 0;
}

/* trie lookup function */
KmppTrieInfo *kmpp_trie_get_info(KmppTrie trie, const rk_u8 *name);
KmppTrieInfo *kmpp_trie_get_info_first(KmppTrie trie);
KmppTrieInfo *kmpp_trie_get_info_next(KmppTrie trie, KmppTrieInfo *info);
/* root base lookup function */
KmppTrieInfo *kmpp_trie_get_info_from_root(void *root, const rk_u8 *name);

void kmpp_trie_dump(KmppTrie trie, const rk_u8 *func);
#define kmpp_trie_dump_f(tire)  kmpp_trie_dump(tire, __FUNCTION__)

void kmpp_trie_timing_test(KmppTrie trie);

#endif /* __KMPP_TRIE_H__ */
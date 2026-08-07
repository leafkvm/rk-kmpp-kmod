/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_trie"

#include <linux/kernel.h>
#include <linux/math64.h>

#include "kmpp_osal.h"
#include "kmpp_trie.h"

#define KMPP_TRIE_DBG_PAVE              (0x00000001)
#define KMPP_TRIE_DBG_SET               (0x00000002)
#define KMPP_TRIE_DBG_GET               (0x00000004)
#define KMPP_TRIE_DBG_CNT               (0x00000008)
#define KMPP_TRIE_DBG_WALK              (0x00000010)
#define KMPP_TRIE_DBG_LAST              (0x00000020)
#define KMPP_TRIE_DBG_LAST_STEP         (0x00000040)
#define KMPP_TRIE_DBG_LAST_CHECK        (0x00000080)
#define KMPP_TRIE_DBG_IMPORT            (0x00000100)

#define trie_dbg(flag, fmt, ...)        kmpp_dbg_f(kmpp_trie_debug, flag, fmt, ## __VA_ARGS__)
#define trie_dbg_pave(fmt, ...)         trie_dbg(KMPP_TRIE_DBG_PAVE, fmt, ## __VA_ARGS__)
#define trie_dbg_set(fmt, ...)          trie_dbg(KMPP_TRIE_DBG_SET, fmt, ## __VA_ARGS__)
#define trie_dbg_get(fmt, ...)          trie_dbg(KMPP_TRIE_DBG_GET, fmt, ## __VA_ARGS__)
#define trie_dbg_cnt(fmt, ...)          trie_dbg(KMPP_TRIE_DBG_CNT, fmt, ## __VA_ARGS__)
#define trie_dbg_walk(fmt, ...)         trie_dbg(KMPP_TRIE_DBG_WALK, fmt, ## __VA_ARGS__)
#define trie_dbg_last(fmt, ...)         trie_dbg(KMPP_TRIE_DBG_LAST, fmt, ## __VA_ARGS__)

#define KMPP_TRIE_KEY_LEN               (4)
#define KMPP_TRIE_KEY_MAX               (1 << (KMPP_TRIE_KEY_LEN))
#define KMPP_TRIE_INFO_MAX              (1 << 12)
#define KMPP_TRIE_NAME_MAX              (1 << 12)

#define DEFAULT_NODE_COUNT              900
#define DEFAULT_INFO_COUNT              80
#define INVALID_NODE_ID                 (-1)
#define KMPP_TRIE_TAG_LEN_MAX           ((sizeof(rk_u64) * 8) / KMPP_TRIE_KEY_LEN)

/* spatial optimized trie tree */
typedef struct KmppTrieNode_t {
    /* next         - next trie node index */
    rk_s16          next[KMPP_TRIE_KEY_MAX];
    /* id           - payload data offset of current trie node */
    rk_s32          id;
    /* idx          - trie node index in ascending order */
    rk_s16          idx;
    /* prev         - previous trie node index */
    rk_s16          prev;

    /* tag_val      - prefix tag */
    rk_u64          tag_val;
    /* key          - current key value in previous node as next */
    rk_u16          key;
    /*
     * tag len      - prefix tag length
     * zero         - normal node with 16 next node
     * positive     - tag node with 64bit prefix tag
     */
    rk_s16          tag_len;

    /* next_cnt     - valid next node count */
    rk_u16          next_cnt;
} KmppTrieNode;

typedef struct KmppTrieImpl_t {
    rk_u8           *name;
    rk_s32          name_len;
    rk_s32          buf_size;

    rk_s32          node_count;
    rk_s32          node_used;
    KmppTrieNode    *nodes;

    /* info and name record buffer */
    void            *info_buf;
    rk_s32          info_count;
    rk_s32          info_buf_size;
    rk_s32          info_buf_pos;
    rk_s32          info_name_max;
} KmppTrieImpl;

/* work structure for trie traversal */
typedef struct KmppTrieWalk_t {
    KmppTrieNode    *root;
    rk_u64          tag;
    rk_u32          len;
    rk_s32          match;
} KmppTrieWalk;

rk_u32 kmpp_trie_debug = 0;

static rk_s32 trie_get_node(KmppTrieImpl *trie, rk_s32 prev, rk_u64 key)
{
    KmppTrieNode *node;
    rk_s32 idx;

    if (trie->node_used >= trie->node_count) {
        rk_s32 old_count = trie->node_count;
        rk_s32 new_count = old_count ? old_count * 2 : DEFAULT_INFO_COUNT;
        KmppTrieNode *new_nodes = kmpp_realloc(trie->nodes, sizeof(KmppTrieNode) * old_count,
                                               sizeof(KmppTrieNode) * new_count);

        if (!new_nodes) {
            kmpp_loge_f("failed to realloc new nodes %d\n", new_count);
            return -1;
        }

        /* NOTE: new memory should be kmpp_memset to zero */
        kmpp_memset(new_nodes + old_count, 0, sizeof(*new_nodes) * old_count);

        trie_dbg_cnt("trie %#px enlarge node %#px:%d -> %#px:%d\n",
                     trie, trie->nodes, trie->node_count, new_nodes, new_count);

        trie->nodes = new_nodes;
        trie->node_count = new_count;
    }

    idx = trie->node_used++;
    node = &trie->nodes[idx];
    node->idx = idx;
    node->prev = (prev > 0) ? prev : 0;
    node->key = key;
    node->id = INVALID_NODE_ID;

    if (prev >= 0)
        trie->nodes[prev].next_cnt++;

    trie_dbg_cnt("get node %d\n", idx);

    return idx;
}

static rk_s32 trie_prepare_buf(KmppTrieImpl *p, rk_s32 info_size)
{
    if (p->info_count >= KMPP_TRIE_INFO_MAX) {
        kmpp_loge_f("invalid trie info count %d larger than max %d\n",
                    p->info_count, KMPP_TRIE_INFO_MAX);
        return rk_nok;
    }

    /* check and enlarge info buffer */
    if (p->info_buf_pos + info_size > p->info_buf_size) {
        rk_s32 old_size = p->info_buf_size;
        rk_s32 new_size = old_size * 2;
        void *ptr = kmpp_realloc(p->info_buf, old_size, new_size);

        if (!ptr) {
            kmpp_loge_f("failed to realloc new info buffer %d\n", new_size);
            return rk_nok;
        }

        trie_dbg_cnt("trie %#px enlarge info_buf %#px:%d -> %#px:%d\n",
                     p, p->info_buf, old_size, ptr, new_size);

        p->info_buf = ptr;
        p->info_buf_size = new_size;
    }

    return rk_ok;
}

static rk_s32 trie_pave_node(KmppTrieImpl *p, const rk_u8 *name, rk_s32 str_len)
{
    KmppTrieNode *node = NULL;
    const rk_u8 *s = (rk_u8 *)name;
    rk_s32 next = 0;
    rk_s32 idx = 0;
    rk_s32 i;

    trie_dbg_pave("trie %#px add info %s len %d\n", p, s, str_len);

    for (i = 0; i < str_len && s[i]; i++) {
        rk_u32 key = s[i];
        rk_s32 key0 = (key >> 4) & 0xf;
        rk_s32 key1 = (key >> 0) & 0xf;

        node = p->nodes + idx;
        next = node->next[key0];

        trie_dbg_pave("trie %#px add %s at %2d rk_u8 %c:%3d:%x:%x node %d -> %d\n",
                      p, name, i, key, key, key0, key1, idx, next);

        if (!next) {
            next = trie_get_node(p, idx, key0);
            if (next < 0)
                return next;

            /* realloc may cause memory address change */
            node = p->nodes + idx;
            node->next[key0] = next;

            trie_dbg_pave("trie %#px add %s at %2d rk_u8 %c:%3d node %d -> %d as new key0\n",
                          p, name, i, key, key, node->idx, next);
        }

        idx = next;
        node = p->nodes + idx;
        next = node->next[key1];

        trie_dbg_pave("trie %#px add %s at %2d rk_u8 %c:%3d:%x:%x node %d -> %d as key0\n",
                      p, s, i, key, key, key0, key1, idx, next);

        if (!next) {
            next = trie_get_node(p, idx, key1);
            if (next < 0)
                return next;

            /* realloc may cause memory address change */
            node = p->nodes + idx;
            node->next[key1] = next;

            trie_dbg_pave("trie %#px add %s at %2d rk_u8 %c:%3d node %d -> %d as new child\n",
                          p, name, i, key, key, node->idx, next);
        }

        idx = next;

        trie_dbg_pave("trie %#px add %s at %2d rk_u8 %c:%3d:%x:%x node %d -> %d as key1\n",
                      p, name, i, key, key, key0, key1, idx, next);
    }

    return idx;
}

rk_s32 kmpp_trie_init(KmppTrie *trie, const rk_u8 *name)
{
    KmppTrieImpl *p;
    rk_s32 name_len;
    rk_s32 ret = rk_nok;

    if (!trie || !name) {
        kmpp_loge_f("invalid trie %#px name %#px\n", trie, name);
        return ret;
    }

    name_len = kmpp_strnlen(name, KMPP_TRIE_NAME_MAX) + 1;
    p = kmpp_calloc(sizeof(*p) + name_len);
    if (!p) {
        kmpp_loge_f("create trie impl %s failed\n", name);
        goto done;
    }

    p->name = (rk_u8 *)(p + 1);
    p->name_len = name_len;
    kmpp_strncpy(p->name, name, name_len);

    p->node_count = DEFAULT_NODE_COUNT;
    p->nodes = kmpp_calloc(sizeof(KmppTrieNode) * p->node_count);
    if (!p->nodes) {
        kmpp_loge_f("create %d nodes failed\n", p->node_count);
        goto done;
    }

    p->info_buf_size = 4096;
    p->info_buf = kmpp_calloc(p->info_buf_size);
    if (!p->info_buf) {
        kmpp_loge_f("failed to alloc %d info buffer\n", p->info_buf_size);
        goto done;
    }

    /* get node 0 as root node*/
    trie_get_node(p, -1, 0);

    ret = rk_ok;

done:
    if (ret && p) {
        kmpp_free(p->info_buf);
        kmpp_free(p->nodes);
        kmpp_free(p);
    }

    *trie = p;
    return ret;
}

rk_s32 kmpp_trie_init_by_root(KmppTrie *trie, void *root)
{
    KmppTrieImpl *p;
    KmppTrieInfo *info;
    rk_s32 i;

    if (!trie || !root) {
        kmpp_loge_f("invalid trie %#px root %#px\n", trie, root);
        return rk_nok;
    }

    *trie = NULL;
    p = kmpp_calloc(sizeof(KmppTrieImpl));
    if (!p) {
        kmpp_loge_f("create trie impl failed\n");
        return rk_nok;
    }

    info = kmpp_trie_get_info_from_root(root, "__name__");
    if (info)
        p->name = kmpp_trie_info_ctx(info);

    info = kmpp_trie_get_info_from_root(root, "__node__");
    if (info)
        p->node_used = *(rk_u32 *)kmpp_trie_info_ctx(info);

    info = kmpp_trie_get_info_from_root(root, "__info__");
    if (info)
        p->info_count = *(rk_u32 *)kmpp_trie_info_ctx(info);

    /* import and update new buffer */
    p->nodes = (KmppTrieNode *)root;

    if (kmpp_trie_debug & KMPP_TRIE_DBG_IMPORT)
        kmpp_trie_dump(p, "root import");

    info = kmpp_trie_get_info_first(p);

    for (i = 0; i < p->info_count; i++) {
        const char *name = kmpp_trie_info_name(info);
        KmppTrieInfo *info_set = info;
        KmppTrieInfo *info_ret = kmpp_trie_get_info(p, name);

        info = kmpp_trie_get_info_next(p, info);

        if (info_ret && info_set == info_ret && info_ret->index == i)
            continue;

        kmpp_loge_f("trie check on import found mismatch info %s [%d:%#px] - [%d:%#px]\n",
                    name, i, info_set, info_ret ? info_ret->index : -1, info_ret);
        return rk_nok;
    }

    *trie = p;

    return rk_ok;
}

rk_s32 kmpp_trie_deinit(KmppTrie trie)
{
    if (trie) {
        KmppTrieImpl *p = (KmppTrieImpl *)trie;

        /* NOTE: do not remvoe imported root */
        if (p->node_count)
            kmpp_free(p->nodes);
        else
            p->nodes = NULL;

        kmpp_free(p->info_buf);
        kmpp_free(p);
        return rk_ok;
    }

    return rk_nok;
}

static rk_s32 kmpp_trie_walk(KmppTrieWalk *p, rk_s32 idx, rk_u32 key, rk_u32 keyx, rk_u32 end)
{
    KmppTrieNode *node = &p->root[idx];
    rk_u8 log_buf[128];
    rk_u32 log_size = sizeof(log_buf) - 1;
    rk_u32 log_len = 0;
    rk_u32 log_en = (kmpp_trie_debug & KMPP_TRIE_DBG_WALK) ? 1 : 0;
    rk_s32 next = -1;

    if (log_en)
        log_len += kmpp_snprintf(log_buf + log_len, log_size - log_len,
                                 "node %d:%d key %02x:%x -> ",
                                 node->idx, node->id, key, keyx);

    /* check high 4 bits */
    if (!node->tag_len || p->match == node->idx) {
        /* not tag or tag has be matched -> normal next switch node */
        next = node->next[keyx];
        if (log_en)
            log_len += kmpp_snprintf(log_buf + log_len, log_size - log_len,
                                     "tag %s -> ",
                                     !node->tag_len ? "n/a" : "match");
        p->tag = 0;
        p->len = 0;
        p->match = -1;
    } else {
        /* with tag check len to fill or to stop */
        rk_u64 val_new = (p->tag << 4) | keyx;
        rk_s32 len_new = p->len + 1;

        if (log_en)
            log_len += kmpp_snprintf(log_buf + log_len, log_size - log_len,
                                     "tag %0*llx:%d - st %0*llx:%d -> ",
                                     node->tag_len, node->tag_val, node->tag_len,
                                     node->tag_len, val_new, len_new);

        /* clear old matched node */
        p->match = -1;
        if (node->tag_len > len_new && !end) {
            /* more tag to fill keep current node */
            p->tag = val_new;
            p->len = len_new;
            next = node->idx;
            if (log_en)
                log_len += kmpp_snprintf(log_buf + log_len, log_size - log_len,
                                         "fill -> ");
        } else {
            /* check tag match with new value */
            p->tag = 0;
            p->len = 0;

            if (node->tag_val != val_new) {
                if (log_en)
                    log_len += kmpp_snprintf(log_buf + log_len, log_size - log_len,
                                             "mismatch -> ");
                next = INVALID_NODE_ID;
            } else {
                if (log_en)
                    log_len += kmpp_snprintf(log_buf + log_len, log_size - log_len,
                                             "match -> ");
                next = node->idx;
                p->match = node->idx;
            }
        }
    }

    if (log_en) {
        kmpp_snprintf(log_buf + log_len, log_size - log_len, "%d", next);
        kmpp_logi_f("%s\n", log_buf);
    }

    return next;
}

static KmppTrieNode *kmpp_trie_get_node(KmppTrieNode *root, const rk_u8 *name)
{
    KmppTrieNode *ret = NULL;
    const rk_u8 *s = name;
    KmppTrieWalk walk;
    rk_s32 idx = 0;

    if (!root || !name) {
        kmpp_loge_f("invalid root %#px name %#px\n", root, name);
        return NULL;
    }

    trie_dbg_get("root %#px search %s start\n", root, name);

    walk.root = root;
    walk.tag = 0;
    walk.len = 0;
    walk.match = -1;

    do {
        rk_u8 key = *s++;
        rk_u32 key0 = (key >> 4) & 0xf;
        rk_u32 key1 = key & 0xf;
        rk_u32 end = (s[0] == '\0');

        idx = kmpp_trie_walk(&walk, idx, key, key0, 0);
        if (idx < 0)
            break;

        idx = kmpp_trie_walk(&walk, idx, key, key1, end);
        if (idx < 0 || end)
            break;
    } while (1);

    ret = (idx >= 0) ? &root[idx] : NULL;

    trie_dbg_get("get %s ret node %d:%d\n", name, idx, ret ? ret->id : INVALID_NODE_ID);

    return ret;
}

static rk_s32 kmpp_trie_check(KmppTrie trie, const rk_u8 *log)
{
    KmppTrieImpl *p = (KmppTrieImpl *)trie;
    rk_u8 *base = p->info_buf;
    rk_s32 info_size = 0;
    rk_s32 pos = 0;
    rk_s32 i;

    for (i = 0; i < p->info_count; i++, pos += info_size) {
        KmppTrieInfo *info = (KmppTrieInfo *)(base + pos);
        const rk_u8 *name = (const rk_u8 *)(info + 1);
        KmppTrieNode *node = kmpp_trie_get_node(p->nodes, name);

        info_size = sizeof(*info) + info->str_len + info->ctx_len;

        if (node && node->id >= 0 && node->id == pos)
            continue;

        kmpp_loge("trie check on %s found mismatch info %s %d - %d\n",
                  log, name, i, node ? node->id : -1);
        return rk_nok;
    }

    return rk_ok;
}

rk_s32 kmpp_trie_last_info(KmppTrie trie)
{
    KmppTrieImpl *p = (KmppTrieImpl *)trie;
    KmppTrieNode *root;
    KmppTrieNode *node;
    rk_u8 *buf;
    rk_s32 node_count;
    rk_s32 node_valid;
    rk_s32 nodes_size;
    rk_s32 len = 0;
    rk_s32 pos = 0;
    rk_s32 i;
    rk_s32 j;

    if (!trie) {
        kmpp_loge_f("invalid NULL trie\n");
        return rk_nok;
    }

    /* write trie self entry info */
    pos = p->info_count + 3;
    kmpp_trie_add_info(trie, "__name__", p->name, p->name_len);
    kmpp_trie_add_info(trie, "__info__", &pos, sizeof(pos));
    /* NOTE: node count need to be update after shrinking */
    kmpp_trie_add_info(trie, "__node__", &p->node_used, sizeof(p->node_used));

    root = p->nodes;
    node_count = p->node_used;
    node_valid = node_count;

    trie_dbg_last("shrink trie node start node %d info %d\n", node_count, p->info_count);

    if (kmpp_trie_debug & KMPP_TRIE_DBG_LAST_STEP)
        kmpp_trie_dump_f(trie);

    for (i = node_count - 1; i > 0; i--) {
        KmppTrieNode *prev;
        rk_s32 prev_idx;

        node = &root[i];
        prev_idx = node->prev;
        prev = &root[prev_idx];

        if (prev->next_cnt > 1) {
            trie_dbg_last("node %d:%d prev %d next count %d stop shrinking for multi next\n",
                          i, node->id, prev_idx, prev->next_cnt);
            continue;
        }

        if (node->tag_len >= (rk_s16)KMPP_TRIE_TAG_LEN_MAX) {
            trie_dbg_last("node %d:%d tag %d - %016llx stop shrinking for max tag len\n",
                          i, node->id, node->tag_len, node->tag_val);
            continue;
        }

        if (prev->id >= 0) {
            trie_dbg_last("node %d:%d tag %d - %016llx stop shrinking for valid info node\n",
                          i, node->id, node->tag_len, node->tag_val);
            continue;
        }

        prev->id = node->id;
        /* NOTE: do NOT increase tag length on root node */
        prev->tag_len = node->tag_len + 1;
        prev->tag_val = ((rk_u64)node->key << (node->tag_len * 4)) | node->tag_val;
        prev->next_cnt = node->next_cnt;
        kmpp_memcpy(prev->next, node->next, sizeof(node->next));

        trie_dbg_last("node %d:%d shrink prev %d key %x tag %016llx -> %016llx\n",
                      i, node->id, prev->idx, prev->key, node->tag_val, prev->tag_val);

        for (j = 0; j < KMPP_TRIE_KEY_MAX; j++) {
            if (!prev->next[j])
                continue;

            root[prev->next[j]].prev = prev_idx;
        }

        kmpp_memset(node, 0, sizeof(*node));
        node->id = INVALID_NODE_ID;
        node_valid--;
    }

    trie_dbg_last("shrink trie node finish count %d -> %d\n", node_count, node_valid);

    if (kmpp_trie_debug & KMPP_TRIE_DBG_LAST_STEP)
        kmpp_trie_dump_f(trie);

    if (kmpp_trie_debug & KMPP_TRIE_DBG_LAST_CHECK)
        kmpp_trie_check(trie, "shrink merge tag stage");

    trie_dbg_last("move trie node start to reduce memory %d -> %d\n", node_count, node_valid);

    for (i = 1; i < node_valid; i++) {
        node = &root[i];

        /* skip valid node */
        if (node->idx)
            continue;

        for (j = i; j < node_count; j++) {
            KmppTrieNode *tmp = &root[j];
            KmppTrieNode *prev;
            rk_s32 k;

            /* skip empty node */
            if (!tmp->idx)
                continue;

            trie_dbg_last("move node %d to %d prev %d\n", j, i, tmp->prev);

            prev = &root[tmp->prev];

            /* relink previous node */
            for (k = 0; k < KMPP_TRIE_KEY_MAX; k++) {
                if (prev->next[k] != tmp->idx)
                    continue;

                prev->next[k] = i;
                break;
            }

            kmpp_memcpy(node, tmp, sizeof(*node));
            node->idx = i;
            kmpp_memset(tmp, 0, sizeof(*tmp));

            /* relink next node */
            for (k = 0; k < KMPP_TRIE_KEY_MAX; k++) {
                if (!node->next[k])
                    continue;

                root[node->next[k]].prev = i;
            }

            break;
        }
    }

    p->node_used = node_valid;

    trie_dbg_last("move trie node finish used %d\n", p->node_used);

    if (kmpp_trie_debug & KMPP_TRIE_DBG_LAST_STEP)
        kmpp_trie_dump_f(trie);

    if (kmpp_trie_debug & KMPP_TRIE_DBG_LAST_CHECK)
        kmpp_trie_check(trie, "shrink move node stage");

    trie_dbg_last("create user buffer start\n");

    nodes_size = sizeof(KmppTrieNode) * p->node_used;
    p->buf_size = nodes_size + p->info_buf_pos;
    p->buf_size = KMPP_ALIGN(p->buf_size, sizeof(void *));

    buf = kmpp_calloc(p->buf_size);
    if (!buf) {
        kmpp_loge("failed to alloc trie buffer size %d\n", p->buf_size);
        return rk_nok;
    }

    p->nodes = (KmppTrieNode *)buf;
    kmpp_memcpy(p->nodes, root, nodes_size);
    pos = nodes_size;
    len = 0;

    for (i = 0; i < p->info_count; i++) {
        KmppTrieInfo *src = (KmppTrieInfo *)(p->info_buf + len);
        KmppTrieInfo *dst = (KmppTrieInfo *)(buf + pos);
        rk_s32 info_size = sizeof(KmppTrieInfo) + src->str_len + src->ctx_len;
        const rk_u8 *name = (rk_u8 *)(src + 1);

        node = kmpp_trie_get_node(p->nodes, name);
        node->id = pos;

        kmpp_memcpy(dst, src, info_size);
        pos += info_size;
        len += info_size;
    }

    kmpp_free(root);
    kmpp_free(p->info_buf);

    /* NOTE: udpate final shrinked node count */
    {
        KmppTrieInfo *info = kmpp_trie_get_info_from_root(p->nodes, "__node__");

        if (info)
            *(rk_u32 *)kmpp_trie_info_ctx(info) = p->node_used;
    }

    return rk_ok;
}

rk_s32 kmpp_trie_add_info(KmppTrie trie, const rk_u8 *name, const void *ctx, rk_u32 ctx_len)
{
    KmppTrieImpl *p = (KmppTrieImpl *)trie;
    rk_s32 info_size;
    rk_s32 ctx_real;
    rk_s32 str_real;
    rk_s32 str_len;
    rk_s32 idx;

    if (!p) {
        kmpp_loge_f("invalid trie %#px name %s ctx %#px\n", p, name, ctx);
        return rk_nok;
    }

    if (!name)
        return kmpp_trie_last_info(p);

    str_real = kmpp_strnlen(name, KMPP_TRIE_NAME_MAX) + 1;
    str_len = KMPP_ALIGN(str_real, 4);
    /* NOTE: align all ctx_len to four bytes */
    ctx_real = ctx_len;
    ctx_len = KMPP_ALIGN(ctx_real, 4);
    info_size = sizeof(KmppTrieInfo) + str_len + ctx_len;

    if (str_len >= KMPP_TRIE_NAME_MAX) {
        kmpp_loge_f("invalid trie name %s len %d larger than max %d\n",
                    name, str_len, KMPP_TRIE_NAME_MAX);
        return rk_nok;
    }

    if (trie_prepare_buf(p, info_size))
        return rk_nok;

    idx = trie_pave_node(p, name, str_real);
    if (idx < 0) {
        kmpp_loge_f("trie %#px pave node %s failed\n", p, name);
        return rk_nok;
    }
    if (p->nodes[idx].id != -1) {
        kmpp_loge_f("trie %#px add info %s already exist\n", p, name);
        return rk_nok;
    }

    p->nodes[idx].id = p->info_buf_pos;
    if (p->info_name_max < str_real - 1)
        p->info_name_max = str_real - 1;

    {
        KmppTrieInfo *info = (KmppTrieInfo *)(p->info_buf + p->info_buf_pos);
        rk_u8 *buf = (rk_u8 *)(info + 1);

        info->index = p->info_count;
        info->ctx_len = ctx_len;
        info->str_len = str_len;

        kmpp_memcpy(buf, name, str_real);
        while (str_real < str_len)
            buf[str_real++] = 0;

        buf += str_len;

        kmpp_memcpy(buf, ctx, ctx_real);
        while (ctx_real < ctx_len)
            buf[ctx_real++] = 0;
    }

    trie_dbg_set("trie %#px add info %d - %s at node %d pos %d ctx %#px size %d\n",
                 p, p->info_count, name, idx, p->info_buf_pos, ctx, ctx_len);

    p->info_buf_pos += info_size;
    p->info_count++;

    return rk_ok;
}

rk_s32 kmpp_trie_add_entry(KmppTrie trie, const rk_u8 *name, KmppEntry entry)
{
    KmppTrieImpl *p = (KmppTrieImpl *)trie;
    rk_s32 info_size;
    rk_s32 str_real;
    rk_s32 str_len;
    rk_s32 idx;

    if (!p) {
        kmpp_loge_f("invalid trie %#px name %s entry %llx\n", p, name, entry.val);
        return rk_nok;
    }

    if (!name)
        return kmpp_trie_last_info(p);

    str_real = kmpp_strnlen(name, KMPP_TRIE_NAME_MAX) + 1;
    str_len = KMPP_ALIGN(str_real, 4);
    info_size = sizeof(KmppTrieInfo) + str_len + sizeof(entry);

    if (str_len >= KMPP_TRIE_NAME_MAX) {
        kmpp_loge_f("invalid trie name %s len %d larger than max %d\n",
                    name, str_len, KMPP_TRIE_NAME_MAX);
        return rk_nok;
    }

    if (trie_prepare_buf(p, info_size))
        return rk_nok;

    idx = trie_pave_node(p, name, str_real);
    if (idx < 0) {
        kmpp_loge_f("trie %#px pave node %s failed\n", p, name);
        return rk_nok;
    }
    if (p->nodes[idx].id != -1) {
        kmpp_loge_f("trie %#px add info %s already exist\n", p, name);
        return rk_nok;
    }

    p->nodes[idx].id = p->info_buf_pos;

    {
        KmppTrieInfo *info = (KmppTrieInfo *)(p->info_buf + p->info_buf_pos);
        rk_u8 *buf = (rk_u8 *)(info + 1);

        info->index = p->info_count;
        info->ctx_len = sizeof(entry);
        info->str_len = str_len;

        kmpp_memcpy(buf, name, str_real);
        while (str_real < str_len)
            buf[str_real++] = 0;

        buf += str_len;
        ((KmppEntry *)buf)->val = entry.val;
    }

    trie_dbg_set("trie %#px add info %d - %s at node %d pos %d entry %llx\n",
                 p, p->info_count, name, idx, p->info_buf_pos, entry.val);

    p->info_buf_pos += info_size;
    p->info_count++;

    return rk_ok;
}

rk_s32 kmpp_trie_get_node_count(KmppTrie trie)
{
    KmppTrieImpl *p = (KmppTrieImpl *)trie;

    return (p) ? p->node_used : 0;
}

rk_s32 kmpp_trie_get_info_count(KmppTrie trie)
{
    KmppTrieImpl *p = (KmppTrieImpl *)trie;

    return (p) ? p->info_count : 0;
}

rk_s32 kmpp_trie_get_buf_size(KmppTrie trie)
{
    KmppTrieImpl *p = (KmppTrieImpl *)trie;

    return (p) ? p->buf_size : 0;
}

rk_s32 kmpp_trie_get_name_max(KmppTrie trie)
{
    KmppTrieImpl *p = (KmppTrieImpl *)trie;

    return (p) ? p->info_name_max : 0;
}

void *kmpp_trie_get_node_root(KmppTrie trie)
{
    KmppTrieImpl *p = (KmppTrieImpl *)trie;

    return (p) ? p->nodes : NULL;
}

KmppTrieInfo *kmpp_trie_get_info(KmppTrie trie, const rk_u8 *name)
{
    KmppTrieImpl *p = (KmppTrieImpl *)trie;
    KmppTrieNode *node;

    if (!trie || !name) {
        kmpp_loge_f("invalid trie %#px name %#px\n", trie, name);
        return NULL;
    }

    node = kmpp_trie_get_node(p->nodes, name);
    if (!node || node->id < 0)
        return NULL;

    return (KmppTrieInfo *)(((rk_u8 *)p->nodes) + node->id);
}

KmppTrieInfo *kmpp_trie_get_info_first(KmppTrie trie)
{
    KmppTrieImpl *p = (KmppTrieImpl *)trie;

    return (p) ? (KmppTrieInfo *)(((rk_u8 *)p->nodes) + p->node_used * sizeof(KmppTrieNode)) : NULL;
}

KmppTrieInfo *kmpp_trie_get_info_next(KmppTrie trie, KmppTrieInfo *info)
{
    KmppTrieImpl *p = (KmppTrieImpl *)trie;

    if (!p || !info || info->index >= p->info_count - 1)
        return NULL;

    return (KmppTrieInfo *)((rk_u8 *)(info + 1) + info->str_len + info->ctx_len);
}

KmppTrieInfo *kmpp_trie_get_info_from_root(void *root, const rk_u8 *name)
{
    KmppTrieNode *node;

    if (!root || !name) {
        kmpp_loge_f("invalid root %#px name %#px\n", root, name);
        return NULL;
    }

    node = kmpp_trie_get_node(root, name);
    if (!node || node->id < 0)
        return NULL;

    return (KmppTrieInfo *)(((rk_u8 *)root) + node->id);
}

void kmpp_trie_dump(KmppTrie trie, const rk_u8 *func)
{
    KmppTrieImpl *p = (KmppTrieImpl *)trie;
    rk_u8 *base = p->info_buf ? (rk_u8 *)p->info_buf : (rk_u8 *)p->nodes;
    rk_s32 i;
    rk_s32 next_cnt[17];
    rk_s32 tag_len[17];

    kmpp_memset(next_cnt, 0, sizeof(next_cnt));
    kmpp_memset(tag_len, 0, sizeof(tag_len));

    kmpp_logi("%s dumping trie %#px\n", func, trie);
    kmpp_logi("name %s size %d node %d info %d\n",
              p->name, p->buf_size, p->node_used, p->info_count);

    for (i = 0; i < p->node_used; i++) {
        KmppTrieNode *node = &p->nodes[i];
        rk_s32 valid_count = 0;
        rk_s32 j;

        if (i && !node->idx)
            continue;

        if (node->id >= 0) {
            KmppTrieInfo *info = (KmppTrieInfo *)((rk_u8 *)base + node->id);

            /* check before and after last info */
            if (node->id < p->node_used * sizeof(KmppTrieNode))
                kmpp_logi("node %d key %x info %d - %s\n", node->idx, node->key, node->id,
                          kmpp_trie_info_name(info));
            else
                kmpp_logi("node %d key %x info %d - %s\n", node->idx, node->key, node->id,
                          kmpp_trie_info_name(info));
        } else
            kmpp_logi("node %d key %x\n", node->idx, node->key);

        if (node->tag_len)
            kmpp_logi("    prev %d count %d tag %d - %llx\n", node->prev, node->next_cnt, node->tag_len, node->tag_val);
        else
            kmpp_logi("    prev %d count %d\n", node->prev, node->next_cnt);

        for (j = 0; j < KMPP_TRIE_KEY_MAX; j++) {
            if (node->next[j] > 0) {
                kmpp_logi("    next %d:%d -> %d\n", valid_count, j, node->next[j]);
                valid_count++;
            }
        }

        next_cnt[valid_count]++;
        tag_len[node->tag_len]++;
    }

    kmpp_logi("node | next |  tag | used %d\n", p->node_used);

    for (i = 0; i < 17; i++) {
        if (next_cnt[i] || tag_len[i])
            kmpp_logi("%2d   | %4d | %4d |\n", i, next_cnt[i], tag_len[i]);
    }

    kmpp_logi("%s dumping node end\n", func, p->node_used);
}

void kmpp_trie_timing_test(KmppTrie trie)
{
    KmppTrieInfo *root = kmpp_trie_get_info_first(trie);
    KmppTrieInfo *info = root;
    rk_s64 sum = 0;
    rk_s32 loop = 0;

    do {
        rk_s64 start, end;

        start = kmpp_time();
        kmpp_trie_get_info(trie, kmpp_trie_info_name(info));
        end = kmpp_time();

        info = kmpp_trie_get_info_next(trie, info);
        sum += end - start;
        loop++;
    } while (info);

    kmpp_logi("trie access %d info %lld us averga %lld us\n",
              loop, sum, div64_s64(sum, loop));
}

EXPORT_SYMBOL(kmpp_trie_init);
EXPORT_SYMBOL(kmpp_trie_init_by_root);
EXPORT_SYMBOL(kmpp_trie_deinit);
EXPORT_SYMBOL(kmpp_trie_add_info);
EXPORT_SYMBOL(kmpp_trie_add_entry);
EXPORT_SYMBOL(kmpp_trie_get_node_count);
EXPORT_SYMBOL(kmpp_trie_get_info_count);
EXPORT_SYMBOL(kmpp_trie_get_buf_size);
EXPORT_SYMBOL(kmpp_trie_get_node_root);
EXPORT_SYMBOL(kmpp_trie_get_info);
EXPORT_SYMBOL(kmpp_trie_get_info_first);
EXPORT_SYMBOL(kmpp_trie_get_info_next);
EXPORT_SYMBOL(kmpp_trie_get_info_from_root);
EXPORT_SYMBOL(kmpp_trie_dump);
EXPORT_SYMBOL(kmpp_trie_timing_test);
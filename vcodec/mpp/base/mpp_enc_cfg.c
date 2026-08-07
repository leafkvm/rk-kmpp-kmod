/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "mpp_enc_cfg"

#include "kmpp_string.h"

#include "rk_venc_cfg.h"

#include "kmpp_obj.h"
#include "kmpp_venc_objs_impl.h"

#include "mpp_enc_cfg.h"

MPP_RET mpp_enc_cfg_init(MppEncCfg *cfg)
{
    return kmpp_venc_st_cfg_get(cfg);
}

MPP_RET mpp_enc_cfg_deinit(MppEncCfg cfg)
{
    return kmpp_venc_st_cfg_put(cfg);
}

#define ENC_CFG_SET_ACCESS(func_name, in_type, cfg_type) \
    MPP_RET func_name(KmppVencStCfg cfg, const char *name, in_type val) \
    { \
        if (NULL == cfg || NULL == name) { \
            kmpp_loge_f("invalid input cfg %p name %p\n", cfg, name); \
            return rk_nok; \
        } \
        return kmpp_obj_set_##cfg_type(cfg, name, val); \
    }

ENC_CFG_SET_ACCESS(mpp_enc_cfg_set_s32, RK_S32, s32);
ENC_CFG_SET_ACCESS(mpp_enc_cfg_set_u32, RK_U32, u32);
ENC_CFG_SET_ACCESS(mpp_enc_cfg_set_s64, RK_S64, s64);
ENC_CFG_SET_ACCESS(mpp_enc_cfg_set_u64, RK_U64, u64);
ENC_CFG_SET_ACCESS(mpp_enc_cfg_set_ptr, void *, kptr);
ENC_CFG_SET_ACCESS(mpp_enc_cfg_set_st,  void *, st);

#define ENC_CFG_GET_ACCESS(func_name, in_type, cfg_type) \
    MPP_RET func_name(KmppVencStCfg cfg, const char *name, in_type *val) \
    { \
        if (NULL == cfg || NULL == name) { \
            kmpp_loge_f("invalid input cfg %p name %p\n", cfg, name); \
            return rk_nok; \
        } \
        return kmpp_obj_get_##cfg_type(cfg, name, val); \
    }

ENC_CFG_GET_ACCESS(mpp_enc_cfg_get_s32, RK_S32, s32);
ENC_CFG_GET_ACCESS(mpp_enc_cfg_get_u32, RK_U32, u32);
ENC_CFG_GET_ACCESS(mpp_enc_cfg_get_s64, RK_S64, s64);
ENC_CFG_GET_ACCESS(mpp_enc_cfg_get_u64, RK_U64, u64);
ENC_CFG_GET_ACCESS(mpp_enc_cfg_get_ptr, void *, kptr);
ENC_CFG_GET_ACCESS(mpp_enc_cfg_get_st,  void, st);

#define ENC_CFG_COPY_SKIP_CHANGE(dst_cfg, src_cfg, name) do { \
    kmpp_memcpy((char *)&(dst_cfg->name) + sizeof(dst_cfg->name.change), \
          (char *)&(src_cfg->name) + sizeof(src_cfg->name.change), \
          sizeof(dst_cfg->name) - sizeof(dst_cfg->name.change)); \
} while(0)

void mpp_enc_cfg_show(void)
{
    KmppObjDef def = NULL;
    KmppTrie trie;
    KmppTrieInfo *root;
    const char *name = "KmppVencStCfg";

    kmpp_objdef_find(&def, name);
    if (!def) {
        kmpp_loge("failed to find object def %s\n", name);
        return;
    }

    trie = kmpp_objdef_get_trie(def, TRIE_ENTRY);
    if (!trie) {
        kmpp_loge("failed to get trie from object def %s\n", name);
        goto done;
    }

    root = kmpp_trie_get_info_first(trie);

    kmpp_logi("dumping valid configure string start\n");

    if (root) {
        KmppTrieInfo *node = root;

        kmpp_logi("%-32s %-6s | %6s | %4s | %4s\n", "name", "type", "offset", "size", "flag (hex)");

        do {
            if (kmpp_trie_info_is_self(node))
                continue;

            if (node->ctx_len == sizeof(KmppEntry)) {
                KmppEntry *entry = (KmppEntry *)kmpp_trie_info_ctx(node);

                kmpp_logi("%-32s %-6s | %-6d | %-4d | %-4x\n", kmpp_trie_info_name(node),
                          strof_ELEM_TYPE(entry->tbl.elem_type), entry->tbl.elem_offset,
                          entry->tbl.elem_size, entry->tbl.flag_offset);
            } else {
                kmpp_logi("%-30s size - %d\n", kmpp_trie_info_name(node), node->ctx_len);
            }
        } while ((node = kmpp_trie_get_info_next(trie, node)));
    }

    kmpp_logi("dumping valid configure string done\n");

    kmpp_logi("total cfg count %d with %d node size %d\n", kmpp_trie_get_info_count(trie),
              kmpp_trie_get_node_count(trie), kmpp_trie_get_buf_size(trie));
done:
    if (def) {
        kmpp_objdef_put(def);
        def = NULL;
    }
}

#define GET_ENC_CFG_CHANGE(name) \
    rk_u32 *mpp_enc_cfg_##name##_change(MppEncCfgSet *cfg) \
    { \
        if (cfg && cfg->ro.pos_##name) \
            return (rk_u32 *)(POS_TO_FLAG(cfg, cfg->ro.pos_##name)); \
        return NULL; \
    }

GET_ENC_CFG_CHANGE(prep)
GET_ENC_CFG_CHANGE(rc)
GET_ENC_CFG_CHANGE(hw)
GET_ENC_CFG_CHANGE(tune)
GET_ENC_CFG_CHANGE(h264)
GET_ENC_CFG_CHANGE(h265)
GET_ENC_CFG_CHANGE(jpeg)

#include <linux/export.h>

EXPORT_SYMBOL(mpp_enc_cfg_init);
EXPORT_SYMBOL(mpp_enc_cfg_deinit);
EXPORT_SYMBOL(mpp_enc_cfg_set_s32);
EXPORT_SYMBOL(mpp_enc_cfg_set_u32);
EXPORT_SYMBOL(mpp_enc_cfg_set_s64);
EXPORT_SYMBOL(mpp_enc_cfg_set_u64);
EXPORT_SYMBOL(mpp_enc_cfg_set_ptr);
EXPORT_SYMBOL(mpp_enc_cfg_set_st);
EXPORT_SYMBOL(mpp_enc_cfg_get_s32);
EXPORT_SYMBOL(mpp_enc_cfg_get_u32);
EXPORT_SYMBOL(mpp_enc_cfg_get_s64);
EXPORT_SYMBOL(mpp_enc_cfg_get_u64);
EXPORT_SYMBOL(mpp_enc_cfg_get_ptr);
EXPORT_SYMBOL(mpp_enc_cfg_get_st);
EXPORT_SYMBOL(mpp_enc_cfg_show);

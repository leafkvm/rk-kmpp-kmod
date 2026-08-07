/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_objs_test"

#include "kmpp_test.h"

#include "kmpp_meta.h"
#include "kmpp_frame.h"
#include "kmpp_buffer.h"
#include "rk_venc_cfg.h"

rk_u32 objdef_dump_flag;
module_param(objdef_dump_flag, uint, 0644);
MODULE_PARM_DESC(objdef_dump_flag, "objdef dump flag");

static rk_s32 kmpp_objdef_dump_all_test(const rk_u8 *name, rk_u32 *flag, void *ctx)
{
    if (is_flag_set(flag, TEST_DUMP))
        kmpp_objdef_dump_all();

    return 0;
}

KmppTest test_kmpp_objdef_dump = {
    .name = "dump all objdef",
    .ctx = NULL,
    .flag = &objdef_dump_flag,
    .func = kmpp_objdef_dump_all_test,
};

#define ENTRY_TABLE(prefix, ENTRY, STRCT, EHOOK, SHOOK, ALIAS) \
    ENTRY(prefix, kptr, void *, arg0, ELEM_FLAG_NONE, arg0) \
    ENTRY(prefix, u32,  rk_u32, arg1, ELEM_FLAG_NONE, arg1) \
    ENTRY(prefix, u64,  rk_u64, arg2, ELEM_FLAG_NONE, arg2) \
    ENTRY(prefix, kfp,  void *, func, ELEM_FLAG_NONE, func)

typedef struct KmppObjMacroTestImpl_t {
    void *arg0;
    rk_u32 arg1;
    rk_u64 arg2;
    rk_s32 (*func)(void *ctx, void *arg);
} KmppObjMacroTestImpl;

static rk_s32 kmpp_obj_test_impl_show(void *ctx, void *arg)
{
    KmppObjMacroTestImpl *cb = (KmppObjMacroTestImpl *)ctx;

    kmpp_log_f("arg0 %px\n", cb->arg0);
    kmpp_log_f("arg1 %d\n", cb->arg1);
    kmpp_log_f("arg2 %lld\n", cb->arg2);

    return 0;
}

static rk_s32 kmpp_obj_test_impl_dump(void *ctx)
{
    return kmpp_obj_test_impl_show(ctx, NULL);
}

static rk_s32 kmpp_obj_test_impl_init(void *entry, KmppObj obj, KmppFsDev *file, const rk_u8 *caller)
{
    KmppObjMacroTestImpl *cb = (KmppObjMacroTestImpl *)entry;

    cb->arg0 = NULL;
    cb->arg1 = 1234;
    cb->arg2 = 5678;
    cb->func = kmpp_obj_test_impl_show;

    return rk_ok;
}

#define KMPP_OBJ_NAME           kmpp_obj_test
#define KMPP_OBJ_INTF_TYPE      void *
#define KMPP_OBJ_IMPL_TYPE      KmppObjMacroTestImpl
#define KMPP_OBJ_ENTRY_TABLE    ENTRY_TABLE
#define KMPP_OBJ_FUNC_INIT      kmpp_obj_test_impl_init
#define KMPP_OBJ_FUNC_DUMP      kmpp_obj_test_impl_dump
#define KMPP_OBJ_FUNC_EXPORT_DISABLE
#include "kmpp_obj_helper.h"

rk_u32 kmpp_obj_macro_flag;
module_param(kmpp_obj_macro_flag, uint, 0644);
MODULE_PARM_DESC(kmpp_obj_macro_flag, "kmpp obj macro test flag");

static rk_s32 kmpp_obj_macro_test(const rk_u8 *name, rk_u32 *flag, void *ctx)
{
    KmppObj obj = NULL;
    KmppTrie src;
    KmppTrie dst;
    void *root;
    rk_s32 ret = rk_nok;

    ret = kmpp_obj_test_objdef_get();
    if (ret) {
        kmpp_loge("%s objdef init failed\n", name);
        goto done;
    }

    test_detail("obj init get def %px\n", kmpp_obj_test_def);

    src = kmpp_objdef_get_trie(kmpp_obj_test_def, TRIE_ENTRY);
    if (!src) {
        kmpp_loge("get original trie failed\n");
        ret = rk_nok;
        goto done;
    }

    root = kmpp_trie_get_node_root(src);
    if (!root) {
        kmpp_loge("get original trie root failed\n");
        ret = rk_nok;
        goto done;
    }

    ret = kmpp_trie_init_by_root(&dst, root);
    if (ret) {
        kmpp_loge("get new trie by original root failed\n");
        goto done;
    }

    test_detail("obj trie root test done\n");

    /* release dst but not release root */
    kmpp_trie_deinit(dst);

    ret = kmpp_obj_get_f(&obj, kmpp_obj_test_def);
    if (ret) {
        kmpp_loge("get obj failed\n");
        goto done;
    }

    test_detail("obj get done\n");

    ret |= kmpp_obj_set_kptr(obj, "arg0", NULL);
    ret |= kmpp_obj_set_s32(obj, "arg1", 1);
    ret |= kmpp_obj_set_u64(obj, "arg2", 2);
    ret |= kmpp_obj_set_kfp(obj, "func", kmpp_obj_test_impl_dump);

    if (ret) {
        kmpp_loge("set obj failed\n");
        goto done;
    }

    test_detail("obj set done\n");

    if (is_flag_set(flag, TEST_DUMP)) {
        ret = kmpp_obj_run(obj, "func", NULL);
        if (ret)
            kmpp_loge("run obj func failed\n");
    }

done:
    if (obj) {
        kmpp_obj_put_f(obj);
        obj = NULL;
    }

    kmpp_obj_test_objdef_put();

    return ret;
}

KmppTest test_kmpp_obj_macro = {
    .name = "KmppObjMacro",
    .ctx = NULL,
    .flag = &kmpp_obj_macro_flag,
    .func = kmpp_obj_macro_test,
};


rk_u32 kmpp_frame_flag;
module_param(kmpp_frame_flag, uint, 0644);
MODULE_PARM_DESC(kmpp_frame_flag, "kmpp frame test flag");

static rk_s32 kmpp_frame_test(const rk_u8 *name, rk_u32 *flag, void *ctx)
{
    KmppShmPtr sptr;
    KmppBuffer buffer = NULL;
    KmppFrame frame = NULL;
    KmppMeta meta = NULL;
    rk_s32 ret = rk_nok;

    ret = kmpp_frame_get(&frame);
    test_detail("get frame %px ret %d\n", frame, ret);
    if (ret)
        goto done;

    sptr.uaddr = 0;
    sptr.kaddr = 0;
    ret = kmpp_frame_get_meta(frame, &sptr);
    meta = sptr.kptr;

    test_detail("get meta %px ret %d\n", meta, ret);

    kmpp_frame_get_buffer(frame, &sptr);
    buffer = sptr.kptr;

    test_detail("get buffer %px ret %d\n", buffer, ret);

    if (is_flag_set(flag, TEST_DUMP))
        kmpp_frame_dump_f(frame);

    if (is_flag_set(flag, TEST_DUMP))
        kmpp_meta_dump(meta);

    if (is_flag_set(flag, TEST_DUMP))
        kmpp_buffer_dump(buffer, __FUNCTION__);

done:
    if (frame) {
        ret = kmpp_frame_put(frame);
        test_detail("put frame %px ret %d\n", frame, ret);
    }
    return ret;
}

KmppTest test_kmpp_frame = {
    .name = "KmppFrame",
    .ctx = NULL,
    .flag = &kmpp_frame_flag,
    .func = kmpp_frame_test,
};

rk_u32 kmpp_venc_cfg_flag;
module_param(kmpp_venc_cfg_flag, uint, 0644);
MODULE_PARM_DESC(kmpp_venc_cfg_flag, "kmpp venc cfg test flag");

static rk_s32 kmpp_venc_cfg_test(const rk_u8 *name, rk_u32 *flag, void *ctx)
{
    MppEncCfg cfg = NULL;

    mpp_enc_cfg_init(&cfg);

    mpp_enc_cfg_deinit(cfg);

    if (is_flag_set(flag, TEST_DUMP))
        mpp_enc_cfg_show();

    return rk_ok;
}

KmppTest test_kmpp_venc_cfg = {
    .name = "kmpp venc cfg",
    .ctx = NULL,
    .flag = &kmpp_venc_cfg_flag,
    .func = kmpp_venc_cfg_test,
};

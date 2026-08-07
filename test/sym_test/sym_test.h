/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __SYM_TEST_H__
#define __SYM_TEST_H__

#include <linux/module.h>
#include <linux/kernel.h>

#include "kmpp_osal.h"
#include "kmpp_sym.h"

typedef struct SymInfo_t {
    const rk_u8 *name;
    KmppFuncPtr func;
} SymInfo;

#define CONCAT(prefix, sufix) prefix##_##sufix

#define SYMBOL_ACCESS(sym, pr_sym) \
static rk_s32 CONCAT(sym, set)(void *arg, const rk_u8 *caller) \
{ \
    rk_s32 ret = rk_nok; \
    if (arg) { \
        typeof(sym) val = sym; \
        rk_u32 debug = 0; \
        kmpp_sym_run(sym_debug, &debug, NULL); \
        kmpp_logi_c(debug, MODULE_TAG #sym " set " #pr_sym " -> " #pr_sym "\n", sym, val); \
        sym = val; \
        ret = rk_ok; \
    } \
    return rk_ok; \
} \
static rk_s32 CONCAT(sym, get)(void *arg, const rk_u8 *caller) \
{ \
    rk_s32 ret = rk_nok; \
    if (arg) { \
        typeof(sym) *val = (typeof(sym) *)arg; \
        rk_u32 debug = 0; \
        kmpp_sym_run(sym_debug, &debug, NULL); \
        kmpp_logi_c(debug, MODULE_TAG #sym " get " #pr_sym " -> " #pr_sym "\n", *val, sym); \
        *val = sym; \
        ret = rk_ok; \
    } \
    return rk_ok; \
}

static KmppWorker *test_worker = NULL;
static KmppWork *test_work = NULL;
static KmppSymDef def = NULL;
static KmppSyms sym_test = NULL;
static KmppSym sym_debug = NULL;

static rk_s32 symbol_s32 = 0x12345678;
static rk_u32 symbol_u32 = 0x23456789;
static rk_s64 symbol_s64 = 0x3456789abcdef;
static rk_u64 symbol_u64 = 0x456789abcdef12;
static void *symbol_ptr = (void *)&test_worker;

SYMBOL_ACCESS(symbol_s32, %x)
SYMBOL_ACCESS(symbol_u32, %x)
SYMBOL_ACCESS(symbol_s64, %llx)
SYMBOL_ACCESS(symbol_u64, %llx)
SYMBOL_ACCESS(symbol_ptr, %px)

static rk_s32 symbol_funcion(void *arg, const rk_u8 *caller)
{
    rk_u32 debug = 0;
    kmpp_sym_run(sym_debug, &debug, NULL);

    kmpp_logi_c(debug, MODULE_TAG " test func enter\n");
    kmpp_logi_c(debug, MODULE_TAG " s32 %x\n", symbol_s32);
    kmpp_logi_c(debug, MODULE_TAG " u32 %x\n", symbol_u32);
    kmpp_logi_c(debug, MODULE_TAG " s64 %llx\n", symbol_s64);
    kmpp_logi_c(debug, MODULE_TAG " u64 %llu\n", symbol_u64);
    kmpp_logi_c(debug, MODULE_TAG " ptr %px\n", symbol_ptr);

    return rk_ok;
}

static SymInfo sym_info[] = {
    {   "set_s32",  symbol_s32_set, },
    {   "get_s32",  symbol_s32_get, },
    {   "set_u32",  symbol_u32_set, },
    {   "get_u32",  symbol_u32_get, },
    {   "set_s64",  symbol_s64_set, },
    {   "get_s64",  symbol_s64_get, },
    {   "set_u64",  symbol_u64_set, },
    {   "get_u64",  symbol_u64_get, },
    {   "set_ptr",  symbol_ptr_set, },
    {   "get_ptr",  symbol_ptr_get, },
    {   "run_func", symbol_funcion  },
};

static rk_s32 sym_test_func(const char *syms_name)
{
    KmppSyms syms = NULL;
    KmppSym sym = NULL;
    rk_s32 ret = rk_nok;
    rk_s32 i;

    ret = kmpp_syms_get(&syms, syms_name);

    kmpp_logi(MODULE_TAG " try get syms %s ret %px\n", syms_name, syms);

    if (!syms)
        return ret;

    for (i = 0; i < ARRAY_SIZE(sym_info); i++) {
        SymInfo *info = &sym_info[i];

        kmpp_sym_get(&sym, syms, info->name);
        if (!sym) {
            kmpp_logi(MODULE_TAG " get sym %s:%s failed\n", syms_name, info->name);
            continue;
        }

        ret = kmpp_sym_run_f(sym, NULL, NULL, MODULE_TAG);

        kmpp_logi(MODULE_TAG " run sym %s:%s ret %d\n", syms_name, info->name, ret);

        kmpp_sym_put(sym);
    }

    kmpp_syms_put(syms);

    return ret;
}

rk_s32 symbol_func(void *param)
{
    rk_s32 i;

    kmpp_logi(MODULE_TAG " test func start\n");

    kmpp_symdef_get(&def, MODULE_TAG);

    kmpp_logi(MODULE_TAG " test func add sym\n");

    for (i = 0; i < ARRAY_SIZE(sym_info); i++) {
        SymInfo *info = &sym_info[i];

        kmpp_symdef_add(def, info->name, info->func);
    }

    kmpp_symdef_install(def);

    if (0) {
        sym_test_func("sym1");
        sym_test_func("sym2");
        sym_test_func("sym3");
        sym_test_func("sym4");
    }

    kmpp_logi(MODULE_TAG " test func done\n");

    return rk_ok;
}

void symbol_end(void)
{
    if (def) {
        kmpp_symdef_uninstall(def);
        kmpp_symdef_put(def);
        def = NULL;
    }
    if (sym_debug) {
        kmpp_sym_put(sym_debug);
        sym_debug = NULL;
    }
    if (sym_test) {
        kmpp_syms_put(sym_test);
        sym_test = NULL;
    }
}

int symbol_init(void)
{
    kmpp_logi(MODULE_TAG " test start\n");

    kmpp_syms_get(&sym_test, "sym_test");
    if (sym_test)
        kmpp_sym_get(&sym_debug, sym_test, "sys_test_get_debug");

    kmpp_worker_init(&test_worker, MODULE_TAG " test");
    kmpp_work_init(&test_work, symbol_func, NULL);

    kmpp_work_queue(test_worker, test_work);

    return 0;
}

void symbol_exit(void)
{
    kmpp_worker_deinit(&test_worker);
    kmpp_work_deinit(&test_work);

    symbol_end();

    kmpp_logi(MODULE_TAG " test end\n");
}

module_init(symbol_init);
module_exit(symbol_exit);

MODULE_AUTHOR("rockchip");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_VERSION("1.0");

#endif /* __SYM_TEST_H__ */

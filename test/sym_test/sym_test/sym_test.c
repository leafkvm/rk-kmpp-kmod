/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "sym_test"

#include <linux/module.h>
#include <linux/kernel.h>

#include "kmpp_osal.h"
#include "kmpp_sym.h"

static KmppWorker *test_worker = NULL;
static KmppWork *test_work = NULL;
static KmppEnvNode sym_test_stop_env = NULL;
static KmppEnvNode sym_test_log_env = NULL;
static KmppSym sym_test = NULL;

static const rk_u8 *syms_names[] = {
    "sym1",
    "sym2",
    "sym3",
    "sym4",
};
static const rk_s32 syms_count = sizeof(syms_names) / sizeof(syms_names[0]);

static const rk_u8 *sym_names[] = {
    "set_s32",
    "get_s32",
    "set_u32",
    "get_u32",
    "set_s64",
    "get_s64",
    "set_u64",
    "get_u64",
    "set_ptr",
    "get_ptr",
    "run_func",
};

static const rk_s32 sym_count = sizeof(sym_names) / sizeof(sym_names[0]);
static volatile rk_u32 sym_test_stop = 0;
static volatile rk_u32 sym_test_log = 0;

static rk_s32 sym_test_func(const rk_u8 *syms_name, rk_s32 *status)
{
    KmppSyms syms = NULL;
    KmppSym sym = NULL;
    rk_s32 ret = rk_nok;
    rk_s32 i;

    ret = kmpp_syms_get(&syms, syms_name);
    if (ret || !syms) {
        kmpp_logi(MODULE_TAG " try get syms %s failed ret %px\n", syms_name, ret);
        return ret;
    }

    for (i = 0; i < sym_count; i++) {
        const rk_u8 *name = sym_names[i];
        rk_s32 valid = 0;

        ret = kmpp_sym_get(&sym, syms, name);
        if (!ret && sym) {
            ret = kmpp_sym_run_f(sym, NULL, NULL, MODULE_TAG);
            if (!ret)
                valid = 1;
        }

        if (status[i] != valid) {
            kmpp_logi(MODULE_TAG " sym %s:%s changed %s -> %s\n", syms_name, name,
                      status[i] ? "valid" : "invalid", valid ? "valid" : "invalid");
            status[i] = valid;
        }

        kmpp_sym_put(sym);
    }

    kmpp_syms_put(syms);

    return ret;
}

rk_s32 symbol_func(void *param)
{
    rk_s32 *status;
    rk_s32 i;

    kmpp_logi(MODULE_TAG " test func start\n");

    status = kmpp_calloc(sizeof(rk_s32) * syms_count * sym_count);
    if (!status) {
        kmpp_loge(MODULE_TAG " kmpp_calloc status failed\n");
        return rk_nok;
    }

    while (!sym_test_stop) {
        for (i = 0; i < syms_count; i++) {
            sym_test_func(syms_names[i], &status[i * sym_count]);
            kmpp_msleep(50);
        }
    }

    kmpp_logi(MODULE_TAG " test func finish\n");

    kmpp_free(status);

    return rk_ok;
}

static rk_s32 sys_test_get_debug(void *arg, const rk_u8 *caller)
{
    rk_u32 *debug = (rk_u32 *)arg;

    if (debug)
        *debug = sym_test_log;

    return rk_ok;
}

void symbol_end(void)
{
    if (sym_test_stop_env) {
        kmpp_env_del(kmpp_env_debug(), sym_test_stop_env);
        sym_test_stop_env = NULL;
    }
    if (sym_test_log_env) {
        kmpp_env_del(kmpp_env_debug(), sym_test_log_env);
        sym_test_log_env = NULL;
    }
    if (sym_test) {
        kmpp_symdef_uninstall(sym_test);
        kmpp_symdef_put(sym_test);
        sym_test = NULL;
    }
}

int symbol_init(void)
{
    KmppEnvInfo env;

    kmpp_logi(MODULE_TAG " test start\n");

    env.type = KmppEnv_u32;
    env.readonly = 0;
    env.name = "sym_test_stop";
    env.val = (void *)&sym_test_stop;
    env.env_show = NULL;

    kmpp_env_add(kmpp_env_debug(), &sym_test_stop_env, &env);

    env.type = KmppEnv_u32;
    env.readonly = 0;
    env.name = "sym_test_log";
    env.val = (void *)&sym_test_log;
    env.env_show = NULL;

    kmpp_env_add(kmpp_env_debug(), &sym_test_log_env, &env);

    kmpp_symdef_get(&sym_test, MODULE_TAG);
    /* Add sys export funciton here */
    if (sym_test) {
        kmpp_symdef_add(sym_test, "sys_test_get_debug", sys_test_get_debug);
        kmpp_symdef_install(sym_test);
    }

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
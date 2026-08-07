/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_sys_info"

#include "kmpp_version.h"

#include "kmpp_env.h"
#include "kmpp_module.h"
#include "kmpp_sys_id.h"

KmppEnvGrp kmpp_env_sys;

KmppEnvGrp sys_get_env(void)
{
    return kmpp_env_sys;
}

static void sys_version_show(KmppEnvNode node, void *data)
{
    kmpp_env_log(node, "kmpp sys version: %s\n", KMPP_VERSION);
}

static void sys_history_show(KmppEnvNode node, void *data)
{
    static const rk_u32 kmpp_history_cnt = KMPP_VER_HIST_CNT;
    static const char *kmpp_history[]  = {
        KMPP_VER_HIST_0,
        KMPP_VER_HIST_1,
        KMPP_VER_HIST_2,
        KMPP_VER_HIST_3,
        KMPP_VER_HIST_4,
        KMPP_VER_HIST_5,
        KMPP_VER_HIST_6,
        KMPP_VER_HIST_7,
        KMPP_VER_HIST_8,
        KMPP_VER_HIST_9,
    };
    rk_u32 i;

    kmpp_env_log(node, "kmpp sys version history %d:\n", kmpp_history_cnt);
    for (i = 0; i < kmpp_history_cnt; i++) {
        kmpp_env_log(node, "%s\n", kmpp_history[i]);
    }
}

static rk_s32 sys_env_init(void)
{
    KmppEnvInfo info;

    kmpp_env_get(&kmpp_env_sys, kmpp_env_root(), "sys");

    info.name = "version";
    info.readonly = 1;
    info.type = KmppEnv_user;
    info.val = NULL;
    info.env_show = sys_version_show;

    kmpp_env_add(kmpp_env_sys, NULL, &info);

    info.name = "version_history";
    info.readonly = 1;
    info.type = KmppEnv_user;
    info.val = NULL;
    info.env_show = sys_history_show;

    kmpp_env_add(kmpp_env_sys, NULL, &info);

    return rk_ok;
}

static rk_s32 sys_env_deinit(void)
{
    kmpp_env_put(kmpp_env_sys);
    kmpp_env_sys = NULL;

    return rk_ok;
}

KMPP_MODULE_ADD(kmpp_sys, KMPP_SYS_ENV, env, sys_env_init, sys_env_deinit);

/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_osal_info"

#include "kmpp_version.h"

#include "kmpp_env.h"
#include "kmpp_module.h"
#include "kmpp_osal_id.h"

static void osal_version_show(KmppEnvNode node, void *data)
{
    kmpp_env_log(node, "kmpp osal version: %s\n", KMPP_VERSION);
}

static void osal_history_show(KmppEnvNode node, void *data)
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

    kmpp_env_log(node, "kmpp osal version history %d:\n", kmpp_history_cnt);
    for (i = 0; i < kmpp_history_cnt; i++) {
        kmpp_env_log(node, "%s\n", kmpp_history[i]);
    }
}

static rk_s32 osal_env_version_init(void)
{
    KmppEnvInfo info;

    info.name = "version";
    info.readonly = 1;
    info.type = KmppEnv_user;
    info.val = NULL;
    info.env_show = osal_version_show;

    kmpp_env_add(kmpp_env_osal(), NULL, &info);

    info.name = "version_history";
    info.readonly = 1;
    info.type = KmppEnv_user;
    info.val = NULL;
    info.env_show = osal_history_show;

    kmpp_env_add(kmpp_env_osal(), NULL, &info);

    return rk_ok;
}

KMPP_MODULE_ADD(kmpp_osal, KMPP_OSAL_VERSION, version, osal_env_version_init, NULL);

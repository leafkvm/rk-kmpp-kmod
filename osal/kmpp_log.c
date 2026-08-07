/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/kernel.h>

#include "kmpp_log.h"
#include "kmpp_env.h"
#include "kmpp_module.h"
#include "kmpp_osal_id.h"

#define LINE_SZ 512

static rk_s32 kmpp_log_level = RK_LOG_INFO;
static KmppEnvNode kmpp_log_env;

static rk_s32 kmpp_log_init(void)
{
    KmppEnvInfo env;

    env.type = KmppEnv_s32;
    env.readonly = 0;
    env.name = "log_level";
    env.val = &kmpp_log_level;
    env.env_show = NULL;

    return kmpp_env_add(kmpp_env_osal(), &kmpp_log_env, &env);
}

static rk_s32 kmpp_log_deinit(void)
{
    return kmpp_env_del(kmpp_env_osal(), kmpp_log_env);
}

KMPP_MODULE_ADD(kmpp_osal, KMPP_OSAL_LOG, log, kmpp_log_init, kmpp_log_deinit);

void kmpp_set_log_level(int level)
{
    kmpp_log_level = level;
}

int kmpp_get_log_level(void)
{
    return kmpp_log_level;
}

void _kmpp_log(int level, const char *tag, const char *fmt, const char *fname, ...)
{
    char line[LINE_SZ];
    rk_s32 len;
    va_list args;
    int log_level;

    if (level <= RK_LOG_UNKNOWN || level >= RK_LOG_SILENT)
        return;

    log_level = kmpp_log_level;
    if (log_level >= RK_LOG_SILENT)
        return;

    if (level > log_level)
        return;

    len = 0;

    if (tag)
        len += snprintf(line, sizeof(line) - 1, "%s: ", tag);

    if (fname)
        len += snprintf(line + len, sizeof(line) - 1 - len, "%s: ", fname);

    snprintf(line + len, sizeof(line) - 1 - len, "%s", fmt);

    va_start(args, fname);
    vprintk(line, args);
    va_end(args);
}

EXPORT_SYMBOL(kmpp_set_log_level);
EXPORT_SYMBOL(kmpp_get_log_level);
EXPORT_SYMBOL(_kmpp_log);

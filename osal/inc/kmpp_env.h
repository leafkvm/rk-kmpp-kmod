/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_ENV_H__
#define __KMPP_ENV_H__

#include "kmpp_log.h"
#include "kmpp_dev.h"

typedef enum KmppEnvType_e {
    KmppEnvNone,
    KmppEnv_s32,
    KmppEnv_u32,
    KmppEnv_s64,
    KmppEnv_u64,
    KmppEnv_str,
    KmppEnv_user,
    KmppEnvTypeButt,
} KmppEnvType;

typedef void* KmppEnvGrp;
typedef void* KmppEnvNode;

typedef struct KmppEnvInfo_t {
    KmppEnvType type;
    rk_u32 readonly;
    const rk_u8 *name;
    /* NOTE: val may change after user writing a new string */
    void *val;
    void (*env_show)(KmppEnvNode node, void *data);
} KmppEnvInfo;

rk_s32 kmpp_env_get(KmppEnvGrp *env, KmppEnvGrp parent, const rk_u8 *name);
rk_s32 kmpp_env_put(KmppEnvGrp env);

rk_s32 kmpp_env_add(KmppEnvGrp env, KmppEnvNode *node, KmppEnvInfo *info);
rk_s32 kmpp_env_del(KmppEnvGrp env, KmppEnvNode node);

/* return info for changeable string variable */
void *kmpp_env_to_ptr(KmppEnvNode node);
/* return linux seq_file from node */
void *kmpp_env_to_seq(KmppEnvNode node);

/* helper function used in user defined env_show function */
void kmpp_env_log(KmppEnvNode node, const rk_u8 *fmt, ...);

/*
 * kmpp module environment value group (directory) definitions
 * root is all env root directory
 * osal is for osal module env only
 * debug is for all kmpp_xxx_debug flags
 */
KmppEnvGrp kmpp_env_root(void);
KmppEnvGrp kmpp_env_osal(void);
KmppEnvGrp kmpp_env_debug(void);

#endif /* __KMPP_ENV_H__ */
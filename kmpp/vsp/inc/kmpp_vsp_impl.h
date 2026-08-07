/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_VSP_IMPL_H__
#define __KMPP_VSP_IMPL_H__

#include "kmpp_frame.h"

typedef enum KmppVspCmd_e {
    KMPP_VSP_CMD_GET_ST_CFG,
    KMPP_VSP_CMD_SET_ST_CFG,
    KMPP_VSP_CMD_GET_RT_CFG,
    KMPP_VSP_CMD_SET_RT_CFG,

    KMPP_VSP_CMD_BUTT,
} KmppVspCmd;

typedef struct KmppVspApi_t {
    const rk_u8     *name;
    rk_s32          ctx_size;

    void *(*id_to_ctx)(rk_s32 id);
    rk_s32 (*ctx_to_id)(void *ctx);

    rk_s32 (*init)(void **ctx, void *cfg);
    rk_s32 (*deinit)(void *ctx);

    rk_s32 (*reset)(void *ctx);
    rk_s32 (*start)(void *ctx);
    rk_s32 (*stop)(void *ctx);
    rk_s32 (*ctrl)(void *ctx, rk_s32 cmd, void *arg);

    rk_s32 (*proc)(void *ctx, KmppFrame in, KmppFrame *out);
    rk_s32 (*put_frm)(void *ctx, KmppFrame frm);
    rk_s32 (*get_frm)(void *ctx, KmppFrame *frm);
} KmppVspApi;

KmppVspApi *kmpp_vsp_get_api(const rk_u8 *name);

#endif /* __KMPP_VSP_IMPL_H__ */

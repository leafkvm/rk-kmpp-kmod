/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#ifndef __RC_MODEL_H__
#define __RC_MODEL_H__

#include "mpp_rc_api.h"

#ifdef  __cplusplus
extern "C" {
#endif

RK_U32 rc_model_ctx_size(void);

MPP_RET rc_model_init(void *ctx, MppEncCfg cfg);
MPP_RET rc_model_deinit(void *ctx);

MPP_RET rc_model_check_reenc(void *ctx, EncRcTask *task);

MPP_RET rc_model_start(void *ctx, EncRcTask *task);
MPP_RET rc_model_end(void *ctx, EncRcTask *task);

MPP_RET rc_model_hal_start(void *ctx, EncRcTask *task);
MPP_RET rc_model_hal_end(void *ctx, EncRcTask *task);

void rc_model_proc_show(KmppEnvNode node, void *ctx, RK_S32 chl_id);

#ifdef  __cplusplus
}
#endif

#endif /* __RC_MODEL_H__ */

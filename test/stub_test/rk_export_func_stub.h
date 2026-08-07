/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __RK_EXPORT_FUNC_STUB_H__
#define __RK_EXPORT_FUNC_STUB_H__

#include "kmpp_stub.h"
#include "rk_export_func.h"

int mpp_vcodec_clear_buf_resource(void)
STUB_FUNC_RET_ZERO(mpp_vcodec_clear_buf_resource);
int mpp_vcodec_run_task(RK_U32 chan_id, RK_S64 pts, RK_S64 dts)
STUB_FUNC_RET_ZERO(mpp_vcodec_run_task);

/* extra function used by external ko */
int mpp_vcodec_set_online_mode(RK_U32 chan_id, RK_U32 mode)
STUB_FUNC_RET_ZERO(mpp_vcodec_set_online_mode);

#endif

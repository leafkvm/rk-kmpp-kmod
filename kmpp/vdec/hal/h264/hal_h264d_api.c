/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#include "vdec_hal_api.h"
#include "hal_h264d_api.h"

static VdecHalApi hal_h264d_vdpu384 = {
    .list = KMPP_LIST_HEAD_INIT(hal_h264d_vdpu384.list),
    .name = "h264d_vdpu384",
    .coding = MPP_VIDEO_CodingAVC,
    .ctx_size = 0x100,
    .flag = HAS_FRAME_INPUT | HAS_FRAME_OUTPUT,
    .init = NULL,
    .deinit = NULL,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
    .get_task = NULL,
    .put_task = NULL,
    .update_task = NULL,
    .poll_task = NULL,
};

void hal_h264d_api_register(void)
{
    vdec_hal_add(&hal_h264d_vdpu384);
}

void hal_h264d_api_unregister(void)
{
    vdec_hal_del(&hal_h264d_vdpu384);
}

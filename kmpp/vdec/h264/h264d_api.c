
/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#include "vdec_parser_api.h"
#include "h264d_api.h"

static ParserApi h264d = {
    .list = KMPP_LIST_HEAD_INIT(h264d.list),
    .name = "h264d",
    .coding = MPP_VIDEO_CodingAVC,
    .ctx_size = 0x100,
    .flag = HAS_FRAME_INPUT,
    .init = NULL,
    .deinit = NULL,
    .reset = NULL,
    .start = NULL,
    .stop = NULL,
    .get_job = NULL,
    .put_job = NULL,
    .get_out = NULL,
    .poll_out = NULL,
    .put_out = NULL,
    .get_frm = NULL,
};

void h264d_api_register(void)
{
    parser_add(&h264d);
}

void h264d_api_unregister(void)
{
    parser_del(&h264d);
}

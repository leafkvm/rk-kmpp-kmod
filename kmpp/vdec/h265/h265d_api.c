
/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#include "vdec_parser_api.h"
#include "h265d_api.h"

static ParserApi h265d = {
    .list = KMPP_LIST_HEAD_INIT(h265d.list),
    .name = "h265d",
    .coding = MPP_VIDEO_CodingHEVC,
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

void h265d_api_register(void)
{
    parser_add(&h265d);
}

void h265d_api_unregister(void)
{
    parser_del(&h265d);
}

/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "jpege_rc"

#include "mpp_mem.h"
#include "mpp_maths.h"

#include "rc_debug.h"
#include "rc_model.h"

const RcApi default_jpege = {
    "default",
    MPP_VIDEO_CodingMJPEG,
    rc_model_ctx_size,
    rc_model_init,
    rc_model_deinit,
    NULL,
    rc_model_check_reenc,
    rc_model_start,
    rc_model_end,
    rc_model_hal_start,
    rc_model_hal_end,
    rc_model_proc_show,
};

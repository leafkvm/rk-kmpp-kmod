/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __RK_ERR_DEFS_H__
#define __RK_ERR_DEFS_H__

#include "rk_type.h"

typedef enum {
    MPP_SUCCESS                 = rk_success,
    MPP_OK                      = rk_ok,

    MPP_NOK                     = -1,
    MPP_ERR_UNKNOW              = -2,
    MPP_ERR_NULL_PTR            = -3,
    MPP_ERR_MALLOC              = -4,
    MPP_ERR_OPEN_FILE           = -5,
    MPP_ERR_VALUE               = -6,
    MPP_ERR_READ_BIT            = -7,
    MPP_ERR_TIMEOUT             = -8,
    MPP_ERR_PERM                = -9,

    /* the err for intterupt from hw */
    MPP_ERR_INT_BASE            = -100,
    MPP_ERR_INT_BS_OVFL         = MPP_ERR_INT_BASE - 1,
    MPP_ERR_INT_SOURCE_MIS      = MPP_ERR_INT_BASE - 2,

    MPP_ERR_BASE                = -1000,

    /* The error in stream processing */
    MPP_ERR_LIST_STREAM         = MPP_ERR_BASE - 1,
    MPP_ERR_INIT                = MPP_ERR_BASE - 2,
    MPP_ERR_VPU_CODEC_INIT      = MPP_ERR_BASE - 3,
    MPP_ERR_STREAM              = MPP_ERR_BASE - 4,
    MPP_ERR_FATAL_THREAD        = MPP_ERR_BASE - 5,
    MPP_ERR_NOMEM               = MPP_ERR_BASE - 6,
    MPP_ERR_PROTOL              = MPP_ERR_BASE - 7,
    MPP_FAIL_SPLIT_FRAME        = MPP_ERR_BASE - 8,
    MPP_ERR_VPUHW               = MPP_ERR_BASE - 9,
    MPP_EOS_STREAM_REACHED      = MPP_ERR_BASE - 11,
    MPP_ERR_BUFFER_FULL         = MPP_ERR_BASE - 12,
    MPP_ERR_DISPLAY_FULL        = MPP_ERR_BASE - 13,
} MPP_RET;

#endif /*__RK_ERR_DEFS_H__*/

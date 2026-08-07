/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 */

#ifndef __MPP_VCODEC_DEBUG_H__
#define __MPP_VCODEC_DEBUG_H__
#include "kmpp_log.h"

extern RK_U32 mpp_vcodec_debug;

#define MPP_VCODEC_DBG_FUNCTION            (0x00000001)
#define MPP_VCODEC_DBG_CONTROL             (0x00000002)
#define MPP_VCODEC_DBG_FLOW                (0x00000004)
#define MPP_VCODEC_DBG_STATUS              (0x00000010)
#define MPP_VCODEC_DBG_DETAIL              (0x00000020)


#define mpp_vcodec_dbg(flag, fmt, ...)     kmpp_logi_c(mpp_vcodec_debug & flag, fmt, ## __VA_ARGS__)
#define mpp_vcodec_dbg_f(flag, fmt, ...)   kmpp_logi_cf(mpp_vcodec_debug & flag, fmt, ## __VA_ARGS__)

#define mpp_vcodec_func(fmt, ...)          mpp_vcodec_dbg_f(MPP_VCODEC_DBG_FUNCTION, fmt, ## __VA_ARGS__)
#define mpp_vcodec_ctrl(fmt, ...)          mpp_vcodec_dbg_f(MPP_VCODEC_DBG_CONTROL, fmt, ## __VA_ARGS__)
#define mpp_vcodec_status(fmt, ...)        mpp_vcodec_dbg_f(MPP_VCODEC_DBG_STATUS, fmt, ## __VA_ARGS__)
#define mpp_vcodec_detail(fmt, ...)        mpp_vcodec_dbg_f(MPP_VCODEC_DBG_DETAIL, fmt, ## __VA_ARGS__)
#define mpp_vcodec_flow(fmt, ...)          mpp_vcodec_dbg_f(MPP_VCODEC_DBG_FLOW, fmt, ## __VA_ARGS__)

#endif

/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_VDEC_DEBUG_H__
#define __KMPP_VDEC_DEBUG_H__

#include "kmpp_log.h"

#define VDEC_DBG_FLOW                   (0x00000001)

#define vdec_dbg(flag, fmt, ...)        kmpp_dbg(kmpp_vdec_debug, flag, fmt, ## __VA_ARGS__)
#define vdec_dbg_f(flag, fmt, ...)      kmpp_dbg_f(kmpp_vdec_debug, flag, fmt, ## __VA_ARGS__)

#define vdec_dbg_flow(fmt, ...)         vdec_dbg_f(VDEC_DBG_FLOW, fmt, ## __VA_ARGS__)

extern rk_u32 kmpp_vdec_debug;

#endif

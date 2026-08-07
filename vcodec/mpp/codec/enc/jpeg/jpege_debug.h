/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#ifndef __JPEGE_DEBUG_H__
#define __JPEGE_DEBUG_H__

#include "kmpp_log.h"

#define JPEGE_DBG_FUNCTION          (0x00000001)
#define JPEGE_DBG_INPUT             (0x00000010)
#define JPEGE_DBG_OUTPUT            (0x00000020)
#define JPEGE_DBG_CTRL              (0x00000040)

#define jpege_dbg(flag, fmt, ...)   kmpp_logi_c(jpege_debug & flag, fmt, ## __VA_ARGS__)
#define jpege_dbg_f(flag, fmt, ...) kmpp_logi_cf(jpege_debug & flag, fmt, ## __VA_ARGS__)

#define jpege_dbg_func(fmt, ...)    jpege_dbg_f(JPEGE_DBG_FUNCTION, fmt, ## __VA_ARGS__)
#define jpege_dbg_input(fmt, ...)   jpege_dbg(JPEGE_DBG_INPUT, fmt, ## __VA_ARGS__)
#define jpege_dbg_output(fmt, ...)  jpege_dbg(JPEGE_DBG_OUTPUT, fmt, ## __VA_ARGS__)
#define jpege_dbg_ctrl(fmt, ...)    jpege_dbg(JPEGE_DBG_CTRL, fmt, ## __VA_ARGS__)

extern RK_U32 jpege_debug;

#endif /* __JPEGE_DEBUG_H__ */

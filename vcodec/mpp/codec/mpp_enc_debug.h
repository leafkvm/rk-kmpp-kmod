/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#ifndef __MPP_ENC_DEBUG_H__
#define __MPP_ENC_DEBUG_H__

#include "kmpp_log.h"

extern RK_U32 mpp_enc_debug;

#define MPP_ENC_DBG_FUNCTION            (0x00000001)
#define MPP_ENC_DBG_CONTROL             (0x00000002)
#define MPP_ENC_DBG_RC_FLOW             (0x00000004)
#define MPP_ENC_DBG_STATUS              (0x00000010)
#define MPP_ENC_DBG_DETAIL              (0x00000020)
#define MPP_ENC_DBG_RESET               (0x00000040)
#define MPP_ENC_DBG_NOTIFY              (0x00000080)
#define MPP_ENC_DBG_REENC               (0x00000100)
#define MPP_ENC_DBG_CFG                 (0x00000200)

#define MPP_ENC_DBG_FRM_STATUS          (0x00010000)

#define mpp_enc_dbg(flag, fmt, ...)     kmpp_logi_c(mpp_enc_debug & flag, fmt, ## __VA_ARGS__)
#define mpp_enc_dbg_f(flag, fmt, ...)   kmpp_logi_cf(mpp_enc_debug & flag, fmt, ## __VA_ARGS__)

#define enc_dbg_func(fmt, ...)          mpp_enc_dbg_f(MPP_ENC_DBG_FUNCTION, fmt, ## __VA_ARGS__)
#define enc_dbg_ctrl(fmt, ...)          mpp_enc_dbg_f(MPP_ENC_DBG_CONTROL, fmt, ## __VA_ARGS__)
#define enc_dbg_rc_flow(fmt, ...)       mpp_enc_dbg_f(MPP_ENC_DBG_RC_FLOW, fmt, ## __VA_ARGS__)
#define enc_dbg_status(fmt, ...)        mpp_enc_dbg_f(MPP_ENC_DBG_STATUS, fmt, ## __VA_ARGS__)
#define enc_dbg_detail(fmt, ...)        mpp_enc_dbg_f(MPP_ENC_DBG_DETAIL, fmt, ## __VA_ARGS__)
#define enc_dbg_notify(fmt, ...)        mpp_enc_dbg_f(MPP_ENC_DBG_NOTIFY, fmt, ## __VA_ARGS__)
#define enc_dbg_reenc(fmt, ...)         mpp_enc_dbg_f(MPP_ENC_DBG_REENC, fmt, ## __VA_ARGS__)
#define enc_dbg_frm_status(fmt, ...)    mpp_enc_dbg_f(MPP_ENC_DBG_FRM_STATUS, fmt, ## __VA_ARGS__)
#define enc_dbg_cfg(fmt, ...)           mpp_enc_dbg_f(MPP_ENC_DBG_CFG, fmt, ## __VA_ARGS__)

#endif /* __MPP_ENC_DEBUG_H__ */

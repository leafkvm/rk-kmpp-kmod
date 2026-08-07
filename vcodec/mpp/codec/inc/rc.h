/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#ifndef __RC_H__
#define __RC_H__

#include "kmpp_log.h"

#include "mpp_rc_api.h"

/*
 * Mpp rate control principle
 *
 * 1. Rate control information is composed by these parts:
 *    a. configuration from mpp user.
 *    b. frame level target bitrate calculated by different RC model.
 *    c. block level hardware related configuration to hardware.
 *    d. statistic information from hardware after one frame is encoded.
 *
 * 2. All the above informatioin is storaged and managed by RcDataCtx in mpp
 *    framework. All these information is shared in encoder and hal module.
 *
 * 3. User (no matter developer or end user) defined rate control should based
 *    on these Rate control information and implement its own rate control
 *    strategy module by providing their own RcApiImpl.
 */

/*
 * Relation between mpp_enc / enc_impl / hal / RcApiCtx / RcApi / RcData
 *
 *  +-----------+
 *  |  mpp_enc  |
 *  +-----+-----+
 *        |
 *  +-----+-----+   +-----------+   +-----------+
 *  |  enc_impl +--->  RcFlow   +--->   RcApi   |
 *  +-----+-----+   +-----+-----+   +-----+-----+
 *        |      \ /      |               |
 *        |       +       |               |
 *        |      / \      |               |
 *  +-----v-----+   +-----v-----+         |
 *  |    hal    +--->  RcData   <---------+
 *  +-----------+   +-----------+
 */

typedef void *RcFlow;

#ifdef __cplusplus
extern "C" {
#endif

MPP_RET rc_init(RcFlow *ctx, MppCodingType type, const char **request_name);
MPP_RET rc_deinit(RcFlow ctx);

const char *rc_api_name(RcFlow ctx);

/* update rc control  */
MPP_RET rc_update_usr_cfg(RcFlow ctx, MppEncCfg cfg);

/* Frame rate convertion */
MPP_RET rc_frm_check_drop(RcFlow ctx, EncRcTask *task);
/* Frame reenc check */
MPP_RET rc_frm_check_reenc(RcFlow ctx, EncRcTask *task);

/* Frame level rate and quality control */
MPP_RET rc_frm_start(RcFlow ctx, EncRcTask *task);
MPP_RET rc_frm_end(RcFlow ctx, EncRcTask *task);

MPP_RET rc_hal_start(RcFlow ctx, EncRcTask *task);
MPP_RET rc_hal_end(RcFlow ctx, EncRcTask *task);

MPP_RET rc_proc_show(KmppEnvNode node, RcFlow ctx, RK_S32 chl_id);

#ifdef __cplusplus
}
#endif

#endif /* __RC_H__ */

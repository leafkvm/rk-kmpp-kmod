/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPI_VDEC_H__
#define __KMPI_VDEC_H__

#include "kmpi_defs.h"

/* string - value config for control flow */
typedef void* KmppVdecInitCfg;
typedef void* KmppVdecDeinitCfg;
typedef void* KmppVdecResetCfg;
typedef void* KmppVdecStartCfg;
typedef void* KmppVdecStopCfg;
typedef void* KmppVdecSuspendCfg;
typedef void* KmppVdecResumeCfg;

/* static and runtime config */
typedef void* KmppVdecCfg;

/*
 * There are two sets for interfaces:
 * context interface and chan id interface
 *
 * Each set of interface has the same interface functions:
 *
 * init / deinit
 * - decoder instance create / destroy interfaces.
 *
 * reset / start / stop / suspend / resume
 * - decoder control flow interfaces.
 *
 * cfg / rt_cfg
 * - decoder static parameter and runtime parameter interfaces.
 *
 * decode / put_pkt / get_frm
 * - decoder data flow interfaces.
 */

rk_s32 kmpp_vdec_init(KmppCtx *ctx, KmppVdecInitCfg cfg);
rk_s32 kmpp_vdec_deinit(KmppCtx ctx, KmppVdecDeinitCfg cfg);

rk_s32 kmpp_vdec_reset(KmppCtx ctx, KmppVdecResetCfg cfg);
rk_s32 kmpp_vdec_start(KmppCtx ctx, KmppVdecStartCfg cfg);
rk_s32 kmpp_vdec_stop(KmppCtx ctx, KmppVdecStopCfg cfg);
rk_s32 kmpp_vdec_suspend(KmppCtx ctx, KmppVdecSuspendCfg cfg);
rk_s32 kmpp_vdec_resume(KmppCtx ctx, KmppVdecResumeCfg cfg);

rk_s32 kmpp_vdec_get_cfg(KmppCtx ctx, KmppVdecCfg cfg);
rk_s32 kmpp_vdec_set_cfg(KmppCtx ctx, KmppVdecCfg cfg);

rk_s32 kmpp_vdec_decode(KmppCtx ctx, KmppPacket pkt, KmppFrame *frm);
rk_s32 kmpp_vdec_get_pkt(KmppCtx ctx, KmppPacket *pkt);
rk_s32 kmpp_vdec_put_pkt(KmppCtx ctx, KmppPacket pkt);
rk_s32 kmpp_vdec_get_frm(KmppCtx ctx, KmppFrame *frm);
rk_s32 kmpp_vdec_put_frm(KmppCtx ctx, KmppFrame frm);

rk_s32 kmpp_vdec_chan_init(KmppChanId *id, KmppVdecInitCfg cfg);
rk_s32 kmpp_vdec_chan_deinit(KmppChanId id, KmppVdecDeinitCfg cfg);

rk_s32 kmpp_vdec_chan_reset(KmppChanId id, KmppVdecResetCfg cfg);
rk_s32 kmpp_vdec_chan_start(KmppChanId id, KmppVdecStartCfg cfg);
rk_s32 kmpp_vdec_chan_stop(KmppChanId id, KmppVdecStopCfg cfg);
rk_s32 kmpp_vdec_chan_suspend(KmppChanId id, KmppVdecSuspendCfg cfg);
rk_s32 kmpp_vdec_chan_resume(KmppChanId id, KmppVdecResumeCfg cfg);

rk_s32 kmpp_vdec_chan_get_cfg(KmppChanId id, KmppVdecCfg cfg);
rk_s32 kmpp_vdec_chan_set_cfg(KmppChanId id, KmppVdecCfg cfg);

rk_s32 kmpp_vdec_chan_decode(KmppChanId id, KmppPacket pkt, KmppFrame *frm);
rk_s32 kmpp_vdec_chan_get_pkt(KmppChanId id, KmppPacket *pkt);
rk_s32 kmpp_vdec_chan_put_pkt(KmppChanId id, KmppPacket pkt);
rk_s32 kmpp_vdec_chan_get_frm(KmppChanId id, KmppFrame *frm);
rk_s32 kmpp_vdec_chan_put_frm(KmppChanId id, KmppFrame frm);

#endif /*__KMPI_VDEC_H__*/

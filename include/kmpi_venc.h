/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPI_VENC_H__
#define __KMPI_VENC_H__

#include "kmpi_defs.h"

/* string - value config for control flow */
typedef void* KmppVencInitCfg;
typedef void* KmppVencDeinitCfg;
typedef void* KmppVencResetCfg;
typedef void* KmppVencStartCfg;
typedef void* KmppVencStopCfg;

/* static config */
typedef void* KmppVencStCfg;
/* runtime config */
typedef void* KmppVencRtCfg;

/*
 * There are two sets for interfaces:
 * context interface and chan id interface
 *
 * Each set of interface has the same interface functions:
 *
 * init / deinit
 * - encoder instance create / destroy interfaces.
 *
 * reset / start / stop / ctrl
 * - encoder control flow interfaces.
 *
 * cfg / rt_cfg
 * - encoder static parameter and runtime parameter interfaces.
 *
 * encode / put_frm / get_pkt
 * - encoder data flow interfaces.
 */

rk_s32 kmpp_venc_init(KmppCtx *ctx, KmppVencInitCfg cfg);
rk_s32 kmpp_venc_deinit(KmppCtx ctx, KmppVencDeinitCfg cfg);
rk_s32 kmpp_venc_chan_init(KmppChanId *id, KmppVencInitCfg cfg);
rk_s32 kmpp_venc_chan_deinit(KmppChanId id, KmppVencDeinitCfg cfg);

rk_s32 kmpp_venc_reset(KmppCtx ctx, KmppVencResetCfg cfg);
rk_s32 kmpp_venc_start(KmppCtx ctx, KmppVencStartCfg cfg);
rk_s32 kmpp_venc_stop(KmppCtx ctx, KmppVencStopCfg cfg);
rk_s32 kmpp_venc_ctrl(KmppCtx ctx, rk_s32 cmd, void *arg);
rk_s32 kmpp_venc_chan_reset(KmppChanId id, KmppVencResetCfg cfg);
rk_s32 kmpp_venc_chan_start(KmppChanId id, KmppVencStartCfg cfg);
rk_s32 kmpp_venc_chan_stop(KmppChanId id, KmppVencStopCfg cfg);
rk_s32 kmpp_venc_chan_ctrl(KmppChanId id, rk_s32 cmd, void *arg);

/* static config */
rk_s32 kmpp_venc_get_cfg(KmppCtx ctx, KmppVencStCfg cfg);
rk_s32 kmpp_venc_set_cfg(KmppCtx ctx, KmppVencStCfg cfg);
rk_s32 kmpp_venc_chan_get_cfg(KmppChanId id, KmppVencStCfg cfg);
rk_s32 kmpp_venc_chan_set_cfg(KmppChanId id, KmppVencStCfg cfg);

/* runtime config */
rk_s32 kmpp_venc_get_rt_cfg(KmppCtx ctx, KmppVencRtCfg cfg);
rk_s32 kmpp_venc_set_rt_cfg(KmppCtx ctx, KmppVencRtCfg cfg);
rk_s32 kmpp_venc_chan_get_rt_cfg(KmppChanId id, KmppVencRtCfg cfg);
rk_s32 kmpp_venc_chan_set_rt_cfg(KmppChanId id, KmppVencRtCfg cfg);

rk_s32 kmpp_venc_encode(KmppCtx ctx, KmppFrame frm, KmppPacket *pkt);
rk_s32 kmpp_venc_put_frm(KmppCtx ctx, KmppFrame frm);
rk_s32 kmpp_venc_get_pkt(KmppCtx ctx, KmppPacket *pkt);
rk_s32 kmpp_venc_chan_encode(KmppChanId id, KmppFrame frm, KmppPacket *pkt);
rk_s32 kmpp_venc_chan_put_frm(KmppChanId id, KmppFrame frm);
rk_s32 kmpp_venc_chan_get_pkt(KmppChanId id, KmppPacket *pkt);

#endif /*__KMPI_VENC_H__*/

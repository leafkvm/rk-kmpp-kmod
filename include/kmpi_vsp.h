/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPI_VSP_H__
#define __KMPI_VSP_H__

#include "kmpi_defs.h"

/* string - value config for control flow */
typedef void* KmppVspInitCfg;
typedef void* KmppVspDeinitCfg;
typedef void* KmppVspResetCfg;
typedef void* KmppVspStartCfg;
typedef void* KmppVspStopCfg;

/* static config */
typedef void* KmppVspStCfg;
/* runtime config */
typedef void* KmppVspRtCfg;
/* runtime output data */
typedef void* KmppVspRtOut;

/*
 * There are two sets for Video Signal Processer (VSP) interfaces:
 * context interface and chan id interface
 *
 * Each set of interface has the same interface functions:
 *
 * init / deinit
 * - video signal processer instance create / destroy interfaces.
 *
 * reset / start / stop / ctrl
 * - video signal processer control flow interfaces.
 *
 * cfg (st_cfg) / rt_cfg
 * - video signal processer static parameter and runtime parameter interfaces.
 *
 * encode / put_frm / get_pkt
 * - video signal processer data flow interfaces.
 */

rk_s32 kmpp_vsp_init(KmppCtx *ctx, KmppVspInitCfg cfg);
rk_s32 kmpp_vsp_deinit(KmppCtx ctx, KmppVspDeinitCfg cfg);
rk_s32 kmpp_vsp_chan_init(KmppChanId *id, KmppVspInitCfg cfg);
rk_s32 kmpp_vsp_chan_deinit(KmppChanId id, KmppVspDeinitCfg cfg);

rk_s32 kmpp_vsp_reset(KmppCtx ctx, KmppVspResetCfg cfg);
rk_s32 kmpp_vsp_start(KmppCtx ctx, KmppVspStartCfg cfg);
rk_s32 kmpp_vsp_stop(KmppCtx ctx, KmppVspStopCfg cfg);
rk_s32 kmpp_vsp_ctrl(KmppCtx ctx, rk_s32 cmd, void *arg);
rk_s32 kmpp_vsp_chan_reset(KmppChanId id, KmppVspResetCfg cfg);
rk_s32 kmpp_vsp_chan_start(KmppChanId id, KmppVspStartCfg cfg);
rk_s32 kmpp_vsp_chan_stop(KmppChanId id, KmppVspStopCfg cfg);
rk_s32 kmpp_vsp_chan_ctrl(KmppChanId id, rk_s32 cmd, void *arg);

rk_s32 kmpp_vsp_get_cfg(KmppCtx ctx, KmppVspStCfg *cfg, const rk_u8 *name);
rk_s32 kmpp_vsp_set_cfg(KmppCtx ctx, KmppVspStCfg cfg);
rk_s32 kmpp_vsp_chan_get_cfg(KmppChanId id, KmppVspStCfg *cfg, const rk_u8 *name);
rk_s32 kmpp_vsp_chan_set_cfg(KmppChanId id, KmppVspStCfg cfg);

rk_s32 kmpp_vsp_get_rt_cfg(KmppCtx ctx, KmppVspRtCfg *cfg, const rk_u8 *name);
rk_s32 kmpp_vsp_set_rt_cfg(KmppCtx ctx, KmppVspRtCfg cfg);
rk_s32 kmpp_vsp_chan_get_rt_cfg(KmppChanId id, KmppVspRtCfg *cfg, const rk_u8 *name);
rk_s32 kmpp_vsp_chan_set_rt_cfg(KmppChanId id, KmppVspRtCfg cfg);

rk_s32 kmpp_vsp_proc(KmppCtx ctx, KmppFrame in, KmppFrame *out);
rk_s32 kmpp_vsp_put_frm(KmppCtx ctx, KmppFrame frm);
rk_s32 kmpp_vsp_get_frm(KmppCtx ctx, KmppFrame *frm);
rk_s32 kmpp_vsp_chan_proc(KmppChanId id, KmppFrame in, KmppFrame *out);
rk_s32 kmpp_vsp_chan_put_frm(KmppChanId id, KmppFrame frm);
rk_s32 kmpp_vsp_chan_get_frm(KmppChanId id, KmppFrame *frm);

rk_s32 kmpp_vsp_proc_buf(KmppCtx ctx, KmppBuffer in, KmppBuffer *out);
rk_s32 kmpp_vsp_put_buf(KmppCtx ctx, KmppBuffer buf);
rk_s32 kmpp_vsp_get_buf(KmppCtx ctx, KmppBuffer *buf);
rk_s32 kmpp_vsp_chan_proc_buf(KmppChanId id, KmppBuffer in, KmppBuffer *out);
rk_s32 kmpp_vsp_chan_put_buf(KmppChanId id, KmppBuffer buf);
rk_s32 kmpp_vsp_chan_get_buf(KmppChanId id, KmppBuffer *buf);

#include "kmpp_vsp_objs.h"

#endif /*__KMPI_VSP_H__*/

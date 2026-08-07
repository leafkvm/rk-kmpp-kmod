/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPI_VSP_STUB_H__
#define __KMPI_VSP_STUB_H__

#include "kmpp_stub.h"
#include "kmpi_vsp.h"

rk_s32 kmpp_vsp_init(KmppCtx *ctx, KmppVspInitCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_vsp_init);
rk_s32 kmpp_vsp_deinit(KmppCtx ctx, KmppVspDeinitCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_vsp_deinit);
rk_s32 kmpp_vsp_chan_init(KmppChanId *id, KmppVspInitCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_vsp_chan_init);
rk_s32 kmpp_vsp_chan_deinit(KmppChanId id, KmppVspDeinitCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_vsp_chan_deinit);

rk_s32 kmpp_vsp_reset(KmppCtx ctx, KmppVspResetCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_vsp_reset);
rk_s32 kmpp_vsp_start(KmppCtx ctx, KmppVspStartCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_vsp_start);
rk_s32 kmpp_vsp_stop(KmppCtx ctx, KmppVspStopCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_vsp_stop);
rk_s32 kmpp_vsp_ctrl(KmppCtx ctx, rk_s32 cmd, void *arg)
STUB_FUNC_RET_ZERO(kmpp_vsp_ctrl);
rk_s32 kmpp_vsp_chan_reset(KmppChanId id, KmppVspResetCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_vsp_chan_reset);
rk_s32 kmpp_vsp_chan_start(KmppChanId id, KmppVspStartCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_vsp_chan_start);
rk_s32 kmpp_vsp_chan_stop(KmppChanId id, KmppVspStopCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_vsp_chan_stop);
rk_s32 kmpp_vsp_chan_ctrl(KmppChanId id, rk_s32 cmd, void *arg)
STUB_FUNC_RET_ZERO(kmpp_vsp_chan_ctrl);

rk_s32 kmpp_vsp_get_cfg(KmppCtx ctx, KmppVspStCfg *cfg, const rk_u8 *name)
STUB_FUNC_RET_ZERO(kmpp_vsp_get_cfg);
rk_s32 kmpp_vsp_set_cfg(KmppCtx ctx, KmppVspStCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_vsp_set_cfg);
rk_s32 kmpp_vsp_chan_get_cfg(KmppChanId id, KmppVspStCfg *cfg, const rk_u8 *name)
STUB_FUNC_RET_ZERO(kmpp_vsp_chan_get_cfg);
rk_s32 kmpp_vsp_chan_set_cfg(KmppChanId id, KmppVspStCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_vsp_chan_set_cfg);

rk_s32 kmpp_vsp_get_rt_cfg(KmppCtx ctx, KmppVspRtCfg *cfg, const rk_u8 *name)
STUB_FUNC_RET_ZERO(kmpp_vsp_get_rt_cfg);
rk_s32 kmpp_vsp_set_rt_cfg(KmppCtx ctx, KmppVspRtCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_vsp_set_rt_cfg);
rk_s32 kmpp_vsp_chan_get_rt_cfg(KmppChanId id, KmppVspRtCfg *cfg, const rk_u8 *name)
STUB_FUNC_RET_ZERO(kmpp_vsp_chan_get_rt_cfg);
rk_s32 kmpp_vsp_chan_set_rt_cfg(KmppChanId id, KmppVspRtCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_vsp_chan_set_rt_cfg);

rk_s32 kmpp_vsp_proc(KmppCtx ctx, KmppFrame in, KmppFrame *out)
STUB_FUNC_RET_ZERO(kmpp_vsp_proc);
rk_s32 kmpp_vsp_put_frm(KmppCtx ctx, KmppFrame frm)
STUB_FUNC_RET_ZERO(kmpp_vsp_put_frm);
rk_s32 kmpp_vsp_get_frm(KmppCtx ctx, KmppFrame *frm)
STUB_FUNC_RET_ZERO(kmpp_vsp_get_frm);
rk_s32 kmpp_vsp_chan_proc(KmppChanId id, KmppFrame in, KmppFrame *out)
STUB_FUNC_RET_ZERO(kmpp_vsp_chan_proc);
rk_s32 kmpp_vsp_chan_put_frm(KmppChanId id, KmppFrame frm)
STUB_FUNC_RET_ZERO(kmpp_vsp_chan_put_frm);
rk_s32 kmpp_vsp_chan_get_frm(KmppChanId id, KmppFrame *frm)
STUB_FUNC_RET_ZERO(kmpp_vsp_chan_get_frm);

rk_s32 kmpp_vsp_proc_buf(KmppCtx ctx, KmppBuffer in, KmppBuffer *out)
STUB_FUNC_RET_ZERO(kmpp_vsp_proc_buf);
rk_s32 kmpp_vsp_put_buf(KmppCtx ctx, KmppBuffer buf)
STUB_FUNC_RET_ZERO(kmpp_vsp_put_buf);
rk_s32 kmpp_vsp_get_buf(KmppCtx ctx, KmppBuffer *buf)
STUB_FUNC_RET_ZERO(kmpp_vsp_get_buf);
rk_s32 kmpp_vsp_chan_proc_buf(KmppChanId id, KmppBuffer in, KmppBuffer *out)
STUB_FUNC_RET_ZERO(kmpp_vsp_chan_proc_buf);
rk_s32 kmpp_vsp_chan_put_buf(KmppChanId id, KmppBuffer buf)
STUB_FUNC_RET_ZERO(kmpp_vsp_chan_put_buf);
rk_s32 kmpp_vsp_chan_get_buf(KmppChanId id, KmppBuffer *buf)
STUB_FUNC_RET_ZERO(kmpp_vsp_chan_get_buf);

#include "kmpp_vsp_objs_stub.h"

#endif /*__KMPI_VSP_STUB_H__*/

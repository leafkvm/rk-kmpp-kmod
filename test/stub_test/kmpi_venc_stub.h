/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPI_VENC_STUB_H__
#define __KMPI_VENC_STUB_H__

#include "kmpp_stub.h"
#include "kmpi_venc.h"

rk_s32 kmpp_venc_init(KmppCtx *ctx, KmppVencInitCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_venc_init);

rk_s32 kmpp_venc_deinit(KmppCtx ctx, KmppVencDeinitCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_venc_deinit);

rk_s32 kmpp_venc_chan_init(KmppChanId *id, KmppVencInitCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_venc_chan_init);

rk_s32 kmpp_venc_chan_deinit(KmppChanId id, KmppVencDeinitCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_venc_chan_deinit);

rk_s32 kmpp_venc_reset(KmppCtx ctx, KmppVencResetCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_venc_reset);

rk_s32 kmpp_venc_start(KmppCtx ctx, KmppVencStartCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_venc_start);

rk_s32 kmpp_venc_stop(KmppCtx ctx, KmppVencStopCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_venc_stop);

rk_s32 kmpp_venc_ctrl(KmppCtx ctx, rk_s32 cmd, void *arg)
STUB_FUNC_RET_ZERO(kmpp_venc_ctrl);

rk_s32 kmpp_venc_chan_reset(KmppChanId id, KmppVencResetCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_venc_chan_reset);

rk_s32 kmpp_venc_chan_start(KmppChanId id, KmppVencStartCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_venc_chan_start);

rk_s32 kmpp_venc_chan_stop(KmppChanId id, KmppVencStopCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_venc_chan_stop);

rk_s32 kmpp_venc_chan_ctrl(KmppChanId id, rk_s32 cmd, void *arg)
STUB_FUNC_RET_ZERO(kmpp_venc_chan_ctrl);

rk_s32 kmpp_venc_get_cfg(KmppCtx ctx, KmppVencStCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_venc_get_cfg);

rk_s32 kmpp_venc_set_cfg(KmppCtx ctx, KmppVencStCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_venc_set_cfg);

rk_s32 kmpp_venc_chan_get_cfg(KmppChanId id, KmppVencStCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_venc_chan_get_cfg);

rk_s32 kmpp_venc_chan_set_cfg(KmppChanId id, KmppVencStCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_venc_chan_set_cfg);

rk_s32 kmpp_venc_get_rt_cfg(KmppCtx ctx, KmppVencRtCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_venc_get_rt_cfg);

rk_s32 kmpp_venc_set_rt_cfg(KmppCtx ctx, KmppVencRtCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_venc_set_rt_cfg);

rk_s32 kmpp_venc_chan_get_rt_cfg(KmppChanId id, KmppVencRtCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_venc_chan_get_rt_cfg);

rk_s32 kmpp_venc_chan_set_rt_cfg(KmppChanId id, KmppVencRtCfg cfg)
STUB_FUNC_RET_ZERO(kmpp_venc_chan_set_rt_cfg);

rk_s32 kmpp_venc_encode(KmppCtx ctx, KmppFrame frm, KmppPacket *pkt)
STUB_FUNC_RET_ZERO(kmpp_venc_encode);

rk_s32 kmpp_venc_put_frm(KmppCtx ctx, KmppFrame frm)
STUB_FUNC_RET_ZERO(kmpp_venc_put_frm);

rk_s32 kmpp_venc_get_pkt(KmppCtx ctx, KmppPacket *pkt)
STUB_FUNC_RET_ZERO(kmpp_venc_get_pkt);

rk_s32 kmpp_venc_chan_encode(KmppChanId id, KmppFrame frm, KmppPacket *pkt)
STUB_FUNC_RET_ZERO(kmpp_venc_chan_encode);

rk_s32 kmpp_venc_chan_put_frm(KmppChanId id, KmppFrame frm)
STUB_FUNC_RET_ZERO(kmpp_venc_chan_put_frm);

rk_s32 kmpp_venc_chan_get_pkt(KmppChanId id, KmppPacket *pkt)
STUB_FUNC_RET_ZERO(kmpp_venc_chan_get_pkt);

#endif /*__KMPI_VENC_STUB_H__*/

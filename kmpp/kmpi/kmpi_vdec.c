/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#include "kmpp_osal.h"
#include "kmpp_sys.h"

#include "kmpi_vdec.h"
#include "kmpp_vdec.h"

rk_s32 kmpp_vdec_init(KmppCtx *ctx, KmppVdecInitCfg cfg)
{
    KmppVdec vdec = NULL;
    rk_s32 ret = rk_nok;

    if (!ctx || !cfg) {
        kmpp_loge_f("invalid ctx %px cfg %px\n", ctx, cfg);
        return ret;
    }

    *ctx = NULL;
    ret = vdec_impl_get(&vdec);
    if (ret || !vdec) {
        kmpp_loge_f("get vdec impl failed ret %d\n", ret);
        return ret;
    }

    ret = vdec_impl_init(vdec, cfg);
    if (ret) {
        kmpp_loge_f("vdec init failed ret %d\n", ret);
        vdec_impl_put(vdec);
        return ret;
    }

    *ctx = (KmppCtx)vdec;
    return ret;
}

rk_s32 kmpp_vdec_deinit(KmppCtx ctx, KmppVdecDeinitCfg cfg)
{
    KmppVdec vdec = (KmppVdec)ctx;
    rk_s32 ret = rk_nok;

    if (!vdec) {
        kmpp_loge_f("invalid ctx %px cfg %px\n", vdec, cfg);
        return ret;
    }

    ret = vdec_impl_deinit(vdec);
    if (ret)
        kmpp_loge_f("vdec deinit failed ret %d\n", ret);

    return ret | vdec_impl_put(vdec);
}

rk_s32 kmpp_vdec_reset(KmppCtx ctx, KmppVdecResetCfg cfg)
{
    KmppVdec vdec = (KmppVdec)ctx;

    if (!vdec) {
        kmpp_loge_f("invalid ctx %px cfg %px\n", vdec, cfg);
        return rk_nok;
    }

    return vdec_impl_reset(vdec);
}

rk_s32 kmpp_vdec_start(KmppCtx ctx, KmppVdecStartCfg cfg)
{
    KmppVdec vdec = (KmppVdec)ctx;

    if (!vdec) {
        kmpp_loge_f("invalid ctx %px cfg %px\n", vdec, cfg);
        return rk_nok;
    }

    return vdec_impl_start(vdec);
}

rk_s32 kmpp_vdec_stop(KmppCtx ctx, KmppVdecStopCfg cfg)
{
    KmppVdec vdec = (KmppVdec)ctx;

    if (!vdec) {
        kmpp_loge_f("invalid ctx %px cfg %px\n", vdec, cfg);
        return rk_nok;
    }

    return vdec_impl_stop(vdec);
}

rk_s32 kmpp_vdec_suspend(KmppCtx ctx, KmppVdecSuspendCfg cfg)
{
    KmppVdec vdec = (KmppVdec)ctx;

    if (!vdec) {
        kmpp_loge_f("invalid ctx %px cfg %px\n", vdec, cfg);
        return rk_nok;
    }

    return vdec_impl_suspend(vdec);
}

rk_s32 kmpp_vdec_resume(KmppCtx ctx, KmppVdecResumeCfg cfg)
{
    KmppVdec vdec = (KmppVdec)ctx;

    if (!vdec) {
        kmpp_loge_f("invalid ctx %px cfg %px\n", vdec, cfg);
        return rk_nok;
    }

    return vdec_impl_resume(vdec);
}

rk_s32 kmpp_vdec_get_cfg(KmppCtx ctx, KmppVdecCfg cfg)
{
    KmppVdec vdec = (KmppVdec)ctx;

    if (!vdec) {
        kmpp_loge_f("invalid ctx %px cfg %px\n", vdec, cfg);
        return rk_nok;
    }

    return vdec_impl_get_cfg(vdec, cfg);
}

rk_s32 kmpp_vdec_set_cfg(KmppCtx ctx, KmppVdecCfg cfg)
{
    KmppVdec vdec = (KmppVdec)ctx;

    if (!vdec) {
        kmpp_loge_f("invalid ctx %px cfg %px\n", vdec, cfg);
        return rk_nok;
    }

    return vdec_impl_set_cfg(vdec, cfg);
}

rk_s32 kmpp_vdec_decode(KmppCtx ctx, KmppPacket pkt, KmppFrame *frm)
{
    KmppVdec vdec = (KmppVdec)ctx;

    if (!vdec) {
        kmpp_loge_f("invalid ctx %px pkt %px frm %px\n", vdec, pkt, frm);
        return rk_nok;
    }

    return vdec_impl_decode(vdec, pkt, frm);
}

rk_s32 kmpp_vdec_get_pkt(KmppCtx ctx, KmppPacket *pkt)
{
    KmppVdec vdec = (KmppVdec)ctx;

    if (!vdec || !pkt) {
        kmpp_loge_f("invalid ctx %px pkt %px\n", vdec, pkt);
        return rk_nok;
    }

    return vdec_impl_get_pkt(vdec, pkt);
}

rk_s32 kmpp_vdec_put_pkt(KmppCtx ctx, KmppPacket pkt)
{
    KmppVdec vdec = (KmppVdec)ctx;

    if (!vdec) {
        kmpp_loge_f("invalid ctx %px pkt %px\n", vdec, pkt);
        return rk_nok;
    }

    return vdec_impl_put_pkt(vdec, pkt);
}

rk_s32 kmpp_vdec_get_frm(KmppCtx ctx, KmppFrame *frm)
{
    KmppVdec vdec = (KmppVdec)ctx;

    if (!vdec || !frm) {
        kmpp_loge_f("invalid ctx %px frm %px\n", vdec, frm);
        return rk_nok;
    }

    return vdec_impl_get_frm(vdec, frm);
}

rk_s32 kmpp_vdec_put_frm(KmppCtx ctx, KmppFrame frm)
{
    KmppVdec vdec = (KmppVdec)ctx;

    if (!vdec || !frm) {
        kmpp_loge_f("invalid ctx %px frm %px\n", vdec, frm);
        return rk_nok;
    }

    return vdec_impl_put_frm(vdec, frm);
}

rk_s32 kmpp_vdec_chan_init(KmppChanId *id, KmppVdecInitCfg cfg)
{
    return rk_ok;
}

rk_s32 kmpp_vdec_chan_deinit(KmppChanId id, KmppVdecDeinitCfg cfg)
{
    return rk_ok;
}

rk_s32 kmpp_vdec_chan_reset(KmppChanId id, KmppVdecResetCfg cfg)
{
    return rk_ok;
}

rk_s32 kmpp_vdec_chan_start(KmppChanId id, KmppVdecStartCfg cfg)
{
    return rk_ok;
}

rk_s32 kmpp_vdec_chan_stop(KmppChanId id, KmppVdecStopCfg cfg)
{
    return rk_ok;
}

rk_s32 kmpp_vdec_chan_suspend(KmppChanId id, KmppVdecSuspendCfg cfg)
{
    return rk_ok;
}

rk_s32 kmpp_vdec_chan_resume(KmppChanId id, KmppVdecResumeCfg cfg)
{
    return rk_ok;
}

rk_s32 kmpp_vdec_chan_get_cfg(KmppChanId id, KmppVdecCfg cfg)
{
    return rk_ok;
}

rk_s32 kmpp_vdec_chan_set_cfg(KmppChanId id, KmppVdecCfg cfg)
{
    return rk_ok;
}

rk_s32 kmpp_vdec_chan_decode(KmppChanId id, KmppPacket pkt, KmppFrame *frm)
{
    return rk_ok;
}

rk_s32 kmpp_vdec_chan_get_pkt(KmppChanId id, KmppPacket *pkt)
{
    return rk_ok;
}

rk_s32 kmpp_vdec_chan_put_pkt(KmppChanId id, KmppPacket pkt)
{
    return rk_ok;
}

rk_s32 kmpp_vdec_chan_get_frm(KmppChanId id, KmppFrame *frm)
{
    return rk_ok;
}

rk_s32 kmpp_vdec_chan_put_frm(KmppChanId id, KmppFrame frm)
{
    return rk_ok;
}

#include <linux/export.h>

EXPORT_SYMBOL(kmpp_vdec_init);
EXPORT_SYMBOL(kmpp_vdec_deinit);
EXPORT_SYMBOL(kmpp_vdec_start);
EXPORT_SYMBOL(kmpp_vdec_stop);
EXPORT_SYMBOL(kmpp_vdec_suspend);
EXPORT_SYMBOL(kmpp_vdec_resume);
EXPORT_SYMBOL(kmpp_vdec_get_cfg);
EXPORT_SYMBOL(kmpp_vdec_set_cfg);
EXPORT_SYMBOL(kmpp_vdec_decode);
EXPORT_SYMBOL(kmpp_vdec_get_pkt);
EXPORT_SYMBOL(kmpp_vdec_put_pkt);
EXPORT_SYMBOL(kmpp_vdec_get_frm);
EXPORT_SYMBOL(kmpp_vdec_put_frm);

EXPORT_SYMBOL(kmpp_vdec_chan_init);
EXPORT_SYMBOL(kmpp_vdec_chan_deinit);
EXPORT_SYMBOL(kmpp_vdec_chan_start);
EXPORT_SYMBOL(kmpp_vdec_chan_stop);
EXPORT_SYMBOL(kmpp_vdec_chan_suspend);
EXPORT_SYMBOL(kmpp_vdec_chan_resume);
EXPORT_SYMBOL(kmpp_vdec_chan_get_cfg);
EXPORT_SYMBOL(kmpp_vdec_chan_set_cfg);
EXPORT_SYMBOL(kmpp_vdec_chan_decode);
EXPORT_SYMBOL(kmpp_vdec_chan_get_pkt);
EXPORT_SYMBOL(kmpp_vdec_chan_put_pkt);
EXPORT_SYMBOL(kmpp_vdec_chan_get_frm);
EXPORT_SYMBOL(kmpp_vdec_chan_put_frm);

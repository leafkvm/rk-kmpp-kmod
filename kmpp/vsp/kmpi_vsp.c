/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/kernel.h>

#include "kmpp_log.h"
#include "kmpp_mem.h"
#include "kmpp_string.h"

#include "kmpp_obj.h"
#include "kmpi_vsp.h"
#include "kmpp_vsp_impl.h"
#include "kmpp_vsp_objs_impl.h"

#include "legacy_rockit.h"

typedef struct KmpiVspImpl_t {
    const rk_u8         *name;
    KmppVspApi          *api;
    void                *ctx;
} KmpiVspImpl;

rk_s32 kmpp_vsp_init(KmppCtx *ctx, KmppVspInitCfg cfg)
{
    KmppVspInitCfgImpl *init_cfg;
    KmpiVspImpl *impl;
    const rk_u8 *name;
    KmppVspApi *api;
    void *vsp_ctx;
    struct pp_chn_attr attr;
    rk_s32 ret;

    if (!ctx || !cfg) {
        kmpp_loge_f("invalid ctx %px cfg %px\n", ctx, cfg);
        return rk_nok;
    }

    *ctx = NULL;
    init_cfg = kmpp_obj_to_entry(cfg);
    name = init_cfg ? init_cfg->name.kptr : NULL;

    if (!init_cfg || !name) {
        kmpp_loge_f("invalid init_cfg entry %px name %px\n", init_cfg, name);
        return rk_nok;
    }

    api = kmpp_vsp_get_api(name);
    if (!api || !api->init)
        return rk_nok;

    vsp_ctx = NULL;
    kmpp_memset(&attr, 0, sizeof(attr));
    attr.max_width = init_cfg->max_width;
    attr.max_height = init_cfg->max_height;
    attr.width = init_cfg->max_width;
    attr.height = init_cfg->max_height;
    ret = api->init(&vsp_ctx, &attr);
    if (ret || !vsp_ctx) {
        kmpp_loge_f("failed to init vsp %s ret %d vsp_ctx %px\n", name, ret, vsp_ctx);
        return rk_nok;
    }

    impl = kmpp_calloc(sizeof(KmpiVspImpl));
    if (!impl) {
        kmpp_loge_f("failed to create KmpiVspImpl\n");
        return rk_nok;
    }

    impl->name = name;
    impl->api = api;
    impl->ctx = vsp_ctx;

    *ctx = impl;

    return rk_ok;
}

rk_s32 kmpp_vsp_deinit(KmppCtx ctx, KmppVspDeinitCfg cfg)
{
    KmpiVspImpl *impl;
    rk_s32 ret = rk_nok;

    if (!ctx) {
        kmpp_loge_f("invalid ctx %px\n", ctx);
        return rk_nok;
    }

    impl = (KmpiVspImpl *)ctx;
    if (impl->api && impl->api->deinit)
        ret = impl->api->deinit(impl->ctx);

    kmpp_free(impl);

    return ret;
}

rk_s32 kmpp_vsp_chan_init(KmppChanId *id, KmppVspInitCfg cfg)
{
    return rk_ok;
}

rk_s32 kmpp_vsp_chan_deinit(KmppChanId id, KmppVspDeinitCfg cfg)
{
    return rk_ok;
}

rk_s32 kmpp_vsp_reset(KmppCtx ctx, KmppVspResetCfg cfg)
{
    return rk_ok;
}

rk_s32 kmpp_vsp_start(KmppCtx ctx, KmppVspStartCfg cfg)
{
    return rk_ok;
}

rk_s32 kmpp_vsp_stop(KmppCtx ctx, KmppVspStopCfg cfg)
{
    return rk_ok;
}

/* runtime query / status check */
rk_s32 kmpp_vsp_ctrl(KmppCtx ctx, rk_s32 cmd, void *arg)
{
    return rk_ok;
}

rk_s32 kmpp_vsp_chan_reset(KmppChanId id, KmppVspResetCfg cfg)
{
    return rk_ok;
}

rk_s32 kmpp_vsp_chan_start(KmppChanId id, KmppVspStartCfg cfg)
{
    return rk_ok;
}

rk_s32 kmpp_vsp_chan_stop(KmppChanId id, KmppVspStopCfg cfg)
{
    return rk_ok;
}

rk_s32 kmpp_vsp_chan_ctrl(KmppChanId id, rk_s32 cmd, void *arg)
{
    return rk_ok;
}

rk_s32 kmpp_vsp_get_cfg(KmppCtx ctx, KmppVspStCfg *cfg, const rk_u8 *name)
{
    return rk_ok;
}

rk_s32 kmpp_vsp_set_cfg(KmppCtx ctx, KmppVspStCfg cfg)
{
    return rk_ok;
}

rk_s32 kmpp_vsp_chan_get_cfg(KmppChanId id, KmppVspStCfg *cfg, const rk_u8 *name)
{
    return rk_ok;
}

rk_s32 kmpp_vsp_chan_set_cfg(KmppChanId id, KmppVspStCfg cfg)
{
    return rk_ok;
}

rk_s32 kmpp_vsp_get_rt_cfg(KmppCtx ctx, KmppVspRtCfg *cfg, const rk_u8 *name)
{
    KmpiVspImpl *impl;
    KmppObj rt_cfg;
    KmppVspPpCfg *entry = NULL;
    rk_s32 one_shot = 0;
    rk_s32 ret = rk_nok;

    if (!ctx || !cfg) {
        kmpp_loge_f("invalid ctx %px cfg %px\n", ctx, cfg);
        return ret;
    }

    impl = (KmpiVspImpl *)ctx;
    if (!impl->api || !impl->api->ctrl) {
        kmpp_loge_f("invalid impl %px with invalid ctrl api\n", impl);
        return ret;
    }

    rt_cfg = *cfg;
    if (rt_cfg) {
        if (kmpp_obj_check(rt_cfg, __FUNCTION__)) {
            kmpp_loge_f("invalid rt_cfg %px\n", rt_cfg);
            return ret;
        }
    } else {
        kmpp_obj_get_by_name_f(&rt_cfg, "KmppVspRtCfg");
        if (!rt_cfg) {
            kmpp_loge_f("failed to get one_shot rt_cfg\n");
            return ret;
        }

        one_shot = 1;
    }

    entry = kmpp_obj_to_entry(rt_cfg);
    ret = impl->api->ctrl(impl->ctx, KMPP_VSP_CMD_GET_RT_CFG, entry);
    if (ret) {
        kmpp_loge_f("failed to get rt_cfg ret %d\n", ret);
        if (one_shot)
            kmpp_obj_put_f(rt_cfg);
        return ret;
    }
    entry->one_shot = one_shot;
    *cfg = rt_cfg;

    return ret;
}

rk_s32 kmpp_vsp_set_rt_cfg(KmppCtx ctx, KmppVspRtCfg cfg)
{
    KmpiVspImpl *impl;
    KmppVspPpCfg *entry = NULL;
    rk_s32 one_shot;
    rk_s32 ret = rk_nok;

    if (!ctx || !cfg || kmpp_obj_check(cfg, __FUNCTION__)) {
        kmpp_loge_f("invalid ctx %px cfg %px\n", ctx, cfg);
        return rk_nok;
    }

    entry = kmpp_obj_to_entry(cfg);
    one_shot = entry->one_shot;

    impl = (KmpiVspImpl *)ctx;
    if (impl->api && impl->api->ctrl) {
        ret = impl->api->ctrl(impl->ctx, KMPP_VSP_CMD_SET_RT_CFG, entry);
    } else {
        kmpp_loge_f("invalid impl %px with invalid ctrl api\n", impl);
    }

    if (one_shot)
        kmpp_obj_put_f(cfg);

    return ret;
}

rk_s32 kmpp_vsp_chan_get_rt_cfg(KmppChanId id, KmppVspRtCfg *cfg, const rk_u8 *name)
{
    return rk_ok;
}

rk_s32 kmpp_vsp_chan_set_rt_cfg(KmppChanId id, KmppVspRtCfg cfg)
{
    return rk_ok;
}

rk_s32 kmpp_vsp_proc(KmppCtx ctx, KmppFrame in, KmppFrame *out)
{
    KmpiVspImpl *impl;

    if (!ctx || !in) {
        kmpp_loge_f("invalid ctx %px in %px out %px\n", ctx, in, out);
        return rk_nok;
    }

    impl = (KmpiVspImpl *)ctx;
    if (impl->api && impl->api->proc)
        return impl->api->proc(impl->ctx, in, out);

    return rk_nok;
}

rk_s32 kmpp_vsp_put_frm(KmppCtx ctx, KmppFrame frm)
{
    return rk_ok;
}

rk_s32 kmpp_vsp_get_frm(KmppCtx ctx, KmppFrame *frm)
{
    return rk_ok;
}

rk_s32 kmpp_vsp_chan_proc(KmppChanId id, KmppFrame in, KmppFrame *out)
{
    return rk_ok;
}

rk_s32 kmpp_vsp_chan_put_frm(KmppChanId id, KmppFrame frm)
{
    return rk_ok;
}

rk_s32 kmpp_vsp_chan_get_frm(KmppChanId id, KmppFrame *frm)
{
    return rk_ok;
}

EXPORT_SYMBOL(kmpp_vsp_init);
EXPORT_SYMBOL(kmpp_vsp_deinit);
EXPORT_SYMBOL(kmpp_vsp_chan_init);
EXPORT_SYMBOL(kmpp_vsp_chan_deinit);
EXPORT_SYMBOL(kmpp_vsp_reset);
EXPORT_SYMBOL(kmpp_vsp_start);
EXPORT_SYMBOL(kmpp_vsp_stop);
EXPORT_SYMBOL(kmpp_vsp_ctrl);
EXPORT_SYMBOL(kmpp_vsp_chan_reset);
EXPORT_SYMBOL(kmpp_vsp_chan_start);
EXPORT_SYMBOL(kmpp_vsp_chan_stop);
EXPORT_SYMBOL(kmpp_vsp_chan_ctrl);
EXPORT_SYMBOL(kmpp_vsp_get_cfg);
EXPORT_SYMBOL(kmpp_vsp_set_cfg);
EXPORT_SYMBOL(kmpp_vsp_chan_get_cfg);
EXPORT_SYMBOL(kmpp_vsp_chan_set_cfg);
EXPORT_SYMBOL(kmpp_vsp_get_rt_cfg);
EXPORT_SYMBOL(kmpp_vsp_set_rt_cfg);
EXPORT_SYMBOL(kmpp_vsp_chan_get_rt_cfg);
EXPORT_SYMBOL(kmpp_vsp_chan_set_rt_cfg);
EXPORT_SYMBOL(kmpp_vsp_proc);
EXPORT_SYMBOL(kmpp_vsp_put_frm);
EXPORT_SYMBOL(kmpp_vsp_get_frm);
EXPORT_SYMBOL(kmpp_vsp_chan_proc);
EXPORT_SYMBOL(kmpp_vsp_chan_put_frm);
EXPORT_SYMBOL(kmpp_vsp_chan_get_frm);

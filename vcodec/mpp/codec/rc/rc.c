/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "rc"

#include "mpp_mem.h"
#include "mpp_2str.h"

#include "kmpp_log.h"
#include "kmpp_obj.h"

#include "rc_debug.h"
#include "rc.h"
#include "rc_api.h"
#include "rc_base.h"

typedef struct RcFlowImpl_t {
    void *ctx;
    const RcApi *api;
    MppEncCfg cfg;
    MppEncCfgSet *cfg_set;

    RK_S32 fps_in_flex;
    RK_S32 fps_in_num;
    RK_S32 fps_in_denom;
    RK_S32 fps_out_flex;
    RK_S32 fps_out_num;
    RK_S32 fps_out_denom;

    RK_S32 frm_cnt;

    RK_U32 frm_send;
    RK_U32 frm_done;
} RcFlowImpl;

const static char default_rc_api[] = "default";

MPP_RET rc_init(RcFlow *ctx, MppCodingType type, const char **request_name)
{
    RcFlowImpl *p = NULL;
    const char *name = NULL;
    RcApi *api = NULL;
    RK_U32 try_api = request_name && *request_name;
    MPP_RET ret = MPP_NOK;

    rc_dbg_flow("enter\n");

    if (try_api) {
        name = *request_name;
        api = rc_api_get(type, name);
        rc_dbg_api("try %s rc api %s -> %px\n", strof_coding_type(type), name, api);

        if (api)
            kmpp_logi("request %s rc api %s success\n", strof_coding_type(type), name);
        else
            kmpp_loge("request %s rc api %s failed -> default\n",
                      strof_coding_type(type), name);
    }

    if (!api) {
        name = default_rc_api;
        api = rc_api_get(type, name);
        rc_dbg_api("use %s rc api default -> %px\n", strof_coding_type(type), api);
    }

    kmpp_assert(api);

    if (api) {
        RK_U32 ctx_size = api->ctx_size();

        p = mpp_calloc_size(RcFlowImpl, sizeof(RcFlowImpl) + ctx_size);
        if (!p) {
            kmpp_loge_f("failed to create context size %d\n", ctx_size);
            ret = MPP_ERR_MALLOC;
        } else {
            p->ctx = (void *)(p + 1);
            p->api = api;
            p->frm_cnt = -1;
            ret = MPP_OK;
        }
    }

    *ctx = p;
    if (request_name)
        *request_name = name;

    rc_dbg_flow("leave %px\n", p);

    return ret;
}

MPP_RET rc_deinit(RcFlow ctx)
{
    RcFlowImpl *p = (RcFlowImpl *) ctx;
    const RcApi *api = p->api;
    MPP_RET ret = MPP_OK;

    rc_dbg_flow("enter %px\n", ctx);

    if (api && api->deinit)
        ret = api->deinit(p->ctx);

    MPP_FREE(p);

    rc_dbg_flow("leave %px\n", ctx);

    return ret;
}

const char *rc_api_name(RcFlow ctx)
{
    RcFlowImpl *p = (RcFlowImpl *) ctx;
    const char *name = default_rc_api;

    if (p && p->api)
        name = p->api->name;

    return name;
}

MPP_RET rc_update_usr_cfg(RcFlow ctx, MppEncCfg cfg)
{
    RcFlowImpl *p = (RcFlowImpl *) ctx;
    const RcApi *api = p->api;
    MPP_RET ret = MPP_OK;

    rc_dbg_flow("enter %px\n", ctx);

    p->cfg = cfg;
    p->cfg_set = (MppEncCfgSet *)kmpp_obj_to_entry(cfg);
    p->fps_in_flex = p->cfg_set->rc.fps_in_flex;
    p->fps_in_num = p->cfg_set->rc.fps_in_num;
    p->fps_in_denom = p->cfg_set->rc.fps_in_denom;
    p->fps_out_flex = p->cfg_set->rc.fps_out_flex;
    p->fps_out_num = p->cfg_set->rc.fps_out_num;
    p->fps_out_denom = p->cfg_set->rc.fps_out_denom;

    if (api && api->init)
        ret = api->init(p->ctx, cfg);

    rc_dbg_flow("leave %px\n", ctx);

    return ret;
}

MPP_RET rc_frm_check_drop(RcFlow ctx, EncRcTask *task)
{
    RcFlowImpl *p = (RcFlowImpl *) ctx;
    const RcApi *api = p->api;
    MPP_RET ret = MPP_OK;

    rc_dbg_flow("enter %px\n", ctx);

    if (api && api->frm_check_drop && task) {
        ret = api->frm_check_drop(p->ctx, task);
    } else {
        RK_S32 frm_cnt = p->frm_cnt;
        RK_S32 rate_in = p->fps_in_num * p->fps_out_denom;
        RK_S32 rate_out = p->fps_out_num * p->fps_in_denom;
        RK_S32 drop = 0;

        kmpp_assert(p->fps_in_denom >= 1);
        kmpp_assert(p->fps_out_denom >= 1);
        kmpp_assert(rate_in >= rate_out);

        // frame counter is inited to (rate_in - rate_out)  to encode first frame
        if (frm_cnt < 0)
            frm_cnt = rate_in - rate_out;

        frm_cnt += rate_out;

        if (frm_cnt < rate_in)
            drop = 1;
        else
            frm_cnt -= rate_in;

        p->frm_cnt = frm_cnt;
        task->frm.drop = drop;
    }

    rc_dbg_flow("leave %p drop %d\n", ctx, task->frm.drop);

    return ret;
}

#define RC_API_DEFINE(flow, name) \
    MPP_RET rc_##flow##_##name(RcFlow ctx, EncRcTask *task) \
    { \
        RcFlowImpl *p = (RcFlowImpl *) ctx; \
        const RcApi *api = p->api; \
        MPP_RET ret = MPP_OK; \
        rc_dbg_flow("enter %px\n", ctx); \
        if (api && api->flow##_##name && p->ctx && task) \
            ret = api->flow##_##name(p->ctx, task); \
        rc_dbg_flow("leave %px ret %d\n", ctx, ret); \
        return ret; \
    }

RC_API_DEFINE(frm, check_reenc)
RC_API_DEFINE(frm, start)
RC_API_DEFINE(frm, end)
RC_API_DEFINE(hal, start)
RC_API_DEFINE(hal, end);

MPP_RET rc_proc_show(KmppEnvNode node, RcFlow ctx, RK_S32 chl_id)
{
    RcFlowImpl *p = (RcFlowImpl *) ctx;

    if (!p || !p->api || !p->api->proc_show || !p->ctx )
        return MPP_OK;

    p->api->proc_show(node, p->ctx, chl_id);
    return 0;
}

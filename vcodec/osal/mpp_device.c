/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "mpp_device"

#include "kmpp_string.h"
#include "kmpp_log.h"
#include "mpp_mem.h"
#include "mpp_buffer_impl.h"
//#include "mpp_device_debug.h"
#include "mpp_service_api.h"
#include "mpp_service.h"

typedef struct MppDevImpl_t {
    MppClientType type;

    void *ctx;
    const MppDevApi *api;
} MppDevImpl;

RK_U32 mpp_device_debug = 0;

MPP_RET mpp_dev_init(MppDev *ctx, MppDevCfg *cfg)
{
    const MppDevApi *api = NULL;
    MppDevImpl *impl = NULL;
    void *impl_ctx = NULL;

    if (!ctx) {
        kmpp_loge_f("found NULL input ctx\n");
        return MPP_ERR_NULL_PTR;
    }
    // mpp_env_get_u32("mpp_device_debug", &mpp_device_debug, 0);
    *ctx = NULL;
    api = &mpp_service_api;
    impl = mpp_calloc(MppDevImpl, 1);
    impl_ctx = mpp_calloc_size(void, api->ctx_size);
    if (!impl || !impl_ctx) {
        kmpp_loge_f("malloc failed impl %p impl_ctx %p\n", impl,
                    impl_ctx);
        MPP_FREE(impl);
        MPP_FREE(impl_ctx);
        return MPP_ERR_MALLOC;
    }

    impl->ctx = impl_ctx;
    impl->api = api;
    impl->type = cfg->type;
    *ctx = impl;

    return api->init(impl_ctx, cfg);
}

MPP_RET mpp_dev_deinit(MppDev ctx)
{
    MppDevImpl *p = (MppDevImpl *) ctx;
    MPP_RET ret = MPP_OK;

    if (!p) {
        kmpp_loge_f("found NULL input ctx\n");
        return MPP_ERR_NULL_PTR;
    }

    if (p->api && p->api->deinit && p->ctx)
        ret = p->api->deinit(p->ctx);

    MPP_FREE(p->ctx);
    MPP_FREE(p);

    return ret;
}

MPP_RET mpp_dev_ioctl(MppDev ctx, RK_S32 cmd, void *param)
{
    MppDevImpl *p = (MppDevImpl *) ctx;
    const MppDevApi *api = p->api;
    void *impl_ctx = p->ctx;
    MPP_RET ret = MPP_OK;

    if (!p || !api)
        return ret;

    switch (cmd) {
    case MPP_DEV_REG_WR: {
        if (api->reg_wr)
            ret = api->reg_wr(impl_ctx, param);
    } break;
    case MPP_DEV_REG_RD: {
        if (api->reg_rd)
            ret = api->reg_rd(impl_ctx, param);
    } break;
    case MPP_DEV_CMD_SEND: {
        if (api->cmd_send)
            ret = api->cmd_send(impl_ctx);
    } break;
    case MPP_DEV_CMD_POLL: {
        if (api->cmd_poll)
            ret = api->cmd_poll(impl_ctx);
    } break;
    case MPP_DEV_CMD_RUN_TASK: {
        if (api->run_task)
            ret = api->run_task(impl_ctx, param);
    } break;
    default: {
        kmpp_loge_f("invalid cmd %d\n", cmd);
    } break;
    }

    return ret;
}

RK_U32 mpp_dev_get_iova_address(MppDev ctx, MppBuffer mpp_buf, RK_U32 reg_idx)
{
    MppDevImpl *p = (MppDevImpl *) ctx;

    if (!p)
        return 0;

    return mpp_buffer_get_iova(mpp_buf, ctx);

}

struct device * mpp_get_dev(MppDev ctx)
{
    MppDevImpl *p = (MppDevImpl *) ctx;
    const MppDevApi *api = p->api;
    void *impl_ctx = p->ctx;

    if (api->chnl_get_dev)
        return api->chnl_get_dev(impl_ctx);

    return NULL;
}

RK_S32 mpp_dev_chnl_check_running(MppDev ctx)
{
    MppDevImpl *p = (MppDevImpl *) ctx;
    const MppDevApi *api = p->api;
    void *impl_ctx = p->ctx;

    if (api->chnl_check_running)
        return api->chnl_check_running(impl_ctx);

    return 0;
}

RK_S32 mpp_dev_chnl_unbind_jpeg_task(MppDev ctx)
{
    MppDevImpl *p = (MppDevImpl *) ctx;
    const MppDevApi *api = p->api;
    void *impl_ctx = p->ctx;

    if (api->control)
        return api->control(impl_ctx, MPP_CMD_UNBIND_JPEG_TASK, NULL);

    return 0;
}

RK_S32 mpp_dev_chnl_control(MppDev ctx, RK_S32 cmd, void *param)
{
    MppDevImpl *p = (MppDevImpl *) ctx;
    const MppDevApi *api = p->api;
    void *impl_ctx = p->ctx;

    if (api->control)
        return api->control(impl_ctx, cmd, param);

    return 0;
}

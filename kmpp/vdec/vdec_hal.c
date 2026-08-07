/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#include "kmpp_mod_id.h"

#include "vdec_hal_api.h"

typedef enum VdecHalTaskStatus_e {
    HAL_VDEC_TASK_IDLE,
    HAL_VDEC_TASK_SETUP,
    HAL_VDEC_TASK_GEN_REG,
    HAL_VDEC_TASK_PENDING,
    HAL_VDEC_TASK_RUNNING,
    HAL_VDEC_TASK_DONE,

    HAL_VDEC_TASK_STATUS_BUTT,
} VdecHalTaskStatus;

typedef struct VdecHalTaskImpl_t {
    VdecHalTask     task;
    rk_s32          index;
    rk_u32          status;
} VdecHalTaskImpl;

typedef struct VdecHalImpl_t {
    KmppVdec        vdec;
    VdecHalApi      *api;

    MppCodingType   coding;

    rk_s32          task_cnt;
    rk_u32          task_id;

    rk_s32          task_wr;
    rk_s32          task_rd;

    rk_s32          ctx_size;
    void            *ctx;

    VdecHalTask     *tasks[];
} VdecHalImpl;

static kmpp_list_head list_vdec_api_api = KMPP_LIST_HEAD_INIT(list_vdec_api_api);

rk_s32 vdec_hal_add(VdecHalApi *api)
{
    if (api) {
        /* TODO: check soc type first to valid device only */
        kmpp_list_add_tail(&api->list, &list_vdec_api_api);
    }

    return rk_ok;
}

rk_s32 vdec_hal_del(VdecHalApi *api)
{
    if (api)
        kmpp_list_del_init(&api->list);

    return rk_ok;
}

static VdecHalApi *vdec_hal_find_api(MppCodingType coding)
{
    VdecHalApi *pos;

    kmpp_list_for_each_entry(pos, &list_vdec_api_api, VdecHalApi, list) {
        if (pos->coding == coding)
            return pos;
    }

    return NULL;
}

rk_s32 vdec_hal_query(VdecHalCfg *cfg)
{
    VdecHalApi *api = vdec_hal_find_api(cfg->coding);
    rk_s32 task_cnt = cfg->task_req;
    rk_s32 task_size = sizeof(VdecHalTaskImpl) * task_cnt;

    cfg->ctx_size = sizeof(VdecHalImpl) + task_size;

    if (!api) {
        kmpp_logi_f("no vdec hal api for coding %d\n", cfg->coding);
        return rk_nok;
    }

    cfg->ctx_size += api->ctx_size;

    return rk_ok;
}

rk_s32 vdec_hal_init(VdecHal hal, VdecHalCfg *cfg)
{
    VdecHalImpl *impl = (VdecHalImpl *)hal;
    VdecHalTaskImpl *task = (VdecHalTaskImpl *)(impl + 1);
    VdecHalApi *api = vdec_hal_find_api(cfg->coding);
    rk_s32 task_size = sizeof(VdecHalTaskImpl) * cfg->task_cnt;

    impl->vdec = cfg->vdec;
    impl->api = api;
    impl->coding = cfg->coding;
    impl->task_cnt = cfg->task_cnt;
    impl->task_id = 0;

    impl->task_wr = 0;
    impl->task_rd = impl->task_cnt;

    for (rk_s32 i = 0; i < cfg->task_cnt; i++) {
        task->index = i;
        task->task.reg_idx = i;
        impl->tasks[i] = &task[i].task;
    }

    if (!api)
        return rk_nok;

    impl->ctx_size = api->ctx_size;
    impl->ctx = ((rk_u8 *)(impl + 1)) + task_size;

    if (api->init)
        return api->init(impl->ctx, cfg);

    return rk_ok;
}

rk_s32 vdec_hal_deinit(VdecHal hal)
{
    VdecHalImpl *impl = (VdecHalImpl *)hal;
    VdecHalApi *api = impl->api;

    if (api && api->deinit)
        return api->deinit(impl->ctx);

    return rk_ok;
}

rk_s32 vdec_hal_reset(VdecHal hal)
{
    VdecHalImpl *impl = (VdecHalImpl *)hal;
    VdecHalApi *api = impl->api;

    if (api && api->reset)
        return api->reset(impl->ctx);

    return rk_ok;
}

rk_s32 vdec_hal_start(VdecHal hal)
{
    VdecHalImpl *impl = (VdecHalImpl *)hal;
    VdecHalApi *api = impl->api;

    if (api && api->start)
        return api->start(impl->ctx);

    return rk_ok;
}

rk_s32 vdec_hal_stop(VdecHal hal)
{
    VdecHalImpl *impl = (VdecHalImpl *)hal;
    VdecHalApi *api = impl->api;

    if (api && api->stop)
        return api->stop(impl->ctx);

    return rk_ok;
}

rk_s32 vdec_hal_get_task(VdecHal hal, VdecHalTask **task)
{
    VdecHalImpl *impl = (VdecHalImpl *)hal;
    VdecHalTaskImpl *t;

    *task = NULL;

    if (impl->task_rd == impl->task_wr)
        return rk_nok;

    t = (VdecHalTaskImpl *)impl->tasks[impl->task_rd];

    if (t->status == HAL_VDEC_TASK_IDLE) {
        t->status = HAL_VDEC_TASK_SETUP;
        impl->task_rd = (impl->task_rd + 1) % impl->task_cnt;
        *task = &t->task;
        return rk_ok;
    }

    return rk_nok;
}

rk_s32 vdec_hal_put_task(VdecHal hal, VdecHalTask *task)
{
    VdecHalImpl *impl = (VdecHalImpl *)hal;
    VdecHalApi *api = impl->api;

    if (api && api->put_task)
        return api->put_task(impl->ctx, task);

    return rk_ok;
}

rk_s32 vdec_hal_poll_task(VdecHal hal, VdecHalTask *task)
{
    VdecHalImpl *impl = (VdecHalImpl *)hal;
    VdecHalApi *api = impl->api;

    if (api && api->poll_task)
        return api->poll_task(impl->ctx, task);

    return rk_ok;
}

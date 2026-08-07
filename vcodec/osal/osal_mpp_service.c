/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "mpp_serivce"

#include "kmpp_string.h"

#include "kmpp_log.h"

//#include "mpp_device_debug.h"
#include "mpp_service.h"
#include "mpp_service_api.h"
#include "rk_export_func.h"
#include "kmpp_session.h"

typedef struct MppDevMppService_t {
    KmppSession session;
    struct device *dev;

    RK_S32 req_cnt;
    MppReqV1 reqs[MAX_REQ_NUM];
} MppDevMppService;

MPP_RET mpp_service_init(void *ctx, MppDevCfg *cfg)
{
    MppDevMppService *p = (MppDevMppService *) ctx;
    MPP_RET ret = MPP_OK;

    ret |= kmpp_session_get(&p->session);
    if (p->session) {
        KmppSessionCfg session_cfg = { 0 };

        session_cfg.client_type = cfg->type;
        session_cfg.chan_id = cfg->chan_id;
        session_cfg.online = cfg->online;
        session_cfg.hook = cfg->hook;

        ret |= kmpp_session_setup(p->session, &session_cfg);
        p->dev = kmpp_session_get_dev(p->session);
    }

    return ret;
}

MPP_RET mpp_service_deinit(void *ctx)
{
    MppDevMppService *p = (MppDevMppService *) ctx;

    return kmpp_session_put(p->session);
}

MPP_RET mpp_service_reg_wr(void *ctx, MppDevRegWrCfg * cfg)
{
    MppDevMppService *p = (MppDevMppService *) ctx;
    MppReqV1 *mpp_req = &p->reqs[p->req_cnt];

    if (!p->req_cnt)
        kmpp_memset(p->reqs, 0, sizeof(p->reqs));

    mpp_req->cmd = MPP_CMD_SET_REG_WRITE;
    mpp_req->flag = 0;
    mpp_req->size = cfg->size;
    mpp_req->offset = cfg->offset;
    mpp_req->data_ptr = REQ_DATA_PTR(cfg->reg);
    p->req_cnt++;

    return MPP_OK;
}

MPP_RET mpp_service_reg_rd(void *ctx, MppDevRegRdCfg * cfg)
{
    MppDevMppService *p = (MppDevMppService *) ctx;
    MppReqV1 *mpp_req = &p->reqs[p->req_cnt];

    if (!p->req_cnt)
        kmpp_memset(p->reqs, 0, sizeof(p->reqs));

    mpp_req->cmd = MPP_CMD_SET_REG_READ;
    mpp_req->flag = 0;
    mpp_req->size = cfg->size;
    mpp_req->offset = cfg->offset;
    mpp_req->data_ptr = REQ_DATA_PTR(cfg->reg);
    p->req_cnt++;

    return MPP_OK;
}

MPP_RET mpp_service_cmd_send(void *ctx)
{
    MppDevMppService *p = (MppDevMppService *) ctx;
    MPP_RET ret = MPP_OK;

    if (p->req_cnt <= 0 || p->req_cnt > MAX_REQ_NUM) {
        kmpp_loge_f("ctx %p invalid request count %d\n", ctx, p->req_cnt);
        return MPP_ERR_VALUE;
    }

    /* setup flag for multi message request */
    if (p->req_cnt > 1) {
        RK_S32 i;

        for (i = 0; i < p->req_cnt; i++)
            p->reqs[i].flag |= MPP_FLAGS_MULTI_MSG;
    }
    p->reqs[p->req_cnt - 1].flag |= MPP_FLAGS_LAST_MSG | MPP_FLAGS_REG_FD_NO_TRANS;
    ret = kmpp_session_add_req(p->session, &p->reqs[0]);
    p->req_cnt = 0;

    return ret;
}

MPP_RET mpp_service_cmd_poll(void *ctx)
{
    MppDevMppService *p = (MppDevMppService *) ctx;
    MppReqV1 dev_req;

    kmpp_memset(&dev_req, 0, sizeof(dev_req));
    dev_req.cmd = MPP_CMD_POLL_HW_FINISH;
    dev_req.flag |= MPP_FLAGS_LAST_MSG;

    return kmpp_session_add_req(p->session, &dev_req);
}

struct device * mpp_service_get_dev(void *ctx)
{
    MppDevMppService *p = (MppDevMppService *)ctx;

    return p ? p->dev : NULL;
}

RK_S32 mpp_service_run_task(void *ctx, void *param)
{
    MppDevMppService *p = (MppDevMppService *)ctx;

    return kmpp_session_run_task(p->session, param);
}

RK_S32 mpp_service_chnl_check_running(void *ctx)
{
    MppDevMppService *p = (MppDevMppService *)ctx;

    return kmpp_session_is_running(p->session);
}

RK_S32 mpp_service_chnl_control(void *ctx, RK_U32 cmd, void *param)
{
    MppDevMppService *p = (MppDevMppService *) ctx;
    MppReqV1 req;

    req.cmd = cmd;
    req.flag = MPP_FLAGS_LAST_MSG;
    req.data_ptr = REQ_DATA_PTR(param);

    return kmpp_session_add_req(p->session, &req);
}

const MppDevApi mpp_service_api = {
    .name               = "mpp_service",
    .ctx_size           = sizeof(MppDevMppService),
    .init               = mpp_service_init,
    .deinit             = mpp_service_deinit,
    .reg_wr             = mpp_service_reg_wr,
    .reg_rd             = mpp_service_reg_rd,
    .cmd_send           = mpp_service_cmd_send,
    .cmd_poll           = mpp_service_cmd_poll,
    .chnl_get_dev       = mpp_service_get_dev,
    .run_task           = mpp_service_run_task,
    .chnl_check_running = mpp_service_chnl_check_running,
    .control            = mpp_service_chnl_control,
};

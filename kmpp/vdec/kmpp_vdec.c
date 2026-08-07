/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_vdec"

#include "kmpp_log.h"
#include "kmpp_macro.h"
#include "kmpp_module.h"

#include "kmpp_obj.h"

#include "kmpp_mod_id.h"
#include "kmpp_buffer.h"
#include "kmpp_vdec.h"
#include "kmpp_vdec_ioctl.h"

#include "mpp_maths.h"
#include "mpp_buffer_impl.h"

#include "vdec_parser.h"
#include "vdec_hal.h"

#include "h264d_api.h"
#include "h265d_api.h"
#include "jpegd_api.h"
#include "hal_h264d_api.h"
#include "hal_h265d_api.h"
#include "hal_jpegd_api.h"

typedef enum VdecTaskStage_e {
    VDEC_TASK_GET_PARSER_OUT,
    VDEC_TASK_GET_HAL_TASK,
    VDEC_TASK_SETUP_BUFFER,
    VDEC_TASK_PUT_HAL_TASK,
    VDEC_TASK_POLL_UPDATE,
    VDEC_TASK_WAIT_HAL_TASK,
    VDEC_TASK_PUT_PARSER_OUT,
    VDEC_TASK_END,
} VdecTaskStage;

#define get_vdec_srv(caller) \
    ({ \
        KmppVdecSrv *__tmp; \
        if (srv_vdec) { \
            __tmp = srv_vdec; \
        } else { \
            kmpp_loge_f("kmpp vdec srv not init at %s\n", caller); \
            __tmp = NULL; \
        } \
        __tmp; \
    })

#define get_vdec_srv_f() get_vdec_srv(__FUNCTION__)

typedef struct KmppVdecSrv_t {
    KmppSpinlock        *lock;
    kmpp_list_head      list;
    KmppEnvNode         env;
} KmppVdecSrv;

typedef struct KmppVdecImpl_t {
    /* list to list in KmppVdecIocSrv */
    kmpp_list_head      list_srv;

    MppCodingType       coding;

    /* output frame queue */
    KmppSpinlock        *lock_frm;
    kmpp_list_head      list_frm;

    /* valid frame buffer queue for quick resource acquire */
    KmppSpinlock        *lock_frm_buf;
    kmpp_list_head      list_frm_buf;

    KmppBufGrp          pkt_grp;
    KmppBufGrp          frm_grp;

    rk_s32              ctx_size;
    void                *ctx_buf;

    /* all internal runtime configs */
    KmppObj             cfg_obj;
    KmppVdecCfgImpl     *cfg;

    VdecParser          parser;
    VdecHal             hal;

    KmppVdecTask        *tasks;
    rk_s32              task_cnt;

    kmpp_list_head      list_run;
    kmpp_list_head      list_idle;
} KmppVdecImpl;

static KmppVdecSrv *srv_vdec = NULL;
rk_u32 kmpp_vdec_debug = 0;

rk_s32 vdec_cfg_init(KmppVdecCfgImpl *cfg)
{
    if (cfg) {
        cfg->base.type = MPP_CTX_BUTT;
        cfg->base.coding = MPP_VIDEO_CodingUnused;

        /* non-block input and input 4 packet slot */
        cfg->input.timeout = 0;
        cfg->input.queue_size = 4;

        /* non-block output */
        cfg->output.timeout = 0;
        cfg->output.queue_size = -1;

        cfg->cfg.dpb_cnt = -1;
        /* two buffer for hardware decoding */
        cfg->cfg.hal_task_cnt = 2;
        /* three buffer for display */
        cfg->cfg.extra_buf_cnt = 3;
        /* default invalid buffer mode */
        cfg->cfg.buf_mode = -1;

        return rk_ok;
    }

    return rk_nok;
}

static rk_s32 vdec_obj_init(void *entry, KmppObj obj, KmppFsDev *file, const rk_u8 *caller)
{
    KmppVdecImpl *impl = (KmppVdecImpl *)kmpp_obj_to_priv(obj);
    KmppVdecSrv *srv = get_vdec_srv(caller);
    KmppShmPtr sptr;

    vdec_dbg_flow("enter\n");

    if (!srv)
        return rk_nok;

    if (impl) {
        KMPP_INIT_LIST_HEAD(&impl->list_srv);
        KMPP_INIT_LIST_HEAD(&impl->list_frm);

        kmpp_vdec_init_cfg_assign(&impl->cfg_obj, (void *)(impl + 1), kmpp_vdec_init_cfg_size());
        impl->cfg = kmpp_obj_to_entry(impl->cfg_obj);

        kmpp_spin_lock(srv->lock);
        kmpp_list_add_tail(&impl->list_srv, &srv->list);
        kmpp_spin_unlock(srv->lock);
    }

    kmpp_obj_to_shmptr(obj, &sptr);

    vdec_dbg_flow("leave\n");

    return rk_ok;
}

static rk_s32 vdec_obj_deinit(void *entry, KmppObj obj, const rk_u8 *caller)
{
    KmppVdecImpl *impl = (KmppVdecImpl *)kmpp_obj_to_priv(obj);
    KmppVdecSrv *srv = get_vdec_srv(caller);
    KmppShmPtr sptr;

    vdec_dbg_flow("enter\n");

    if (!srv)
        return rk_nok;

    kmpp_spin_lock(srv->lock);
    kmpp_list_del_init(&impl->list_srv);
    kmpp_spin_unlock(srv->lock);

    kmpp_obj_to_shmptr(obj, &sptr);

    if (impl->cfg_obj) {
        kmpp_obj_put(impl->cfg_obj, caller);
        impl->cfg_obj = NULL;
    }
    impl->cfg = NULL;

    vdec_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 vdec_impl_init(KmppVdec vdec, KmppVdecInitCfg init_cfg)
{
    KmppVdecImpl *impl = (KmppVdecImpl *)kmpp_obj_to_priv(vdec);
    KmppVdecCfgImpl *cfg = impl->cfg;
    VdecHalCfg hal_cfg = { 0 };
    ParserCfg parser_cfg = { 0 };
    rk_s32 ret;

    if (!impl) {
        kmpp_loge_f("invalid input vdec %px\n", vdec);
        return rk_nok;
    }

    vdec_dbg_flow("enter\n");

    /* check input mode and create input data queue first */
    kmpp_obj_update(impl->cfg_obj, init_cfg);

    /* init input resource management */

    hal_cfg.coding = cfg->base.coding;
    hal_cfg.task_req = 2;

    ret = vdec_hal_query(&hal_cfg);

    parser_cfg.coding = cfg->base.coding;

    ret = parser_query(&parser_cfg);

    {
        rk_s32 parser_ctx_size = parser_cfg.ctx_size;
        rk_s32 hal_ctx_size = hal_cfg.ctx_size;
        rk_s32 vdec_task_cnt = hal_cfg.task_cnt;
        rk_s32 total_size = parser_ctx_size + hal_ctx_size + vdec_task_cnt * sizeof(KmppVdecTask);
        rk_u8 *ctx_buf = kmpp_calloc(total_size);

        if (!ctx_buf) {
            kmpp_loge_f("kmpp vdec ctx buf size %d alloc failed\n", total_size);
            return rk_nok;
        }

        impl->ctx_buf = ctx_buf;
        impl->ctx_size = total_size;

        impl->parser = (VdecParser)ctx_buf;
        impl->hal = (VdecHal *)(ctx_buf + parser_ctx_size);
        impl->tasks = (KmppVdecTask *)(ctx_buf + parser_ctx_size + hal_ctx_size);
    }

    do {
        /* init hal first to get device feature */
        hal_cfg.vdec = impl;

        ret = vdec_hal_init(impl->hal, &hal_cfg);
        if (ret) {
            kmpp_loge_f("kmpp vdec hal init failed\n");
            break;
        }

        /* init parser with device feature info */
        parser_cfg.vdec = impl;

        ret = parser_init(impl->parser, &parser_cfg);
        if (ret)
            kmpp_loge_f("kmpp vdec parser init failed\n");
    } while (0);

    if (ret) {
        kmpp_free(impl->ctx_buf);
        impl->ctx_buf = NULL;
        impl->ctx_size = 0;
    }

    vdec_dbg_flow("leave\n");

    return ret;
}

rk_s32 vdec_impl_deinit(KmppVdec vdec)
{
    KmppVdecImpl *impl = (KmppVdecImpl *)kmpp_obj_to_priv(vdec);

    if (!impl) {
        kmpp_loge_f("invalid input vdec %px\n", vdec);
        return rk_nok;
    }

    vdec_dbg_flow("enter\n");

    kmpp_free(impl->ctx_buf);

    vdec_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 vdec_impl_reset(KmppVdec vdec)
{
    KmppVdecImpl *impl = (KmppVdecImpl *)kmpp_obj_to_priv(vdec);

    if (!impl) {
        kmpp_loge_f("invalid input vdec %px\n", vdec);
        return rk_nok;
    }

    vdec_dbg_flow("enter\n");

    vdec_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 vdec_impl_start(KmppVdec vdec)
{
    KmppVdecImpl *impl = (KmppVdecImpl *)kmpp_obj_to_priv(vdec);

    if (!impl) {
        kmpp_loge_f("invalid input vdec %px\n", vdec);
        return rk_nok;
    }

    vdec_dbg_flow("enter\n");

    vdec_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 vdec_impl_stop(KmppVdec vdec)
{
    KmppVdecImpl *impl = (KmppVdecImpl *)kmpp_obj_to_priv(vdec);

    if (!impl) {
        kmpp_loge_f("invalid input vdec %px\n", vdec);
        return rk_nok;
    }

    vdec_dbg_flow("enter\n");

    vdec_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 vdec_impl_suspend(KmppVdec vdec)
{
    KmppVdecImpl *impl = (KmppVdecImpl *)kmpp_obj_to_priv(vdec);

    if (!impl) {
        kmpp_loge_f("invalid input vdec %px\n", vdec);
        return rk_nok;
    }

    vdec_dbg_flow("enter\n");

    vdec_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 vdec_impl_resume(KmppVdec vdec)
{
    KmppVdecImpl *impl = (KmppVdecImpl *)kmpp_obj_to_priv(vdec);

    if (!impl) {
        kmpp_loge_f("invalid input vdec %px\n", vdec);
        return rk_nok;
    }

    vdec_dbg_flow("enter\n");

    vdec_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 vdec_impl_get_cfg(KmppVdec vdec, KmppVdecCfg cfg)
{
    KmppVdecImpl *impl = (KmppVdecImpl *)kmpp_obj_to_priv(vdec);

    if (!impl) {
        kmpp_loge_f("invalid input vdec %px\n", vdec);
        return rk_nok;
    }

    vdec_dbg_flow("enter\n");

    vdec_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 vdec_impl_set_cfg(KmppVdec vdec, KmppVdecCfg cfg)
{
    KmppVdecImpl *impl = (KmppVdecImpl *)kmpp_obj_to_priv(vdec);

    if (!impl) {
        kmpp_loge_f("invalid input vdec %px\n", vdec);
        return rk_nok;
    }

    vdec_dbg_flow("enter\n");

    vdec_dbg_flow("leave\n");

    return rk_ok;
}

static rk_s32 vdec_try_proc_pkt_in(KmppVdecImpl *impl, KmppPacket pkt)
{
    ParserJob *job = NULL;
    rk_s32 ret;

    /* 1. try get parser job */
    ret = parser_get_job(impl->parser, &job, 0);
    if (ret || !job)
        return rk_nok;

    /* 2. set input packet to parser input job */
    job->pkt = pkt;

    /* 3. start parser job */
    return parser_put_job(impl->parser, job);
}

static KmppVdecTask *vdec_get_task(KmppVdecImpl *impl)
{
    return NULL;
}

static rk_s32 vdec_try_proc_parser_out(KmppVdecImpl *impl)
{
    KmppVdecTask *task = NULL;
    ParserOut *out = NULL;
    VdecHalTask *hal_task = NULL;
    rk_s32 ret = rk_ok;

    /* 1. try get a vdec task first */
    task = vdec_get_task(impl);
    if (!task)
        return rk_ok;

    do {
        switch (task->stage) {
        case VDEC_TASK_GET_PARSER_OUT : {
            /* 2. setup parser output */
            if (task->parser_out_idx < 0) {
                ret = parser_get_out(impl->parser, &out);
                if (ret || !out)
                    break;

                task->parser_out_idx = out->index;
                task->parser_out = out;
                task->stage = VDEC_TASK_GET_HAL_TASK;
            }
        } break;
        case VDEC_TASK_GET_HAL_TASK : {
            /* 3. setup hal task */
            if (task->hal_task_idx < 0) {
                ret = vdec_hal_get_task(impl->hal, &hal_task);
                if (ret || !hal_task)
                    break;

                task->hal_task_idx = hal_task->index;
                task->hal_task = hal_task;
                task->stage = VDEC_TASK_SETUP_BUFFER;
            }
        } break;
        case VDEC_TASK_SETUP_BUFFER : {
            KmppFrame frm = task->parser_out->frm;

            /* 4. setup buffer resource */
            if (!kmpp_frame_has_buffer(frm)) {
                KmppBuffer buf = NULL;

                kmpp_buffer_get(&buf);
                if (buf) {
                    KmppBufCfg buf_cfg = kmpp_buffer_get_cfg_k(buf);
                    KmppShmPtr sptr = {0};

                    kmpp_obj_to_shmptr(impl->frm_grp, &sptr);
                    kmpp_buf_cfg_set_group(buf_cfg, &sptr);
                    kmpp_buf_cfg_set_size(buf_cfg, 4096);
                    kmpp_buf_cfg_set_offset(buf_cfg, 0);
                    kmpp_buf_cfg_set_flag(buf_cfg, 0);
                    kmpp_buffer_setup_f(buf, NULL);

                    kmpp_frame_set_buffer(frm, buf);
                    task->stage = VDEC_TASK_PUT_HAL_TASK;
                }
            }
        } break;
        case VDEC_TASK_PUT_HAL_TASK : {
            /* 5. put hal task */
            ret = vdec_hal_put_task(impl->hal, task->hal_task);
            if (ret)
                break;

            if (task->parser_out->need_update)
                task->stage = VDEC_TASK_POLL_UPDATE;
            else
                task->stage = VDEC_TASK_WAIT_HAL_TASK;
        } break;
        case VDEC_TASK_POLL_UPDATE : {
            /* 6. poll both parser out and hal task */
            ret = parser_poll_out(impl->parser, task->parser_out);
            if (ret > 0) {
                /* found parser out update */
                vdec_hal_update_task(impl->hal, task->hal_task);
            } else if (!ret) {
                /* no more parser out update */
                task->stage = VDEC_TASK_WAIT_HAL_TASK;
            }
        } break;
        case VDEC_TASK_WAIT_HAL_TASK : {
            ret = vdec_hal_poll_task(impl->hal, task->hal_task);
            if (ret)
                break;

            task->stage = VDEC_TASK_GET_PARSER_OUT;
        } break;
        case VDEC_TASK_PUT_PARSER_OUT : {
            /* 6. put parser output */
            ret = parser_put_out(impl->parser, task->parser_out);
        } break;
        default : {
            kmpp_logi_f("invalid task state %d\n", task->stage);
            ret = rk_nok;
        } break;
        }
    } while (!ret);

    return ret;
}

rk_s32 vdec_impl_decode(KmppVdec vdec, KmppPacket pkt, KmppFrame *frame)
{
    KmppVdecImpl *impl = (KmppVdecImpl *)kmpp_obj_to_priv(vdec);
    rk_s32 ret = rk_nok;

    if (!impl) {
        kmpp_loge_f("invalid input vdec %px\n", vdec);
        return rk_nok;
    }

    vdec_dbg_flow("enter\n");

    /* 1. try process input packet */
    ret = vdec_try_proc_pkt_in(impl, pkt);
    if (ret)
        return ret;

    /* 2. try get output from parser */
    ret = vdec_try_proc_parser_out(impl);

    ret = parser_get_frm(impl->parser, frame);

    vdec_dbg_flow("leave\n");

    return ret;
}

rk_s32 vdec_impl_get_pkt(KmppVdec vdec, KmppPacket *pkt)
{
    KmppVdecImpl *impl = (KmppVdecImpl *)kmpp_obj_to_priv(vdec);

    if (!impl) {
        kmpp_loge_f("invalid input vdec %px\n", vdec);
        return rk_nok;
    }

    vdec_dbg_flow("enter\n");

    vdec_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 vdec_impl_put_pkt(KmppVdec vdec, KmppPacket pkt)
{
    KmppVdecImpl *impl = (KmppVdecImpl *)kmpp_obj_to_priv(vdec);

    if (!impl) {
        kmpp_loge_f("invalid input vdec %px\n", vdec);
        return rk_nok;
    }

    vdec_dbg_flow("enter\n");

    vdec_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 vdec_impl_get_frm(KmppVdec vdec, KmppFrame *frm)
{
    KmppVdecImpl *impl = (KmppVdecImpl *)kmpp_obj_to_priv(vdec);

    if (!impl) {
        kmpp_loge_f("invalid input vdec %px\n", vdec);
        return rk_nok;
    }

    vdec_dbg_flow("enter\n");

    vdec_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 vdec_impl_put_frm(KmppVdec vdec, KmppFrame frm)
{
    KmppVdecImpl *impl = (KmppVdecImpl *)kmpp_obj_to_priv(vdec);

    if (!impl) {
        kmpp_loge_f("invalid input vdec %px\n", vdec);
        return rk_nok;
    }

    vdec_dbg_flow("enter\n");

    vdec_dbg_flow("leave\n");

    return rk_ok;
}

#define KMPP_OBJ_NAME               vdec_impl
#define KMPP_OBJ_INTF_TYPE          KmppVdec
#define KMPP_OBJ_PRIV_SIZE          (sizeof(KmppVdecImpl) + kmpp_vdec_init_cfg_size())
#define KMPP_OBJ_FUNC_INIT          vdec_obj_init
#define KMPP_OBJ_FUNC_DEINIT        vdec_obj_deinit
#define KMPP_OBJ_FUNC_IOCTL         kmpp_vdec_ioc_info
#include "kmpp_obj_helper.h"

static rk_s32 kmpp_vdec_srv_init(void)
{
    KmppVdecSrv *srv = srv_vdec;
    rk_s32 lock_size;
    rk_s32 size;

    if (srv)
        return rk_ok;

    lock_size = kmpp_spinlock_size();
    size = sizeof(KmppVdecSrv) + lock_size;
    srv = (KmppVdecSrv *)kmpp_calloc_atomic(size);
    if (!srv) {
        kmpp_loge_f("failed to alloc vdec srv size %d\n", size);
        return rk_nok;
    }

    kmpp_spinlock_assign(&srv->lock, (void *)(srv + 1), lock_size);

    KMPP_INIT_LIST_HEAD(&srv->list);

    vdec_impl_objdef_get();

    h264d_api_register();
    h265d_api_register();
    jpegd_api_register();
    hal_h264d_api_register();
    hal_h265d_api_register();
    hal_jpegd_api_register();

    {
        KmppEnvInfo env;

        env.type = KmppEnv_u32;
        env.readonly = 0;
        env.name = "kmpp_vdec_debug";
        env.val = &kmpp_vdec_debug;
        env.env_show = NULL;

        kmpp_env_add(kmpp_env_debug(), &srv->env, &env);
    }

    srv_vdec = srv;

    return rk_ok;
}

static rk_s32 kmpp_vdec_srv_deinit(void)
{
    KmppVdecSrv *srv = srv_vdec;

    if (!srv)
        return rk_nok;

    h264d_api_unregister();
    h265d_api_unregister();
    jpegd_api_unregister();
    hal_h264d_api_unregister();
    hal_h265d_api_unregister();
    hal_jpegd_api_unregister();

    vdec_impl_objdef_put();

    kmpp_env_del(kmpp_env_debug(), srv->env);

    kmpp_spinlock_deinit(&srv->lock);

    kmpp_free(srv);
    srv_vdec = NULL;

    return rk_ok;
}

KMPP_MODULE_ADD(kmpp, KMPP_MOD_VDEC, vdec, kmpp_vdec_srv_init, kmpp_vdec_srv_deinit);

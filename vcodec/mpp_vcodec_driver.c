/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>

#include "kmpp_cls.h"
#include "kmpp_env.h"
#include "kmpp_poll.h"
#include "kmpp_module.h"

#include "mpp_vcodec_chan.h"
#include "mpp_vcodec_base.h"
#include "mpp_vcodec_flow.h"
#include "mpp_vcodec_intf.h"

#include "kmpp_log.h"
#include "mpp_enc.h"
#include "mpp_buffer_impl.h"
#include "mpp_vcodec_thread.h"
#include "rk_venc_cfg.h"

#include "kmpp_uaccess.h"
#include "kmpp_mem.h"
#include "kmpp_obj.h"
#include "kmpp_sys_defs.h"
#include "kmpp_shm.h"
#include "kmpp_sema.h"
#include "kmpp_cls.h"
#include "rk-mpp-kobj.h"
#include "kmpp_venc_objs_impl.h"

struct vcodec_msg {
    __u32 cmd;
    __u32 ctrl_cmd;
    __u32 size;
    __u64 data_ptr;
};

struct chanid_ctx {
    RK_S32 chan_id;
    MppCtxType type;
    void *param;
    RK_U32 size;
    KmppAtomic *release_request;
    KmppSemaphore *ioctl_sem;
    RK_U32 chan_dup;
    RK_U32 chan_create;
};

static rk_s32 vcodec_open(KmppFsDev *file)
{
    struct chanid_ctx *ctx = kmpp_calloc(sizeof(*ctx));

    if (!ctx)
        return -ENOMEM;

    file->priv_data = (void *)ctx;
    kmpp_atomic_init(&ctx->release_request);
    kmpp_atomic_set(ctx->release_request, 0);
    kmpp_sema_init(&ctx->ioctl_sem, 1);

    return 0;
}

static rk_s32 vcodec_close(KmppFsDev *file)
{
    struct chanid_ctx *ctx = file->priv_data;

    if (!ctx) {
        kmpp_loge_f("chan id ctx is null\n");
        return -EINVAL;
    }

    kmpp_atomic_inc(ctx->release_request);

    if (!ctx->chan_dup && ctx->chan_create)
        mpp_vcodec_chan_destory(ctx->chan_id, ctx->type);
    if (ctx->param) {
        kmpp_free(ctx->param);
        ctx->param = NULL;
        ctx->size = 0;
    }
    kmpp_atomic_deinit(&ctx->release_request);
    kmpp_sema_deinit(&ctx->ioctl_sem);
    kmpp_free(ctx);

    return 0;
}

static int vcodec_process_msg(struct vcodec_msg *msg, struct vcodec_request *req)
{
    req->cmd = msg->cmd;
    req->ctrl_cmd = msg->ctrl_cmd;
    req->size = msg->size;
    req->data = (void *)(unsigned long)msg->data_ptr;

    return 0;
}

static int vcodec_process_cmd(void *private, struct vcodec_request *req)
{
    struct chanid_ctx *ctx = private;
    int chan_id = ctx->chan_id;
    MppCtxType type = ctx->type;
    void *param = ctx->param;
    int ret = 0;

    if (req->size > 0 && req->size > ctx->size) {
        if (ctx->param) {
            kmpp_free(ctx->param);
            ctx->param = NULL;
            ctx->size = 0;
        }
        param = kmpp_calloc(req->size);
        if (!param)
            return -ENOMEM;
        ctx->param = param;
        ctx->size = req->size;
    }

    switch (req->cmd) {
    case VCODEC_CHAN_CREATE: {
        KmppVencInitCfgImpl *cfg = NULL;
        KmppObjShm *ioc = (KmppObjShm *)param;
        KmppShm shm;
        void *kbase;

        if (kmpp_copy_from_user(param, req->data, req->size)) {
            ret = -EFAULT;
            kmpp_loge_f("kmpp_copy_from_user failed\n");
            goto fail;
        }

        shm = (KmppShm)(uintptr_t)ioc->kobj_kaddr;
        kbase = kmpp_shm_get_kbase(shm);
        if (!kbase || kbase != shm) {
            kmpp_loge_f("invalid obj kbase %px shm %px\n", kbase, shm);
            ret = -EINVAL;
            goto fail;
        }

        cfg = (KmppVencInitCfgImpl *)kmpp_shm_get_kaddr(shm);
        ret = mpp_vcodec_chan_create(kmpp_shm_get_kpriv(shm));
        if (ret) {
            /* no free channel or encoder init failure */
            ret = -ENODEV;
            goto fail;
        }

        ctx->type = cfg->type;
        ctx->chan_id = cfg->chan_id;
        ctx->chan_dup = cfg->chan_dup;
        ctx->chan_create = 1;
    } break;
    case VCODEC_CHAN_DESTROY: {
        if (ctx->chan_create) {
            ret = mpp_vcodec_chan_destory(chan_id, type);
            ctx->chan_create = 0;
        }
        if (ret) {
            ret = -EINVAL;
            goto fail;
        }
    } break;
    case VCODEC_CHAN_RESET: {
        ;//ret = mpp_vcodec_chan_stop(chan_id, type);
        //if (ret)
        //  goto fail;
    } break;
    case VCODEC_CHAN_START: {
        ret = mpp_vcodec_chan_start(chan_id, type);
        if (ret) {
            ret = -EINVAL;
            goto fail;
        }
    } break;
    case VCODEC_CHAN_STOP: {
        ret = mpp_vcodec_chan_stop(chan_id, type);
        if (ret) {
            ret = -EINVAL;
            goto fail;
        }
    } break;
    case VCODEC_CHAN_PAUSE: {
        ret = mpp_vcodec_chan_pause(chan_id, type);
        if (ret) {
            ret = -EINVAL;
            goto fail;
        }
    } break;
    case VCODEC_CHAN_RESUME: {
        ret = mpp_vcodec_chan_resume(chan_id, type);
        if (ret) {
            ret = -EINVAL;
            goto fail;
        }
    } break;
    case VCODEC_CHAN_CONTROL: {
        if (req->data) {
            if (kmpp_copy_from_user(param, req->data, req->size)) {
                ret = -EFAULT;
                goto fail;
            }
        }
        ret = mpp_vcodec_chan_control(chan_id, type, req->ctrl_cmd, param);
        if (ret) {
            ret = -EINVAL;
            goto fail;
        }
        if (req->data) {
            if (kmpp_copy_to_user(req->data, param, req->size)) {
                ret = -EFAULT;
                goto fail;
            }
        }
    } break;
    case VCODEC_CHAN_IN_FRM_RDY: {
        if (!req->data) {
            ret = -EFAULT;
            goto fail;
        }
        if (kmpp_copy_from_user(param, req->data, req->size)) {
            ret = -EFAULT;
            goto fail;
        }
        ret = mpp_vcodec_chan_push_frm(chan_id, param);
        if (ret) {
            /* input frame queue full (MAX_FRAME_NUM) */
            ret = -EAGAIN;
            goto fail;
        }
    } break;
    case VCODEC_CHAN_OUT_STRM_BUF_RDY: {
        static rk_s32 once = 1;

        if (once) {
            kmpp_logi_f("VCODEC_CHAN_OUT_STRM_BUF_RDY is deprected use VCODEC_CHAN_OUT_PKT_RDY instead\n");
            once = 0;
        }
        if (!req->data) {
            ret = -EFAULT;
            goto fail;
        }
        ret = mpp_vcodec_chan_get_stream_legacy(chan_id, type, param);
        if (ret) {
            ret = -EINVAL;
            goto fail;
        }

        if (kmpp_copy_to_user(req->data, param, req->size)) {
            kmpp_loge_f("kmpp_copy_to_user failed.\n");
            ret = -EINVAL;
            goto fail;
        }
    } break;
    case VCODEC_CHAN_OUT_PKT_RDY: {
        if (!req->data) {
            ret = -EFAULT;
            goto fail;
        }
        ret = mpp_vcodec_chan_get_stream(chan_id, type, param);
        if (ret) {
            /* no packet queued */
            ret = -EAGAIN;
            goto fail;
        }

        if (kmpp_copy_to_user(req->data, param, req->size)) {
            kmpp_loge_f("kmpp_copy_to_user failed.\n");
            ret = -EFAULT;
            goto fail;
        }
    } break;
    case VCODEC_CHAN_OUT_STRM_END: {
        if (!req->data) {
            ret = -EFAULT;
            goto fail;
        }
        if (kmpp_copy_from_user(param, req->data, req->size)) {
            ret = -EFAULT;
            goto fail;
        }
        ret = mpp_vcodec_chan_put_stream_legacy(chan_id, type, param);
        if (ret) {
            ret = -EINVAL;
            goto fail;
        }
    } break;
    default: {
        kmpp_loge_f("unknown vcode req cmd %x\n", req->cmd);
        ret = -EINVAL;
    } break;
    }
fail:
    return ret;
}

static rk_s32 vcodec_ioctl(KmppFsDev *file, rk_s32 cmd, void *arg)
{
    void __user *msg = (void __user *)arg;
    struct chanid_ctx *ctx = (struct chanid_ctx *)file->priv_data;
    struct vcodec_request req;
    int ret = 0;

    if (!ctx) {
        kmpp_loge_f("ctx %p\n", ctx);
        return -EINVAL;
    }
    if (kmpp_atomic_read(ctx->release_request) > 0) {
        kmpp_loge_f("chl_id %d release request", ctx->chan_id);
        return -EBUSY;
    }

    switch (cmd) {
    case VOCDEC_IOC_CFG: {
        struct vcodec_msg v_msg;

        kmpp_memset(&v_msg, 0, sizeof(v_msg));
        if (kmpp_copy_from_user(&v_msg, msg, sizeof(v_msg)))
            return -EFAULT;

        kmpp_down(ctx->ioctl_sem);
        ret = vcodec_process_msg(&v_msg, &req);
        if (ret) {
            kmpp_up(ctx->ioctl_sem);
            return -EFAULT;
        }
        ret = vcodec_process_cmd(ctx, &req);
        kmpp_up(ctx->ioctl_sem);
        /*
         * Pass the specific errno through to userspace so callers can tell
         * backpressure (-EAGAIN on frame queue full / packet queue empty)
         * apart from bad pointers (-EFAULT), bad state (-EINVAL) and
         * channel creation failure (-ENODEV). Guard against a positive
         * MPP-style code leaking out of a future process_cmd change.
         */
        if (ret)
            return (ret < 0) ? ret : -EINVAL;
    } break;
    default: {
        kmpp_loge_f("unknown ioctl cmd %x\n", cmd);
        return -EINVAL;
    } break;
    }

    return 0;
}

static rk_s32 vcodec_poll(KmppFsDev *file, void *poll_table)
{
    struct chanid_ctx *ctx = file->priv_data;
    int chan_id = ctx->chan_id;
    MppCtxType type = ctx->type;
    unsigned int mask = 0;
    struct mpp_chan *chan_entry = mpp_vcodec_get_chan_entry(chan_id, type);
    rk_ul flags;

    kmpp_spin_lock_irqsave(chan_entry->pkt_lock, &flags);
    if (!kmpp_list_empty(&chan_entry->pkt_list)) {
        mask |= KMPP_POLLIN | KMPP_POLLRDNORM;
        kmpp_spin_unlock_irqrestore(chan_entry->pkt_lock, &flags);
        return mask;
    }
    kmpp_spin_unlock_irqrestore(chan_entry->pkt_lock, &flags);

    if (chan_entry->state == CHAN_STATE_SUSPEND)
        return mask;

    kmpp_poll_wait(file, chan_entry->wait, poll_table);

    kmpp_spin_lock_irqsave(chan_entry->pkt_lock, &flags);
    if (!kmpp_list_empty(&chan_entry->pkt_list))
        mask |= KMPP_POLLIN | KMPP_POLLRDNORM;
    kmpp_spin_unlock_irqrestore(chan_entry->pkt_lock, &flags);

    return mask;
}

static KmppDevFops vcodec_fops = {
    .open = vcodec_open,
    .release = vcodec_close,
    .poll = vcodec_poll,
    .ioctl = vcodec_ioctl,
};

struct vcodec_dev {
    KmppCls *cls;
    KmppFsDevMgr *mgr;

    KmppEnvGrp procfs;
    KmppEnvGrp venc_procfs;
    KmppEnvGrp pp_procfs;
    KmppEnvNode vcodec_info;
    KmppEnvNode pp_info;
};

struct vcodec_dev vdev;

#define MPP_VCODEC_NAME "vcodec"
#define MPP_ENC_NAME    "enc"
#define MPP_PP_NAME     "ivs"
#define MPP_DEC_NAME    "dec"

static int vcodec_procfs_remove(struct vcodec_dev *vdev)
{
    if (vdev->vcodec_info) {
        kmpp_env_del(vdev->venc_procfs, vdev->vcodec_info);
        kmpp_env_put(vdev->venc_procfs);
    }

    if (vdev->pp_info) {
        kmpp_env_del(vdev->pp_procfs, vdev->pp_info);
        kmpp_env_put(vdev->pp_procfs);
    }

    kmpp_env_put(vdev->procfs);

    return 0;
}

static void venc_proc_debug(KmppEnvNode node, void *data)
{
    RK_U32 i = 0;
    MppCtxType type = MPP_CTX_ENC;
    struct venc_module *venc = mpp_vcodec_get_enc_module_entry();

    if (venc->thd) {
        kmpp_env_log(node, "\n--------venc thread status-------------------------------------------------------------------------------------------------------\n");

        kmpp_env_log(node, "%15s|%15s|%15s\n", "last_runing", "run_cnt", "que_cnt");
        kmpp_env_log(node, "%15lld|%15lld|%15lld\n",
                     venc->thd->worker->last_us, venc->thd->worker->run_cnt, venc->thd->queue_cnt);
    }

    for (i = 0; i < MAX_ENC_NUM; i++) {
        struct mpp_chan *chan = mpp_vcodec_get_chan_entry(i, type);

        if (chan->state != CHAN_STATE_RUN)
            continue;
        kmpp_mutex_lock(chan->chan_mutex);
        if (chan->handle) {
            RK_U32 runing = kmpp_atomic_read(chan->runing) > 0;
            RK_U32 comb_run = kmpp_atomic_read(chan->cfg.comb_runing) > 0;
            kmpp_env_log(node, "\n--------venc chn runing status---------------------------------------------------------------------------------------------------\n");

            kmpp_env_log(node, "%8s|%8s|%10s|%10s|%10s|%10s|%14s|%15s|%15s\n",
                         "ID", "runing", "combo_run", "cfg_gap", "strm_cnt",
                         "gap_time", "cb_gap_time", "last_cb_start", "last_cb_end");

            kmpp_env_log(node, "%8d|%8u|%10u|%10u|%10u|%10u|%14u|%15llu|%15llu\n",
                         i, runing, comb_run, chan->last_cfg_time,
                         kmpp_atomic_read(chan->stream_count), chan->gap_time,
                         chan->combo_gap_time, chan->last_jeg_combo_start,
                         chan->last_jeg_combo_end);

            kmpp_env_log(node, "%8s|%15s|%15s|%15s|%15s|%15s\n",
                         "ID", "pkt_total_cnt", "pkt_user_get", "seq_user_get",
                         "seq_encoded", "seq_encoding");

            kmpp_env_log(node, "%8d|%15u|%15u|%15llu|%15llu|%15llu\n",
                         i, kmpp_atomic_read(chan->pkt_total_num),
                         kmpp_atomic_read(chan->pkt_user_get),
                         chan->seq_user_get, chan->seq_encoded, chan->seq_encoding);

            mpp_enc_proc_debug(node, chan->handle, i);
        }
        kmpp_mutex_unlock(chan->chan_mutex);
    }
    mpp_buf_pool_info_show(node);
}

extern void vepu_show_pp_info(KmppEnvNode node);

static void rkvenc_show_pp_info(KmppEnvNode node, void *data)
{
#ifdef RKVEPU511_PP_ENABLE
    vepu_show_pp_info(node);
#endif
}

static int vcodec_procfs_init(struct vcodec_dev *vdev)
{
    rk_s32 ret;

    ret = kmpp_env_get(&vdev->procfs, NULL, "vcodec");
    if (ret) {
        kmpp_loge_f("failed on open procfs %s\n", MPP_VCODEC_NAME);
        vdev->procfs = NULL;
        return -EIO;
    }

    ret = kmpp_env_get(&vdev->venc_procfs, vdev->procfs, MPP_ENC_NAME);
    if (ret) {
        kmpp_loge_f("failed on open procfs %s\n", MPP_ENC_NAME);
        vdev->venc_procfs = NULL;
        return -EIO;
    }

    /* for show enc chnl info */
    {
        KmppEnvInfo info = {
            .name = "venc_info",
            .readonly = 1,
            .type = KmppEnv_user,
            .val = NULL,
            .env_show = venc_proc_debug,
        };
        kmpp_env_add(vdev->venc_procfs, &vdev->vcodec_info, &info);
    }

    /* pp info */
    ret = kmpp_env_get(&vdev->pp_procfs, vdev->procfs, MPP_PP_NAME);
    if (ret) {
        kmpp_loge_f("failed on open procfs %s\n", MPP_PP_NAME);
        vdev->pp_procfs = NULL;
        return -EIO;
    }

    /* for show pp chnl info */
    {
        KmppEnvInfo info = {
            .name = "ivs_info",
            .readonly = 1,
            .type = KmppEnv_user,
            .val = NULL,
            .env_show = rkvenc_show_pp_info,
        };
        kmpp_env_add(vdev->pp_procfs, &vdev->pp_info, &info);
    }

    return 0;
}

int vcodec_init(void)
{
    rk_s32 ret;
    KmppFsDevCfg cfg;

    /* create a device */
    memset(&vdev, 0, sizeof(struct vcodec_dev));
    ret = kmpp_class_init(&vdev.cls, "vcodec_class");
    if (ret)
        return rk_nok;

    cfg.cls = vdev.cls;
    cfg.name = "vcodec";
    cfg.fops = &vcodec_fops;
    cfg.drv_data = NULL;
    cfg.priv_size = 0;
    cfg.buf = NULL;
    cfg.size = 0;

    ret = kmpp_fs_dev_mgr_init(&vdev.mgr, &cfg);
    if (ret)
        kmpp_loge_f("kmpp_fs_dev_mgr_init failed ret %d\n", ret);

    vcodec_procfs_init(&vdev);

    mpp_vcodec_init();

    return 0;
}

void vcodec_exit(void)
{
    mpp_vcodec_deinit();

    vcodec_procfs_remove(&vdev);
    kmpp_fs_dev_mgr_deinit(vdev.mgr);
    kmpp_class_deinit(vdev.cls);
}

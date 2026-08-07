/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_session"

#include <linux/clk-provider.h>
#include <linux/interrupt.h>
#include <linux/nospec.h>
#include <linux/iopoll.h>
#include <linux/version.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>

#include "rk-mpp.h"

#include "kmpp_osal.h"
#include "kmpp_sys.h"

#include "mpp_iommu.h"
#include "kmpp_session_legacy.h"

/* input parmater structure for version 1 */
struct mpp_msg_v1 {
    __u32 cmd;
    __u32 flags;
    __u32 size;
    __u32 offset;
    __u64 data_ptr;
};

struct mpp_service *g_srv;

static int mpp_session_push_pending(struct mpp_session *session, struct mpp_task *task)
{
    unsigned long flags;

    kref_get(&task->ref);
    spin_lock_irqsave(&session->pending_lock, flags);
    if (session->srv->timing_en) {
        task->on_pending = ktime_get();
        set_bit(TASK_TIMING_PENDING, &task->state);
    }
    list_add_tail(&task->pending_link, &session->pending_list);
    spin_unlock_irqrestore(&session->pending_lock, flags);

    return 0;
}

int mpp_session_pop_pending(struct mpp_session *session, struct mpp_task *task)
{
    unsigned long flags;

    spin_lock_irqsave(&session->pending_lock, flags);
    list_del_init(&task->pending_link);
    spin_unlock_irqrestore(&session->pending_lock, flags);
    kref_put(&task->ref, mpp_free_task);

    return 0;
}

struct mpp_task *mpp_session_get_pending_task(struct mpp_session *session)
{
    struct mpp_task *task = NULL;
    unsigned long flags;

    spin_lock_irqsave(&session->pending_lock, flags);
    task = list_first_entry_or_null(&session->pending_list, struct mpp_task, pending_link);
    spin_unlock_irqrestore(&session->pending_lock, flags);

    return task;
}


static void task_msgs_reset(struct mpp_task_msgs *msgs)
{
    list_del_init(&msgs->list);

    msgs->flags = 0;
    msgs->req_cnt = 0;
    msgs->set_cnt = 0;
    msgs->poll_cnt = 0;
}

static void task_msgs_init(struct mpp_task_msgs *msgs, struct mpp_session *session)
{
    INIT_LIST_HEAD(&msgs->list);

    msgs->session = session;
    msgs->queue = NULL;
    msgs->task = NULL;
    msgs->mpp = NULL;

    msgs->ext_fd = -1;

    task_msgs_reset(msgs);
}

static struct mpp_task_msgs *get_task_msgs(struct mpp_session *session)
{
    unsigned long flags;
    struct mpp_task_msgs *msgs;

    spin_lock_irqsave(&session->lock_msgs, flags);
    msgs = list_first_entry_or_null(&session->list_msgs_idle, struct mpp_task_msgs, list_session);
    if (msgs) {
        list_move_tail(&msgs->list_session, &session->list_msgs);
        spin_unlock_irqrestore(&session->lock_msgs, flags);

        return msgs;
    }
    spin_unlock_irqrestore(&session->lock_msgs, flags);

    msgs = kzalloc(sizeof(*msgs), GFP_KERNEL);
    task_msgs_init(msgs, session);
    INIT_LIST_HEAD(&msgs->list_session);

    spin_lock_irqsave(&session->lock_msgs, flags);
    list_move_tail(&msgs->list_session, &session->list_msgs);
    session->msgs_cnt++;
    spin_unlock_irqrestore(&session->lock_msgs, flags);

    mpp_debug_func(DEBUG_TASK_INFO, "session %d:%d msgs cnt %d\n",
                   session->pid, session->index, session->msgs_cnt);

    return msgs;
}

static void put_task_msgs(struct mpp_task_msgs *msgs)
{
    struct mpp_session *session = msgs->session;
    unsigned long flags;

    if (!session) {
        pr_err("invalid msgs without session\n");
        return;
    }

    if (msgs->ext_fd >= 0) {
        fdput(msgs->f);
        msgs->ext_fd = -1;
    }

    task_msgs_reset(msgs);

    spin_lock_irqsave(&session->lock_msgs, flags);
    list_move_tail(&msgs->list_session, &session->list_msgs_idle);
    spin_unlock_irqrestore(&session->lock_msgs, flags);
}

static void clear_task_msgs(struct mpp_session *session)
{
    struct mpp_task_msgs *msgs, *n;
    LIST_HEAD(list_to_free);
    unsigned long flags;

    spin_lock_irqsave(&session->lock_msgs, flags);

    list_for_each_entry_safe(msgs, n, &session->list_msgs, list_session) {
        list_move_tail(&msgs->list_session, &list_to_free);
    }

    list_for_each_entry_safe(msgs, n, &session->list_msgs_idle, list_session) {
        list_move_tail(&msgs->list_session, &list_to_free);
    }

    spin_unlock_irqrestore(&session->lock_msgs, flags);

    list_for_each_entry_safe(msgs, n, &list_to_free, list_session) {
        kfree(msgs);
    }
}

static void mpp_session_clear_pending(struct mpp_session *session)
{
    struct mpp_task *task = NULL, *n;
    unsigned long flags;

    /* clear session pending list */
    spin_lock_irqsave(&session->pending_lock, flags);
    list_for_each_entry_safe(task, n, &session->pending_list, pending_link) {
        /* abort task in taskqueue */
        atomic_inc(&task->abort_request);
        list_del_init(&task->pending_link);
        kref_put(&task->ref, mpp_free_task);
    }
    spin_unlock_irqrestore(&session->pending_lock, flags);
}

void mpp_session_deinit(struct mpp_session *session)
{
    mpp_dbg_session("session %d:%d task %d deinit\n", session->pid,
                    session->index, atomic_read(&session->task_count));

    if (likely(session->deinit))
        session->deinit(session);
    else
        pr_err("invalid NULL session deinit function\n");

    clear_task_msgs(session);

    // kfree(session);
}

void mpp_session_cleanup_detach(struct mpp_taskqueue *queue, struct kthread_work *work)
{
    struct mpp_session *session, *n;

    if (!atomic_read(&queue->detach_count))
        return;

    mutex_lock(&queue->session_lock);
    list_for_each_entry_safe(session, n, &queue->session_detach, session_link) {
        s32 task_count;

        if (session->online)
            mpp_session_clear_online_task(session->mpp, session);

        task_count = atomic_read(&session->task_count);

        if (!task_count) {
            list_del_init(&session->session_link);
            atomic_dec(&queue->detach_count);
        }

        mutex_unlock(&queue->session_lock);

        if (task_count) {
            mpp_dbg_session("session %d:%d not finished %d task cnt %d\n",
                            session->device_type, session->index,
                            atomic_read(&queue->detach_count), task_count);

            mpp_session_clear_pending(session);
        } else {
            mpp_dbg_session("queue detach %d\n",
                            atomic_read(&queue->detach_count));

            kmpp_logi_f("queue detach %d\n", atomic_read(&queue->detach_count));

            if (session->session_obj)
                kmpp_session_put(session->session_obj);
            else {
                mpp_session_deinit(session);
                kfree(session);
            }
        }

        mutex_lock(&queue->session_lock);
    }
    mutex_unlock(&queue->session_lock);

    if (atomic_read(&queue->detach_count)) {
        mpp_dbg_session("queue detach %d again\n",
                        atomic_read(&queue->detach_count));

        kthread_queue_work(&queue->worker, work);
    }
}

static void mpp_task_timeout_work(struct work_struct *work_s)
{
    struct mpp_task *task = container_of(to_delayed_work(work_s), struct mpp_task, timeout_work);
    struct mpp_dev *mpp;
    struct mpp_session *session = task->session;

    if (!session) {
        mpp_err("task %p, task->session is null.\n", task);
        return;
    }

    mpp = mpp_get_task_used_device(task, session);
    if (!mpp) {
        mpp_err("session %d:%d mpp is null\n", session->device_type, session->index);
        return;
    }
    disable_irq(mpp->irq);
    if (test_and_set_bit(TASK_STATE_HANDLE, &task->state)) {
        mpp_err("session %d:%d task %d state %#lx has been handled\n",
                session->device_type, session->index, task->task_index, task->state);
        enable_irq(mpp->irq);
        return;
    }
    mpp_err("session %d:%d task %d processing time out!\n",
            session->device_type, session->index, task->task_index);

    mpp_task_dump_timing(task, ktime_us_delta(ktime_get(), task->on_create));
    set_bit(TASK_STATE_TIMEOUT, &task->state);

    enable_irq(mpp->irq);
    mpp_taskqueue_trigger_work(mpp);
}

static int mpp_process_task_default(struct mpp_session *session, struct mpp_task_msgs *msgs)
{
    struct mpp_task *task = NULL;
    struct mpp_dev *mpp = session->mpp;
    u32 timing_en;
    ktime_t on_create;

    if (unlikely(!mpp)) {
        mpp_err("pid %d client %d found invalid process function\n",
                session->pid, session->device_type);
        return -EINVAL;
    }

    timing_en = session->srv->timing_en;
    if (timing_en)
        on_create = ktime_get();

    if (mpp->dev_ops->alloc_task)
        task = mpp->dev_ops->alloc_task(session, msgs);
    if (!task) {
        mpp_err("alloc_task failed.\n");
        return -ENOMEM;
    }

    if (timing_en) {
        task->on_create_end = ktime_get();
        task->on_create = on_create;
        set_bit(TASK_TIMING_CREATE_END, &task->state);
        set_bit(TASK_TIMING_CREATE, &task->state);
    }

    /* ensure current device */
    mpp = mpp_get_task_used_device(task, session);

    kref_init(&task->ref);
    init_waitqueue_head(&task->wait);
    atomic_set(&task->abort_request, 0);
    task->task_index = atomic_fetch_inc(&mpp->task_index);
    task->task_id = atomic_fetch_inc(&mpp->queue->task_id);
    INIT_DELAYED_WORK(&task->timeout_work, mpp_task_timeout_work);

    if (mpp->auto_freq_en && mpp->hw_ops->get_freq)
        mpp->hw_ops->get_freq(mpp, task);

    msgs->queue = mpp->queue;
    msgs->task = task;
    msgs->mpp = mpp;

    /*
     * Push task to session should be in front of push task to queue.
     * Otherwise, when mpp_task_finish finish and worker_thread call
     * task worker, it may be get a task who has push in queue but
     * not in session, cause some errors.
     */
    atomic_inc(&session->task_count);
    mpp_session_push_pending(session, task);
    mpp_debug_func(DEBUG_TASK_INFO, "session %d:%d task %d state 0x%lx\n",
                   session->device_type, session->index,
                   task->task_index, task->state);

    return 0;
}

static int mpp_wait_result_default(struct mpp_session *session, struct mpp_task_msgs *msgs)
{
    struct mpp_task *task;
    struct mpp_dev *mpp;
    int ret;

    task = mpp_session_get_pending_task(session);
    if (!task) {
        mpp_err("session %d:%d pending list is empty!\n",
                session->pid, session->index);
        return -EIO;
    }
    mpp = mpp_get_task_used_device(task, session);

    ret = wait_event_interruptible(task->wait, test_bit(TASK_STATE_DONE, &task->state));
    if (ret == -ERESTARTSYS)
        mpp_err("wait task break by signal\n");

    if (mpp->dev_ops->result)
        ret = mpp->dev_ops->result(mpp, task);
    mpp_debug_func(DEBUG_TASK_INFO, "wait done session %d:%d count %d task %d state %lx\n",
                   session->device_type, session->index, atomic_read(&session->task_count),
                   task->task_index, task->state);

    mpp_session_pop_pending(session, task);

    return ret;
}

static int mpp_wait_result(struct mpp_session *session, struct mpp_task_msgs *msgs)
{
    if (likely(session->wait_result))
        return session->wait_result(session, msgs);

    pr_err("invalid NULL wait result function\n");
    return -EINVAL;
}

static void mpp_session_deinit_default(struct mpp_session *session)
{
    if (session->mpp) {
        struct mpp_dev *mpp = session->mpp;

        if (mpp->dev_ops->free_session)
            mpp->dev_ops->free_session(session);

        mpp_session_clear_pending(session);

        if (session->dma) {
            mpp_iommu_down_read(mpp->iommu_info);
            mpp_dma_session_destroy(session->dma);
            mpp_iommu_up_read(mpp->iommu_info);
            session->dma = NULL;
        }
    }

    if (session->srv) {
        struct mpp_service *srv = session->srv;

        mutex_lock(&srv->session_lock);
        list_del_init(&session->service_link);
        mutex_unlock(&srv->session_lock);
    }

    list_del_init(&session->session_link);
}

static void mpp_session_attach_workqueue(struct mpp_session *session, struct mpp_taskqueue *queue)
{
    mpp_dbg_session("session %d:%d attach\n", session->pid, session->index);

    mutex_lock(&queue->session_lock);
    list_add_tail(&session->session_link, &queue->session_attach);
    mutex_unlock(&queue->session_lock);
}

static void mpp_session_detach_workqueue(struct mpp_session *session)
{
    struct mpp_taskqueue *queue;
    struct mpp_dev *mpp;

    if (!session->mpp || !session->mpp->queue)
        return;

    mpp_dbg_session("session %d:%d detach\n", session->pid, session->index);
    mpp = session->mpp;
    queue = mpp->queue;

    mutex_lock(&queue->session_lock);
    list_del_init(&session->session_link);
    list_add_tail(&session->session_link, &queue->session_detach);
    atomic_inc(&queue->detach_count);
    mutex_unlock(&queue->session_lock);

    mpp_taskqueue_trigger_work(mpp);
}

rk_s32 kmpp_session_init_legacy(KmppSessionOld *session, KmppSession obj)
{
    session->pid = current->pid;

    spin_lock_init(&session->pending_lock);
    INIT_LIST_HEAD(&session->pending_list);
    INIT_LIST_HEAD(&session->service_link);
    INIT_LIST_HEAD(&session->session_link);

    atomic_set(&session->task_count, 0);
    atomic_set(&session->release_request, 0);
    atomic_set(&session->on_detach, 0);

    INIT_LIST_HEAD(&session->list_msgs);
    INIT_LIST_HEAD(&session->list_msgs_idle);
    spin_lock_init(&session->lock_msgs);

    session->srv = g_srv;

    if (g_srv) {
        mutex_lock(&g_srv->session_lock);
        list_add_tail(&session->service_link, &g_srv->session_list);
        mutex_unlock(&g_srv->session_lock);
    }

    session->process_task = mpp_process_task_default;
    session->wait_result = mpp_wait_result_default;
    session->deinit = mpp_session_deinit_default;

    session->session_obj = obj;

    mpp_dbg_session("session %p init\n", session);

    return rk_ok;
}

rk_s32 kmpp_session_deinit_legacy(KmppSessionOld *session)
{
    /* wait for task all done */
    atomic_inc(&session->release_request);

    if (session->mpp && !atomic_read(&session->on_detach) &&
        atomic_read(&session->task_count)) {
        atomic_inc(&session->on_detach);

        mpp_session_detach_workqueue(session);
        return rk_nok;
    }

    mpp_session_deinit(session);

    return rk_ok;
}

static int mpp_check_cmd_v1(__u32 cmd)
{
    bool found;

    found = (cmd < MPP_CMD_QUERY_BUTT) ? true : false;
    found = (cmd >= MPP_CMD_INIT_BASE && cmd < MPP_CMD_INIT_BUTT) ? true : found;
    found = (cmd >= MPP_CMD_SEND_BASE && cmd < MPP_CMD_SEND_BUTT) ? true : found;
    found = (cmd >= MPP_CMD_POLL_BASE && cmd < MPP_CMD_POLL_BUTT) ? true : found;
    found = (cmd >= MPP_CMD_CONTROL_BASE && cmd < MPP_CMD_CONTROL_BUTT) ? true : found;
    found = (cmd >= MPP_CMD_KMPP_CONTROL_BASE && cmd < MPP_CMD_KMPP_CONTROL_BUTT) ? true : found;

    return found ? 0 : -EINVAL;
}

static __u32 mpp_get_cmd_butt(__u32 cmd)
{
    __u32 mask = 0;

    switch (cmd) {
    case MPP_CMD_QUERY_BASE:
        mask = MPP_CMD_QUERY_BUTT;
        break;
    case MPP_CMD_INIT_BASE:
        mask = MPP_CMD_INIT_BUTT;
        break;

    case MPP_CMD_SEND_BASE:
        mask = MPP_CMD_SEND_BUTT;
        break;
    case MPP_CMD_POLL_BASE:
        mask = MPP_CMD_POLL_BUTT;
        break;
    case MPP_CMD_CONTROL_BASE:
        mask = MPP_CMD_CONTROL_BUTT;
        break;
    default:
        mpp_err("unknown dev cmd 0x%x\n", cmd);
        break;
    }

    return mask;
}

static int mpp_process_request(struct mpp_session *session, struct mpp_service *srv,
                               struct mpp_request *req, struct mpp_task_msgs *msgs)
{
    int ret;
    struct mpp_dev *mpp;

    mpp_debug(DEBUG_IOCTL, "cmd %x process\n", req->cmd);

    switch (req->cmd) {
    case MPP_CMD_QUERY_HW_SUPPORT: {
        u32 hw_support = srv->hw_support;

        mpp_debug(DEBUG_IOCTL, "hw_support %08x\n", hw_support);
        if (put_user(hw_support, (u32 __user *)req->data))
            return -EFAULT;
    } break;
    case MPP_CMD_QUERY_HW_ID: {
        struct mpp_hw_info *hw_info;

        mpp = NULL;
        if (session && session->mpp) {
            mpp = session->mpp;
        } else {
            u32 client_type;

            if (get_user(client_type, (u32 __user *)req->data))
                return -EFAULT;

            mpp_debug(DEBUG_IOCTL, "client %d\n", client_type);
            client_type = array_index_nospec(client_type, MPP_DEVICE_BUTT);
            if (test_bit(client_type, &srv->hw_support))
                mpp = srv->sub_devices[client_type];
        }

        if (!mpp)
            return -EINVAL;

        hw_info = mpp->var->hw_info;
        mpp_debug(DEBUG_IOCTL, "hw_id %08x\n", hw_info->hw_id);
        if (put_user(hw_info->hw_id, (u32 __user *)req->data))
            return -EFAULT;
    } break;
    case MPP_CMD_QUERY_CMD_SUPPORT: {
        __u32 cmd = 0;

        if (get_user(cmd, (u32 __user *)req->data))
            return -EINVAL;

        if (put_user(mpp_get_cmd_butt(cmd), (u32 __user *)req->data))
            return -EFAULT;
    } break;
    case MPP_CMD_INIT_CLIENT_TYPE: {
        u32 client_type;

        if (get_user(client_type, (u32 __user *)req->data))
            return -EFAULT;

        mpp_debug(DEBUG_IOCTL, "client %d\n", client_type);
        if (client_type >= MPP_DEVICE_BUTT) {
            mpp_err("client_type must less than %d\n",
                    MPP_DEVICE_BUTT);
            return -EINVAL;
        }
        client_type = array_index_nospec(client_type, MPP_DEVICE_BUTT);
        mpp = srv->sub_devices[client_type];
        if (!mpp)
            return -EINVAL;

        session->device_type = (enum MPP_DEVICE_TYPE)client_type;
        session->dma = mpp_dma_session_create(mpp->dev, mpp->session_max_buffers);
        session->mpp = mpp;
        if (mpp->dev_ops) {
            if (mpp->dev_ops->process_task)
                session->process_task = mpp->dev_ops->process_task;

            if (mpp->dev_ops->wait_result)
                session->wait_result = mpp->dev_ops->wait_result;

            if (mpp->dev_ops->deinit)
                session->deinit = mpp->dev_ops->deinit;
        }
        session->index = atomic_fetch_inc(&mpp->session_index);
        if (mpp->dev_ops && mpp->dev_ops->init_session) {
            ret = mpp->dev_ops->init_session(session);
            if (ret)
                return ret;
        }

        mpp_session_attach_workqueue(session, mpp->queue);
    } break;
    case MPP_CMD_INIT_DRIVER_DATA: {
        // u32 val;

        // mpp = session->mpp;
        // if (!mpp)
        //     return -EINVAL;
        // if (get_user(val, (u32 __user *)req->data))
        //     return -EFAULT;
        // if (mpp->grf_info->grf)
        //     regmap_write(mpp->grf_info->grf, 0x5d8, val);
        return 0;
    } break;
    case MPP_CMD_INIT_TRANS_TABLE: {
        if (session && req->size) {
            int trans_tbl_size = sizeof(session->trans_table);

            if (req->size > trans_tbl_size) {
                mpp_err("init table size %d more than %d\n",
                        req->size, trans_tbl_size);
                return -ENOMEM;
            }

            if (kmpp_copy_from_user(session->trans_table, req->data, req->size)) {
                mpp_err("kmpp_copy_from_user failed\n");
                return -EINVAL;
            }
            session->trans_count =
                req->size / sizeof(session->trans_table[0]);
        }
    } break;
    case MPP_CMD_SET_REG_WRITE:
    case MPP_CMD_SET_REG_READ:
    case MPP_CMD_SET_REG_ADDR_OFFSET:
    case MPP_CMD_SET_RCB_INFO: {
        msgs->flags |= req->flags;
        msgs->set_cnt++;
    } break;
    case MPP_CMD_POLL_HW_FINISH: {
        msgs->flags |= req->flags;
        msgs->poll_cnt++;
        msgs->poll_req = NULL;
    } break;
    case MPP_CMD_POLL_HW_IRQ: {
        if (msgs->poll_cnt || msgs->poll_req)
            mpp_err("Do NOT poll hw irq when previous call not return\n");

        msgs->flags |= req->flags;
        msgs->poll_cnt++;

        if (req->size && req->data) {
            if (!msgs->poll_req)
                msgs->poll_req = req;
        } else {
            msgs->poll_req = NULL;
        }
    } break;
    case MPP_CMD_RESET_SESSION: {
        int ret;
        int val;

        ret = readx_poll_timeout(atomic_read, &session->task_count, val, val == 0, 1000, 500000);
        if (ret == -ETIMEDOUT) {
            mpp_err("wait task running time out\n");
        } else {
            mpp = session->mpp;
            if (!mpp)
                return -EINVAL;

            mpp_session_clear_pending(session);
            mpp_iommu_down_write(mpp->iommu_info);
            ret = mpp_dma_session_destroy(session->dma);
            mpp_iommu_up_write(mpp->iommu_info);
        }
        return ret;
    } break;
    case MPP_CMD_TRANS_FD_TO_IOVA: {
        u32 i;
        u32 count;
        u32 data[MPP_MAX_REG_TRANS_NUM];

        mpp = session->mpp;
        if (!mpp)
            return -EINVAL;

        if (req->size <= 0 ||
            req->size > sizeof(data))
            return -EINVAL;

        memset(data, 0, sizeof(data));
        if (kmpp_copy_from_user(data, req->data, req->size)) {
            mpp_err("kmpp_copy_from_user failed.\n");
            return -EINVAL;
        }
        count = req->size / sizeof(u32);
        for (i = 0; i < count; i++) {
            struct mpp_dma_buffer *buffer;
            int fd = data[i];

            mpp_iommu_down_read(mpp->iommu_info);
            buffer = mpp_dma_import_fd(mpp->iommu_info, session->dma, fd, 1);
            mpp_iommu_up_read(mpp->iommu_info);
            if (IS_ERR_OR_NULL(buffer)) {
                mpp_err("can not import fd %d\n", fd);
                return -EINVAL;
            }
            data[i] = (u32)buffer->iova;
            mpp_debug(DEBUG_IOMMU, "fd %d => iova %08x\n",
                      fd, data[i]);
        }
        if (kmpp_copy_to_user(req->data, data, req->size)) {
            mpp_err("kmpp_copy_to_user failed.\n");
            return -EINVAL;
        }
    } break;
    case MPP_CMD_RELEASE_FD: {
        u32 i;
        int ret;
        u32 count;
        u32 data[MPP_MAX_REG_TRANS_NUM];

        if (req->size <= 0 ||
            req->size > sizeof(data))
            return -EINVAL;

        memset(data, 0, sizeof(data));
        if (kmpp_copy_from_user(data, req->data, req->size)) {
            mpp_err("kmpp_copy_from_user failed.\n");
            return -EINVAL;
        }
        count = req->size / sizeof(u32);
        for (i = 0; i < count; i++) {
            ret = mpp_dma_release_fd(session->dma, data[i]);
            if (ret) {
                mpp_err("release fd %d failed.\n", data[i]);
                return ret;
            }
        }
    } break;
    default: {
        mpp = session->mpp;
        if (!mpp) {
            mpp_err("pid %d not find client %d\n",
                    session->pid, session->device_type);
            return -EINVAL;
        }
        if (mpp->dev_ops->ioctl)
            return mpp->dev_ops->ioctl(session, req);

        mpp_debug(DEBUG_IOCTL, "unknown mpp ioctl cmd %x\n", req->cmd);
    } break;
    }

    return 0;
}

static int mpp_process_task(struct mpp_session *session, struct mpp_task_msgs *msgs)
{
    if (likely(session->process_task))
        return session->process_task(session, msgs);

    pr_err("invalid NULL process task function\n");
    return -EINVAL;
}

static void task_msgs_add(struct mpp_task_msgs *msgs, struct list_head *head)
{
    struct mpp_session *session = msgs->session;
    int ret = 0;

    /* process each task */
    if (msgs->set_cnt) {
        /* NOTE: update msg_flags for fd over 1024 */
        session->msg_flags = msgs->flags;
        ret = mpp_process_task(session, msgs);
    }

    if (!ret) {
        INIT_LIST_HEAD(&msgs->list);
        list_add_tail(&msgs->list, head);
    } else {
        put_task_msgs(msgs);
    }
}

static int mpp_collect_msgs(struct list_head *head, struct mpp_session *session, void __user *msg)
{
    struct mpp_msg_v1 msg_v1;
    struct mpp_request *req;
    struct mpp_task_msgs *msgs = NULL;
    int last = 1;
    int ret;

next:
    /* first, parse to fixed struct */
    if (kmpp_copy_from_user(&msg_v1, msg, sizeof(msg_v1)))
        return -EFAULT;

    msg += sizeof(msg_v1);

    mpp_debug(DEBUG_IOCTL, "cmd %x collect flags %08x, size %d, offset %x\n",
              msg_v1.cmd, msg_v1.flags, msg_v1.size, msg_v1.offset);

    if (mpp_check_cmd_v1(msg_v1.cmd)) {
        mpp_err("mpp cmd %x is not supported.\n", msg_v1.cmd);
        return -EFAULT;
    }

    if (msg_v1.flags & MPP_FLAGS_MULTI_MSG)
        last = (msg_v1.flags & MPP_FLAGS_LAST_MSG) ? 1 : 0;
    else
        last = 1;

    /* check cmd for change msgs session */
    if (msg_v1.cmd == MPP_CMD_SET_SESSION_FD) {
        struct mpp_bat_msg bat_msg;
        struct mpp_bat_msg __user *usr_cmd;
        struct fd f;

        /* try session switch here */
        usr_cmd = (struct mpp_bat_msg __user *)(unsigned long)msg_v1.data_ptr;

        if (kmpp_copy_from_user(&bat_msg, usr_cmd, sizeof(bat_msg)))
            return -EFAULT;

        /* skip finished message */
        if (bat_msg.flag & MPP_BAT_MSG_DONE)
            goto session_switch_done;

        f = fdget(bat_msg.fd);
        if (!f.file) {
            int ret = -EBADF;

            mpp_err("fd %d get session failed\n", bat_msg.fd);

            if (kmpp_copy_to_user(&usr_cmd->ret, &ret, sizeof(usr_cmd->ret)))
                mpp_err("kmpp_copy_to_user failed.\n");
            goto session_switch_done;
        }

        /* NOTE: add previous ready task to queue and drop empty task */
        if (msgs) {
            if (msgs->req_cnt)
                task_msgs_add(msgs, head);
            else
                put_task_msgs(msgs);

            msgs = NULL;
        }

        /* switch session */
        {
            KmppSession sess = f.file->private_data;

            session = kmpp_obj_to_priv(sess);
            // session = f.file->private_data;
            msgs = get_task_msgs(session);

            // if (f.file->private_data == session)
            if (f.file->private_data == sess)
                msgs->ext_fd = bat_msg.fd;
        }

        msgs->f = f;

        mpp_debug(DEBUG_IOCTL, "fd %d, session %d msg_cnt %d\n",
                  bat_msg.fd, session->index, session->msgs_cnt);

session_switch_done:
        /* session id should NOT be the last message */
        if (last)
            return 0;

        goto next;
    }

    if (!msgs)
        msgs = get_task_msgs(session);

    if (!msgs) {
        pr_err("session %d:%d failed to get task msgs",
               session->pid, session->index);
        return -EINVAL;
    }

    if (msgs->req_cnt >= MPP_MAX_MSG_NUM) {
        mpp_err("session %d message count %d more than %d.\n",
                session->index, msgs->req_cnt, MPP_MAX_MSG_NUM);
        return -EINVAL;
    }

    req = &msgs->reqs[msgs->req_cnt++];
    req->cmd = msg_v1.cmd;
    req->flags = msg_v1.flags;
    req->size = msg_v1.size;
    req->offset = msg_v1.offset;
    req->data = (void __user *)(unsigned long)msg_v1.data_ptr;

    ret = mpp_process_request(session, session->srv, req, msgs);
    if (ret) {
        mpp_err("session %d process cmd %x ret %d\n",
                session->index, req->cmd, ret);
        return ret;
    }

    if (!last)
        goto next;

    task_msgs_add(msgs, head);
    msgs = NULL;

    return 0;
}

static void mpp_msgs_trigger(struct list_head *msgs_list)
{
    struct mpp_task_msgs *msgs, *n;
    struct mpp_dev *mpp_prev = NULL;
    struct mpp_taskqueue *queue_prev = NULL;

    /* push task to queue */
    list_for_each_entry_safe(msgs, n, msgs_list, list) {
        struct mpp_dev *mpp;
        struct mpp_task *task;
        struct mpp_taskqueue *queue;

        if (!msgs->set_cnt || !msgs->queue)
            continue;

        mpp = msgs->mpp;
        task = msgs->task;
        queue = msgs->queue;

        if (queue_prev != queue) {
            if (queue_prev && mpp_prev) {
                spin_unlock(&queue_prev->pending_lock);
                mpp_taskqueue_trigger_work(mpp_prev);
            }

            if (queue)
                spin_lock(&queue->pending_lock);

            mpp_prev = mpp;
            queue_prev = queue;
        }

        if (test_bit(TASK_STATE_ABORT, &task->state))
            pr_info("try to trigger abort task %d\n", task->task_id);

        set_bit(TASK_STATE_PENDING, &task->state);
        list_add_tail(&task->queue_link, &queue->pending_list);
    }

    if (mpp_prev && queue_prev) {
        spin_unlock(&queue_prev->pending_lock);
        mpp_taskqueue_trigger_work(mpp_prev);
    }
}

static void mpp_msgs_wait(struct list_head *msgs_list)
{
    struct mpp_task_msgs *msgs, *n;

    /* poll and release each task */
    list_for_each_entry_safe(msgs, n, msgs_list, list) {
        struct mpp_session *session = msgs->session;

        if (msgs->poll_cnt) {
            int ret = mpp_wait_result(session, msgs);

            if (ret) {
                mpp_err("session %d wait result ret %d\n",
                        session->index, ret);
            }
        }

        put_task_msgs(msgs);

    }
}

rk_s32 kmpp_session_ioctl_legacy(KmppSessionOld *session, void *arg)
{
    struct mpp_service *srv;
    struct list_head msgs_list;
    rk_s32 ret = 0;

    mpp_debug_enter();

    if (!session || !session->srv) {
        mpp_err("session %p\n", session);
        return -EINVAL;
    }

    srv = g_srv;

    if (atomic_read(&session->release_request) > 0) {
        mpp_debug(DEBUG_IOCTL, "release session had request\n");
        return -EBUSY;
    }
    if (atomic_read(&srv->shutdown_request) > 0) {
        mpp_debug(DEBUG_IOCTL, "shutdown had request\n");
        return -EBUSY;
    }

    INIT_LIST_HEAD(&msgs_list);

    ret = mpp_collect_msgs(&msgs_list, session, (void __user *)arg);
    if (ret)
        mpp_err("collect msgs failed %d\n", ret);

    mpp_msgs_trigger(&msgs_list);

    mpp_msgs_wait(&msgs_list);

    mpp_debug_leave();

    return ret;
}

/* kmpp venc flow start here */
struct mpp_task *mpp_session_get_idle_task(struct mpp_session *session)
{
    struct mpp_task *task, *n;
    u32 found = 0;
    unsigned long flags;

    spin_lock_irqsave(&session->pending_lock, flags);
    list_for_each_entry_safe(task, n, &session->pending_list, pending_link) {
        if (task)
            mpp_debug_func(DEBUG_TASK_INFO, "task %d %#lx session %p chan %d find session %p\n",
                           task->task_index, task->state, task->session,
                           task->session->chn_id, session);
        if (task && !test_bit(TASK_STATE_RUNNING, &task->state)) {
            found = 1;
            break;
        }
    }
    spin_unlock_irqrestore(&session->pending_lock, flags);

    if (!found)
        task = NULL;

    return task;
}

static bool kmpp_taskqueue_is_running(struct mpp_taskqueue *queue)
{
    unsigned long flags;
    bool flag;

    spin_lock_irqsave(&queue->running_lock, flags);
    flag = !list_empty(&queue->running_list);
    spin_unlock_irqrestore(&queue->running_lock, flags);

    return flag;
}

static int kmpp_session_clear(struct mpp_dev *mpp, struct mpp_session *session)
{
    struct mpp_task *task = NULL, *n;
    unsigned long flags;

    /* clear session pending list */
    spin_lock_irqsave(&session->pending_lock, flags);
    list_for_each_entry_safe(task, n, &session->pending_list, pending_link) {
        /* abort task in taskqueue */
        atomic_inc(&task->abort_request);
        list_del_init(&task->pending_link);
        kref_put(&task->ref, mpp_free_task);
    }
    spin_unlock_irqrestore(&session->pending_lock, flags);

    return 0;
}

void mpp_session_clear_online_task(struct mpp_dev *mpp, struct mpp_session *session)
{
    struct mpp_task *mpp_task;

    while (1) {
        struct mpp_taskqueue *queue = mpp->queue;

        mpp_task = mpp_session_get_idle_task(session);

        if (!mpp_task)
            break;

        atomic_inc(&mpp_task->abort_request);
        mpp_taskqueue_pop_pending(queue, mpp_task);
        mpp_session_pop_pending(session, mpp_task);
    }
}

static void kmpp_venc_session_deinit(struct mpp_session *session)
{
    if (session->mpp) {
        struct mpp_dev *mpp = session->mpp;

        if (mpp->dev_ops->free_session)
            mpp->dev_ops->free_session(session);

        kmpp_session_clear(mpp, session);
    }

    if (session->srv) {
        struct mpp_service *srv = session->srv;

        mutex_lock(&srv->session_lock);
        list_del_init(&session->service_link);
        mutex_unlock(&srv->session_lock);
    }

    list_del_init(&session->session_link);
}

static void kmpp_task_timeout_work(struct work_struct *work_s)
{
    struct mpp_task *task = container_of(to_delayed_work(work_s), struct mpp_task, timeout_work);
    struct mpp_session *session;
    struct mpp_dev *mpp;
    u32 clbk_en = task->clbk_en;

    if (!task->session) {
        mpp_err("task %p, task->session is null.\n", task);
        return;
    }
    session = task->session;

    if (!session->mpp) {
        mpp_err("session %p, session->mpp is null.\n", session);
        return;
    }

    mpp = mpp_get_task_used_device(task, session);
    disable_irq(mpp->irq);
    if (test_and_set_bit(TASK_STATE_HANDLE, &task->state)) {
        mpp_err("session %d:%d task %d state %#lx has been handled\n",
                session->device_type, session->index, task->task_index, task->state);
        enable_irq(mpp->irq);
        return;
    }

    mpp_err("chan %d session %d:%d task %d state %#lx processing time out!\n",
            session->chn_id, session->device_type, session->index, task->task_index, task->state);
    mpp_dev_debug |= DEBUG_DUMP_ERR_REG;
    if (mpp && mpp->var->dev_ops->dump_dev)
        mpp->var->dev_ops->dump_dev(mpp);
    mpp_dev_debug &= ~DEBUG_DUMP_ERR_REG;
    /* hardware maybe dead, reset it */
    mpp_reset_up_read(mpp->reset_group);
    mpp_dev_reset(mpp);
    mpp_power_off(mpp);

    set_bit(TASK_STATE_TIMEOUT, &task->state);
    set_bit(TASK_STATE_DONE, &task->state);
    /* Wake up the GET thread */
    wake_up(&task->wait);

    /* remove task from taskqueue running list */
    mpp_iommu_dev_deactivate(mpp->iommu_info, mpp);
    enable_irq(mpp->irq);
    if (IS_ENABLED(RKVEPU500_ENABLE) || IS_ENABLED(RKVEPU540C_ENABLE)) {
        mpp_taskqueue_pop_running(mpp->queue, task);
        if (session->callback && clbk_en)
            session->callback(session->chn_id, MPP_VCODEC_EVENT_FRAME, NULL);
    }
    mpp_taskqueue_trigger_work(mpp);
}

static int kmpp_venc_process_task(struct mpp_session *session, struct mpp_task_msgs *msgs)
{
    struct mpp_dev *mpp = session->mpp;
    struct mpp_task *task = NULL;

    if (unlikely(!mpp)) {
        mpp_err("pid %d client %d found invalid process function\n",
                session->pid, session->device_type);
        return -EINVAL;
    }

    if (mpp->dev_ops->alloc_task)
        task = mpp->dev_ops->alloc_task(session, msgs);
    if (!task) {
        mpp_err("alloc_task failed.\n");
        return -ENOMEM;
    }
    /* ensure current device */
    mpp = mpp_get_task_used_device(task, session);

    kref_init(&task->ref);
    init_waitqueue_head(&task->wait);
    atomic_set(&task->abort_request, 0);
    task->task_index = atomic_fetch_inc(&mpp->task_index);
    INIT_DELAYED_WORK(&task->timeout_work, kmpp_task_timeout_work);

    if (mpp->auto_freq_en && mpp->hw_ops->get_freq)
        mpp->hw_ops->get_freq(mpp, task);

    msgs->queue = mpp->queue;
    msgs->task = task;
    msgs->mpp = mpp;

    /*
     * Push task to session should be in front of push task to queue.
     * Otherwise, when mpp_task_finish finish and worker_thread call
     * task worker, it may be get a task who has push in queue but
     * not in session, cause some errors.
     */
    atomic_inc(&session->task_count);
    mpp_session_push_pending(session, task);
    mpp_debug_func(DEBUG_TASK_INFO, "session %d:%d task %d state 0x%lx\n",
                   session->device_type, session->index,
                   task->task_index, task->state);
    return 0;
}

static int kmpp_venc_wait_result(struct mpp_session *session, struct mpp_task_msgs *msgs)
{
    struct mpp_task *task;
    struct mpp_dev *mpp;
    int ret;

    task = mpp_session_get_pending_task(session);
    if (!task) {
        mpp_err("session %p pending list is empty!\n", session);
        return -EIO;
    }
    mpp = mpp_get_task_used_device(task, session);

    ret = wait_event_interruptible(task->wait, test_bit(TASK_STATE_DONE, &task->state));
    if (ret == -ERESTARTSYS)
        mpp_err("wait task break by signal\n");

    if (mpp->dev_ops->result)
        ret = mpp->dev_ops->result(mpp, task);
    mpp_debug_func(DEBUG_TASK_INFO, "session %d:%d task %d state 0x%lx kref_rd %d ret %d\n",
                   session->device_type, session->index, task->task_index, task->state,
                   kref_read(&task->ref), ret);

    mpp_session_pop_pending(session, task);

    return ret;
}

rk_s32 kmpp_session_setup_legacy(KmppSessionOld *session, KmppSessionCfg *cfg)
{
    struct mpp_dev *mpp;
    rk_s32 client_type;
    rk_s32 ret;

    session->k_space = 1;
    session->pp_session = false;
    session->process_task = kmpp_venc_process_task;
    session->wait_result = kmpp_venc_wait_result;
    session->deinit = kmpp_venc_session_deinit;

    client_type = cfg->client_type;
    if (IS_ENABLED(RKVEPU500_PP_ENABLE) || IS_ENABLED(RKVEPU511_PP_ENABLE)) {
        if (client_type == MPP_DEVICE_RKVENC_PP) {
            session->pp_session = true;
            client_type = MPP_DEVICE_RKVENC;
        }
    }
    client_type = array_index_nospec(client_type, MPP_DEVICE_BUTT);
    mpp = g_srv->sub_devices[client_type];
    if (!mpp) {
        rk_s32 i;

        kmpp_logi_f("subdevice %d is invalid\n", client_type);

        for (i = 0; i < MPP_DEVICE_BUTT; i++) {
            mpp = g_srv->sub_devices[i];
            kmpp_logi_f("sub_device %2d - %px\n", i, mpp);
        }

        return rk_nok;
    }
    session->device_type = (enum MPP_DEVICE_TYPE)client_type;
    session->mpp = mpp;
    if (mpp->dev_ops) {
        if (mpp->dev_ops->process_task)
            session->process_task = mpp->dev_ops->process_task;

        if (mpp->dev_ops->wait_result)
            session->wait_result = mpp->dev_ops->wait_result;

        if (mpp->dev_ops->deinit)
            session->deinit = mpp->dev_ops->deinit;
    }
    session->index = atomic_fetch_inc(&mpp->session_index);
    if (mpp->dev_ops && mpp->dev_ops->init_session) {
        ret = mpp->dev_ops->init_session(session);
        if (ret) {
            kmpp_logi_f("failed to init session ret %d\n", ret);
            return ret;
        }
    }
    mpp_session_attach_workqueue(session, mpp->queue);

    session->callback = cfg->hook;
    session->chn_id = cfg->chan_id;
    session->online = cfg->online;
    session->mpp->online_mode = session->online;
    if (session->online) {
        session->mpp->always_on = 1;
        mpp_power_on(session->mpp);
    }

    return rk_ok;
}

rk_s32 kmpp_session_is_running_legacy(KmppSessionOld *session)
{
    struct mpp_dev *mpp = session->mpp;
    struct mpp_taskqueue *queue = mpp->queue;
    struct mpp_task *task, *n;
    unsigned long flags, flags1;
    rk_s32 running = 0;

    spin_lock_irqsave(&queue->dev_lock, flags);
    spin_lock_irqsave(&session->pending_lock, flags1);

    list_for_each_entry_safe(task, n, &session->pending_list, pending_link) {
        if (test_bit(TASK_STATE_RUNNING, &task->state)) {
            running = 1;
            break;
        }
    }

    spin_unlock_irqrestore(&session->pending_lock, flags1);
    spin_unlock_irqrestore(&queue->dev_lock, flags);

    return running;
}

rk_s32 kmpp_session_run_task_legacy(KmppSessionOld *session, void *param)
{
    struct mpp_dev *mpp = session->mpp;
    struct mpp_taskqueue *queue = mpp->queue;
    struct mpp_task_info *info = (struct mpp_task_info *)param;
    struct mpp_task *task = NULL;
    unsigned long flags, flags1;
    int ret = 0;

    spin_lock_irqsave(&queue->dev_lock, flags);
    mpp_debug_func(DEBUG_TASK_INFO, "chan_id %d ++\n", session->chn_id);

again:
    task = mpp_session_get_idle_task(session);
    if (!task) {
        ret = -EAGAIN;
        goto done;
    }

    /* when resolution switch case, need drop task if resolution mismatch. */
    if (info) {
        if ((ALIGN(info->width, 8) != task->width) || (ALIGN(info->height, 8) != task->height)) {
            mpp_dbg_warning("chan %d task %d pipe %d frame %d wxh [%d %d] != [%d %d]\n",
                            session->chn_id, task->task_index, info->pipe_id, info->frame_id,
                            info->width, info->height, task->width, task->height);
            mpp->cur_task = task;
            atomic_inc(&task->abort_request);
            set_bit(TASK_STATE_RUNNING, &task->state);
            set_bit(TASK_STATE_HANDLE, &task->state);
            set_bit(TASK_STATE_DONE, &task->state);
            spin_lock_irqsave(&queue->running_lock, flags1);
            list_move_tail(&task->queue_link, &queue->running_list);
            spin_unlock_irqrestore(&queue->running_lock, flags1);
            goto done;
        }
    }

    /* if task timeout and aborted, remove it */
    if (atomic_read(&task->abort_request) > 0) {
        kref_put(&task->ref, mpp_free_task);
        goto again;
    }

    /*
     * In the link table mode, the prepare function of the device
     * will check whether I can insert a new task into device.
     * If the device supports the task status query(like the HEVC
     * encoder), it can report whether the device is busy.
     * If the device does not support multiple task or task status
     * query, leave this job to mpp service.
     */
    if (mpp->dev_ops->prepare)
        task = mpp->dev_ops->prepare(mpp, task);
    else if (kmpp_taskqueue_is_running(queue))
        task = NULL;

    if (!task) {
        ret = -EBUSY;
        mpp_debug_func(DEBUG_TASK_INFO, "chan_id %d device is busy now\n",
                       session->chn_id);
        goto done;
    }

    if (info) {
        task->pipe_id = info->pipe_id;
        task->frame_id = info->frame_id;
    }

    if (atomic_read(&mpp->suspend_en) && atomic_read(&mpp->power_enabled)) {
        ret = -EAGAIN;
        goto done;
    }

    if (atomic_read(&session->release_request) > 0)
        goto done;

    /* run task */
    mpp = mpp_get_task_used_device(task, session);

    spin_lock_irqsave(&queue->running_lock, flags1);
    list_move_tail(&task->queue_link, &queue->running_list);
    spin_unlock_irqrestore(&queue->running_lock, flags1);

    mpp_reset_down_read(mpp->reset_group);
    mpp_time_record(task);
    mpp_iommu_dev_activate(mpp->iommu_info, mpp);
    if (mpp->dev_ops->run)
        mpp->dev_ops->run(mpp, task);
    set_bit(TASK_STATE_START, &task->state);

done:
    mpp_debug_func(DEBUG_TASK_INFO, "chan_id %d --\n", session->chn_id);
    spin_unlock_irqrestore(&queue->dev_lock, flags);
    mpp_taskqueue_trigger_work(mpp);

    return ret;
}

static int mpp_parse_msg_v1(struct mpp_msg_v1 *msg, struct mpp_request *req)
{
    int ret = 0;

    req->cmd = msg->cmd;
    req->flags = msg->flags;
    req->size = msg->size;
    req->offset = msg->offset;
    req->data = (void *)(unsigned long)msg->data_ptr;

    mpp_debug(DEBUG_IOCTL, "cmd %x, flags %08x, size %d, offset %x\n",
              req->cmd, req->flags, req->size, req->offset);

    ret = mpp_check_cmd_v1(req->cmd);
    if (ret)
        mpp_err("mpp cmd %x is not supproted.\n", req->cmd);

    return ret;
}

static inline int mpp_msg_is_last(struct mpp_request *req)
{
    int flag;

    if (req->flags & MPP_FLAGS_MULTI_MSG)
        flag = (req->flags & MPP_FLAGS_LAST_MSG) ? 1 : 0;
    else
        flag = 1;

    return flag;
}

rk_s32 kmpp_session_add_req_legacy(KmppSessionOld *session, void *param)
{
    struct mpp_task_msgs task_msgs;
    struct mpp_request *req = NULL;
    void *msg = param;
    int ret = 0;

    memset(&task_msgs, 0, sizeof(task_msgs));
    do {
        req = &task_msgs.reqs[task_msgs.req_cnt];
        /* first, parse to fixed struct */
        {
            struct mpp_msg_v1 *msg_v1;

            msg_v1 = (struct mpp_msg_v1 *)msg;
            ret = mpp_parse_msg_v1(msg_v1, req);
            if (ret)
                return -EFAULT;

            msg += sizeof(struct mpp_msg_v1);
        }
        task_msgs.req_cnt++;
        /* check loop times */
        if (task_msgs.req_cnt > MPP_MAX_MSG_NUM) {
            mpp_err("fail, message count %d more than %d.\n",
                    task_msgs.req_cnt, MPP_MAX_MSG_NUM);
            return -EINVAL;
        }
        /* second, process request */
        ret = mpp_process_request(session, session->srv, req, &task_msgs);
        if (ret)
            return -EFAULT;
        /* last, process task message */
        if (mpp_msg_is_last(req)) {
            session->msg_flags = task_msgs.flags;
            if (task_msgs.set_cnt > 0) {
                struct mpp_dev *mpp = NULL;
                struct mpp_task *task = NULL;
                struct mpp_taskqueue *queue = NULL;
                ret = mpp_process_task(session, &task_msgs);
                if (ret)
                    return ret;

                mpp = task_msgs.mpp;
                task = task_msgs.task;
                queue = task_msgs.queue;

                atomic_inc(&mpp->task_count);
                set_bit(TASK_STATE_PENDING, &task->state);

                /* online task trigger by user */
                if (!session->online) {
                    spin_lock(&queue->pending_lock);
                    list_add_tail(&task->queue_link, &queue->pending_list);
                    spin_unlock(&queue->pending_lock);
                    mpp_taskqueue_trigger_work(mpp);
                }
            }
            if (task_msgs.poll_cnt > 0) {
                ret = mpp_wait_result(session, &task_msgs);
                if (ret)
                    return ret;
            }
        }
    } while (!mpp_msg_is_last(req));

    return 0;
}

void *kmpp_session_get_dev_legacy(KmppSessionOld *session)
{
    struct mpp_dev *mpp = session->mpp;

    return mpp->dev;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0))
#define PDE_DATA(inode)         pde_data(inode)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
/* already has the Macro definition */
#else /* (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)) */
#define PDE_DATA(inode)         NULL
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)) */

#ifdef CONFIG_PROC_FS
static int fops_show_clk(struct seq_file *file, void *v)
{
    struct mpp_clk_info *mpp_clk = file->private;
    u32 clk_rate = clk_get_rate(mpp_clk->clk);
    struct clk_hw *hw = __clk_get_hw(mpp_clk->clk);

    if (hw) {
        u32 parent_num = clk_hw_get_num_parents(hw);
        struct clk *p_clk = clk_get_parent(mpp_clk->clk);
        u32 i;

        seq_printf(file, "%s %dHz\n", __clk_get_name(mpp_clk->clk), clk_rate);

        if (p_clk) {
            seq_printf(file, "------ parents info ------\n");
            seq_printf(file, "current parent: %s %ldHz\n",
                       __clk_get_name(p_clk), clk_get_rate(p_clk));
        }

        for (i = 0; i < parent_num; i++) {
            struct clk_hw *p_hw = clk_hw_get_parent_by_index(hw, i);

            if (!p_hw)
                continue;

            seq_printf(file, "parent[%d]: %s %ldHz ",
                       i, clk_hw_get_name(p_hw), clk_hw_get_rate(p_hw));

            do {
                p_hw = clk_hw_get_parent(p_hw);
                if (p_hw)
                    seq_printf(file, "-> %s %ldHz ",
                               clk_hw_get_name(p_hw), clk_hw_get_rate(p_hw));
            } while (p_hw);
            seq_printf(file, "\n");
        }
    } else
        seq_printf(file, "%s %dHz\n", __clk_get_name(mpp_clk->clk), clk_rate);

    return 0;
}

static int fops_open_clk(struct inode *inode, struct file *file)
{
    return single_open(file, fops_show_clk, PDE_DATA(inode));
}

static ssize_t fops_write_clk(struct file *file, const char __user *buf,
                              size_t count, loff_t *ppos)
{
    struct seq_file *priv = file->private_data;
    struct mpp_clk_info *mpp_clk = priv->private;
    char kbuf[64];
    u32 values[3] = {0};
    char *cur, *token;
    u32 clk_rate, cur_rate;
    int i = 0, j = 0;

    if (count >= sizeof(kbuf))
        return -EINVAL;

    if (copy_from_user(kbuf, buf, count))
        return -EFAULT;

    kbuf[count] = '\0';
    cur = kbuf;

    while ((token = strsep(&cur, " ")) != NULL && i < 3) {
        if (kstrtouint(token, 10, &values[i]) == 0) {
            values[i] = values[i] < MHZ ? values[i] * MHZ : values[i];
            i++;
        }
    }

    for (j = i - 1; j > 0 ; j--) {
        struct clk *p_clk = mpp_clk->clk;
        u32 p_idx = j;

        while (p_idx--)
            p_clk = clk_get_parent(p_clk);

        if (p_clk)
            clk_set_rate(p_clk, values[j]);
    }

    clk_rate = values[0];
    cur_rate = clk_get_rate(mpp_clk->clk);
    if (clk_rate != cur_rate)
        clk_set_rate(mpp_clk->clk, clk_rate);

    mpp_clk->debug_rate_hz = clk_rate;

    return count;
}

static const struct proc_ops procfs_fops_clk = {
    .proc_open = fops_open_clk,
    .proc_read = seq_read,
    .proc_release = single_release,
    .proc_write = fops_write_clk,
};

struct proc_dir_entry *mpp_procfs_create_clk_rw(const char *name, umode_t mode,
                                                struct proc_dir_entry *parent,
                                                struct mpp_clk_info *mpp_clk)
{
    return proc_create_data(name, mode, parent, &procfs_fops_clk, (void*)mpp_clk);
}
#endif


int mpp_show_session_info(struct seq_file *seq, u32 chan_id)
{
    struct mpp_session *session = NULL, *n;
    struct mpp_service *srv = g_srv;

    mutex_lock(&srv->session_lock);
    list_for_each_entry_safe(session, n, &srv->session_list, service_link) {
        struct  mpp_dev *mpp;

        if (session->chn_id != chan_id)
            continue;

        if (!session->priv)
            continue;

        if (!session->mpp)
            continue;

        mpp = session->mpp;

        if (mpp->dev_ops->dump_session)
            mpp->dev_ops->dump_session(session, seq);
    }
    mutex_unlock(&srv->session_lock);

    return 0;
}
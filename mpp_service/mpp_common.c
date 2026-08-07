// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Fuzhou Rockchip Electronics Co., Ltd
 *
 * author:
 *  Alpha Lin, alpha.lin@rock-chips.com
 *  Randy Li, randy.li@rock-chips.com
 *  Ding Wei, leo.ding@rock-chips.com
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/proc_fs.h>
#include <linux/pm_runtime.h>
#include <linux/poll.h>
#include <linux/regmap.h>
#include <linux/rwsem.h>
#include <linux/mfd/syscon.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/nospec.h>
#include <linux/version.h>

#include <soc/rockchip/pm_domains.h>

#include "rk-mpp.h"
#include "mpp_debug.h"
#include "mpp_common.h"
#include "mpp_iommu.h"
#include "mpp_osal.h"
#include "kmpp_obj.h"
#include "kmpp_session_legacy.h"

const char *mpp_device_name[MPP_DEVICE_BUTT] = {
    [MPP_DEVICE_VDPU1]          = "VDPU1",
    [MPP_DEVICE_VDPU2]          = "VDPU2",
    [MPP_DEVICE_VDPU1_PP]       = "VDPU1_PP",
    [MPP_DEVICE_VDPU2_PP]       = "VDPU2_PP",
    [MPP_DEVICE_AV1DEC]         = "AV1DEC",
    [MPP_DEVICE_HEVC_DEC]       = "HEVC_DEC",
    [MPP_DEVICE_RKVDEC]         = "RKVDEC",
    [MPP_DEVICE_AVSPLUS_DEC]    = "AVSPLUS_DEC",
    [MPP_DEVICE_RKJPEGD]        = "RKJPEGD",
    [MPP_DEVICE_RKVENC]         = "RKVENC",
    [MPP_DEVICE_VEPU1]          = "VEPU1",
    [MPP_DEVICE_VEPU2]          = "VEPU2",
    [MPP_DEVICE_VEPU2_JPEG]     = "VEPU2",
    [MPP_DEVICE_RKJPEGE]        = "RKJPEGE",
    [MPP_DEVICE_VEPU22]         = "VEPU22",
    [MPP_DEVICE_IEP2]           = "IEP2",
    [MPP_DEVICE_VDPP]           = "VDPP",
    [MPP_DEVICE_RKVENC_DVBM]    = "RKVENC_DVBM",
    [MPP_DEVICE_RKVENC_PP]      = "RKVENC_PP",
};

const char *enc_info_item_name[ENC_INFO_BUTT] = {
    [ENC_INFO_BASE]             = "null",
    [ENC_INFO_WIDTH]            = "width",
    [ENC_INFO_HEIGHT]           = "height",
    [ENC_INFO_FORMAT]           = "format",
    [ENC_INFO_FPS_IN]           = "fps_in",
    [ENC_INFO_FPS_OUT]          = "fps_out",
    [ENC_INFO_RC_MODE]          = "rc_mode",
    [ENC_INFO_BITRATE]          = "bitrate",
    [ENC_INFO_GOP_SIZE]         = "gop_size",
    [ENC_INFO_FPS_CALC]         = "fps_calc",
    [ENC_INFO_PROFILE]          = "profile",
};

static void mpp_attach_workqueue(struct mpp_dev *mpp, struct mpp_taskqueue *queue);

int mpp_taskqueue_pop_pending(struct mpp_taskqueue *queue, struct mpp_task *task)
{
    if (!task->session || !task->session->mpp)
        return -EINVAL;

    spin_lock(&queue->pending_lock);
    list_del_init(&task->queue_link);
    spin_unlock(&queue->pending_lock);
    kref_put(&task->ref, mpp_free_task);

    return 0;
}

struct mpp_task *mpp_taskqueue_get_pending_task(struct mpp_taskqueue *queue)
{
    struct mpp_task *task = NULL;

    spin_lock(&queue->pending_lock);
    task = list_first_entry_or_null(&queue->pending_list,
                                    struct mpp_task,
                                    queue_link);
    spin_unlock(&queue->pending_lock);

    return task;
}

static bool
mpp_taskqueue_is_running(struct mpp_taskqueue *queue)
{
    unsigned long flags;
    bool flag;

    spin_lock_irqsave(&queue->running_lock, flags);
    flag = !list_empty(&queue->running_list);
    spin_unlock_irqrestore(&queue->running_lock, flags);

    return flag;
}

int mpp_taskqueue_pending_to_run(struct mpp_taskqueue *queue, struct mpp_task *task)
{
    unsigned long flags;

    spin_lock(&queue->pending_lock);
    spin_lock_irqsave(&queue->running_lock, flags);
    list_move_tail(&task->queue_link, &queue->running_list);
    spin_unlock_irqrestore(&queue->running_lock, flags);
    spin_unlock(&queue->pending_lock);


    return 0;
}

struct mpp_task *mpp_taskqueue_get_running_task(struct mpp_taskqueue *queue)
{
    unsigned long flags;
    struct mpp_task *task = NULL;

    spin_lock_irqsave(&queue->running_lock, flags);
    task = list_first_entry_or_null(&queue->running_list,
                                    struct mpp_task,
                                    queue_link);
    spin_unlock_irqrestore(&queue->running_lock, flags);

    return task;
}

int mpp_taskqueue_pop_running(struct mpp_taskqueue *queue, struct mpp_task *task)
{
    unsigned long flags;

    if (!task->session || !task->session->mpp)
        return -EINVAL;

    spin_lock_irqsave(&queue->running_lock, flags);
    list_del_init(&task->queue_link);
    spin_unlock_irqrestore(&queue->running_lock, flags);
    kref_put(&task->ref, mpp_free_task);

    return 0;
}

void mpp_taskqueue_trigger_work(struct mpp_dev *mpp)
{
    kthread_queue_work(&mpp->queue->worker, &mpp->work);
}

int mpp_power_on(struct mpp_dev *mpp)
{
    if (!atomic_xchg(&mpp->power_enabled, 1)) {
        pm_runtime_get_sync(mpp->dev);
        mpp_pm_stay_awake(mpp->dev);

        if (mpp->hw_ops->clk_on)
            mpp->hw_ops->clk_on(mpp);
    }

    return 0;
}

int mpp_power_off(struct mpp_dev *mpp)
{
    if (mpp->always_on)
        return 0;

    if (atomic_read(&mpp->suspend_en))
        return 0;

    if (atomic_xchg(&mpp->power_enabled, 0)) {
        if (mpp->hw_ops->clk_off)
            mpp->hw_ops->clk_off(mpp);

        mpp_pm_relax(mpp->dev);
        if (mpp_taskqueue_get_pending_task(mpp->queue) ||
            mpp_taskqueue_get_running_task(mpp->queue)) {
            pm_runtime_mark_last_busy(mpp->dev);
            pm_runtime_put_autosuspend(mpp->dev);
        } else
            pm_runtime_put_sync_suspend(mpp->dev);
    }

    return 0;
}

void mpp_free_task(struct kref *ref)
{
    struct mpp_dev *mpp;
    struct mpp_session *session;
    struct mpp_task *task = container_of(ref, struct mpp_task, ref);

    if (!task->session) {
        mpp_err("task %p, task->session is null.\n", task);
        return;
    }
    session = task->session;

    mpp_debug_func(DEBUG_TASK_INFO, "session %d:%d task %d state 0x%lx abort %d\n",
                   session->device_type, session->index, task->task_index,
                   task->state, atomic_read(&task->abort_request));

    mpp = mpp_get_task_used_device(task, session);
    if (mpp->dev_ops->free_task)
        mpp->dev_ops->free_task(session, task);

    /* Decrease reference count */
    atomic_dec(&session->task_count);
    atomic_dec(&mpp->task_count);
}

struct reset_control *
mpp_reset_control_get(struct mpp_dev *mpp, enum MPP_RESET_TYPE type, const char *name)
{
    struct reset_control *rst = NULL;
    char shared_name[32] = "shared_";
    struct mpp_reset_group *group;
    void *of_node = mpp_dev_of_node(mpp->dev);
    int index;

    /* check reset whether belone to device alone */
    index = of_property_match_string(of_node, "reset-names", name);
    if (index >= 0) {
        rst = devm_reset_control_get(mpp->dev, name);
        mpp_safe_unreset(rst);

        return rst;
    }

    /* check reset whether is shared */
    strncat(shared_name, name,
            sizeof(shared_name) - strlen(shared_name) - 1);
    index = of_property_match_string(of_node, "reset-names", shared_name);
    if (index < 0) {
        dev_err(mpp->dev, "%s is not found!\n", shared_name);
        return NULL;
    }

    if (!mpp->reset_group) {
        dev_err(mpp->dev, "reset group is empty!\n");
        return NULL;
    }
    group = mpp->reset_group;

    down_write(&group->rw_sem);
    rst = group->resets[type];
    if (!rst) {
        rst = devm_reset_control_get(mpp->dev, shared_name);
        mpp_safe_unreset(rst);
        group->resets[type] = rst;
        group->queue = mpp->queue;
    }
    /* if reset not in the same queue, it means different device
     * may reset in the same time, then rw_sem_on should set true.
     */
    group->rw_sem_on |= (group->queue != mpp->queue) ? true : false;
    dev_info(mpp->dev, "reset %s rw_sem_on=%d\n", shared_name, group->rw_sem_on);
    up_write(&group->rw_sem);

    return rst;
}

int mpp_dev_reset(struct mpp_dev *mpp)
{
    dev_info(mpp->dev, "resetting...\n");

    disable_irq(mpp->irq);
    if (mpp->iommu_info && mpp->iommu_info->got_irq)
        disable_irq(mpp->iommu_info->irq);
    /*
     * before running, we have to switch grf ctrl bit to ensure
     * working in current hardware
     */
    if (mpp->hw_ops->set_grf)
        mpp->hw_ops->set_grf(mpp);
    else
        mpp_set_grf(mpp->grf_info);

    if (mpp->auto_freq_en && mpp->hw_ops->reduce_freq)
        mpp->hw_ops->reduce_freq(mpp);
    /* FIXME lock resource lock of the other devices in combo */
    mpp_iommu_down_write(mpp->iommu_info);
    mpp_reset_down_write(mpp->reset_group);
    atomic_set(&mpp->reset_request, 0);

    if (mpp->hw_ops->reset)
        mpp->hw_ops->reset(mpp);

    /* Note: if the domain does not change, iommu attach will be return
     * as an empty operation. Therefore, force to close and then open,
     * will be update the domain. In this way, domain can really attach.
     */
    mpp_iommu_refresh(mpp->iommu_info, mpp->dev);

    mpp_reset_up_write(mpp->reset_group);
    mpp_iommu_up_write(mpp->iommu_info);

    enable_irq(mpp->irq);
    if (mpp->iommu_info && mpp->iommu_info->got_irq)
        enable_irq(mpp->iommu_info->irq);

    dev_info(mpp->dev, "reset done\n");

    return 0;
}

void mpp_task_run_begin(struct mpp_task *task, u32 timing_en, u32 timeout)
{
    preempt_disable();

    set_bit(TASK_STATE_START, &task->state);

    mpp_time_record(task);
    schedule_delayed_work(&task->timeout_work, msecs_to_jiffies(timeout));
    set_bit(TASK_STATE_RUNNING, &task->state);
    if (timing_en) {
        task->on_sched_timeout = ktime_get();
        set_bit(TASK_TIMING_TO_SCHED, &task->state);
    }
}

void mpp_task_run_end(struct mpp_task *task, u32 timing_en)
{
    if (timing_en) {
        task->on_run_end = ktime_get();
        set_bit(TASK_TIMING_RUN_END, &task->state);
    }

#ifdef MODULE
    preempt_enable();
#else
    preempt_enable_no_resched();
#endif
}

static int mpp_task_run(struct mpp_dev *mpp, struct mpp_task *task)
{
    struct mpp_session *session = task->session;
    int ret;

    mpp_debug_enter();

    if (mpp->srv->timing_en) {
        task->on_run = ktime_get();
        set_bit(TASK_TIMING_RUN, &task->state);
    }

    /*
     * before running, we have to switch grf ctrl bit to ensure
     * working in current hardware
     */
    if (mpp->hw_ops->set_grf) {
        ret = mpp->hw_ops->set_grf(mpp);
        if (ret) {
            dev_err(mpp->dev, "set grf failed\n");
            return ret;
        }
    } else {
        mpp_set_grf(mpp->grf_info);
    }
    /*
     * Lock the reader locker of the device resource lock here,
     * release at the finish operation
     */
    mpp_reset_down_read(mpp->reset_group);

    /*
     * for iommu share hardware, should attach to ensure
     * working in current device
     */
    ret = mpp_iommu_attach(mpp->iommu_info);
    if (ret) {
        dev_err(mpp->dev, "mpp_iommu_attach failed\n");
        mpp_reset_up_read(mpp->reset_group);
        return -ENODATA;
    }

    mpp_power_on(mpp);
    mpp_debug_func(DEBUG_TASK_INFO, "%s session %d:%d task %d state 0x%lx\n",
                   dev_name(mpp->dev), session->device_type,
                   session->index, task->task_index, task->state);

    if (mpp->auto_freq_en && mpp->hw_ops->set_freq)
        mpp->hw_ops->set_freq(mpp, task);

    mpp_iommu_dev_activate(mpp->iommu_info, mpp);
    if (mpp->dev_ops->run)
        mpp->dev_ops->run(mpp, task);

    mpp_debug_leave();

    return 0;
}

void mpp_dev_load(struct mpp_dev *mpp, struct mpp_task *mpp_task)
{
    struct mpp_load_info *load_info = &mpp->load_info;
    ktime_t now;
    s64 time_diff_us;

    if (!mpp->srv->load_interval) {
        if (mpp->load_en) {
            mpp_dev_load_clear(mpp);
            mpp->srv->timing_en = 0;
            mpp->load_en = 0;
        }
        return;
    }

    if (!mpp->load_en) {
        mpp->srv->timing_en = 1;
        mpp->load_en = 1;
        load_info->load_time = ktime_get();
        return;
    }

    if (!mpp_task->on_run)
        return;

    now = ktime_get();
    time_diff_us = ktime_us_delta(now, load_info->load_time);
    load_info->busy_time += ktime_us_delta(now, mpp_task->on_run);
    if (mpp_task->hw_time)
        load_info->hw_busy_time += mpp_task->hw_time;
    else
        load_info->hw_busy_time += ktime_us_delta(mpp_task->on_irq, mpp_task->on_sched_timeout);
    /* 1s update */
    if (time_diff_us > mpp->srv->load_interval * 1000) {
        u32 tmp = div64_s64(load_info->busy_time * 10000, time_diff_us);
        u32 load = tmp / 100;
        u32 load_frac = tmp % 100;
        u32 max_load = 100;

        if (mpp->queue->core_count > 1)
            max_load *= mpp->queue->core_count;

        load_info->load = load > max_load ? max_load : load;
        load_info->load_frac = load > max_load ? 0 : load_frac;

        tmp = div64_s64(load_info->hw_busy_time * 10000, time_diff_us);
        load = tmp / 100;
        load_frac = tmp % 100;
        load_info->utilization = load > max_load ? max_load : load;
        load_info->utilization_frac = load > max_load ? 0 : load_frac;

        load_info->busy_time = 0;
        load_info->hw_busy_time = 0;
        load_info->load_time = now;
    }
}

void mpp_dev_load_clear(struct mpp_dev *mpp)
{
    struct mpp_load_info *load_info = &mpp->load_info;

    memset(load_info, 0, sizeof(*load_info));
}

static void try_process_running_task(struct mpp_dev *mpp)
{
    struct mpp_task *mpp_task, *n;
    struct mpp_taskqueue *queue = mpp->queue;
    unsigned long flags;

    /* try process running task */
    spin_lock_irqsave(&queue->running_lock, flags);
    list_for_each_entry_safe(mpp_task, n, &queue->running_list, queue_link) {
        spin_unlock_irqrestore(&queue->running_lock, flags);
        mpp = mpp_get_task_used_device(mpp_task, mpp_task->session);
        disable_irq(mpp->irq);
        if (!test_bit(TASK_STATE_HANDLE, &mpp_task->state)) {
            enable_irq(mpp->irq);
            spin_lock_irqsave(&queue->running_lock, flags);
            continue;
        }

        /* process timeout task */
        if (test_bit(TASK_STATE_TIMEOUT, &mpp_task->state)) {
            atomic_inc(&mpp->reset_request);
            mpp_iommu_dev_deactivate(mpp->iommu_info, mpp);
        }

        if (mpp->auto_freq_en && mpp->hw_ops->reduce_freq &&
            list_empty(&mpp->queue->pending_list))
            mpp->hw_ops->reduce_freq(mpp);
        mpp_dev_load(mpp, mpp_task);
        if (mpp->dev_ops->isr)
            mpp->dev_ops->isr(mpp);
        enable_irq(mpp->irq);
        spin_lock_irqsave(&queue->running_lock, flags);
    }
    spin_unlock_irqrestore(&queue->running_lock, flags);
}

static void mpp_task_worker_default(struct kthread_work *work_s)
{
    struct mpp_dev *mpp = container_of(work_s, struct mpp_dev, work);
    struct mpp_taskqueue *queue = mpp->queue;
    struct mpp_task *task;
    unsigned long flags;

    mpp_debug_enter();

    try_process_running_task(mpp);

    while (1) {
        task = mpp_taskqueue_get_pending_task(queue);
        if (!task)
            break;

        /* if task timeout and aborted, remove it */
        if (atomic_read(&task->abort_request) > 0) {
            mpp_taskqueue_pop_pending(queue, task);
            continue;
        }

        /* get device for current task */
        mpp = task->session->mpp;
        spin_lock_irqsave(&queue->dev_lock, flags);
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
        else if (mpp_taskqueue_is_running(queue))
            task = NULL;

        if (!task) {
            spin_unlock_irqrestore(&queue->dev_lock, flags);
            break;
        }

        /* Push a pending task to running queue */
        mpp = mpp_get_task_used_device(task, task->session);
        if (atomic_read(&mpp->suspend_en) && atomic_read(&mpp->power_enabled)) {
            spin_unlock_irqrestore(&queue->dev_lock, flags);
            break;
        }

        atomic_inc(&mpp->task_count);
        mpp_taskqueue_pending_to_run(queue, task);
        set_bit(TASK_STATE_RUNNING, &task->state);
        spin_unlock_irqrestore(&queue->dev_lock, flags);
        if (mpp_task_run(mpp, task))
            mpp_taskqueue_pop_running(queue, task);
    }
    mpp_session_cleanup_detach(queue, work_s);
}

static int mpp_attach_service(struct mpp_dev *mpp, struct device *dev)
{
    u32 taskqueue_node = 0;
    u32 reset_group_node = 0;
    struct device_node *np = NULL;
    struct platform_device *pdev = NULL;
    struct mpp_taskqueue *queue = NULL;
    int ret = 0;

    np = of_parse_phandle(mpp_dev_of_node(dev), "rockchip,srv", 0);
    if (!np || !of_device_is_available(np)) {
        dev_err(dev, "failed to get the mpp service node\n");
        return -ENODEV;
    }

    pdev = of_find_device_by_node(np);
    of_node_put(np);
    if (!pdev) {
        dev_err(dev, "failed to get mpp service from node\n");
        return -ENODEV;
    }

    mpp->srv = (struct mpp_service *)pdev_get_legacy(pdev);
    platform_device_put(pdev);
    if (!mpp->srv) {
        dev_err(dev, "failed attach service\n");
        return -EINVAL;
    }

    ret = of_property_read_u32(mpp_dev_of_node(dev), "rockchip,taskqueue-node", &taskqueue_node);
    if (ret) {
        dev_err(dev, "failed to get taskqueue-node\n");
        return ret;
    } else if (taskqueue_node >= mpp->srv->taskqueue_cnt) {
        dev_err(dev, "taskqueue-node %d must less than %d\n",
                taskqueue_node, mpp->srv->taskqueue_cnt);
        return -ENODEV;
    }
    /* set taskqueue according dtsi */
    queue = mpp->srv->task_queues[taskqueue_node];
    if (!queue) {
        dev_err(dev, "taskqueue attach to invalid node %d\n", taskqueue_node);
        return -ENODEV;
    }
    mpp_attach_workqueue(mpp, queue);

    ret = of_property_read_u32(mpp_dev_of_node(dev),
                               "rockchip,resetgroup-node", &reset_group_node);
    if (!ret) {
        /* set resetgroup according dtsi */
        if (reset_group_node >= mpp->srv->reset_group_cnt) {
            dev_err(dev, "resetgroup-node %d must less than %d\n",
                    reset_group_node, mpp->srv->reset_group_cnt);
            return -ENODEV;
        } else {
            mpp->reset_group = mpp->srv->reset_groups[reset_group_node];
            if (!mpp->reset_group->queue)
                mpp->reset_group->queue = queue;
            if (mpp->reset_group->queue != mpp->queue)
                mpp->reset_group->rw_sem_on = true;
        }
    }

    return 0;
}

rk_u32 mpp_get_hw_id(enum MPP_DEVICE_TYPE dev_type)
{
    struct mpp_service *srv = g_srv;
    rk_u32 type = array_index_nospec(dev_type, MPP_DEVICE_BUTT);

    if (type < MPP_DEVICE_BUTT && srv && srv->sub_devices[type] &&
        srv->sub_devices[type]->var && srv->sub_devices[type]->var->hw_info)
        return srv->sub_devices[type]->var->hw_info->hw_id;

    return (rk_u32)(-1);
}

const char *mpp_get_hw_name(enum MPP_DEVICE_TYPE dev_type)
{
    struct mpp_service *srv = g_srv;
    rk_u32 type = array_index_nospec(dev_type, MPP_DEVICE_BUTT);

    if (type < MPP_DEVICE_BUTT && srv && srv->sub_devices[type])
        return mpp_device_name[type];

    return "invalid";
}

struct mpp_taskqueue *mpp_taskqueue_init(struct device *dev)
{
    struct mpp_taskqueue *queue = devm_kzalloc(dev, sizeof(*queue), GFP_KERNEL);

    if (!queue)
        return NULL;

    mutex_init(&queue->session_lock);
    spin_lock_init(&queue->pending_lock);
    spin_lock_init(&queue->running_lock);
    mutex_init(&queue->mmu_lock);
    spin_lock_init(&queue->dev_lock);
    INIT_LIST_HEAD(&queue->session_attach);
    INIT_LIST_HEAD(&queue->session_detach);
    INIT_LIST_HEAD(&queue->pending_list);
    INIT_LIST_HEAD(&queue->running_list);
    INIT_LIST_HEAD(&queue->mmu_list);
    INIT_LIST_HEAD(&queue->dev_list);

    /* default taskqueue has max 16 task capacity */
    queue->task_capacity = MPP_MAX_TASK_CAPACITY;
    atomic_set(&queue->reset_request, 0);
    atomic_set(&queue->detach_count, 0);
    atomic_set(&queue->task_id, 0);
    queue->dev_active_flags = 0;

    return queue;
}

static void mpp_attach_workqueue(struct mpp_dev *mpp, struct mpp_taskqueue *queue)
{
    s32 core_id;
    unsigned long flags;

    INIT_LIST_HEAD(&mpp->queue_link);

    spin_lock_irqsave(&queue->dev_lock, flags);

    if (mpp->core_id >= 0)
        core_id = mpp->core_id;
    else
        core_id = queue->core_count;

    if (core_id < 0 || core_id >= MPP_MAX_CORE_NUM) {
        dev_err(mpp->dev, "invalid core id %d\n", core_id);
        goto done;
    }

    /*
     * multi devices with no multicores share one queue,
     * the core_id is default value 0.
     */
    if (queue->cores[core_id]) {
        if (queue->cores[core_id] == mpp)
            goto done;

        core_id = queue->core_count;
    }

    queue->cores[core_id] = mpp;
    queue->core_count++;

    set_bit(core_id, &queue->core_idle);
    list_add_tail(&mpp->queue_link, &queue->dev_list);
    if (queue->core_id_max < (u32)core_id)
        queue->core_id_max = (u32)core_id;

    mpp->core_id = core_id;
    mpp->queue = queue;

    mpp_dbg_core("%s attach queue as core %d\n",
                 dev_name(mpp->dev), mpp->core_id);

    if (queue->task_capacity > mpp->task_capacity)
        queue->task_capacity = mpp->task_capacity;

done:
    spin_unlock_irqrestore(&queue->dev_lock, flags);
}

static void mpp_detach_workqueue(struct mpp_dev *mpp)
{
    struct mpp_taskqueue *queue = mpp->queue;

    if (queue) {
        unsigned long flags;

        spin_lock_irqsave(&queue->dev_lock, flags);

        queue->cores[mpp->core_id] = NULL;
        queue->core_count--;

        clear_bit(mpp->core_id, &queue->core_idle);
        list_del_init(&mpp->queue_link);

        mpp->queue = NULL;

        spin_unlock_irqrestore(&queue->dev_lock, flags);
    }
}

static long mpp_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    KmppSession session = (KmppSession )filp->private_data;

    return kmpp_session_ioctl_legacy(kmpp_obj_to_priv(session), (void *)arg);
}

static int mpp_dev_open(struct inode *inode, struct file *filp)
{
    KmppSession session = NULL;

    mpp_debug_enter();

    kmpp_session_get(&session);
    filp->private_data = (void *)session;

    mpp_debug_leave();

    return nonseekable_open(inode, filp);
}

static int mpp_dev_release(struct inode *inode, struct file *filp)
{
    KmppSession session = filp->private_data;

    mpp_debug_enter();

    kmpp_session_put(session);
    filp->private_data = NULL;

    mpp_debug_leave();

    return 0;
}

const struct file_operations rockchip_mpp_fops = {
    .open       = mpp_dev_open,
    .release    = mpp_dev_release,
    .unlocked_ioctl = mpp_dev_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl   = mpp_dev_ioctl,
#endif
};

struct mpp_mem_region *
mpp_task_attach_fd(struct mpp_task *task, int fd)
{
    struct mpp_mem_region *mem_region = NULL, *loop = NULL, *n;
    struct mpp_dma_buffer *buffer = NULL;
    struct mpp_dev *mpp = task->session->mpp;
    struct mpp_dma_session *dma = task->session->dma;
    u32 mem_num = ARRAY_SIZE(task->mem_regions);
    bool found = false;

    if (fd <= 0 || !dma || !mpp)
        return ERR_PTR(-EINVAL);

    if (task->mem_count > mem_num) {
        mpp_err("mem_count %d must less than %d\n", task->mem_count, mem_num);
        return ERR_PTR(-ENOMEM);
    }

    /* find fd whether had import */
    list_for_each_entry_safe_reverse(loop, n, &task->mem_region_list, reg_link) {
        if (loop->fd == fd) {
            found = true;
            break;
        }
    }

    mem_region = &task->mem_regions[task->mem_count];
    if (found) {
        memcpy(mem_region, loop, sizeof(*loop));
        mem_region->is_dup = true;
    } else {
        mpp_iommu_down_read(mpp->iommu_info);
        buffer = mpp_dma_import_fd(mpp->iommu_info, dma, fd, 0);
        mpp_iommu_up_read(mpp->iommu_info);
        if (IS_ERR(buffer)) {
            mpp_err("can't import dma-buf %d\n", fd);
            return ERR_CAST(buffer);
        }

        mem_region->hdl = buffer;
        mem_region->iova = buffer->iova;
        mem_region->len = buffer->size;
        mem_region->fd = fd;
        mem_region->is_dup = false;
    }
    task->mem_count++;
    INIT_LIST_HEAD(&mem_region->reg_link);
    list_add_tail(&mem_region->reg_link, &task->mem_region_list);

    return mem_region;
}

int mpp_translate_reg_address(struct mpp_session *session,
                              struct mpp_task *task, int fmt,
                              u32 *reg, struct reg_offset_info *off_inf)
{
    int i;
    int cnt;
    const u16 *tbl;

    mpp_debug_enter();

    if (session->trans_count > 0) {
        cnt = session->trans_count;
        tbl = session->trans_table;
    } else {
        struct mpp_dev *mpp = mpp_get_task_used_device(task, session);
        struct mpp_trans_info *trans_info = mpp->var->trans_info;

        cnt = trans_info[fmt].count;
        tbl = trans_info[fmt].table;
    }

    for (i = 0; i < cnt; i++) {
        int usr_fd;
        u32 offset;
        struct mpp_mem_region *mem_region = NULL;

        if (session->msg_flags & MPP_FLAGS_REG_NO_OFFSET) {
            usr_fd = reg[tbl[i]];
            offset = 0;
        } else {
            usr_fd = reg[tbl[i]] & 0x3ff;
            offset = reg[tbl[i]] >> 10;
        }

        if (usr_fd == 0)
            continue;

        mem_region = mpp_task_attach_fd(task, usr_fd);
        if (IS_ERR(mem_region)) {
            mpp_err("reg[%3d]: 0x%08x fd %d failed\n",
                    tbl[i], reg[tbl[i]], usr_fd);
            return PTR_ERR(mem_region);
        }
        mpp_debug(DEBUG_IOMMU,
                  "reg[%3d]: %d => %pad, offset %10d, size %lx\n",
                  tbl[i], usr_fd, &mem_region->iova,
                  offset, mem_region->len);
        mem_region->reg_idx = tbl[i];
        reg[tbl[i]] = mem_region->iova + offset;
    }

    mpp_debug_leave();

    return 0;
}

int mpp_check_req(struct mpp_request *req, int base,
                  int max_size, u32 off_s, u32 off_e)
{
    int req_off;

    if (req->offset < base) {
        mpp_err("error: base %x, offset %x\n",
                base, req->offset);
        return -EINVAL;
    }
    req_off = req->offset - base;
    if ((req_off + req->size) < off_s) {
        mpp_err("error: req_off %x, req_size %x, off_s %x\n",
                req_off, req->size, off_s);
        return -EINVAL;
    }
    if (max_size < off_e) {
        mpp_err("error: off_e %x, max_size %x\n",
                off_e, max_size);
        return -EINVAL;
    }
    if (req_off > max_size) {
        mpp_err("error: req_off %x, max_size %x\n",
                req_off, max_size);
        return -EINVAL;
    }
    if ((req_off + req->size) > max_size) {
        mpp_err("error: req_off %x, req_size %x, max_size %x\n",
                req_off, req->size, max_size);
        req->size = req_off + req->size - max_size;
    }

    return 0;
}

int mpp_extract_reg_offset_info(struct reg_offset_info *off_inf,
                                struct mpp_request *req)
{
    int max_size = ARRAY_SIZE(off_inf->elem);
    int cnt = req->size / sizeof(off_inf->elem[0]);

    if ((cnt + off_inf->cnt) > max_size) {
        mpp_err("count %d, total %d, max_size %d\n",
                cnt, off_inf->cnt, max_size);
        return -EINVAL;
    }
    if (kmpp_copy_from_user(&off_inf->elem[off_inf->cnt],
                            req->data, req->size)) {
        mpp_err("kmpp_copy_from_user failed\n");
        return -EINVAL;
    }
    off_inf->cnt += cnt;

    return 0;
}

int mpp_query_reg_offset_info(struct reg_offset_info *off_inf,
                              u32 index)
{
    mpp_debug_enter();

    if (off_inf) {
        int i;

        for (i = 0; i < off_inf->cnt; i++) {
            if (off_inf->elem[i].index == index)
                return off_inf->elem[i].offset;
        }
    }

    mpp_debug_leave();

    return 0;
}

int mpp_translate_reg_offset_info(struct mpp_task *task,
                                  struct reg_offset_info *off_inf,
                                  u32 *reg)
{
    mpp_debug_enter();

    if (off_inf) {
        int i;

        for (i = 0; i < off_inf->cnt; i++) {
            mpp_debug(DEBUG_IOMMU, "reg[%d] + offset %d\n",
                      off_inf->elem[i].index,
                      off_inf->elem[i].offset);
            reg[off_inf->elem[i].index] += off_inf->elem[i].offset;
        }
    }

    mpp_debug_leave();

    return 0;
}

int mpp_task_init(struct mpp_session *session, struct mpp_task *task)
{
    INIT_LIST_HEAD(&task->pending_link);
    INIT_LIST_HEAD(&task->queue_link);
    INIT_LIST_HEAD(&task->mem_region_list);
    task->state = 0;
    task->mem_count = 0;
    task->session = session;

    return 0;
}

int mpp_task_finish(struct mpp_session *session, struct mpp_task *task)
{
    struct mpp_dev *mpp = mpp_get_task_used_device(task, session);
    u32 clbk_en = task->clbk_en;

    if (mpp->dev_ops->finish)
        mpp->dev_ops->finish(mpp, task);

    mpp_reset_up_read(mpp->reset_group);
    if (atomic_read(&mpp->reset_request) > 0)
        mpp_dev_reset(mpp);
    mpp_power_off(mpp);

    set_bit(TASK_STATE_FINISH, &task->state);
    set_bit(TASK_STATE_DONE, &task->state);

    if (session->srv->timing_en) {
        s64 time_diff;

        task->on_finish = ktime_get();
        set_bit(TASK_TIMING_FINISH, &task->state);

        time_diff = ktime_us_delta(task->on_finish, task->on_create);

        if (mpp->timing_check && time_diff > (s64)mpp->timing_check)
            mpp_task_dump_timing(task, time_diff);
    }

    /* Wake up the GET thread */
    wake_up(&task->wait);
    mpp_taskqueue_pop_running(mpp->queue, task);

    /* from kmpp */
    if (session->callback && clbk_en)
        session->callback(session->chn_id, MPP_VCODEC_EVENT_FRAME, NULL);

    return 0;
}

int mpp_task_finalize(struct mpp_session *session, struct mpp_task *task)
{
    struct mpp_mem_region *mem_region = NULL, *n;
    struct mpp_dev *mpp = mpp_get_task_used_device(task, session);

    /* release memory region attach to this registers table. */
    list_for_each_entry_safe(mem_region, n, &task->mem_region_list, reg_link) {
        if (!mem_region->is_dup) {
            mpp_iommu_down_read(mpp->iommu_info);
            mpp_dma_release(session->dma, mem_region->hdl);
            mpp_iommu_up_read(mpp->iommu_info);
        }
        list_del_init(&mem_region->reg_link);
    }

    /* from kmpp */
    task->state = 0;

    return 0;
}

int mpp_task_dump_mem_region(struct mpp_dev *mpp, struct mpp_task *task)
{
    struct mpp_mem_region *mem = NULL, *n;

    if (!task)
        return -EIO;

    mpp_err("--- dump task %d mem region ---\n", task->task_index);
    if (!list_empty(&task->mem_region_list)) {
        list_for_each_entry_safe(mem, n,
                                 &task->mem_region_list,
                                 reg_link) {
            mpp_err("reg[%3d]: %pad, size %lx\n",
                    mem->reg_idx, &mem->iova, mem->len);
        }
    } else {
        dev_err(mpp->dev, "no memory region mapped\n");
    }

    return 0;
}

int mpp_task_dump_reg(struct mpp_dev *mpp, struct mpp_task *task)
{
    if (!task)
        return -EIO;

    if (mpp_debug_unlikely(DEBUG_DUMP_ERR_REG)) {
        mpp_err("--- dump task register ---\n");
        if (task->reg) {
            u32 i;
            u32 s = task->hw_info->reg_start;
            u32 e = task->hw_info->reg_end;

            for (i = s; i <= e; i++) {
                u32 reg = i * sizeof(u32);

                mpp_err("reg[%03d]: %04x: 0x%08x\n",
                        i, reg, task->reg[i]);
            }
        }
    }

    return 0;
}

int mpp_task_dump_hw_reg(struct mpp_dev *mpp)
{
    u32 i;
    u32 s = mpp->var->hw_info->reg_start;
    u32 e = mpp->var->hw_info->reg_end;

    mpp_err("--- dump hardware register ---\n");
    for (i = s; i <= e; i++) {
        u32 reg = i * sizeof(u32);

        mpp_err("reg[%03d]: %04x: 0x%08x\n",
                i, reg, readl_relaxed(mpp->reg_base + reg));
    }

    return 0;
}

void mpp_reg_show(struct mpp_dev *mpp, u32 offset)
{
    if (!mpp)
        return;

    dev_err(mpp->dev, "reg[%03d]: %04x: 0x%08x\n",
            offset >> 2, offset, mpp_read_relaxed(mpp, offset));
}

void mpp_reg_show_range(struct mpp_dev *mpp, u32 start, u32 end)
{
    u32 offset;

    if (!mpp)
        return;

    for (offset = start; offset < end; offset += sizeof(u32))
        mpp_reg_show(mpp, offset);
}

/* The device will do more probing work after this */
int mpp_dev_probe(struct mpp_dev *mpp, struct platform_device *pdev)
{
    int ret;
    struct resource *res = NULL;
    struct device *dev = &pdev->dev;
    struct device_node *np = mpp_dev_of_node(&pdev->dev);
    struct mpp_hw_info *hw_info = mpp->var->hw_info;

    /* Get disable auto frequent flag from dtsi */
    mpp->auto_freq_en = !device_property_read_bool(dev, "rockchip,disable-auto-freq");
    /* read flag for pum idle request */
    mpp->skip_idle = device_property_read_bool(dev, "rockchip,skip-pmu-idle-request");

    /* read link table capacity */
    ret = of_property_read_u32(np, "rockchip,task-capacity", &mpp->task_capacity);
    if (ret)
        mpp->task_capacity = 1;

    mpp->dev = dev;
    mpp->hw_ops = mpp->var->hw_ops;
    mpp->dev_ops = mpp->var->dev_ops;

    /* from kmpp*/
    atomic_set(&mpp->power_enabled, 0);
    mpp->always_on = 0;
    atomic_set(&mpp->suspend_en, 0);
    sema_init(&mpp->work_sem, 1);

    /* Get and attach to service */
    ret = mpp_attach_service(mpp, dev);
    if (ret) {
        dev_err(dev, "failed to attach service\n");
        return -ENODEV;
    }

    /* power domain autosuspend delay 2s */
    pm_runtime_set_autosuspend_delay(dev, 2000);
    pm_runtime_use_autosuspend(dev);

    kthread_init_work(&mpp->work, mpp_task_worker_default);

    atomic_set(&mpp->reset_request, 0);
    atomic_set(&mpp->session_index, 0);
    atomic_set(&mpp->task_count, 0);
    atomic_set(&mpp->task_index, 0);

    mpp_device_init_wakeup(dev, true);
    pm_runtime_enable(dev);
    mpp->irq = platform_get_irq(pdev, 0);
    if (mpp->irq < 0) {
        dev_err(dev, "No interrupt resource found\n");
        ret = -ENODEV;
        goto failed;
    }

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        dev_err(&pdev->dev, "no memory resource defined\n");
        ret = -ENODEV;
        goto failed;
    }
    /*
     * Tips: here can not use function devm_ioremap_resource. The resion is
     * that hevc and vdpu map the same register address region in rk3368.
     * However, devm_ioremap_resource will call function
     * devm_request_mem_region to check region. Thus, use function
     * devm_ioremap can avoid it.
     */
    mpp->reg_base = devm_ioremap(dev, res->start, resource_size(res));
    if (!mpp->reg_base) {
        dev_err(dev, "ioremap failed for resource %pR\n", res);
        ret = -ENOMEM;
        goto failed;
    }
    mpp->io_base = res->start;

    /*
     * TODO: here or at the device itself, some device does not
     * have the iommu, maybe in the device is better.
     */
    mpp->iommu_info = mpp_iommu_probe(dev);
    if (IS_ERR(mpp->iommu_info)) {
        dev_err(dev, "failed to attach iommu\n");
        mpp->iommu_info = NULL;
    }

    if (mpp->hw_ops->init) {
        ret = mpp->hw_ops->init(mpp);
        if (ret)
            goto failed;
    }

    /* read hardware id */
    if (hw_info->reg_id >= 0) {
        pm_runtime_get_sync(dev);
        if (mpp->hw_ops->clk_on)
            mpp->hw_ops->clk_on(mpp);

        hw_info->hw_id = mpp_read(mpp, hw_info->reg_id * sizeof(u32));

        if (mpp->hw_ops->clk_off)
            mpp->hw_ops->clk_off(mpp);
        pm_runtime_put_sync(dev);
    }

    return ret;
failed:
    mpp_detach_workqueue(mpp);
    mpp_device_init_wakeup(dev, false);
    pm_runtime_disable(dev);

    return ret;
}

int mpp_dev_remove(struct mpp_dev *mpp)
{
    if (mpp->hw_ops->exit)
        mpp->hw_ops->exit(mpp);

    mpp_iommu_remove(mpp->iommu_info);
    mpp_detach_workqueue(mpp);
    mpp_device_init_wakeup(mpp->dev, false);
    pm_runtime_disable(mpp->dev);

    return 0;
}

void mpp_dev_shutdown(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct mpp_dev *mpp = pdev_get_legacy(pdev);
    int ret;
    int val;

    dev_info(dev, "shutdown device\n");

    atomic_inc(&mpp->srv->shutdown_request);
    ret = readx_poll_timeout(atomic_read, &mpp->task_count, val, val == 0, 20000, 200000);
    if (ret == -ETIMEDOUT)
        dev_err(dev, "wait total %d running time out\n",
                atomic_read(&mpp->task_count));
    else
        dev_info(dev, "shutdown success\n");
}

int mpp_dev_register_srv(struct mpp_dev *mpp, struct mpp_service *srv)
{
    enum MPP_DEVICE_TYPE device_type = mpp->var->device_type;

    srv->sub_devices[device_type] = mpp;
    set_bit(device_type, &srv->hw_support);

    return 0;
}

irqreturn_t mpp_dev_irq(int irq, void *param)
{
    struct mpp_dev *mpp = param;
    struct mpp_task *task = mpp->cur_task;
    irqreturn_t irq_ret = IRQ_NONE;
    u32 timing_en = mpp->srv->timing_en;

    if (task && timing_en) {
        task->on_irq = ktime_get();
        set_bit(TASK_TIMING_IRQ, &task->state);
    }

    if (mpp->dev_ops->irq)
        irq_ret = mpp->dev_ops->irq(mpp);

    if (!task) {
        mpp_debug(DEBUG_IRQ_CHECK, "error, task is null\n");
        irq_ret = IRQ_HANDLED;
        goto done;
    }

    if (irq_ret == IRQ_WAKE_THREAD) {
        /* if wait or delayed work timeout, abort request will turn on,
         * isr should not to response, and handle it in delayed work
         */
        if (test_and_set_bit(TASK_STATE_HANDLE, &task->state)) {
            dev_err(mpp->dev, "error, task %d has been handled, irq_status %#x\n",
                    task->task_index, mpp->irq_status);
            irq_ret = IRQ_HANDLED;
            goto done;
        }
        if (timing_en) {
            task->on_cancel_timeout = ktime_get();
            set_bit(TASK_TIMING_TO_CANCEL, &task->state);
        }
        cancel_delayed_work(&task->timeout_work);
        /* normal condition, set state and wake up isr thread */
        set_bit(TASK_STATE_IRQ, &task->state);
        task->irq_status = mpp->irq_status;
        mpp_iommu_dev_deactivate(mpp->iommu_info, mpp);
        irq_ret = IRQ_HANDLED;
        mpp_taskqueue_trigger_work(mpp);
    }

done:
    return irq_ret;
}

u32 mpp_get_grf(struct mpp_grf_info *grf_info)
{
    u32 val = 0;

    if (grf_info && grf_info->grf && grf_info->val)
        regmap_read(grf_info->grf, grf_info->offset, &val);

    return (val & MPP_GRF_VAL_MASK);
}

bool mpp_grf_is_changed(struct mpp_grf_info *grf_info)
{
    bool changed = false;

    if (grf_info && grf_info->grf && grf_info->val) {
        u32 grf_status = mpp_get_grf(grf_info);
        u32 grf_val = grf_info->val & MPP_GRF_VAL_MASK;

        changed = (grf_status == grf_val) ? false : true;
    }

    return changed;
}

int mpp_set_grf(struct mpp_grf_info *grf_info)
{
    if (grf_info && grf_info->grf && grf_info->val)
        regmap_write(grf_info->grf, grf_info->offset, grf_info->val);

    return 0;
}

int mpp_time_record(struct mpp_task *task)
{
    if (mpp_debug_unlikely(DEBUG_TIMING) && task) {
        task->start = ktime_get();
        task->part = task->start;
    }

    return 0;
}

int mpp_time_part_diff(struct mpp_task *task)
{
    if (mpp_debug_unlikely(DEBUG_TIMING)) {
        ktime_t end;
        struct mpp_dev *mpp = mpp_get_task_used_device(task, task->session);

        end = ktime_get();
        mpp_debug(DEBUG_PART_TIMING, "%s:%d session %d:%d part time: %lld us\n",
                  dev_name(mpp->dev), task->core_id, task->session->pid,
                  task->session->index, ktime_us_delta(end, task->part));
        task->part = end;
    }

    return 0;
}

int mpp_time_diff(struct mpp_task *task)
{
    if (mpp_debug_unlikely(DEBUG_TIMING)) {
        ktime_t end;
        struct mpp_dev *mpp = mpp_get_task_used_device(task, task->session);

        end = ktime_get();
        mpp_debug(DEBUG_TIMING, "%s:%d session %d:%d time: %lld us\n",
                  dev_name(mpp->dev), task->core_id, task->session->pid,
                  task->session->index, ktime_us_delta(end, task->start));
    }

    return 0;
}

int mpp_time_diff_with_hw_time(struct mpp_task *task, u32 clk_hz)
{
    if (mpp_debug_unlikely(DEBUG_TIMING)) {
        ktime_t end;
        struct mpp_dev *mpp = mpp_get_task_used_device(task, task->session);

        end = ktime_get();

        if (clk_hz)
            mpp_debug(DEBUG_TIMING, "%s:%d session %d:%d time: %lld us hw %d us\n",
                      dev_name(mpp->dev), task->core_id, task->session->pid,
                      task->session->index, ktime_us_delta(end, task->start),
                      task->hw_cycles / (clk_hz / 1000000));
        else
            mpp_debug(DEBUG_TIMING, "%s:%d session %d:%d time: %lld us\n",
                      dev_name(mpp->dev), task->core_id, task->session->pid,
                      task->session->index, ktime_us_delta(end, task->start));
    }

    return 0;
}

#define LOG_TIMING(state, id, stage, time, base) \
    do { \
        if (test_bit(id, &state)) \
            pr_info("timing: %-14s : %lld us\n", stage, ktime_us_delta(time, base)); \
        else \
            pr_info("timing: %-14s : invalid\n", stage); \
    } while (0)

void mpp_task_dump_timing(struct mpp_task *task, s64 time_diff)
{
    ktime_t s = task->on_create;
    unsigned long state = task->state;

    pr_info("task %d dump timing at %lld us:", task->task_id, time_diff);

    pr_info("timing: %-14s : %lld us\n", "create", ktime_to_us(s));
    LOG_TIMING(state, TASK_TIMING_CREATE_END, "create end",     task->on_create_end, s);
    LOG_TIMING(state, TASK_TIMING_PENDING,    "pending",        task->on_pending, s);
    LOG_TIMING(state, TASK_TIMING_RUN,        "run",            task->on_run, s);
    LOG_TIMING(state, TASK_TIMING_TO_SCHED,   "timeout start",  task->on_sched_timeout, s);
    LOG_TIMING(state, TASK_TIMING_RUN_END,    "run end",        task->on_run_end, s);
    LOG_TIMING(state, TASK_TIMING_IRQ,        "irq",            task->on_irq, s);
    LOG_TIMING(state, TASK_TIMING_TO_CANCEL,  "timeout cancel", task->on_cancel_timeout, s);
    LOG_TIMING(state, TASK_TIMING_FINISH,     "finish",         task->on_finish, s);
}

int mpp_write_req(struct mpp_dev *mpp, u32 *regs, u32 start_idx, u32 end_idx, u32 en_idx)
{
    int i;

    for (i = start_idx; i < end_idx; i++) {
        if (i == en_idx)
            continue;
        mpp_write_relaxed(mpp, i * sizeof(u32), regs[i]);
    }

    return 0;
}

int mpp_read_req(struct mpp_dev *mpp, u32 *regs, u32 start_idx, u32 end_idx)
{
    int i;

    for (i = start_idx; i < end_idx; i++)
        regs[i] = mpp_read_relaxed(mpp, i * sizeof(u32));

    return 0;
}

int mpp_get_clk_info(struct mpp_dev *mpp,
                     struct mpp_clk_info *clk_info,
                     const char *name)
{
    int index = of_property_match_string(mpp_dev_of_node(mpp->dev), "clock-names", name);

    if (index < 0)
        return -EINVAL;

    clk_info->clk = devm_clk_get(mpp->dev, name);
    of_property_read_u32_index(mpp_dev_of_node(mpp->dev), "rockchip,normal-rates",
                               index, &clk_info->normal_rate_hz);
    of_property_read_u32_index(mpp_dev_of_node(mpp->dev), "rockchip,advanced-rates",
                               index, &clk_info->advanced_rate_hz);

    return 0;
}

int mpp_set_clk_info_rate_hz(struct mpp_clk_info *clk_info, enum MPP_CLOCK_MODE mode,
                             unsigned long val)
{
    if (!clk_info->clk || !val)
        return 0;

    switch (mode) {
    case CLK_MODE_DEBUG:
        clk_info->debug_rate_hz = val;
        break;
    case CLK_MODE_REDUCE:
        clk_info->reduce_rate_hz = val;
        break;
    case CLK_MODE_NORMAL:
        clk_info->normal_rate_hz = val;
        break;
    case CLK_MODE_ADVANCED:
        clk_info->advanced_rate_hz = val;
        break;
    case CLK_MODE_DEFAULT:
        clk_info->default_rate_hz = val;
        break;
    default:
        mpp_err("error mode %d\n", mode);
        break;
    }

    return 0;
}

#define MPP_REDUCE_RATE_HZ (50 * MHZ)

unsigned long mpp_get_clk_info_rate_hz(struct mpp_clk_info *clk_info, enum MPP_CLOCK_MODE mode)
{
    unsigned long clk_rate_hz = 0;

    if (!clk_info->clk)
        return 0;

    if (clk_info->debug_rate_hz)
        return clk_info->debug_rate_hz;

    switch (mode) {
    case CLK_MODE_REDUCE: {
        if (clk_info->reduce_rate_hz)
            clk_rate_hz = clk_info->reduce_rate_hz;
        else
            clk_rate_hz = MPP_REDUCE_RATE_HZ;
    } break;
    case CLK_MODE_NORMAL: {
        if (clk_info->normal_rate_hz)
            clk_rate_hz = clk_info->normal_rate_hz;
        else
            clk_rate_hz = clk_info->default_rate_hz;
    } break;
    case CLK_MODE_ADVANCED: {
        if (clk_info->advanced_rate_hz)
            clk_rate_hz = clk_info->advanced_rate_hz;
        else if (clk_info->normal_rate_hz)
            clk_rate_hz = clk_info->normal_rate_hz;
        else
            clk_rate_hz = clk_info->default_rate_hz;
    } break;
    case CLK_MODE_DEFAULT:
    default: {
        clk_rate_hz = clk_info->default_rate_hz;
    } break;
    }

    return clk_rate_hz;
}

int mpp_clk_set_rate(struct mpp_clk_info *clk_info, enum MPP_CLOCK_MODE mode)
{
    unsigned long clk_rate_hz;

    if (!clk_info->clk)
        return -EINVAL;

    clk_rate_hz = mpp_get_clk_info_rate_hz(clk_info, mode);
    if (clk_rate_hz) {
        clk_info->used_rate_hz = clk_rate_hz;
        clk_set_rate(clk_info->clk, clk_rate_hz);
        clk_info->real_rate_hz = clk_get_rate(clk_info->clk);
    }

    return 0;
}

static int __maybe_unused mpp_common_runtime_suspend(struct device *dev)
{
    struct mpp_dev *mpp = dev_get_legacy(dev);

    mpp_dev_load_clear(mpp);

    return 0;
}

static int __maybe_unused mpp_common_runtime_resume(struct device *dev)
{
    return 0;
}

const struct dev_pm_ops mpp_common_pm_ops = {
    SET_RUNTIME_PM_OPS(mpp_common_runtime_suspend, mpp_common_runtime_resume, NULL)
    MPP_SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend, pm_runtime_force_resume)
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0))
#define PDE_DATA(inode)         pde_data(inode)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
/* already has the Macro definition */
#else /* (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)) */
#define PDE_DATA(inode)         NULL
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)) */

#ifdef CONFIG_ROCKCHIP_MPP_PROC_FS
static int fops_show_u32(struct seq_file *file, void *v)
{
    u32 *val = file->private;

    seq_printf(file, "%d\n", *val);

    return 0;
}

static int fops_open_u32(struct inode *inode, struct file *file)
{
    return single_open(file, fops_show_u32, PDE_DATA(inode));
}

static ssize_t fops_write_u32(struct file *file, const char __user *buf,
                              size_t count, loff_t *ppos)
{
    int rc;
    struct seq_file *priv = file->private_data;

    rc = kstrtou32_from_user(buf, count, 0, priv->private);
    if (rc)
        return rc;

    return count;
}

static const struct proc_ops procfs_fops_u32 = {
    .proc_open = fops_open_u32,
    .proc_read = seq_read,
    .proc_release = single_release,
    .proc_write = fops_write_u32,
};

struct proc_dir_entry *
mpp_procfs_create_u32(const char *name, umode_t mode, struct proc_dir_entry *parent, void *data)
{
    return proc_create_data(name, mode, parent, &procfs_fops_u32, data);
}

void mpp_procfs_create_common(struct proc_dir_entry *parent, struct mpp_dev *mpp)
{
    mpp_procfs_create_u32("disable_work", 0644, parent, &mpp->disable);
    mpp_procfs_create_u32("timing_check", 0644, parent, &mpp->timing_check);
}
#endif

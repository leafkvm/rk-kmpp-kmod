/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_DEV_IMPL_H__
#define __KMPP_DEV_IMPL_H__

#include <linux/iommu.h>
#include <linux/platform_device.h>

#include "kmpp_list.h"
#include "kmpp_spinlock.h"
#include "kmpp_dev.h"
#include "kmpp_cls.h"

struct device;
struct device_node;
struct platform_driver;

typedef struct KmppDrvImpl_t {
    KmppDrv drv;

    /* list to list_drv in kmpp_dev_srv */
    kmpp_list_head list_srv;
    /* list head for list_drv in KmppDevImpl */
    kmpp_list_head list_dev;

    /* driver name */
    const char *name;

    /* for platform driver register */
    rk_s32 id_cnt;
    struct of_device_id *ids;
    KmppDrvOps dops;

    /* register driver for kernel */
    struct platform_driver *pdev;

    /* driver session property */
    KmppSpinlock *lock_session;
    kmpp_list_head list_session;

    /* record all driver device */
    rk_s32 dev_cnt;
    KmppDevs devs;
} KmppDrvImpl;

typedef struct KmppDevImpl_t {
    /* device structure for user */
    KmppDev dev;

    /* device cluster using the same driver */
    KmppDevs *devs;

    /* list to list_dev in kmpp_dev_srv */
    kmpp_list_head list_srv;
    /* list to list_dev in KmppDrvImpl */
    kmpp_list_head list_drv;

    /* device property */
    struct platform_device *pdev;
    struct device *device;
    struct device_node *np;
    KmppDrvImpl *drv;

    /* device mmu property */
    struct platform_device *pdev_mmu;
    struct iommu_group *group;
    struct iommu_domain *domain;

    /* device reset property */
    rk_s32 rst_cnt;
    struct reset_control_bulk_data *rstcs;

    /* device interrupt callback */
    KmppIrq irq;
    KmppIrq isr;
    KmppIrqMmu irq_mmu;
} KmppDevImpl;

typedef struct KmppClsImpl_t {
    KmppCls cls;

    /* always service kmpp class */
    struct class *class;

    rk_u32 need_free;
} KmppClsImpl;

struct KmppFsDevMgr_t {
    const rk_u8 *name;
    rk_s32 is_root;

    /* always service kmpp class */
    rk_s32 cdev_size;
    struct class *cls;
    KmppDevFops fops;
    void *drv_data;
    rk_s32 priv_size;

    /* cdev register */
    dev_t dev_id;
    struct cdev *cdev;
    struct device *dev;

    /* device vm property */
    struct vm_operations_struct *vm_ops;

    rk_u32 need_free;
};

#define devs_to_drv(x) container_of(x, KmppDrvImpl, devs)
#define dev_to_impl(x) container_of(x, KmppDevImpl, dev)
#define drv_to_impl(x) container_of(x, KmppDrvImpl, drv)
#define cls_to_impl(x) container_of(x, KmppClsImpl, cls)

#endif /* __KMPP_DEV_IMPL_H__ */

/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_DEV_H__
#define __KMPP_DEV_H__

#include "rk_type.h"

typedef struct KmppDevRes_t {
    const char *name;
    rk_u32 *base;
    rk_u32 *end;
} KmppDevRes;

typedef struct KmppDrv_t {
    /* driver session and task context size from driver property */
    rk_s32 session_size;
    rk_s32 task_size;
    rk_s32 result_size;
} KmppDrv;

typedef struct KmppGrf_t {
    rk_u32 offset;
    rk_u32 value;
} KmppGrf;

/* interface for device users, MUST be used in pointer form */
typedef struct kmpp_dev_t {
    /* device name */
    const char *name;

    rk_s32 dev_uid;
    rk_s32 core_id;

    /* device resources */
    rk_s32 res_cnt;
    KmppDevRes *res;

    rk_s32 irq;
    rk_s32 irq_mmu;

    KmppDrv *drv;
    KmppGrf *grf;

    /* driver data for device probe process */
    void *drv_data;

    /* private data for driver ops functions */
    void *private;
} KmppDev;

typedef struct KmppDrvId_t {
    const char *compatible_name;
    const void *drv_data;
} KmppDrvId;

/* device driver ops */
typedef struct KmppDrvOps_t {
    rk_s32 (*probe)(KmppDev *);
    rk_s32 (*remove)(KmppDev *);

    void (*shutdown)(KmppDev *);
    rk_s32 (*suspend)(KmppDev *);
    rk_s32 (*resume)(KmppDev *);
} KmppDrvOps;

typedef struct KmppDrvProp_t {
    const char *name;

    rk_s32 id_cnt;
    KmppDrvId *ids;

    KmppDrvOps dops;

    rk_s32 session_size;
    rk_s32 task_size;
} KmppDrvProp;

/* device cluster info for multi core control */
typedef struct KmppDevs_t {
    rk_s32 count;
    KmppDev *dev[];
} KmppDevs;

typedef rk_s32 (*KmppIrq)(KmppDev *dev);
typedef rk_s32 (*KmppIrqMmu)(KmppDev *dev, rk_ul iova, rk_s32 status);

void *kmpp_dev_get_device(KmppDev *dev);

const rk_u8 *kmpp_device_name(void *device);

#endif /* __KMPP_DEV_H__ */
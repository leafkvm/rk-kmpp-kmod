/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_DMABUF_H__
#define __KMPP_DMABUF_H__

#include "kmpp_dev.h"

typedef enum SysDmaDirection_e {
    SYS_DMA_BIDIRECTIONAL   = 0,
    SYS_DMA_TO_DEVICE       = 1,
    SYS_DMA_FROM_DEVICE     = 2,
    SYS_DMA_NONE            = 3,
} SysDmaDirection;

typedef struct SysDmabuf_t {
    rk_u32 size;
    rk_u32 flag;

    /* daddr is iova address for the below device */
    rk_u64 daddr;
    KmppDev *dev;
    void *device;

    /* kernel address for cpu access */
    void *kaddr;
} SysDmabuf;

rk_s32 sys_dmabuf_alloc(SysDmabuf **dmabuf, KmppDev *dev, rk_s64 size, rk_u32 flag);
rk_s32 sys_dmabuf_free(SysDmabuf *dmabuf);
rk_s32 sys_dmabuf_sync_for_cpu(SysDmabuf *dmabuf, SysDmaDirection dir);
rk_s32 sys_dmabuf_sync_for_dev(SysDmabuf *dmabuf, SysDmaDirection dir);

rk_s32 sys_dmabuf_attach(SysDmabuf **dst, SysDmabuf *src, KmppDev *dev);
rk_s32 sys_dmabuf_detach(SysDmabuf *dmabuf);

/* SysDmabuf by struct device */
rk_s32 sys_dmabuf_alloc_by_device(SysDmabuf **dmabuf, void *device, rk_s64 size, rk_u32 flag);
rk_s32 sys_dmabuf_attach_by_device(SysDmabuf **dst, SysDmabuf *src, void *device);

#endif /* __KMPP_DMABUF_H__ */
/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPI_DEFS_H__
#define __KMPI_DEFS_H__

#include "rk_type.h"

/* context pointer and integer id */
typedef rk_s32 KmppChanId;
typedef void* KmppCtx;

/* data flow structure */
typedef void* KmppMeta;
typedef void* KmppPacket;
typedef void* KmppFrame;
typedef void* KmppBuffer;
typedef void* KmppBufGrp;
typedef void* KmppPacketBulk;
typedef void* KmppFrameBulk;

/* MUST be the same to the KmppObjShm in rk-mpp-kobj.h */
typedef struct KmppShmPtr_t {
    /* uaddr - the userspace base address for userspace access */
    union {
        rk_u64 uaddr;
        void *uptr;
    };
    /* kaddr - the kernel base address for kernel access */
    union {
        rk_u64 kaddr;
        void *kptr;
    };
    /* DO NOT access reserved data only used by kernel */
} KmppShmPtr;

#endif /*__KMPI_DEFS_H__*/

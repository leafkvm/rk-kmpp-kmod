/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Rockchip mpp system driver ioctl object
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef _UAPI_RK_MPP_KOBJ_H
#define _UAPI_RK_MPP_KOBJ_H

#include <linux/types.h>

#define KMPP_SHM_IOC_MAX_COUNT  64

/* kernel object share memory userspace visable header data */
typedef struct KmppObjShm_t {
    /* kobj_uaddr   - the userspace base address for kernel object */
    __u64           kobj_uaddr;
    /* kobj_kaddr   - the kernel base address for kernel object */
    __u64           kobj_kaddr;
    /* DO NOT access reserved data only used by kernel */
} KmppObjShm;

typedef struct KmppShmReq_t {
    /* shm_name     - NULL name addresss for shm direct allocation */
    __u64           shm_name;
    /* shm_size     - share memory size for shm direct allocation */
    __u32           shm_size;
    /* shm_flag     - share memory allocation flags */
    __u32           shm_flag;
} KmppShmReq;

/* kernel object share memory get / put ioctl data */
typedef struct KmppObjIocArg_t {
    /* address array element count */
    __u32           count;

    /* flag for batch operation */
    __u32           flag;

    /*
     * at KMPP_SHM_IOC_GET_SHM
     * name_uaddr   - kernel object name in userspace address
     * obj_sptr     - kernel object userspace address for KmppObjShm
     *
     * at KMPP_SHM_IOC_PUT_SHM
     * obj_sptr     - kernel object userspace address for KmppObjShm
     */
    union {
        __u64       name_uaddr[0];
        /* ioctl object userspace / kernel address */
        KmppObjShm  obj_sptr[0];
        KmppShmReq  shm_req[0];
    };
} KmppObjIocArg;

#endif /* _UAPI_RK_MPP_KOBJ_H */

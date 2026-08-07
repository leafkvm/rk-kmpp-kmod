/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_IOCTL_IMPL_H__
#define __KMPP_IOCTL_IMPL_H__

#include "kmpp_ioctl.h"

typedef struct KmppIocImpl_t {
    /* object defintion index for ioctl functions */
    rk_u32              def;
    /* object defintion ioctl command */
    rk_u32              cmd;
    /*
     * flags for:
     * last in the batch / not last
     * block / non-block
     * return / non-return
     * sync / async
     * ack / non-ack
     * direct call / timer call
     */
    rk_u32              flags;
    /* ioc object id for input and output queue match */
    rk_u32              id;

    /* return value for this ioctl operation */
    rk_s32              ret;

    /*
     * shm[IOC_SHM_CTX] - the context object share memory
     * shm[IOC_SHM_IN]  - the input object share memory
     * shm[IOC_SHM_OUT] - the output object share memory
     */
    KmppShmPtr          shm[IOC_SHM_MAX];
} KmppIocImpl;

#endif /* __KMPP_IOCTL_IMPL_H__ */

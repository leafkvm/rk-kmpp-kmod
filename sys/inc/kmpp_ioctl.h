/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_IOCTL_H__
#define __KMPP_IOCTL_H__

#include "rk_type.h"

typedef enum KmppObjIocShmType_t {
    IOC_SHM_CTX,
    IOC_SHM_IN,
    IOC_SHM_OUT,
    IOC_SHM_MAX,
} KmppObjIocShmType;

typedef void* KmppIoc;

#define KMPP_IOC_ENTRY_TABLE(prefix, ENTRY, STRCT, EHOOK, SHOOK, ALIAS) \
    ENTRY(prefix, u32,  rk_u32,         def,    FLAG_NONE,  def) \
    ENTRY(prefix, u32,  rk_u32,         cmd,    FLAG_NONE,  cmd) \
    ENTRY(prefix, u32,  rk_u32,         flags,  FLAG_NONE,  flags) \
    ENTRY(prefix, u32,  rk_u32,         id,     FLAG_NONE,  id) \
    ENTRY(prefix, s32,  rk_s32,         ret,    FLAG_NONE,  ret) \
    STRCT(prefix, shm,  KmppShmPtr,     ctx,    FLAG_NONE,  shm[IOC_SHM_CTX]) \
    STRCT(prefix, shm,  KmppShmPtr,     in,     FLAG_NONE,  shm[IOC_SHM_IN]) \
    STRCT(prefix, shm,  KmppShmPtr,     out,    FLAG_NONE,  shm[IOC_SHM_OUT])

#define KMPP_OBJ_NAME                   kmpp_ioc
#define KMPP_OBJ_INTF_TYPE              KmppIoc
#define KMPP_OBJ_ENTRY_TABLE            KMPP_IOC_ENTRY_TABLE
#define KMPP_OBJ_ACCESS_DISABLE
#include "kmpp_obj_func.h"

#endif /*__KMPP_IOCTL_H__*/

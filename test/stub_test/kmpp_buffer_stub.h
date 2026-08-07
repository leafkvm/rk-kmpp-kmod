/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_BUFFER_STUB_H__
#define __KMPP_BUFFER_STUB_H__

#include "kmpp_stub.h"
#include "kmpp_buffer.h"

rk_s32 kmpp_buf_grp_setup(KmppBufGrp group, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_buf_grp_setup);
rk_s32 kmpp_buf_grp_reset(KmppBufGrp group, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_buf_grp_reset);

#define KMPP_OBJ_NAME                   kmpp_buf_grp
#define KMPP_OBJ_INTF_TYPE              KmppBufGrp
#define KMPP_OBJ_ENTRY_TABLE            KMPP_BUF_GRP_ENTRY_TABLE
#include "kmpp_obj_func_stub.h"

KmppShmPtr *kmpp_buf_grp_get_cfg_s(KmppBufGrp group)
STUB_FUNC_RET_NULL(kmpp_buf_grp_get_cfg_s);
KmppBufGrpCfg kmpp_buf_grp_get_cfg_k(KmppBufGrp group)
STUB_FUNC_RET_NULL(kmpp_buf_grp_get_cfg_k);
rk_u64 kmpp_buf_grp_get_cfg_u(KmppBufGrp group)
STUB_FUNC_RET_ZERO(kmpp_buf_grp_get_cfg_u);


#define KMPP_OBJ_NAME                   kmpp_buf_grp_cfg
#define KMPP_OBJ_INTF_TYPE              KmppBufGrpCfg
#define KMPP_OBJ_ENTRY_TABLE            KMPP_BUF_GRP_CFG_ENTRY_TABLE
#include "kmpp_obj_func_stub.h"


rk_s32 kmpp_buffer_setup(KmppBuffer buffer, KmppFsDev *file, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_buffer_setup);
rk_s32 kmpp_buffer_inc_ref(KmppBuffer buffer, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_buffer_inc_ref);

/* helper functions */
KmppShmPtr *kmpp_buffer_get_cfg_s(KmppBuffer buffer)
STUB_FUNC_RET_NULL(kmpp_buffer_get_cfg_s);
KmppBufCfg kmpp_buffer_get_cfg_k(KmppBuffer buffer)
STUB_FUNC_RET_NULL(kmpp_buffer_get_cfg_k);
rk_u64 kmpp_buffer_get_cfg_u(KmppBuffer buffer)
STUB_FUNC_RET_ZERO(kmpp_buffer_get_cfg_u);
void *kmpp_buffer_get_kptr(KmppBuffer buffer)
STUB_FUNC_RET_NULL(kmpp_buffer_get_kptr);
rk_u64 kmpp_buffer_get_uptr(KmppBuffer buffer)
STUB_FUNC_RET_ZERO(kmpp_buffer_get_uptr);
rk_u32 kmpp_buffer_get_size(KmppBuffer buffer)
STUB_FUNC_RET_ZERO(kmpp_buffer_get_size);
rk_s32 kmpp_buffer_get_fd(KmppBuffer buffer)
STUB_FUNC_RET_ZERO(kmpp_buffer_get_fd);
KmppDmaBuf kmpp_buffer_get_dmabuf(KmppBuffer buffer)
STUB_FUNC_RET_NULL(kmpp_buffer_get_dmabuf);

#define KMPP_OBJ_NAME                   kmpp_buffer
#define KMPP_OBJ_INTF_TYPE              KmppBuffer
#define KMPP_OBJ_ENTRY_TABLE            KMPP_BUFFER_ENTRY_TABLE
#include "kmpp_obj_func_stub.h"

#define KMPP_OBJ_NAME                   kmpp_buf_cfg
#define KMPP_OBJ_INTF_TYPE              KmppBufCfg
#define KMPP_OBJ_ENTRY_TABLE            KMPP_BUF_CFG_ENTRY_TABLE
#include "kmpp_obj_func_stub.h"

rk_s32 kmpp_buffer_read(KmppBuffer buffer, rk_u32 offset, void *data, rk_s32 size, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_buffer_read);
rk_s32 kmpp_buffer_write(KmppBuffer buffer, rk_u32 offset, void *data, rk_s32 size, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_buffer_write);

rk_s32 kmpp_buffer_get_iova(KmppBuffer buffer, rk_u64 *iova, KmppDev *dev)
STUB_FUNC_RET_ZERO(kmpp_buffer_get_iova);
rk_s32 kmpp_buffer_put_iova(KmppBuffer buffer, rk_u64 iova, KmppDev *dev)
STUB_FUNC_RET_ZERO(kmpp_buffer_put_iova);
rk_s32 kmpp_buffer_get_iova_by_device(KmppBuffer buffer, rk_u64 *iova, void *device)
STUB_FUNC_RET_ZERO(kmpp_buffer_get_iova_by_device);
rk_s32 kmpp_buffer_put_iova_by_device(KmppBuffer buffer, rk_u64 iova, void *device)
STUB_FUNC_RET_ZERO(kmpp_buffer_put_iova_by_device);

rk_s32 kmpp_buffer_flush_for_cpu(KmppBuffer buffer)
STUB_FUNC_RET_ZERO(kmpp_buffer_flush_for_cpu);
rk_s32 kmpp_buffer_flush_for_dev(KmppBuffer buffer, KmppDev *dev)
STUB_FUNC_RET_ZERO(kmpp_buffer_flush_for_dev);
rk_s32 kmpp_buffer_flush_for_device(KmppBuffer buffer, void *device)
STUB_FUNC_RET_ZERO(kmpp_buffer_flush_for_device);
rk_s32 kmpp_buffer_flush_for_cpu_partial(KmppBuffer buffer, rk_u32 offset, rk_u32 size)
STUB_FUNC_RET_ZERO(kmpp_buffer_flush_for_cpu_partial);
rk_s32 kmpp_buffer_flush_for_dev_partial(KmppBuffer buffer, KmppDev *dev, rk_u32 offset, rk_u32 size)
STUB_FUNC_RET_ZERO(kmpp_buffer_flush_for_dev_partial);
rk_s32 kmpp_buffer_flush_for_device_partial(KmppBuffer buffer, void *device, rk_u32 offset, rk_u32 size)
STUB_FUNC_RET_ZERO(kmpp_buffer_flush_for_device_partial);

#endif /*__KMPP_BUFFER_H__*/

/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __MPP_BUFFER_STUB_H__
#define __MPP_BUFFER_STUB_H__

#include "kmpp_stub.h"
#include "mpp_buffer.h"

MPP_RET mpp_buffer_import_with_tag(MppBufferGroup group, MppBufferInfo *info, MppBuffer *buffer,
                                   const char *tag, const char *caller)
STUB_FUNC_RET_ZERO(mpp_buffer_import_with_tag);
MPP_RET mpp_buffer_get_with_tag(MppBufferGroup group, MppBuffer *buffer, size_t size,
                                const char *tag, const char *caller)
STUB_FUNC_RET_ZERO(mpp_buffer_get_with_tag);
MPP_RET mpp_ring_buffer_get_with_tag(MppBufferGroup group, MppBuffer *buffer, size_t size,
                                     const char *tag, const char *caller)
STUB_FUNC_RET_ZERO(mpp_ring_buffer_get_with_tag);

MPP_RET mpp_buffer_put_with_caller(MppBuffer buffer, const char *caller)
STUB_FUNC_RET_ZERO(mpp_buffer_put_with_caller);
MPP_RET mpp_buffer_inc_ref_with_caller(MppBuffer buffer, const char *caller)
STUB_FUNC_RET_ZERO(mpp_buffer_inc_ref_with_caller);

MPP_RET mpp_buffer_info_get_with_caller(MppBuffer buffer, MppBufferInfo *info, const char *caller)
STUB_FUNC_RET_ZERO(mpp_buffer_info_get_with_caller);
MPP_RET mpp_buffer_read_with_caller(MppBuffer buffer, size_t offset, void *data, size_t size,
                                    const char *caller)
STUB_FUNC_RET_ZERO(mpp_buffer_read_with_caller);
MPP_RET mpp_buffer_write_with_caller(MppBuffer buffer, size_t offset, void *data, size_t size,
                                     const char *caller)
STUB_FUNC_RET_ZERO(mpp_buffer_write_with_caller);
void   *mpp_buffer_get_ptr_with_caller(MppBuffer buffer, const char *caller)
STUB_FUNC_RET_NULL(mpp_buffer_get_ptr_with_caller);
int     mpp_buffer_get_fd_with_caller(MppBuffer buffer, const char *caller)
STUB_FUNC_RET_ZERO(mpp_buffer_get_fd_with_caller);
size_t  mpp_buffer_get_size_with_caller(MppBuffer buffer, const char *caller)
STUB_FUNC_RET_ZERO(mpp_buffer_get_size_with_caller);
int     mpp_buffer_get_index_with_caller(MppBuffer buffer, const char *caller)
STUB_FUNC_RET_ZERO(mpp_buffer_get_index_with_caller);
MPP_RET mpp_buffer_set_index_with_caller(MppBuffer buffer, int index, const char *caller)
STUB_FUNC_RET_ZERO(mpp_buffer_set_index_with_caller);
size_t  mpp_buffer_get_offset_with_caller(MppBuffer buffer, const char *caller)
STUB_FUNC_RET_ZERO(mpp_buffer_get_offset_with_caller);
MPP_RET mpp_buffer_set_offset_with_caller(MppBuffer buffer, size_t offset, const char *caller)
STUB_FUNC_RET_ZERO(mpp_buffer_set_offset_with_caller);

KmppDmaBuf mpp_buffer_get_dmabuf(MppBuffer buffer)
STUB_FUNC_RET_NULL(mpp_buffer_get_dmabuf);

MPP_RET mpp_buffer_group_get(MppBufferGroup *group, MppBufferType type, MppBufferMode mode,
                             const char *tag, const char *caller)
STUB_FUNC_RET_ZERO(mpp_buffer_group_get);
MPP_RET mpp_buffer_group_put(MppBufferGroup group)
STUB_FUNC_RET_ZERO(mpp_buffer_group_put);
MPP_RET mpp_buffer_group_clear(MppBufferGroup group)
STUB_FUNC_RET_ZERO(mpp_buffer_group_clear);
RK_S32  mpp_buffer_group_unused(MppBufferGroup group)
STUB_FUNC_RET_ZERO(mpp_buffer_group_unused);
size_t  mpp_buffer_group_usage(MppBufferGroup group)
STUB_FUNC_RET_ZERO(mpp_buffer_group_usage);
MppBufferMode mpp_buffer_group_mode(MppBufferGroup group)
STUB_FUNC_RET_ZERO(mpp_buffer_group_mode);
MppBufferType mpp_buffer_group_type(MppBufferGroup group)
STUB_FUNC_RET_ZERO(mpp_buffer_group_type);
struct dma_buf *mpp_buffer_get_dma_with_caller(MppBuffer buffer, const char *caller)
STUB_FUNC_RET_NULL(mpp_buffer_get_dma_with_caller);
MPP_RET mpp_buffer_flush_for_cpu_with_caller(MppBuffer buffer, const char *caller)
STUB_FUNC_RET_ZERO(mpp_buffer_flush_for_cpu_with_caller);
MPP_RET mpp_buffer_flush_for_device_with_caller(MppBuffer buffer, const char *caller)
STUB_FUNC_RET_ZERO(mpp_buffer_flush_for_device_with_caller);
MPP_RET mpp_buffer_flush_for_cpu_partial_with_caller(MppBuffer buffer, RK_U32 offset, RK_U32 len,
                                                     const char *caller)
STUB_FUNC_RET_ZERO(mpp_buffer_flush_for_cpu_partial_with_caller);
MPP_RET mpp_buffer_flush_for_device_partial_with_caller(MppBuffer buffer, RK_U32 offset, RK_U32 len,
                                                        const char *caller)
STUB_FUNC_RET_ZERO(mpp_buffer_flush_for_device_partial_with_caller);
RK_S32 mpp_buffer_to_shmptr(MppBuffer buffer, KmppShmPtr *sptr)
STUB_FUNC_RET_ZERO(mpp_buffer_to_shmptr);

#endif /*__MPP_BUFFER_STUB_H__*/

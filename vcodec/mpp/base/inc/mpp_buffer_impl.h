/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __MPP_BUFFER_IMPL_H__
#define __MPP_BUFFER_IMPL_H__

#include "mpp_stream_ring_buf.h"
#include "mpp_device.h"
#include "mpp_buffer.h"
#include "kmpp_env.h"

#define mpp_ring_buffer_get(group, buffer, size) \
        mpp_ring_buffer_get_with_tag(group, buffer, size, MODULE_TAG, __FUNCTION__)

#define mpp_buffer_get_iova(buffer, dev) \
        mpp_buffer_get_iova_f(buffer, dev, __FUNCTION__)

#ifdef __cplusplus
extern "C" {
#endif

RK_U32 mpp_buffer_get_iova_f(MppBuffer buffe, MppDev dev, const char *caller);

/* mpp buffer pool */
MPP_RET mpp_buffer_pool_init(RK_U32 max_cnt);
MPP_RET mpp_buffer_pool_deinit(void);
void mpp_buf_pool_info_show(KmppEnvNode node);
RK_U64 mpp_buffer_get_uptr(MppBuffer buffer);
RK_S32 mpp_buffer_put_uptr(MppBuffer buffer);
RK_S32 mpp_buffer_to_shmptr(MppBuffer buffer, KmppShmPtr *sptr);

#ifdef __cplusplus
}
#endif

#endif /*__MPP_BUFFER_IMPL_H__*/

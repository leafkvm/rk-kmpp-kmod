/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#ifndef __MPP_STREAM_RING_BUF_H__
#define __MPP_STREAM_RING_BUF_H__

#include "rk_type.h"

typedef struct RingBufPool_t {
    rk_u32      r_pos;
    rk_u32      w_pos;
    rk_u32      len;
    rk_u32      use_len;
    void        *buf_base;
    MppBuffer   buf;
    rk_u32      init_done;
    rk_u32      min_buf_size;
    rk_u32      l_r_pos;
    rk_u32      l_w_pos;
    rk_u32      max_use_len;
    rk_u32      ref;
} RingBufPool;

typedef struct RingBuf_t {
    MppBuffer   buf;
    RingBufPool *ring_pool;
    rk_u32      start_offset;
    rk_u32      r_pos;
    rk_u32      use_len;
    rk_u32      size;
    rk_u32      cir_flag;
} RingBuf;

rk_s32 ring_buf_pool_get(RingBufPool **pool);
rk_s32 ring_buf_pool_setup(RingBufPool *pool, MppBuffer buf, rk_u32 max_strm_cnt);
rk_s32 ring_buf_pool_put(RingBufPool *pool);

rk_u32 ring_buf_get_use_size(RingBufPool *pool);
rk_s32 kmpp_packet_ring_buf_put_used(KmppPacket packet);
rk_s32 ring_buf_put_free(RingBufPool *pool, RingBuf *buf);
rk_s32 ring_buf_get_free(RingBufPool *pool, RingBuf *buf, rk_u32 align,
                         rk_u32 min_size, rk_u32 stream_num);
rk_s32 ring_buf_update_min_size(RingBufPool *pool, rk_u32 min_zie);

rk_s32 ring_buf_flush(RingBuf *buf, RK_U8 for_cpu);

#endif /* __MPP_STREAM_RING_BUF_H__ */

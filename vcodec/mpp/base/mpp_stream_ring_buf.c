/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "ring_buf"

#include <linux/module.h>

#include "kmpp_atomic.h"
#include "kmpp_log.h"
#include "kmpp_mem.h"
#include "kmpp_string.h"

#include "mpp_maths.h"
#include "mpp_buffer_impl.h"

#include "kmpp_obj.h"
#include "kmpp_packet_impl.h"

#define RBUF_DBG_FLOW               (0x00000001)

#define rbuf_dbg(flag, fmt, ...)    kmpp_dbg(ring_buf_debug, flag, fmt, ## __VA_ARGS__)
#define rbuf_dbg_f(flag, fmt, ...)  kmpp_dbg_f(ring_buf_debug, flag, fmt, ## __VA_ARGS__)

#define rbuf_dbg_flow(fmt, ...)     rbuf_dbg(RBUF_DBG_FLOW, fmt, ## __VA_ARGS__)

#define DEFALUT_STRM_CNT_IN_POOL    4
#define DEFAULT_MIN_ZISE            (10 * SZ_1K)

rk_u32 ring_buf_debug = 0;
module_param(ring_buf_debug, uint, 0644);
MODULE_PARM_DESC(ring_buf_debug, "ring_buf debug information");

rk_s32 ring_buf_pool_get(RingBufPool **pool)
{
    RingBufPool *p;

    if (!pool) {
        kmpp_loge_f("invalid pool %px\n", pool);
        return rk_nok;
    }

    p = *pool;
    if (!p)
        p = kmpp_calloc(sizeof(RingBufPool));
    *pool = p;

    rbuf_dbg_flow("pool %px inited at %px\n", p, pool);

    return p ? rk_ok : rk_nok;
}

rk_s32 ring_buf_pool_setup(RingBufPool *pool, MppBuffer buf, rk_u32 max_strm_cnt)
{
    RingBufPool *p = pool;

    if (!pool) {
        kmpp_loge_f("invalid pool %px buf %px max_strm_cnt %d\n", pool, buf, max_strm_cnt);
        return rk_nok;
    }

    rbuf_dbg_flow("pool %px setup with buffer %px\n", p, buf);

    if (!buf) {
        kmpp_memset(p, 0, sizeof(RingBufPool));
        return rk_ok;
    }

    if (!max_strm_cnt)
        max_strm_cnt = DEFALUT_STRM_CNT_IN_POOL;

    p->w_pos = 0;
    p->r_pos = 0;
    p->len = mpp_buffer_get_size(buf);
    p->use_len = 0;
    p->max_use_len = 0;
    p->buf_base = mpp_buffer_get_ptr(buf);
    p->buf = buf;
    p->min_buf_size = DEFAULT_MIN_ZISE;

    rbuf_dbg_flow("pool %px setup len %d max_strm_cnt %d min_buf_size %d\n",
                  p, p->len, max_strm_cnt, p->min_buf_size);

    if (!p->buf_base)
        return rk_nok;

    p->init_done = 1;

    KMPP_ADD_FETCH(&p->ref, 1);

    return rk_ok;
}

rk_s32 ring_buf_pool_put(RingBufPool *pool)
{
    if (!pool)
        return rk_nok;

    if (!pool->init_done || !KMPP_SUB_FETCH(&pool->ref, 1)) {
        kmpp_free(pool);
    }

    return rk_ok;
}

rk_u32 ring_buf_get_use_size(RingBufPool *pool)
{
    if (!pool)
        return 0;

    if (pool->r_pos > pool->w_pos)
        return pool->len + pool->w_pos - pool->r_pos;

    return pool->w_pos - pool->r_pos;
}

static rk_s32 ring_buf_put_use(RingBufPool *pool, RingBuf *buf)
{
    rk_u32 invalid_pos;
    rk_u32 start_pos;
    rk_u32 end_pos;
    rk_u32 w_pos;
    rk_u32 r_pos;

    if (!pool || !buf || (pool->buf != buf->buf) || !buf->use_len)
        return rk_nok;

    w_pos = pool->w_pos;
    r_pos = pool->r_pos;
    start_pos = buf->start_offset;
    end_pos = ((buf->start_offset + buf->use_len) % pool->len);

    invalid_pos = 0;
    if (w_pos >= r_pos) {
        if ((start_pos < w_pos && start_pos > r_pos) ||
            (end_pos >= r_pos && end_pos < w_pos)) {
            invalid_pos = 1;
        }
    } else {
        if (start_pos >= r_pos || start_pos < w_pos ||
            end_pos < w_pos || end_pos >= r_pos) {
            invalid_pos = 1;
        }
    }

    kmpp_loge_cf(invalid_pos, "INVALID param: r_pos %x w_pos %x start %x size %x buf_len %x\n",
                 r_pos, w_pos, start_pos, buf->use_len, buf->size);

    if (end_pos < r_pos)
        pool->use_len = pool->len + end_pos - r_pos;
    else
        pool->use_len = end_pos - r_pos;

    if (pool->max_use_len < pool->use_len)
        pool->max_use_len = pool->use_len;

    pool->w_pos = end_pos;
    pool->l_w_pos = end_pos;

    rbuf_dbg_flow("pool %px put use  %d update r_pos %d w_pos %d use_len %d\n",
                  pool, buf->use_len, pool->r_pos, pool->w_pos, pool->use_len);

    KMPP_ADD_FETCH(&pool->ref, 1);

    return rk_ok;
}

rk_s32 ring_buf_put_free(RingBufPool *pool, RingBuf *buf)
{
    rk_u32 invalid_pos;
    rk_u32 start_pos;
    rk_u32 end_pos;
    rk_u32 w_pos;
    rk_u32 r_pos;

    if (!pool || !buf || !buf->use_len)
        return rk_nok;

    w_pos = pool->w_pos;
    r_pos = pool->r_pos;
    start_pos = buf->start_offset;
    end_pos = ((buf->start_offset + buf->use_len) % pool->len);

    invalid_pos = 0;
    if (r_pos >= w_pos) {
        if ((end_pos < r_pos && end_pos > w_pos) ||
            (start_pos >= w_pos && start_pos < r_pos)) {
            invalid_pos = 1;
        }
    } else {
        if (start_pos < r_pos || start_pos >= w_pos ||
            end_pos < r_pos || end_pos > w_pos) {
            invalid_pos = 1;
        }
    }

    kmpp_loge_cf(invalid_pos, "INVALID param: r_pos %x w_pos %x start %x size %x buf_len %x\n",
                 r_pos, w_pos, start_pos, buf->use_len, buf->size);

    pool->r_pos = end_pos;
    pool->l_r_pos = end_pos;

    if (end_pos > w_pos)
        pool->use_len = pool->len - end_pos + w_pos;
    else
        pool->use_len = w_pos - end_pos;

    rbuf_dbg_flow("pool %px put free %d update r_pos %d w_pos %d use_len %d\n",
                  pool, buf->use_len, pool->r_pos, pool->w_pos, pool->use_len);

    if (!KMPP_SUB_FETCH(&pool->ref, 1))
        kmpp_free(pool);

    return rk_ok;
}

rk_s32 ring_buf_get_free(RingBufPool *pool, RingBuf *buf, rk_u32 align,
                         rk_u32 min_size, rk_u32 stream_num)
{
    rk_u32 align_offset = 0;
    rk_u32 align_w_pos = 0, align_r_pos = 0;
    rk_u32 w_pos = 0, r_pos = 0;

    if (!pool || !buf)
        return rk_nok;

    if (!min_size)
        min_size = pool->w_pos == pool->r_pos ? DEFAULT_MIN_ZISE : pool->min_buf_size;

    w_pos = pool->w_pos;
    r_pos = pool->r_pos;
    buf->cir_flag = 1;

    rbuf_dbg_flow("pool %px get free r_pos %d w_pos %d use_len %d\n",
                  pool, pool->r_pos, pool->w_pos, pool->use_len);

    if (w_pos & (align - 1))
        align_offset = align - (w_pos & (align - 1));

    align_r_pos = (r_pos & ~(align - 1));
    if (r_pos > w_pos) {
        if (min_size + w_pos + align_offset >= r_pos)
            return rk_nok;

        align_w_pos = w_pos + align_offset;
        buf->start_offset = align_w_pos;
        buf->r_pos = align_r_pos;
        buf->buf = pool->buf;
        buf->size = r_pos - w_pos - align_offset;
        pool->min_buf_size = DEFAULT_MIN_ZISE;
        return rk_ok;
    }

    /* w_pos >= r_pos*/
    if (pool->len - (w_pos + align_offset - r_pos) > min_size) {
        align_w_pos = w_pos + align_offset;
        buf->start_offset = align_w_pos;
        buf->size = pool->len - align_w_pos + r_pos;
        buf->r_pos = align_r_pos;
        buf->buf = pool->buf;
        pool->min_buf_size = DEFAULT_MIN_ZISE;
        return rk_ok;
    }

    return rk_nok;
}

rk_s32 ring_buf_update_min_size(RingBufPool *pool, rk_u32 min_size)
{
    if (!pool)
        return rk_nok;

    pool->min_buf_size = min_size;

    return rk_ok;
}

rk_s32 ring_buf_flush(RingBuf *buf, RK_U8 for_cpu)
{
    MppBuffer buffer = buf->buf;
    rk_u32 offset = buf->start_offset;
    rk_u32 len, size;
    rk_s32 ret = rk_ok;

    if (!buffer) {
        kmpp_loge_f("invalid NULL input\n");
        return MPP_ERR_UNKNOW;
    }

    size = mpp_buffer_get_size(buffer);
    if (buf->start_offset + buf->use_len >= size) {
        len = buf->start_offset + buf->use_len - size;
        if (for_cpu) {
            ret = mpp_buffer_flush_for_cpu_partial(buffer, offset, size - offset);
            ret |= mpp_buffer_flush_for_cpu_partial(buffer, 0, len);
        } else {
            ret = mpp_buffer_flush_for_device_partial(buffer, offset, size - offset);
            ret |= mpp_buffer_flush_for_device_partial(buffer, 0, len);
        }
    } else {
        len = buf->use_len;
        if (for_cpu)
            ret = mpp_buffer_flush_for_cpu_partial(buffer, offset, len);
        else
            ret = mpp_buffer_flush_for_device_partial(buffer, offset, len);
    }

    return ret;
}

rk_s32 kmpp_packet_ring_buf_put_used(KmppPacket packet)
{
    KmppPacketImpl *impl = (KmppPacketImpl *)kmpp_obj_to_entry(packet);

    impl->buf.use_len = impl->length;
    if (impl->buf.ring_pool) {
        if (impl->length > impl->buf.size) {
            kmpp_loge_f("ring_buf used may be error");
            return rk_nok;
        }
        ring_buf_put_use(impl->buf.ring_pool, &impl->buf);
    }

    return rk_ok;
}

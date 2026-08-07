/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "mpp_buffer"

#include "kmpp_osal.h"

#include "kmpp_obj.h"
#include "kmpp_shm.h"
#include "kmpp_mem_pool.h"

#include "mpp_buffer_impl.h"
#include "kmpp_buffer.h"

typedef struct MppBufferImpl_t {
    MppBufferInfo   info;
    KmppAtomic      *ref_count;
    KmppBuffer      kmpp_buf;
    size_t          offset;
    RK_U32          cir_flag;
    RK_U64          uaddr;
    RK_U64          iova;
} MppBufferImpl;

static const char *module_name = MODULE_TAG;
static KmppMemPool mppbuf_pool = NULL;

MPP_RET mpp_buffer_pool_init(RK_U32 max_cnt)
{
    if (!mppbuf_pool) {
        rk_s32 atomic_size = kmpp_atomic_size();
        rk_s32 size = sizeof(MppBufferImpl) + atomic_size;

        mppbuf_pool = kmpp_mem_get_pool_f(module_name, size, max_cnt, 0);
    }

    return MPP_OK;
}

MPP_RET mpp_buffer_pool_deinit(void)
{
    if (mppbuf_pool) {
        kmpp_mem_put_pool_f(mppbuf_pool);
        mppbuf_pool = NULL;
    }

    return MPP_OK;
}

void mpp_buf_pool_info_show(KmppEnvNode node)
{
    return;
}

MPP_RET mpp_buffer_import_with_tag(MppBufferGroup group, MppBufferInfo *info, MppBuffer *buffer,
                                   const char *tag, const char *caller)
{
    MppBufferImpl *buf = NULL;
    KmppBuffer kmpp_buf = NULL;
    KmppBufCfg buf_cfg = NULL;
    rk_u32 size;
    MPP_RET ret = MPP_NOK;

    if (!info || !buffer) {
        kmpp_loge_f("invalid group %px info %px buffer %px at %s\n",
                    group, info, buffer, caller);
        return MPP_ERR_NULL_PTR;
    }

    buf = kmpp_mem_pool_get(mppbuf_pool, caller);
    if (!buf) {
        kmpp_loge_f("mpp_buffer_import fail %s\n", caller);
        return MPP_ERR_NULL_PTR;
    }

    kmpp_atomic_assign(&buf->ref_count, (void *)(buf + 1), kmpp_atomic_size());

    ret = kmpp_buffer_get(&kmpp_buf);
    if (ret) {
        kmpp_loge_f("kmpp buffer get failed ret %d\n", ret);
        goto __failed;
    }

    buf_cfg = kmpp_buffer_get_cfg_k(kmpp_buf);
    if (!buf_cfg) {
        kmpp_loge_f("get buffer cfg failed\n");
        ret = rk_nok;
        goto __failed;
    }

    if (info->fd > 0) {
        ret = kmpp_buf_cfg_set_fd(buf_cfg, info->fd);
    } else if (info->dma_buf) {
        ret = kmpp_buf_cfg_set_dmabuf(buf_cfg, info->dma_buf);
    }

    if (ret) {
        kmpp_loge_f("set buffer cfg failed\n");
        goto __failed;
    }

    ret = kmpp_buffer_setup(kmpp_buf, NULL, caller);
    if (ret) {
        kmpp_loge_f("buffer setup failed ret %d\n", ret);
        goto __failed;
    }

    kmpp_buf_cfg_get_size(buf_cfg, &size);
    info->size = size;
    buf->info = *info;
    buf->iova = kmpp_invalid_iova();
    buf->kmpp_buf = kmpp_buf;
    kmpp_atomic_fetch_inc(buf->ref_count);
    *buffer = buf;

    return MPP_OK;
__failed:
    if (buf)
        kmpp_mem_pool_put_f(mppbuf_pool, buf);
    if (kmpp_buf)
        kmpp_buffer_put(kmpp_buf);

    return ret;
}

MPP_RET mpp_buffer_get_with_tag(MppBufferGroup group, MppBuffer *buffer,
                                size_t size, const char *tag, const char *caller)
{
    MppBufferImpl *buf_impl = NULL;
    KmppBuffer kmpp_buf = NULL;
    KmppBufCfg buf_cfg = NULL;
    MPP_RET ret = MPP_OK;

    if (size <= 0) {
        kmpp_loge_f("size is 0 caller %s\n", caller);
        return MPP_NOK;
    }

    buf_impl = kmpp_mem_pool_get(mppbuf_pool, caller);
    if (!buf_impl) {
        kmpp_loge_f("buf impl malloc fail : group %px buffer %px size %u at %s\n",
                    group, buffer, (RK_U32)size, caller);
        return MPP_ERR_UNKNOW;
    }

    kmpp_atomic_assign(&buf_impl->ref_count, (void *)(buf_impl + 1), kmpp_atomic_size());

    ret = kmpp_buffer_get(&kmpp_buf);
    if (ret) {
        kmpp_loge_f("kmpp buffer get failed ret %d\n", ret);
        goto __failed;
    }

    buf_cfg = kmpp_buffer_get_cfg_k(kmpp_buf);
    if (!buf_cfg) {
        kmpp_loge_f("get buffer cfg failed\n");
        ret = rk_nok;
        goto __failed;
    }

    ret = kmpp_buf_cfg_set_size(buf_cfg, size);
    if (ret) {
        kmpp_loge_f("set buffer cfg failed\n");
        goto __failed;
    }

    ret = kmpp_buffer_setup(kmpp_buf, NULL, caller);
    if (ret) {
        kmpp_loge_f("buffer setup failed ret %d\n", ret);
        goto __failed;
    }

    buf_impl->info.size = size;
    buf_impl->iova = kmpp_invalid_iova();
    buf_impl->kmpp_buf = kmpp_buf;
    kmpp_atomic_fetch_inc(buf_impl->ref_count);
    *buffer = buf_impl;

    return (buf_impl) ? (MPP_OK) : (MPP_NOK);

__failed:
    if (buf_impl)
        kmpp_mem_pool_put_f(mppbuf_pool, buf_impl);
    if (kmpp_buf)
        kmpp_buffer_put(kmpp_buf);
    return ret;
}

MPP_RET mpp_ring_buffer_get_with_tag(MppBufferGroup group, MppBuffer *buffer,
                                     size_t size, const char *tag, const char *caller)
{
    MppBufferImpl *buf_impl = NULL;
    KmppBuffer kmpp_buf = NULL;
    KmppBufCfg buf_cfg = NULL;
    MPP_RET ret = MPP_OK;

    buf_impl = kmpp_mem_pool_get(mppbuf_pool, caller);
    if (!buf_impl) {
        kmpp_loge_f("buf impl pool get failed at %s\n", caller);
        return MPP_ERR_UNKNOW;
    }

    kmpp_atomic_assign(&buf_impl->ref_count, (void *)(buf_impl + 1), kmpp_atomic_size());

    ret = kmpp_buffer_get(&kmpp_buf);
    if (ret) {
        kmpp_loge_f("kmpp buffer get failed ret %d\n", ret);
        goto __failed;
    }
    buf_cfg = kmpp_buffer_get_cfg_k(kmpp_buf);
    if (!buf_cfg) {
        kmpp_loge_f("get buffer cfg failed\n");
        ret = rk_nok;
        goto __failed;
    }

    ret = kmpp_buf_cfg_set_flag(buf_cfg, KMPP_DMABUF_FLAGS_DUP_MAP);
    ret |= kmpp_buf_cfg_set_size(buf_cfg, size);
    if (ret) {
        kmpp_loge_f("set buffer cfg failed\n");
        goto __failed;
    }

    ret = kmpp_buffer_setup(kmpp_buf, NULL, caller);
    if (ret) {
        kmpp_loge_f("buffer setup failed ret %d\n", ret);
        goto __failed;
    }

    buf_impl->info.size = size;
    buf_impl->info.fd = -1;
    kmpp_atomic_fetch_inc(buf_impl->ref_count);
    buf_impl->cir_flag = 1;
    buf_impl->iova = kmpp_invalid_iova();
    buf_impl->kmpp_buf = kmpp_buf;
    *buffer = buf_impl;

    return (buf_impl) ? (MPP_OK) : (MPP_NOK);

__failed:
    if (buf_impl)
        kmpp_mem_pool_put_f(mppbuf_pool, buf_impl);
    if (kmpp_buf)
        kmpp_buffer_put(kmpp_buf);

    return ret;
}

MPP_RET mpp_buffer_put_with_caller(MppBuffer buffer, const char *caller)
{
    MppBufferImpl *buf_impl = (MppBufferImpl *)buffer;

    if (!buf_impl) {
        kmpp_loge("mpp_buffer_put invalid input: buffer NULL at %s\n", caller);
        return MPP_ERR_UNKNOW;
    }

    if (kmpp_atomic_dec_and_test(buf_impl->ref_count)) {
        kmpp_buffer_put(buf_impl->kmpp_buf);
        kmpp_mem_pool_put_f(mppbuf_pool, buf_impl);
    }

    return MPP_OK;
}

static MPP_RET mpp_buffer_map(MppBufferImpl *p)
{
    if (!p->info.ptr) {
        void *ptr = NULL;

        ptr = kmpp_buffer_get_kptr(p->kmpp_buf);
        if (!ptr) {
            kmpp_loge_f("buffer %px get kptr failed size %zu\n", p, p->info.size);
            return MPP_ERR_UNKNOW;
        }
        p->info.ptr = ptr;
    }

    return MPP_OK;
}

MPP_RET mpp_buffer_inc_ref_with_caller(MppBuffer buffer, const char *caller)
{
    MppBufferImpl *buf_impl = (MppBufferImpl *)buffer;

    if (!buf_impl) {
        kmpp_loge("mpp_buffer_inc_ref invalid input buffer NULL at %s\n", caller);
        return MPP_ERR_UNKNOW;
    }
    kmpp_atomic_fetch_inc(buf_impl->ref_count);

    return MPP_OK;
}

MPP_RET mpp_buffer_read_with_caller(MppBuffer buffer, size_t offset, void *data,
                                    size_t size, const char *caller)
{
    MppBufferImpl *p = (MppBufferImpl *)buffer;
    void *src = NULL;

    if (!p || !data) {
        kmpp_loge("mpp_buffer_read invalid input: buffer %px data %px at %s\n",
                  buffer, data, caller);
        return MPP_ERR_UNKNOW;
    }

    if (0 == size)
        return MPP_OK;

    if (mpp_buffer_map(p))
        return MPP_NOK;

    src = p->info.ptr;
    kmpp_assert(src);
    if (src)
        kmpp_memcpy(data, (char *)src + offset, size);

    return MPP_OK;
}

MPP_RET mpp_buffer_write_with_caller(MppBuffer buffer, size_t offset,
                                     void *data, size_t size, const char *caller)
{
    MppBufferImpl *p = (MppBufferImpl *)buffer;
    void *dst = NULL;

    if (!p || !data) {
        kmpp_loge("mpp_buffer_write invalid input: buffer %px data %px at %s\n",
                  buffer, data, caller);
        return MPP_ERR_UNKNOW;
    }

    if (0 == size)
        return MPP_OK;

    if (offset + size > p->info.size)
        return MPP_ERR_VALUE;

    if (mpp_buffer_map(p))
        return MPP_NOK;

    dst = p->info.ptr;
    kmpp_assert(dst != NULL);
    if (dst)
        kmpp_memcpy((char *)dst + offset, data, size);

    return MPP_OK;
}

void *mpp_buffer_get_ptr_with_caller(MppBuffer buffer, const char *caller)
{
    MppBufferImpl *p = (MppBufferImpl *)buffer;

    if (!p) {
        kmpp_loge_f("mpp_buffer_get_ptr invalid NULL input at %s\n", caller);
        return NULL;
    }

    if (p->info.ptr)
        return p->info.ptr;

    if (mpp_buffer_map(p))
        return NULL;

    kmpp_assert(p->info.ptr != NULL);
    if (!p->info.ptr)
        kmpp_loge_f("mpp_buffer_get_ptr buffer %px ret NULL at %s\n", buffer, caller);

    return p->info.ptr;
}

int mpp_buffer_get_fd_with_caller(MppBuffer buffer, const char *caller)
{
    MppBufferImpl *p = (MppBufferImpl *)buffer;

    if (!p) {
        kmpp_loge("mpp_buffer_get_fd invalid NULL input at %s\n", caller);
        return -1;
    }

    //TODO: maybe need a new fd by dma_buf_fd
    return p->info.fd;
}

struct dma_buf *mpp_buffer_get_dma_with_caller(MppBuffer buffer, const char *caller)
{
    MppBufferImpl *p = (MppBufferImpl *)buffer;

    if (!p) {
        kmpp_loge("mpp_buffer_get_dma invalid NULL input at %s\n", caller);
        return NULL;
    }

    if (!p->info.dma_buf)
        kmpp_loge("mpp_buffer_get_dma invalid dma buf %px at %s\n", buffer, caller);

    return p->info.dma_buf;
}

size_t mpp_buffer_get_size_with_caller(MppBuffer buffer, const char *caller)
{
    MppBufferImpl *p = (MppBufferImpl *)buffer;

    if (!p) {
        kmpp_loge("mpp_buffer_get_size invalid NULL input at %s\n", caller);
        return 0;
    }

    if (p->info.size == 0)
        kmpp_loge("mpp_buffer_get_size invalid size buffer %px at %s\n", buffer, caller);

    return p->info.size;
}

int mpp_buffer_get_index_with_caller(MppBuffer buffer, const char *caller)
{
    MppBufferImpl *p = (MppBufferImpl *)buffer;

    if (!p) {
        kmpp_loge("mpp_buffer_get_index invalid NULL input at %s\n", caller);
        return -1;
    }

    return p->info.index;
}

MPP_RET mpp_buffer_set_index_with_caller(MppBuffer buffer, int index, const char *caller)
{
    MppBufferImpl *p = (MppBufferImpl *)buffer;

    if (!p) {
        kmpp_loge("mpp_buffer_set_index invalid NULL input at %s\n", caller);
        return MPP_ERR_UNKNOW;
    }

    p->info.index = index;

    return MPP_OK;
}

size_t mpp_buffer_get_offset_with_caller(MppBuffer buffer, const char *caller)
{
    MppBufferImpl *p = (MppBufferImpl *)buffer;

    if (!p) {
        kmpp_loge("mpp_buffer_get_offset invalid NULL input at %s\n", caller);
        return -1;
    }

    return p->offset;
}

MPP_RET mpp_buffer_set_offset_with_caller(MppBuffer buffer, size_t offset, const char *caller)
{
    MppBufferImpl *p = (MppBufferImpl *)buffer;

    if (!p) {
        kmpp_loge("mpp_buffer_set_offset invalid NULL input at %s\n", caller);
        return MPP_ERR_UNKNOW;
    }

    p->offset = offset;

    return MPP_OK;
}

MPP_RET mpp_buffer_info_get_with_caller(MppBuffer buffer, MppBufferInfo *info, const char *caller)
{
    MppBufferImpl *p = (MppBufferImpl *)buffer;

    if (!buffer || !info) {
        kmpp_loge("mpp_buffer_info_get invalid input buffer %px info %px at %s\n",
                  buffer, info, caller);
        return MPP_ERR_UNKNOW;
    }

    *info = p->info;

    return MPP_OK;
}

MPP_RET mpp_buffer_flush_for_cpu_with_caller(MppBuffer buffer, const char *caller)
{
    MppBufferImpl *p = (MppBufferImpl *)buffer;

    if (!p) {
        kmpp_loge("mpp_buffer_flush_for_cpu invalid NULL input at %s\n", caller);
        return MPP_ERR_UNKNOW;
    }

    return kmpp_buffer_flush_for_cpu(p->kmpp_buf);
}

MPP_RET mpp_buffer_flush_for_device_with_caller(MppBuffer buffer, const char *caller)
{
    MppBufferImpl *p = (MppBufferImpl *)buffer;

    if (!p) {
        kmpp_loge("mpp_buffer_flush_for_device invalid NULL input at %s\n", caller);
        return MPP_ERR_UNKNOW;
    }

    return kmpp_buffer_flush_for_device(p->kmpp_buf, NULL);
}

MPP_RET mpp_buffer_flush_for_cpu_partial_with_caller(MppBuffer buffer, RK_U32 offset,
                                                     RK_U32 len, const char *caller)
{
    MppBufferImpl *p = (MppBufferImpl *)buffer;

    if (!p) {
        kmpp_loge("mpp_buffer_flush_for_cpu_partial invalid NULL input at %s\n", caller);
        return MPP_ERR_UNKNOW;
    }

    return kmpp_buffer_flush_for_cpu_partial(p->kmpp_buf, offset, len);
}

MPP_RET mpp_buffer_flush_for_device_partial_with_caller(MppBuffer buffer, RK_U32 offset,
                                                        RK_U32 len, const char *caller)
{
    MppBufferImpl *p = (MppBufferImpl *)buffer;

    if (!p) {
        kmpp_loge("mpp_buffer_flush_for_device_partial invalid NULL input at %s\n", caller);
        return MPP_ERR_UNKNOW;
    }

    return kmpp_buffer_flush_for_device_partial(p->kmpp_buf, NULL, offset, len);
}

RK_U32 mpp_buffer_get_iova_f(MppBuffer buffer, MppDev dev, const char *caller)
{
    MppBufferImpl *p = (MppBufferImpl *)buffer;
    rk_u64 invalid = kmpp_invalid_iova();
    rk_u64 iova = 0;
    MPP_RET ret;

    if (!p) {
        kmpp_loge("mpp_buffer_get_iova invalid NULL input at %s\n", caller);
        return invalid;
    }

    if (p->iova != invalid)
        return (RK_U32)p->iova;

    ret = kmpp_buffer_get_iova_by_device(p->kmpp_buf, &iova, mpp_get_dev(dev));
    if (ret)
        return invalid;

    p->iova = iova;

    return (rk_u32)p->iova;
}

RK_U64 mpp_buffer_get_uptr(MppBuffer buffer)
{
    MppBufferImpl *p = (MppBufferImpl *)buffer;

    if (!p) {
        kmpp_loge_f("invalid NULL input\n");
        return 0;
    }

    if (!p->uaddr)
        p->uaddr = kmpp_buffer_get_uptr(p->kmpp_buf);

    return p->uaddr;
}

RK_S32 mpp_buffer_put_uptr(MppBuffer buffer)
{
    MppBufferImpl *p = (MppBufferImpl *)buffer;

    if (!p) {
        kmpp_loge_f("invalid NULL input\n");
        return MPP_NOK;
    }

    if (p->uaddr) {
        kmpp_buffer_put_uptr(p->kmpp_buf);
        p->uaddr = 0;
    }

    return MPP_OK;
}

KmppDmaBuf mpp_buffer_get_dmabuf(MppBuffer buffer)
{
    MppBufferImpl *p = (MppBufferImpl *)buffer;

    if (!p) {
        kmpp_loge_f("invalid NULL input\n");
        return NULL;
    }

    return kmpp_buffer_get_dmabuf(p->kmpp_buf);
}

RK_S32 mpp_buffer_to_shmptr(MppBuffer buffer, KmppShmPtr *sptr)
{
    MppBufferImpl *p = (MppBufferImpl *)buffer;

    if (!p) {
        kmpp_loge_f("invalid NULL input\n");
        return MPP_NOK;
    }

    return kmpp_obj_to_shmptr(p->kmpp_buf, sptr);
}

#include <linux/export.h>

EXPORT_SYMBOL(mpp_buffer_import_with_tag);
EXPORT_SYMBOL(mpp_buffer_get_with_tag);
EXPORT_SYMBOL(mpp_ring_buffer_get_with_tag);
EXPORT_SYMBOL(mpp_buffer_put_with_caller);
EXPORT_SYMBOL(mpp_buffer_get_ptr_with_caller);
EXPORT_SYMBOL(mpp_buffer_get_fd_with_caller);
EXPORT_SYMBOL(mpp_buffer_get_dma_with_caller);
EXPORT_SYMBOL(mpp_buffer_get_size_with_caller);
EXPORT_SYMBOL(mpp_buffer_get_index_with_caller);
EXPORT_SYMBOL(mpp_buffer_set_index_with_caller);
EXPORT_SYMBOL(mpp_buffer_get_offset_with_caller);
EXPORT_SYMBOL(mpp_buffer_set_offset_with_caller);
EXPORT_SYMBOL(mpp_buffer_info_get_with_caller);
EXPORT_SYMBOL(mpp_buffer_flush_for_cpu_with_caller);
EXPORT_SYMBOL(mpp_buffer_flush_for_device_with_caller);
EXPORT_SYMBOL(mpp_buffer_flush_for_cpu_partial_with_caller);
EXPORT_SYMBOL(mpp_buffer_flush_for_device_partial_with_caller);
EXPORT_SYMBOL(mpp_buffer_get_iova_f);
EXPORT_SYMBOL(mpp_buffer_get_dmabuf);
EXPORT_SYMBOL(mpp_buffer_to_shmptr);

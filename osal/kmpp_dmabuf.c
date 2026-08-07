/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_dmabuf"

#include <linux/dma-buf.h>
#include <linux/scatterlist.h>

#include "kmpp_mem.h"
#include "kmpp_log.h"
#include "kmpp_dmabuf.h"
#include "kmpp_dev_impl.h"

struct device;

typedef struct SysDmabufImpl_t {
    SysDmabuf buf;
    kmpp_list_head list_srv;
    kmpp_list_head list_dev;
    struct sg_table *sgt;
    KmppDev *orig_dev;
    rk_s32 is_attachment;
} SysDmabufImpl;

#define sys_dmabuf_to_impl(dmabuf) container_of(dmabuf, SysDmabufImpl, buf)

static struct device *dmabuf_to_device(SysDmabuf *dmabuf)
{
    struct device *dev = NULL;

    if (!dmabuf)
        return dev;

    if (dmabuf->device) {
        dev = (struct device *)dmabuf->device;
    } else {
        KmppDevImpl *impl = dev_to_impl(dmabuf->dev);

        if (impl->device)
            dev = impl->device;
    }

    return dev;
}

static SysDmabufImpl *alloc_dmabuf(rk_s64 size, rk_u32 flag, void *device, const rk_u8 *caller)
{
    SysDmabufImpl *buf = NULL;
    struct sg_table *sgt = NULL;
    dma_addr_t daddr = 0;
    void *kaddr = NULL;
    rk_s32 ret = rk_ok;

    buf = kmpp_calloc(sizeof(SysDmabufImpl) + sizeof(struct sg_table));
    if (!buf) {
        kmpp_loge("alloc dmabuf struct failed at %s\n", caller);
        goto failed;
    }

    sgt = (struct sg_table *)(buf + 1);

    kaddr = dma_alloc_attrs(device, size, &daddr, GFP_KERNEL, flag | DMA_ATTR_NO_WARN);
    if (IS_ERR_OR_NULL(kaddr)) {
        kmpp_loge("alloc dmabuf size %d failed %d at %s\n", size, PTR_ERR(kaddr), caller);
        goto failed;
    }

    ret = dma_get_sgtable_attrs(device, sgt, kaddr, daddr, size, 0);
    if (ret) {
        kmpp_loge("alloc dmabuf sgt for %px:%pad size %d failed %d at %s\n",
                  kaddr, &daddr, size, ret, caller);
        goto failed;
    }

    KMPP_INIT_LIST_HEAD(&buf->list_srv);
    KMPP_INIT_LIST_HEAD(&buf->list_dev);

    buf->buf.kaddr = kaddr;
    buf->buf.daddr = daddr;
    buf->buf.size = size;
    buf->sgt = sgt;
    buf->is_attachment = 0;

    return buf;

failed:
    if (daddr)
        dma_free_attrs(device, size, kaddr, daddr, DMA_ATTR_NO_WARN);

    kmpp_free(buf);
    return NULL;
}

rk_s32 sys_dmabuf_alloc(SysDmabuf **dmabuf, KmppDev *dev, rk_s64 size, rk_u32 flag)
{
    KmppDevImpl *impl = dev_to_impl(dev);
    SysDmabufImpl *buf = NULL;

    if (!dmabuf || !dev || !size) {
        kmpp_loge_f("invalid dmabuf %px dev %px size %llx\n", dmabuf, dev, size);
        return rk_nok;
    }

    buf = alloc_dmabuf(size, flag, impl->device, __FUNCTION__);
    if (buf) {
        buf->buf.dev = dev;
        buf->buf.device = impl->device;
        buf->orig_dev = dev;

        *dmabuf = &buf->buf;
        return rk_ok;
    }

    *dmabuf = NULL;
    return rk_nok;
}
EXPORT_SYMBOL(sys_dmabuf_alloc);

rk_s32 sys_dmabuf_free(SysDmabuf *dmabuf)
{
    if (dmabuf) {
        SysDmabufImpl *buf = sys_dmabuf_to_impl(dmabuf);
        struct device *dev = dmabuf_to_device(dmabuf);

        sg_free_table(buf->sgt);
        dma_free_attrs(dev, dmabuf->size, dmabuf->kaddr, dmabuf->daddr, DMA_ATTR_NO_WARN);
        kmpp_free(buf);
        return rk_ok;
    }
    return rk_nok;
}
EXPORT_SYMBOL(sys_dmabuf_free);

rk_s32 sys_dmabuf_sync_for_cpu(SysDmabuf *dmabuf, SysDmaDirection dir)
{
    if (dmabuf) {
        SysDmabufImpl *buf = sys_dmabuf_to_impl(dmabuf);
        struct device *dev = dmabuf_to_device(dmabuf);

        dma_sync_sg_for_cpu(dev, buf->sgt->sgl, buf->sgt->nents, (enum dma_data_direction)dir);
        return rk_ok;
    }

    return rk_nok;
}
EXPORT_SYMBOL(sys_dmabuf_sync_for_cpu);

rk_s32 sys_dmabuf_sync_for_dev(SysDmabuf *dmabuf, SysDmaDirection dir)
{
    if (dmabuf) {
        SysDmabufImpl *buf = sys_dmabuf_to_impl(dmabuf);
        struct device *dev = dmabuf_to_device(dmabuf);

        dma_sync_sg_for_device(dev, buf->sgt->sgl, buf->sgt->nents, (enum dma_data_direction)dir);
        return rk_ok;
    }

    return rk_nok;
}
EXPORT_SYMBOL(sys_dmabuf_sync_for_dev);

static SysDmabufImpl *attach_dmabuf(SysDmabufImpl *src, void *device, const rk_u8 *caller)
{
    SysDmabufImpl *impl = NULL;
    struct sg_table *sgt = NULL;
    rk_s32 ret;

    impl = kmpp_calloc(sizeof(SysDmabufImpl) + sizeof(struct sg_table));
    if (!impl) {
        kmpp_loge("alloc dmabuf struct failed at %s\n", caller);
        goto failed;
    }

    impl->sgt = (struct sg_table *)(impl + 1);
    sgt = impl->sgt;

    ret = sg_alloc_table(sgt, src->sgt->orig_nents, GFP_KERNEL);
    if (ret) {
        kmpp_loge("alloc dmabuf sg table failed at %s\n", caller);
        goto failed;
    }

    {
        struct scatterlist *sg, *new_sg;
        rk_s32 i;

        new_sg = sgt->sgl;
        for_each_sgtable_sg(sgt, sg, i) {
            sg_set_page(new_sg, sg_page(sg), sg->length, sg->offset);
            new_sg = sg_next(new_sg);
        }
    }

    ret = dma_map_sgtable(device, sgt, DMA_BIDIRECTIONAL, 0);
    if (ret) {
        kmpp_loge("alloc dmabuf dma_map_sgtable failed %d at %s\n", ret, caller);
        goto failed;
    }

    impl->buf.kaddr = src->buf.kaddr;
    impl->buf.daddr = sg_dma_address(sgt->sgl);;
    impl->buf.size = src->buf.size;
    impl->orig_dev = src->orig_dev;
    impl->is_attachment = 1;

    return impl;

failed:
    if (sgt)
        sg_free_table(sgt);

    kmpp_free(impl);

    return NULL;
}

rk_s32 sys_dmabuf_attach(SysDmabuf **dst, SysDmabuf *src, KmppDev *dev)
{
    KmppDevImpl *impl_dev;
    SysDmabufImpl *impl;
    SysDmabufImpl *impl_src;

    if (!dst || !src || !dev) {
        kmpp_loge_f("invalid param dst %px src %px dev %px\n", dst, src, dev);
        return rk_nok;
    }

    impl_src = sys_dmabuf_to_impl(src);
    impl_dev = dev_to_impl(dev);

    *dst = NULL;
    if (src->dev == dev) {
        kmpp_loge_f("can not attach buffer %px to same device %s again\n", src, dev->name);
        return rk_nok;
    }

    impl = attach_dmabuf(impl_src, impl_dev->device, __FUNCTION__);
    if (impl) {
        impl->buf.dev = dev;
        impl->buf.device = impl_dev->device;

        *dst = &impl->buf;
        return rk_ok;
    }

    return rk_nok;
}
EXPORT_SYMBOL(sys_dmabuf_attach);

rk_s32 sys_dmabuf_detach(SysDmabuf *dmabuf)
{
    if (dmabuf) {
        SysDmabufImpl *buf = sys_dmabuf_to_impl(dmabuf);

        if (buf->is_attachment) {
            struct device *dev = dmabuf_to_device(dmabuf);

            dma_unmap_sgtable(dev, buf->sgt, DMA_BIDIRECTIONAL, 0);
            sg_free_table(buf->sgt);
            kmpp_free(buf);
            return rk_ok;
        } else {
            kmpp_loge_f("can not detach buffer %px which is not attachment\n", dmabuf);
        }
    }

    return rk_ok;
}
EXPORT_SYMBOL(sys_dmabuf_detach);

rk_s32 sys_dmabuf_alloc_by_device(SysDmabuf **dmabuf, void *device, rk_s64 size, rk_u32 flag)
{
    SysDmabufImpl *buf = NULL;

    if (!dmabuf || !device || !size) {
        kmpp_loge_f("invalid dmabuf %px dev %px size %llx\n", dmabuf, device, size);
        return rk_nok;
    }

    buf = alloc_dmabuf(size, flag, device, __FUNCTION__);
    if (buf) {
        buf->buf.dev = NULL;
        buf->buf.device = device;
        buf->orig_dev = NULL;

        *dmabuf = &buf->buf;
        return rk_ok;
    }

    *dmabuf = NULL;
    return rk_nok;
}
EXPORT_SYMBOL(sys_dmabuf_alloc_by_device);

rk_s32 sys_dmabuf_attach_by_device(SysDmabuf **dst, SysDmabuf *src, void *device)
{
    SysDmabufImpl *impl;
    SysDmabufImpl *impl_src;

    if (!dst || !src || !device) {
        kmpp_loge_f("invalid param dst %px src %px dev %px\n", dst, src, device);
        return rk_nok;
    }

    impl_src = sys_dmabuf_to_impl(src);

    *dst = NULL;
    if (src->device == device) {
        kmpp_loge_f("can not attach buffer %px to same device again\n", src);
        return rk_nok;
    }

    impl = attach_dmabuf(impl_src, (struct device *)device, __FUNCTION__);
    if (impl) {
        impl->buf.dev = NULL;
        impl->buf.device = device;

        *dst = &impl->buf;
        return rk_ok;
    }

    return rk_nok;
}
EXPORT_SYMBOL(sys_dmabuf_attach_by_device);

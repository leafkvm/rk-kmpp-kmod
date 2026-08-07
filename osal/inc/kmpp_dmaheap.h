/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_DMAHEAP_H__
#define __KMPP_DMAHEAP_H__

#include "kmpp_dev.h"

#define KMPP_DMAHEAP_FLAGS_DEFAULT  0x0000
#define KMPP_DMAHEAP_FLAGS_DMA32    0x0001
#define KMPP_DMAHEAP_FLAGS_CACHABLE 0x0002
#define KMPP_DMAHEAP_FLAGS_CONTIG   0x0004
#define KMPP_DMAHEAP_FLAGS_SECURE   0x0008
#define KMPP_DMAHEAP_FLAGS_MASK     ( KMPP_DMAHEAP_FLAGS_DMA32 \
                                    | KMPP_DMAHEAP_FLAGS_CACHABLE \
                                    | KMPP_DMAHEAP_FLAGS_CONTIG \
                                    | KMPP_DMAHEAP_FLAGS_SECURE)

#define KMPP_DMABUF_FLAGS_DUP_MAP   0x0001      // vmap and mmap double size for ring buffer
#define KMPP_DMABUF_FLAGS_DUP_IOVA  0x0002      // map double size iova for hardware access
#define KMPP_DMABUF_FLAGS_MASK      ( KMPP_DMABUF_FLAGS_DUP_MAP \
                                    | KMPP_DMABUF_FLAGS_DUP_IOVA)

typedef void* KmppDmaHeap;
typedef void* KmppDmaBuf;
typedef void* KmppDmaBufDevMap;

/* open default dmaheap with flag */
rk_s32 kmpp_dmaheap_get(KmppDmaHeap *heap, rk_u32 flag, const rk_u8 *caller);
/* open special dmaheap by name with flag */
rk_s32 kmpp_dmaheap_get_by_name(KmppDmaHeap *heap, const rk_u8 *name, rk_u32 flag, const rk_u8 *caller);
/* close dmaheap */
rk_s32 kmpp_dmaheap_put(KmppDmaHeap heap, const rk_u8 *caller);

rk_s32 kmpp_dmabuf_alloc(KmppDmaBuf *buf, KmppDmaHeap heap, rk_s32 size, rk_u32 flag, const rk_u8 *caller);
rk_s32 kmpp_dmabuf_free(KmppDmaBuf buf, const rk_u8 *caller);
rk_s32 kmpp_dmabuf_import_fd(KmppDmaBuf *buf, rk_s32 fd, rk_u32 flag, const rk_u8 *caller);
/* the ctx is struct dma_buf * */
rk_s32 kmpp_dmabuf_import_ctx(KmppDmaBuf *buf, void *ctx, rk_u32 flag, const rk_u8 *caller);

void *kmpp_dmabuf_get_kptr(KmppDmaBuf buf);
rk_u64 kmpp_dmabuf_get_uptr(KmppDmaBuf buf);
rk_s32 kmpp_dmabuf_put_uptr(KmppDmaBuf buf);
rk_s32 kmpp_dmabuf_get_size(KmppDmaBuf buf);
rk_u32 kmpp_dmabuf_get_flag(KmppDmaBuf buf);
rk_s32 kmpp_dmabuf_get_fd(KmppDmaBuf buf);
/* attach dmabuf to device */
rk_s32 kmpp_dmabuf_get_iova(KmppDmaBuf buf, rk_u64 *iova, KmppDev *dev);
/* detach dmabuf frome device */
rk_s32 kmpp_dmabuf_put_iova(KmppDmaBuf buf, rk_u64 iova, KmppDev *dev);
/* attach dmabuf to device */
rk_s32 kmpp_dmabuf_get_iova_by_device(KmppDmaBuf buf, rk_u64 *iova, void *device);
/* detach dmabuf frome device */
rk_s32 kmpp_dmabuf_put_iova_by_device(KmppDmaBuf buf, rk_u64 iova, void *device);

rk_u64 kmpp_invalid_iova(void);

void kmpp_dmabuf_set_priv(KmppDmaBuf buf, void *priv);
void *kmpp_dmabuf_get_priv(KmppDmaBuf buf);

rk_s32 kmpp_dmabuf_flush_for_cpu(KmppDmaBuf buf);
rk_s32 kmpp_dmabuf_flush_for_dev(KmppDmaBuf buf, KmppDev *dev);
rk_s32 kmpp_dmabuf_flush_for_cpu_partial(KmppDmaBuf buf, rk_u32 offset, rk_u32 size);
rk_s32 kmpp_dmabuf_flush_for_dev_partial(KmppDmaBuf buf, KmppDev *dev, rk_u32 offset, rk_u32 size);

#define kmpp_dmaheap_get_f(heap, flag)              kmpp_dmaheap_get(heap, flag, __FUNCTION__)
#define kmpp_dmaheap_put_f(heap)                    kmpp_dmaheap_put(heap, __FUNCTION__)
#define kmpp_dmabuf_alloc_f(buf, heap, size, flag)  kmpp_dmabuf_alloc(buf, heap, size, flag, __FUNCTION__)
#define kmpp_dmabuf_free_f(buf)                     kmpp_dmabuf_free(buf, __FUNCTION__)
#define kmpp_dmabuf_import_fd_f(buf, fd, flag)      kmpp_dmabuf_import_fd(buf, fd, flag, __FUNCTION__)
#define kmpp_dmabuf_import_ctx_f(buf, ctx, flag)    kmpp_dmabuf_import_ctx(buf, ctx, flag, __FUNCTION__)

#endif /* __KMPP_DMAHEAP_H__ */

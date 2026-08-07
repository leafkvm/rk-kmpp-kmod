/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_dmaheap"

#include <linux/version.h>
#include <linux/mman.h>
#include <linux/iommu.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dma-buf-cache.h>
#include <linux/scatterlist.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "mpp_common.h"

#include "kmpp_list.h"
#include "kmpp_dev.h"
#include "kmpp_env.h"
#include "kmpp_log.h"
#include "kmpp_mem.h"
#include "kmpp_atomic.h"
#include "kmpp_dmaheap.h"
#include "kmpp_string.h"
#include "kmpp_spinlock.h"
#include "kmpp_mutex.h"
#include "kmpp_macro.h"
#include "kmpp_delay.h"
#include "kmpp_module.h"
#include "kmpp_osal_id.h"
#include "kmpp_cls.h"

#define MAX_DMAHEAP_NUM                 8
#define CACHE_LINE_SIZE                 (64)
#define KMPP_INVALID_IOVA               (~(dma_addr_t)0)
#define KMPP_INVALID_PHYS               (~(phys_addr_t)0)

#define DMABUF_DBG_FLOW                 (0x00000001)
#define DMABUF_DBG_HEAPS                (0x00000002)
#define DMABUF_DBG_BUF                  (0x00000004)
#define DMABUF_DBG_IOCTL                (0x00000008)
#define DMABUF_DBG_SGT                  (0x00000010)
#define DMABUF_DBG_SGT_CHK              (0x00000020)
#define DMABUF_DBG_DUP_IOVA             (0x00000040)
#define DMABUF_DBG_DUMP                 (0x00000100)
#define DMABUF_DBG_DUP_IOVA_DUMP_PHYS   (0x00000200)

#define dmabuf_dbg(flag, fmt, ...)      kmpp_dbg(kmpp_dmabuf_debug, flag, fmt, ## __VA_ARGS__)
#define dmabuf_dbg_f(flag, fmt, ...)    kmpp_dbg_f(kmpp_dmabuf_debug, flag, fmt, ## __VA_ARGS__)

#define dmabuf_dbg_flow(fmt, ...)       dmabuf_dbg_f(DMABUF_DBG_FLOW, fmt, ## __VA_ARGS__)
#define dmabuf_dbg_heaps(fmt, ...)      dmabuf_dbg(DMABUF_DBG_HEAPS, fmt, ## __VA_ARGS__)
#define dmabuf_dbg_buf(fmt, ...)        dmabuf_dbg(DMABUF_DBG_BUF, fmt, ## __VA_ARGS__)
#define dmabuf_dbg_ioctl(fmt, ...)      dmabuf_dbg(DMABUF_DBG_IOCTL, fmt, ## __VA_ARGS__)
#define dmabuf_dbg_sgt(fmt, ...)        dmabuf_dbg(DMABUF_DBG_SGT, fmt, ## __VA_ARGS__)
#define dmabuf_dbg_sgt_chk(fmt, ...)    dmabuf_dbg(DMABUF_DBG_SGT_CHK, fmt, ## __VA_ARGS__)
#define dmabuf_dbg_dup_iova(fmt, ...)   dmabuf_dbg(DMABUF_DBG_DUP_IOVA, fmt, ## __VA_ARGS__)

#define get_dmaheaps_srv(caller) \
    ({ \
        KmppDmaHeapsSrv *__tmp; \
        if (srv_dmaheaps) { \
            __tmp = srv_dmaheaps; \
        } else { \
            kmpp_err_f("kmpp dmaheaps srv not init at %s : %s\n", __FUNCTION__, caller); \
            __tmp = NULL; \
        } \
        __tmp; \
    })

typedef void *(*HeapFindFunc)(const char *name);
typedef void (*HeapPutFunc)(void *);

typedef struct dma_buf *(*HeapAllocFunc)(void *heap, rk_u32 len, unsigned int fd_flags,
                                         unsigned int heap_flags, const char *name);

typedef struct KmppDmaHeapInfo_t {
    const char          *name;
    const char          *heap_find;
    const char          *heap_put;
    const char          *heap_alloc;
    const char          *heap_names[MAX_DMAHEAP_NUM];
} KmppDmaHeapInfo;

typedef struct KmppDmaHeapImpl_t    KmppDmaHeapImpl;
typedef struct KmppDmaHeaps_t       KmppDmaHeaps;

struct KmppDmaHeapImpl_t {
    /* pointer to global heaps */
    KmppDmaHeaps            *heaps;
    /* handle for (struct dma_heap *) or (struct rk_dma_heap *) */
    void                    *handle;
    /* list for KmppDmaBuf list */
    kmpp_list_head          list_used;
    rk_u32                  heap_id;
    rk_u32                  buf_id;
    rk_s32                  buf_count;
    /* flag value for the heap */
    rk_s32                  index;
    rk_s32                  valid;
    rk_s32                  ref_cnt;
    rk_u32                  flag;
};

struct KmppDmaHeaps_t {
    kmpp_list_head          list;
    KmppDmaHeapInfo         *info;
    KmppSpinlock            *lock;
    HeapFindFunc            find;
    HeapPutFunc             put;
    HeapAllocFunc           alloc;
    KmppDmaHeapImpl         heaps[MAX_DMAHEAP_NUM];
};

typedef struct RemapSlot_t {
    rk_u32                  offset;
    rk_u32                  len;
    phys_addr_t             orig_pa;
    phys_addr_t             new_pa;
} RemapSlot;

typedef struct SgInfo_t {
    dma_addr_t              iova;
    rk_u32                  total;
    rk_u32                  half;
    rk_u32                  slot_back;

    /* slot count */
    rk_u32                  cnt;
    RemapSlot               slots[0];
} SgInfo;

typedef struct RemapSlotTmp_t {
    kmpp_list_head          list;
    rk_u32                  index;
    rk_u32                  offset;
    rk_u32                  len;
    phys_addr_t             orig_pa;
    phys_addr_t             new_pa;
} RemapSlotTmp;

typedef struct RemapList_t {
    rk_u32                  cnt;
    RemapSlot               slot[0];
} RemapList;

typedef struct KmppDmaBufIova_t {
    /* list to list_iova in KmppDmaBufImpl */
    kmpp_list_head          list;
    /* buf - KmppDmaBufImpl pointer */
    void                    *buf;
    KmppDev                 *dev;
    struct device           *device;
    struct dma_buf          *dmabuf;
    struct dma_buf_attachment *attach;
    struct iommu_domain     *domain;
    struct sg_table         *sgt;
    rk_u64                  iova;
    rk_u64                  iova2;
    rk_u32                  flag;
} KmppDmaBufIova;

typedef struct KmppDmaBufImpl_t {
    /* list to list_used in KmppDmaHeapImpl */
    kmpp_list_head          list;
    /* list for list in KmppDmaBufIova */
    kmpp_list_head          list_iova;
    KmppSpinlock            *lock_iova;
    /* pointer to global heaps */
    KmppDmaHeapImpl         *heap;

    struct dma_buf          *dma_buf;
    void                    *priv;

    /* basic info */
    void                    *kptr;
    rk_u64                  uptr;
    rk_u32                  heap_id;
    rk_u32                  buf_id;
    rk_s32                  fd;
    rk_s32                  size;
    rk_s32                  align_size;
    rk_s32                  real_size;
    rk_s32                  map_size;
    rk_u32                  flag;
    struct mm_struct        *mm;

    /* iova dup info: fh -> first half sh -> second half */
    RemapList               *remap_list;
} KmppDmaBufImpl;

typedef struct KmppDmaHeapsSrv_t {
    /* default heaps */
    KmppDmaHeaps            *heaps;
    KmppEnvNode             env;

    /* the reset heaps */
    kmpp_list_head          list_heap;

    rk_u32                  heap_id;
    rk_s32                  heap_cnt;

    /* external buffer for import */
    kmpp_list_head          list_ext_buf;
    rk_u32                  ext_buf_id;
    rk_s32                  ext_buf_cnt;

    /* dummy device for buf attachment of kernel double range address vmap */
    struct device           *dev;
    struct platform_device  *pdev;
} KmppDmaHeapsSrv;

static KmppDmaHeapInfo dmaheap_infos[] = {
    {    /* default system heap */
        .name       = "default dma heap",
        .heap_find  = "dma_heap_find",
        .heap_put   = "dma_heap_put",
        .heap_alloc = "dma_heap_buffer_alloc",
        .heap_names = {
            "system-uncached",          /*                     DEFAULT */
            "system-uncached-dma32",    /*                     DMA32   */
            "system",                   /*          CACHABLE           */
            "system-dma32",             /*          CACHABLE | DMA32   */
            "cma-uncached",             /* CONTIG                      */
            "cma-uncached-dma32",       /* CONTIG            | DMA32   */
            "cma",                      /* CONTIG | CACHABLE           */
            "cma-dma32",                /* CONTIG | CACHABLE | DMA32   */
        },
    },
    {    /* rk dma heap */
        .name       = "rk dma heap",
        .heap_find  = "rk_dma_heap_find",
        .heap_put   = "rk_dma_heap_put",
        .heap_alloc = "rk_dma_heap_buffer_alloc",
        .heap_names = {                 /* cma heap only */
            "rk-dma-heap-cma",
            "rk-dma-heap-cma",
            "rk-dma-heap-cma",
            "rk-dma-heap-cma",
            "rk-dma-heap-cma",
            "rk-dma-heap-cma",
            "rk-dma-heap-cma",
            "rk-dma-heap-cma",
        },
    },
};

static KmppDmaHeapsSrv *srv_dmaheaps = NULL;
static rk_u32 kmpp_dmabuf_debug = 0;

void kmpp_dmabuf_dev_init(KmppDmaHeapsSrv *srv)
{
    struct platform_device *pdev;
    struct platform_device_info dev_info = {
        .name       = "kmpp_dmabuf",
        .id         = PLATFORM_DEVID_NONE,
        .dma_mask   = DMA_BIT_MASK(64),
    };

    pdev = platform_device_register_full(&dev_info);
    if (!IS_ERR(pdev)) {
        dma_set_max_seg_size(&pdev->dev, (unsigned int)DMA_BIT_MASK(64));
    } else {
        kmpp_loge_f("register kmpp_dmabuf platform device failed\n");
    }

    srv->pdev = pdev;
    srv->dev = &pdev->dev;
}

void kmpp_dmabuf_dev_deinit(KmppDmaHeapsSrv *srv)
{
    if (srv) {
        platform_device_unregister(srv->pdev);
        srv->pdev = NULL;
        srv->dev = NULL;
    }
}

static void __kmpp_dmaheap_deinit(KmppDmaHeapsSrv *srv, KmppDmaHeaps *heaps)
{
    KmppDmaHeapInfo *info = heaps->info;
    rk_s32 i;

    kmpp_list_del_init(&heaps->list);

    for (i = MAX_DMAHEAP_NUM - 1; i >= 0; i--) {
        KmppDmaHeapImpl *impl = &heaps->heaps[i];
        KmppDmaBufImpl *buf, *n;
        rk_s32 once = 1;

        kmpp_list_for_each_entry_safe(buf, n, &impl->list_used, KmppDmaBufImpl, list) {
            if (once) {
                kmpp_logi("releasing leaked used dmabuf:\n", buf);
                once = 0;
            }
            kmpp_list_del_init(&buf->list);

            kmpp_logi("buf %d:%d - %#px\n", buf->heap_id, buf->buf_id, buf);
            kmpp_dmabuf_free_f(buf);
        }

        if (impl->valid && heaps->put)
            heaps->put(impl->handle);

        impl->handle = NULL;
        impl->valid = 0;
        srv->heap_cnt--;
    }

    if (info) {
        if (heaps->find)
            __symbol_put(info->heap_find);
        if (heaps->put)
            __symbol_put(info->heap_put);
        if (heaps->alloc)
            __symbol_put(info->heap_alloc);
    }

    kmpp_spinlock_deinit(&heaps->lock);
    kmpp_free(heaps);
}

static rk_s32 kmpp_dmaheap_deinit(void)
{
    KmppDmaHeapsSrv *srv = srv_dmaheaps;
    KmppDmaHeaps *heaps, *n;

    if (!srv)
        return rk_ok;

    if (srv->heaps) {
        __kmpp_dmaheap_deinit(srv, srv->heaps);
        srv->heaps = NULL;
    }

    kmpp_list_for_each_entry_safe(heaps, n, &srv->list_heap, KmppDmaHeaps, list) {
        __kmpp_dmaheap_deinit(srv, heaps);
    }

    if (srv->heap_cnt)
        kmpp_loge_f("found %d heaps not deinited\n", srv->heap_cnt);

    kmpp_dmabuf_dev_deinit(srv);
    kmpp_env_del(kmpp_env_debug(), srv->env);
    kmpp_free(srv);

    srv_dmaheaps = NULL;

    return rk_ok;
}

static rk_s32 kmpp_dmaheap_init(void)
{
    KmppDmaHeapsSrv *srv = srv_dmaheaps;
    rk_s32 lock_size = kmpp_spinlock_size();
    rk_s32 i;

    if (srv) {
        kmpp_dmaheap_deinit();
        srv = NULL;
    }

    srv = kmpp_calloc_atomic(sizeof(KmppDmaHeapsSrv));
    if (!srv) {
        kmpp_loge_f("create dmaheaps srv failed\n");
        return rk_nok;
    }

    srv_dmaheaps = srv;

    {
        KmppEnvInfo info;

        info.type = KmppEnv_u32;
        info.readonly = 0;
        info.name = "kmpp_dmabuf_debug";
        info.val = &kmpp_dmabuf_debug;
        info.env_show = NULL;

        kmpp_env_add(kmpp_env_debug(), &srv->env, &info);
    }

    KMPP_INIT_LIST_HEAD(&srv->list_heap);
    KMPP_INIT_LIST_HEAD(&srv->list_ext_buf);

    for (i = 0; i < sizeof(dmaheap_infos) / sizeof(dmaheap_infos[0]); i++) {
        KmppDmaHeapInfo *info = &dmaheap_infos[i];
        HeapFindFunc find = (HeapFindFunc)__symbol_get(info->heap_find);
        HeapPutFunc put = (HeapPutFunc)__symbol_get(info->heap_put);
        HeapAllocFunc alloc = (HeapAllocFunc)__symbol_get(info->heap_alloc);

        /* NOTE: put may not exist */
        if (find && alloc) {
            KmppDmaHeaps *heaps = NULL;
            void *prev_valid = NULL;
            rk_s32 j;
            rk_u8 find_valid = 0;
            rk_u32 heap_flag = 0;
            rk_u32 retry_cnt = 0;

            heaps = (KmppDmaHeaps *)kmpp_calloc_atomic(sizeof(KmppDmaHeaps) + lock_size);
            if (!heaps) {
                kmpp_loge_f("create heap impl failed\n");
                return rk_nok;
            }

            kmpp_spinlock_assign(&heaps->lock, (void *)(heaps + 1), lock_size);

            heaps->info = info;
            heaps->find = find;
            heaps->put = put;
            heaps->alloc = alloc;

            kmpp_logi("probing %s\n", info->name);

            for (j = 0; j < MAX_DMAHEAP_NUM; j++) {
                KmppDmaHeapImpl *impl = &heaps->heaps[j];
                const char *name = info->heap_names[j];
                void *curr = NULL;

                /* retry to find dma heap only first one */
                do {
                    curr = find(name);
                    if (curr)
                        break;
                    if (j || (retry_cnt++ > 3))
                        break;
                    kmpp_msleep(1);
                } while (1);

                if (curr && prev_valid != curr) {
                    kmpp_logi("find heap %s\n", name);
                    prev_valid = curr;
                    impl->valid = 1;
                    find_valid = 1;
                    heap_flag = j;
                } else {
                    /* use previous valid heap */
                    curr = prev_valid;
                    impl->valid = 0;
                }

                impl->heaps = heaps;
                impl->handle = curr;
                impl->index = j;
                impl->flag = heap_flag;
                KMPP_INIT_LIST_HEAD(&impl->list_used);
                impl->heap_id = srv->heap_id++;
                srv->heap_cnt++;
            }

            if (!srv->heaps && find_valid) {
                srv->heaps = heaps;
                kmpp_logi("set %s as default\n", info->name);
            } else {
                kmpp_logi("add %s\n", info->name);
            }
            kmpp_list_add_tail(&heaps->list, &srv->list_heap);
        } else {
            kmpp_logi("probe heap %s failed\n", info->name);
        }
    }

    kmpp_dmabuf_dev_init(srv);

    return rk_ok;
}

KMPP_MODULE_ADD(kmpp_osal, KMPP_OSAL_DMAHEAP, dmaheap,
                kmpp_dmaheap_init, kmpp_dmaheap_deinit);

rk_s32 kmpp_dmaheap_get(KmppDmaHeap *heap, rk_u32 flag, const rk_u8 *caller)
{
    KmppDmaHeapsSrv *srv = get_dmaheaps_srv(caller);
    KmppDmaHeapImpl *impl;
    KmppDmaHeaps *heaps;
    rk_s32 old;

    if (!srv)
        return rk_nok;

    if (!heap) {
        kmpp_loge_f("invalid NULL heap\n");
        return rk_nok;
    }

    if (flag >= MAX_DMAHEAP_NUM) {
        kmpp_loge_f("suppoort dmaheap flag %x\n", flag);
        *heap = NULL;
        return rk_nok;
    }

    heaps = srv->heaps;
    if (!heaps) {
        kmpp_loge_f("found NULL default dmaheaps\n");
        return rk_nok;
    }

    impl = &heaps->heaps[flag];
    if (!impl->handle) {
        kmpp_loge_f("heap %s flag %x invalid NULL heap handle at %s\n",
                    impl->heaps->info->name, flag, caller);
        *heap = NULL;
        return rk_nok;
    }

    old = KMPP_FETCH_ADD(&impl->ref_cnt, 1);
    *heap = impl;

    dmabuf_dbg_heaps("heap %s - %d ref %d -> %d\n", heaps->info->name,
                     impl->index, old, impl->ref_cnt);

    return rk_ok;
}

rk_s32 kmpp_dmaheap_get_by_name(KmppDmaHeap *heap, const rk_u8 *name, rk_u32 flag, const rk_u8 *caller)
{
    KmppDmaHeapsSrv *srv = get_dmaheaps_srv(caller);
    KmppDmaHeaps *heaps, *n;

    if (!heap) {
        kmpp_loge_f("invalid NULL heap\n");
        return rk_nok;
    }

    *heap = NULL;

    if (flag >= MAX_DMAHEAP_NUM) {
        kmpp_loge_f("suppoort dmaheap flag %x\n", flag);
        return rk_nok;
    }

    kmpp_list_for_each_entry_safe(heaps, n, &srv->list_heap, KmppDmaHeaps, list) {
        dmabuf_dbg_heaps("checking %s for %s\n", heaps->info->name, name);
        if (!kmpp_strcmp(heaps->info->name, name)) {
            KmppDmaHeapImpl *impl = &heaps->heaps[flag];
            rk_s32 old;

            old = KMPP_FETCH_ADD(&impl->ref_cnt, 1);
            *heap = impl;

            dmabuf_dbg_heaps("heap %s - %d ref %d -> %d\n", impl->heaps->info->name,
                             flag, old, impl->ref_cnt);

            return rk_ok;
        }
    }

    return rk_nok;
}

rk_s32 kmpp_dmaheap_put(KmppDmaHeap heap, const rk_u8 *caller)
{
    KmppDmaHeapsSrv *srv = get_dmaheaps_srv(caller);
    KmppDmaHeapImpl *impl;
    KmppDmaHeaps *heaps;
    rk_s32 old;

    if (!srv)
        return rk_nok;

    if (!heap) {
        kmpp_loge_f("invalid NULL heap\n");
        return rk_nok;
    }

    heaps = srv->heaps;
    if (!heaps) {
        kmpp_loge_f("found NULL default dmaheaps\n");
        return rk_nok;
    }

    impl = (KmppDmaHeapImpl *)heap;
    old = KMPP_FETCH_SUB(&impl->ref_cnt, 1);

    dmabuf_dbg_heaps("heap %s - %d ref %d -> %d\n", impl->heaps->info->name,
                     impl->index, old, impl->ref_cnt);

    if (impl->ref_cnt < 0)
        kmpp_loge_f("invalid ref_cnt %d at %s\n", impl->ref_cnt, caller);

    return rk_ok;
}

static void *dmabuf_double_vmap(KmppDmaBufImpl *impl, rk_u32 cacheable, const rk_u8 *caller)
{
    KmppDmaHeapsSrv *srv = get_dmaheaps_srv(caller);
    struct dma_buf *dma_buf = impl->dma_buf;
    struct dma_buf_attachment *attach;
    void *vaddr = NULL;

    if (!srv)
        return NULL;

    attach = dma_buf_attach(dma_buf, srv->dev);
    if (IS_ERR_OR_NULL(attach)) {
        kmpp_loge_f("failed to attach dma_buf %px to %px at %s\n",
                    dma_buf, srv->dev, caller);
    } else {
        struct sg_table *sgt;

        sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
        if (IS_ERR(sgt)) {
            kmpp_loge_f("dma_buf_map_attachment failed ret %d at %s\n",
                        PTR_ERR(sgt), caller);
        } else {
            struct sg_table *table = sgt;
            rk_s32 npages = impl->align_size >> PAGE_SHIFT;
            struct page **pages = kmpp_calloc(sizeof(struct page *) * npages * 2);
            struct page **tmp = pages;
            struct sg_page_iter piter;
            struct page *page;
            pgprot_t pgprot = PAGE_KERNEL;

            if (!cacheable)
                pgprot = pgprot_writecombine(PAGE_KERNEL);

            for_each_sgtable_page(table, &piter, 0) {
                if (tmp - pages >= npages)
                    break;

                page = sg_page_iter_page(&piter);
                *(tmp + npages) = page;
                *(tmp + 0) = page;
                tmp++;
            }

            vaddr = vmap(pages, 2 * npages, VM_MAP, pgprot);

            dma_buf_unmap_attachment(attach, sgt, DMA_BIDIRECTIONAL);
            kmpp_free(pages);
        }

        dma_buf_detach(dma_buf, attach);
    }

    return vaddr;
}

static void dump_phys(void *domain, rk_ul iova, rk_u32 len, const rk_u8 *caller)
{
    rk_u32 offset;

    for (offset = 0; offset < len; offset += PAGE_SIZE) {
        phys_addr_t phys = iommu_iova_to_phys(domain, iova + offset);
        kmpp_logi("%s: iova %08x phys %pa\n", caller, iova + offset, &phys);
    }
}

static SgInfo *get_sg_info(void *domain, struct sg_table *sgt)
{
    struct scatterlist *first_sg = sgt->sgl;
    struct scatterlist *sg;
    SgInfo *info;
    rk_u32 slot_total = sgt->orig_nents;
    rk_u32 total = 0;
    rk_u32 count = 0;
    rk_u32 i;

    info = kmpp_calloc(sizeof(*info) + sizeof(RemapSlot) * slot_total);
    if (!info) {
        kmpp_loge_f("alloc %d remap sgt info failed\n", slot_total);
        return NULL;
    }

    dmabuf_dbg_sgt("======== generate sg info ========\n");

    info->iova = sg_dma_address(first_sg);

    for_each_sg(first_sg, sg, slot_total, i) {
        RemapSlot *s = &info->slots[count];
        dma_addr_t iova = info->iova + total;
        phys_addr_t pa = iommu_iova_to_phys(domain, iova);
        rk_ul len = sg->length;
        const rk_u8 *str;

        /* record first node */
        if (i == 0) {
            s->offset = total;
            s->len = len;
            s->orig_pa = pa;
            s->new_pa = pa + len;
            str = "first";
        } else {
            if (pa == s->new_pa) {
                s->len += len;
                str = "merged";
            } else {
                s = &info->slots[++count];
                s->offset = total;
                s->len = len;
                s->orig_pa = pa;
                s->new_pa = pa + len;
                str = "new";
            }
        }

        dmabuf_dbg_sgt("sg %2d offset %8x iova %pad phys %pa len %6x -> slot %2d pa %pa len %6x %s\n",
                       i, total, &iova, &pa, len, count, &s->orig_pa, s->len, str);

        total += len;
    }

    info->cnt = count + 1;
    info->total = total;
    info->half = total / 2;

    for (i = 0; i < info->cnt; i++) {
        RemapSlot *s = &info->slots[i];

        if (s->offset + s->len > info->half) {
            info->slot_back = i;
            break;
        }
    }

    return info;
}

static RemapList *get_remap_list_from_sg_info(SgInfo *info)
{
    RemapSlot *t = &info->slots[0];
    RemapSlot *s = &info->slots[info->slot_back];
    RemapSlotTmp *slot, *n;
    KMPP_LIST_HEAD(list);
    RemapList *ret;
    rk_u32 front_base = t->offset;
    rk_u32 front_start = front_base;
    rk_u32 front_len = t->len;
    rk_u32 back_base = s->offset;
    rk_u32 back_size = s->len;
    rk_u32 back_start = back_base > info->half ? back_base : info->half;
    rk_u32 back_len = back_base + back_size - back_start;
    rk_u32 overlay_len = KMPP_MIN(front_len, back_len);
    rk_u32 count = 0;
    rk_u32 i, j;

    dmabuf_dbg_sgt("======== generate remap list fron %d info ========\n", info->cnt);

    /* scan and generate remap list */
    i = info->slot_back;
    j = 0;
    do {
        slot = kmpp_calloc(sizeof(RemapSlotTmp));
        if (!slot) {
            kmpp_loge_f("alloc remap slot %d failed\n", count);
            goto failed;
        }

        KMPP_INIT_LIST_HEAD(&slot->list);
        slot->index = count++;
        slot->offset = back_start;
        slot->len = overlay_len;
        slot->orig_pa = s->orig_pa + back_start - back_base;
        slot->new_pa = t->orig_pa + front_start - front_base;

        dmabuf_dbg_sgt("slot %d remap offset %8x len %8x back pa %pa -> front pa %pa\n",
                       slot->index, slot->offset, slot->len, &slot->orig_pa, &slot->new_pa);

        kmpp_list_add_tail(&slot->list, &list);

        front_len -= overlay_len;
        back_len -= overlay_len;

        /* check update front */
        if (!front_len) {
            if (++j >= info->cnt)
                break;

            t = &info->slots[j];
            front_base = t->offset;
            front_start = t->offset;
            front_len = t->len;
        } else {
            front_start += overlay_len;
        }

        /* check update back */
        if (!back_len) {
            if (++i >= info->cnt)
                break;

            s = &info->slots[i];
            back_base = s->offset;
            back_start = s->offset;
            back_len = s->len;
        } else {
            back_start += overlay_len;
        }

        overlay_len = KMPP_MIN(front_len, back_len);
    } while (1);

    ret = kmpp_calloc(sizeof(RemapList) + sizeof(RemapSlot) * count);
    if (ret) {
        ret->cnt = count;
        count = 0;
        kmpp_list_for_each_entry_safe(slot, n, &list, RemapSlotTmp, list) {
            RemapSlot *s = &ret->slot[count];

            s->offset = slot->offset;
            s->len = slot->len;
            s->orig_pa = slot->orig_pa;
            s->new_pa = slot->new_pa;
            count++;
        }
    } else {
        kmpp_loge_f("alloc remap list %d failed\n", count);
    }

failed:
    kmpp_list_for_each_entry_safe(slot, n, &list, RemapSlotTmp, list) {
        kmpp_list_del_init(&slot->list);
        kmpp_free(slot);
    }

    return ret;
}

static RemapList *get_golden_list(void *domain, struct sg_table *sgt, rk_u32 total, rk_u32 half)
{
    KMPP_LIST_HEAD(list);
    RemapList *ret = NULL;
    dma_addr_t front_iova = sg_dma_address(sgt->sgl);
    dma_addr_t back_iova = front_iova + half;
    phys_addr_t front_phys_prev = KMPP_INVALID_PHYS;
    phys_addr_t back_phys_prev = KMPP_INVALID_PHYS;
    RemapSlotTmp *slot = NULL;
    RemapSlotTmp *n;
    rk_u32 offset;
    rk_u32 step = PAGE_SIZE;
    rk_u32 count = 0;

    for (offset = 0; offset < half; offset += step) {
        phys_addr_t front_phys = iommu_iova_to_phys(domain, front_iova + offset);
        phys_addr_t back_phys = iommu_iova_to_phys(domain, back_iova + offset);

        if (front_phys != front_phys_prev + step || back_phys != back_phys_prev + step) {
            slot = kmpp_calloc(sizeof(RemapSlotTmp));
            if (!slot) {
                kmpp_loge_f("alloc remap slot %d failed\n", count);
                goto failed;
            }

            KMPP_INIT_LIST_HEAD(&slot->list);
            slot->index = count++;
            slot->offset = half + offset;
            slot->len = step;
            slot->orig_pa = back_phys;
            slot->new_pa = front_phys;

            kmpp_list_add_tail(&slot->list, &list);
        } else {
            slot->len += step;
        }

        front_phys_prev = front_phys;
        back_phys_prev = back_phys;
    }

    ret = kmpp_calloc(sizeof(RemapList) + sizeof(RemapSlot) * count);
    if (!ret) {
        kmpp_loge_f("alloc remap list %d failed\n", count);
        goto failed;
    }

    kmpp_logi("======== generate golden list by iommu_iova_to_phys ========\n");

    ret->cnt = count;
    count = 0;
    kmpp_list_for_each_entry_safe(slot, n, &list, RemapSlotTmp, list) {
        RemapSlot *s = &ret->slot[count];

        s->offset = slot->offset;
        s->len = slot->len;
        s->orig_pa = slot->orig_pa;
        s->new_pa = slot->new_pa;

        kmpp_logi("slot %d remap offset %8x len %8x back pa %pa -> front pa %pa\n",
                  count, s->offset, s->len, &s->orig_pa, &s->new_pa);
        count++;
    }

failed:
    kmpp_list_for_each_entry_safe(slot, n, &list, RemapSlotTmp, list) {
        kmpp_list_del_init(&slot->list);
        kmpp_free(slot);
    }

    return ret;
}

static rk_s32 check_remap_list(RemapList *list, RemapList *golden)
{
    rk_u32 check_cnt = KMPP_MIN(golden->cnt, list->cnt);
    rk_s32 ret = rk_ok;
    rk_u32 i;

    if (golden->cnt != list->cnt) {
        kmpp_logi("remap slot count mismatch %d / %d\n", list->cnt, golden->cnt);
        ret = rk_nok;
    }

    for (i = 0; i < check_cnt; i++) {
        RemapSlot *s = &list->slot[i];
        RemapSlot *t = &golden->slot[i];

        if (s->offset != t->offset || s->len != t->len ||
            s->orig_pa != t->orig_pa || s->new_pa != t->new_pa) {
            kmpp_logi("remap slot %2d mismatch offset %8x/%8x len %6x/%6x pa %pa/%pa -> pa %pa/%pa\n",
                      i, s->offset, t->offset, s->len, t->len,
                      &s->orig_pa, &t->orig_pa, &s->new_pa, &t->new_pa);
            ret = rk_nok;
        }
    }

    kmpp_logi("======== check remap list %s ========\n", ret ? "failed" : "success");

    return ret;
}


static RemapList *scan_sgt(void *domain, struct sg_table *sgt)
{
    SgInfo *info = get_sg_info(domain, sgt);
    RemapList *list;

    if (!info)
        return NULL;

    list = get_remap_list_from_sg_info(info);
    if (!list) {
        kmpp_free(info);
        return NULL;
    }

    if (kmpp_dmabuf_debug & DMABUF_DBG_SGT_CHK) {
        RemapList *golden = get_golden_list(domain, sgt, info->total, info->half);

        if (golden) {
            check_remap_list(list, golden);
            kmpp_free(golden);
        }
    }

    kmpp_free(info);

    return list;
}

static rk_s32 iova_dup(KmppDmaBufIova *iova, KmppDmaBufImpl *buf)
{
    RemapList *list = buf->remap_list;
    dma_addr_t base = iova->iova;
    rk_ul base_iova = (rk_ul)iova->iova;
    rk_u32 i;

    if (kmpp_dmabuf_debug & DMABUF_DBG_DUP_IOVA_DUMP_PHYS)
        dump_phys(iova->domain, base_iova, buf->real_size, "before dup");

    for (i = 0; i < list->cnt; i++) {
        RemapSlot *s = &list->slot[i];
        dma_addr_t slot_iova = base + s->offset;
        rk_ul map_iova = base_iova + s->offset;
        rk_s32 ret;

        dmabuf_dbg_dup_iova("dmabuf %d:%d dup iova %2d [%lx:%x] - [%pad:%x] remap pa %pa -> %pa\n",
                            buf->heap_id, buf->buf_id, i, base_iova, s->offset, &slot_iova, s->len,
                            &s->orig_pa, &s->new_pa);

        ret = iommu_unmap(iova->domain, map_iova, s->len);
        ret = iommu_map_atomic(iova->domain, map_iova, s->new_pa, s->len, IOMMU_READ|IOMMU_WRITE);
    }

    if (kmpp_dmabuf_debug & DMABUF_DBG_DUP_IOVA_DUMP_PHYS)
        dump_phys(iova->domain, base_iova, buf->real_size, "after dup");

    return rk_ok;
}

static rk_s32 iova_undup(KmppDmaBufIova *iova)
{
    KmppDmaBufImpl *buf = (KmppDmaBufImpl *)iova->buf;
    RemapList *list = buf->remap_list;
    dma_addr_t base = iova->iova;
    rk_ul base_iova = (rk_ul)iova->iova;
    rk_u32 i;

    if (kmpp_dmabuf_debug & DMABUF_DBG_DUP_IOVA_DUMP_PHYS)
        dump_phys(iova->domain, base_iova, buf->real_size, "before undup");

    for (i = 0; i < list->cnt; i++) {
        RemapSlot *s = &list->slot[i];
        dma_addr_t slot_iova = base + s->offset;
        rk_ul map_iova = base_iova + s->offset;
        rk_s32 ret;

        dmabuf_dbg_dup_iova("dmabuf %d:%d fix iova %2d [%lx:%x] - [%pad:%x] remap pa %pa -> %pa\n",
                            buf->heap_id, buf->buf_id, i, base_iova, s->offset, &slot_iova, s->len,
                            &s->new_pa, &s->orig_pa);

        ret = iommu_unmap(iova->domain, map_iova, s->len);
        ret = iommu_map_atomic(iova->domain, map_iova, s->orig_pa, s->len, IOMMU_READ|IOMMU_WRITE);
    }

    if (kmpp_dmabuf_debug & DMABUF_DBG_DUP_IOVA_DUMP_PHYS)
        dump_phys(iova->domain, base_iova, buf->real_size, "after undup");

    return rk_ok;
}

static KmppDmaBufIova *attach_iova(KmppDmaBuf buf, KmppDev *dev, struct device *device, const rk_u8 *caller)
{
    KmppDmaBufImpl *impl = (KmppDmaBufImpl *)buf;
    KmppDmaBufIova *node = NULL;
    struct dma_buf_attachment *attach;
    struct dma_buf *dmabuf;
    struct sg_table *sgt;

    node = kmpp_calloc(sizeof(*node));
    if (!node) {
        kmpp_loge_f("create buffer iova node failed at\n", caller);
        return NULL;
    }

    dmabuf = impl->dma_buf;

    node->buf = buf;
    node->dev = dev;
    node->device = device;
    node->dmabuf = dmabuf;
    node->flag = impl->flag;
    {
        KmppDevData *data = dev_get_data(device);

        node->domain = data ? data->domain : NULL;
    }
    //node->domain = iommu_get_domain_for_dev(device);

    attach = dma_buf_attach(dmabuf, device);
    if (IS_ERR_OR_NULL(attach)) {
        kmpp_loge_f("dma_buf_attach dmabuf %px:%px failed ret %d at %s\n",
                    buf, dmabuf, PTR_ERR(attach), caller);
        goto failed;
    }

    sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
    if (IS_ERR_OR_NULL(sgt)) {
        kmpp_loge_f("dma_buf_map_attachment dmabuf %px:%x failed ret %d at %s\n",
                    buf, dmabuf, PTR_ERR(sgt), caller);
        goto failed;
    }

    node->attach = attach;
    node->sgt = sgt;
    node->iova = sg_dma_address(sgt->sgl);

    if (impl->flag & KMPP_DMABUF_FLAGS_DUP_IOVA) {
        if (!impl->remap_list)
            impl->remap_list = scan_sgt(node->domain, sgt);

        iova_dup(node, impl);
        node->iova2 = node->iova + impl->align_size;
    }

    KMPP_INIT_LIST_HEAD(&node->list);
    kmpp_spin_lock(impl->lock_iova);
    kmpp_list_add_tail(&node->list, &impl->list_iova);
    kmpp_spin_unlock(impl->lock_iova);

    return node;

failed:
    if (dmabuf && attach) {
        dma_buf_detach(dmabuf, attach);
        attach = NULL;
    }

    kmpp_free(node);
    return NULL;
}

static void detach_iova(KmppDmaBufIova *node)
{
    if (node->flag & KMPP_DMABUF_FLAGS_DUP_IOVA)
        iova_undup(node);

    dma_buf_unmap_attachment(node->attach, node->sgt, DMA_BIDIRECTIONAL);
    dma_buf_detach(node->dmabuf, node->attach);
    kmpp_free(node);
}

#define attach_iova_f(buf, dev, device) attach_iova(buf, dev, device, __FUNCTION__)

rk_s32 kmpp_dmabuf_alloc(KmppDmaBuf *buf, KmppDmaHeap heap, rk_s32 size, rk_u32 flag, const rk_u8 *caller)
{
    KmppDmaHeapImpl *impl_heap = (KmppDmaHeapImpl *)heap;
    KmppDmaBufImpl *impl;
    KmppDmaHeaps *heaps;
    HeapAllocFunc func;
    struct dma_buf *dma_buf;
    rk_s32 lock_size;
    rk_s32 align_size;
    rk_s32 real_size;
    rk_s32 map_size;

    if (!buf || !heap || size <= 0 || flag >= MAX_DMAHEAP_NUM) {
        kmpp_loge_f("invalid buf %px heap %px size %d flag %x at %s\n",
                    buf, heap, size, flag, caller);
        return rk_nok;
    }

    if (!impl_heap->heaps || !impl_heap->heaps->alloc) {
        kmpp_loge_f("invalid heaps %px at %s\n", impl_heap->heaps, caller);
        return rk_nok;
    }

    *buf = NULL;
    lock_size = kmpp_spinlock_size();
    impl = kmpp_calloc(sizeof(KmppDmaBufImpl) + lock_size);
    if (!impl) {
        kmpp_loge_f("failed to create KmppDmaBufImpl\n");
        return rk_nok;
    }

    heaps = impl_heap->heaps;
    func = impl_heap->heaps->alloc;
    align_size = PAGE_ALIGN(size);
    real_size = align_size;
    map_size = align_size;
    if (flag & KMPP_DMABUF_FLAGS_DUP_IOVA)
        real_size *= 2;
    /* dup map and dup iova will double the map_size */
    if (flag & (KMPP_DMABUF_FLAGS_DUP_MAP | KMPP_DMABUF_FLAGS_DUP_IOVA))
        map_size *= 2;

    dma_buf = (struct dma_buf *)func(impl_heap->handle, real_size, O_CLOEXEC | O_RDWR, 0, "kmpp");
    if (IS_ERR_OR_NULL(dma_buf)) {
        kmpp_loge_f("failed to alloc dma_buf %d(%d) bytes ret %d\n", size, real_size, PTR_ERR(dma_buf));
        kmpp_free(impl);
        return rk_nok;
    }

    KMPP_INIT_LIST_HEAD(&impl->list);
    KMPP_INIT_LIST_HEAD(&impl->list_iova);
    kmpp_spinlock_assign(&impl->lock_iova, impl + 1, lock_size);

    impl->heap = impl_heap;
    impl->dma_buf = dma_buf;
    impl->size = size;
    impl->align_size = align_size;
    impl->real_size = real_size;
    impl->map_size = map_size;
    impl->flag = flag;
    impl->fd = -1;
    impl->mm = NULL;

    kmpp_spin_lock(heaps->lock);
    kmpp_list_add_tail(&impl->list, &impl_heap->list_used);
    impl->heap_id = impl_heap->heap_id;
    impl->buf_id = impl_heap->buf_id++;
    impl_heap->buf_count++;
    kmpp_spin_unlock(heaps->lock);

    dmabuf_dbg_buf("dmabuf %d:%d size %d (%d:%d) flag %x alloc\n", impl->heap_id,
                   impl->buf_id, size, impl->align_size, impl->real_size, impl->flag);

    *buf = impl;

    return rk_ok;
}

rk_s32 kmpp_dmabuf_free(KmppDmaBuf buf, const rk_u8 *caller)
{
    KmppDmaHeapsSrv *srv = get_dmaheaps_srv(caller);
    KmppDmaBufImpl *impl = (KmppDmaBufImpl *)buf;

    if (!srv)
        return rk_nok;

    if (!impl) {
        kmpp_loge_f("invalid NULL buf\n");
        return rk_nok;
    }

    if (impl->heap) {
        KmppDmaHeapImpl *heap = impl->heap;
        KmppDmaHeaps *heaps = heap->heaps;

        kmpp_spin_lock(heaps->lock);
        kmpp_list_del_init(&impl->list);
        heap->buf_count--;
        kmpp_spin_unlock(heaps->lock);
    } else {
        KmppDmaHeaps *heaps = srv->heaps;

        kmpp_spin_lock(heaps->lock);
        kmpp_list_del_init(&impl->list);
        srv->ext_buf_cnt--;
        kmpp_spin_unlock(heaps->lock);
    }

    {   /* remove all iova attachment */
        KmppDmaBufIova *iova, *n;
        KMPP_LIST_HEAD(list_tmp);

        kmpp_spin_lock(impl->lock_iova);
        kmpp_list_for_each_entry_safe(iova, n, &impl->list_iova, KmppDmaBufIova, list) {
            kmpp_list_move_tail(&iova->list, &list_tmp);
        }
        kmpp_spin_unlock(impl->lock_iova);

        kmpp_list_for_each_entry_safe(iova, n, &list_tmp, KmppDmaBufIova, list) {
            kmpp_list_del_init(&iova->list);
            detach_iova(iova);
        }
    }

    if (impl->kptr) {
        if (impl->map_size != impl->align_size) {
            vunmap(impl->kptr);
        } else {
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 01, 0)
            dma_buf_vunmap(impl->dma_buf, impl->kptr);
#else
            struct iosys_map map = IOSYS_MAP_INIT_VADDR(impl->kptr);

            dma_buf_vunmap(impl->dma_buf, &map);
#endif
        }
        impl->kptr = NULL;
    }

    /*
     * If the current process killed, the current mm will be recycle first,
     * so it does not need to vm_mnunmap uaddr here
     */
    if (impl->uptr) {
        void *mm = kmpp_get_curr_mm();

        if (mm && mm == impl->mm) {
            vm_munmap((rk_ul)impl->uptr, impl->map_size);
            impl->uptr = 0;
            impl->mm = NULL;
        }

        kmpp_put_mm(mm);
    }

    if (impl->dma_buf) {
        dma_buf_put(impl->dma_buf);
        impl->dma_buf = NULL;
    }

    dmabuf_dbg_buf("dmabuf %d:%d size %d (%d:%d) flag %x free\n", impl->heap_id,
                   impl->buf_id, impl->size, impl->align_size, impl->real_size, impl->flag);

    kmpp_spinlock_deinit(&impl->lock_iova);
    kmpp_free(impl->remap_list);
    kmpp_free(impl);

    return rk_ok;
}

rk_s32 kmpp_dmabuf_import_fd(KmppDmaBuf *buf, rk_s32 fd, rk_u32 flag, const rk_u8 *caller)
{
    KmppDmaHeapsSrv *srv = get_dmaheaps_srv(caller);
    struct dma_buf *dma_buf;
    KmppDmaBufImpl *impl;
    rk_s32 lock_size;

    if (!srv)
        return rk_nok;

    if (!buf || fd < 0) {
        kmpp_loge_f("invalid buf %px fd %d at %s\n", buf, fd, caller);
        return rk_nok;
    }

    *buf = NULL;
    dma_buf = dma_buf_get(fd);
    if (IS_ERR_OR_NULL(dma_buf)) {
        kmpp_loge_f("failed to dma_buf_get fd %d ret %d at %s\n",
                    fd, PTR_ERR(dma_buf), caller);
        return rk_nok;
    }

    lock_size = kmpp_spinlock_size();
    impl = kmpp_calloc(sizeof(KmppDmaBufImpl) + lock_size);
    if (!impl) {
        kmpp_loge_f("failed to create KmppDmaBufImpl\n");
        dma_buf_put(dma_buf);
        return rk_nok;
    }

    KMPP_INIT_LIST_HEAD(&impl->list);
    KMPP_INIT_LIST_HEAD(&impl->list_iova);
    kmpp_spinlock_assign(&impl->lock_iova, impl + 1, lock_size);
    impl->dma_buf = dma_buf;
    impl->fd = fd;
    impl->size = dma_buf->size;
    impl->align_size = PAGE_ALIGN(dma_buf->size);
    impl->real_size = impl->align_size;
    impl->flag = flag;

    if (srv->heaps) {
        KmppDmaHeaps *heaps = srv->heaps;

        kmpp_spin_lock(heaps->lock);
        kmpp_list_add_tail(&impl->list, &srv->list_ext_buf);
        srv->ext_buf_cnt++;
        impl->heap_id = (rk_u32)-1;
        impl->buf_id = srv->ext_buf_id++;
        kmpp_spin_unlock(heaps->lock);
    }

    dmabuf_dbg_buf("dmabuf %d:%d size %d flag %x import by fd %d\n",
                   impl->heap_id, impl->buf_id, impl->size, impl->flag, impl->fd);

    *buf = impl;

    return rk_ok;
}

rk_s32 kmpp_dmabuf_import_ctx(KmppDmaBuf *buf, void *ctx, rk_u32 flag, const rk_u8 *caller)
{
    KmppDmaHeapsSrv *srv = get_dmaheaps_srv(caller);
    struct dma_buf *dma_buf = (struct dma_buf *)ctx;
    KmppDmaBufImpl *impl;
    rk_s32 lock_size;

    if (!srv)
        return rk_nok;

    if (!buf || !ctx) {
        kmpp_loge_f("invalid buf %px ctx %px at %s\n", buf, ctx, caller);
        return rk_nok;
    }

    *buf = NULL;
    lock_size = kmpp_spinlock_size();
    impl = kmpp_calloc_atomic(sizeof(KmppDmaBufImpl) + lock_size);
    if (!impl) {
        kmpp_loge_f("failed to create KmppDmaBufImpl\n");
        return rk_nok;
    }

    KMPP_INIT_LIST_HEAD(&impl->list);
    KMPP_INIT_LIST_HEAD(&impl->list_iova);
    kmpp_spinlock_assign(&impl->lock_iova, impl + 1, lock_size);
    impl->dma_buf = dma_buf;
    impl->fd = -1;
    impl->size = dma_buf->size;
    impl->align_size = PAGE_ALIGN(dma_buf->size);
    impl->real_size = impl->align_size;
    impl->flag = flag;
    get_dma_buf(dma_buf);

    if (srv->heaps) {
        KmppDmaHeaps *heaps = srv->heaps;

        kmpp_spin_lock(heaps->lock);
        kmpp_list_add_tail(&impl->list, &srv->list_ext_buf);
        srv->ext_buf_cnt++;
        impl->heap_id = (rk_u32)-1;
        impl->buf_id = srv->ext_buf_id++;
        kmpp_spin_unlock(heaps->lock);
    }

    dmabuf_dbg_buf("dmabuf %d:%d size %d flag %x import by ctx %#px\n",
                   impl->heap_id, impl->buf_id, impl->size, impl->flag, dma_buf);

    *buf = impl;

    return rk_ok;
}

void *kmpp_dmabuf_get_kptr(KmppDmaBuf buf)
{
    KmppDmaBufImpl *impl = (KmppDmaBufImpl *)buf;
    struct dma_buf *dma_buf;
    void *ptr;

    if (!impl)
        return NULL;

    if (impl->kptr)
        return impl->kptr;

    dma_buf = impl->dma_buf;
    if (impl->map_size != impl->align_size) {
        rk_u32 cacheable = impl->heap->flag & KMPP_DMAHEAP_FLAGS_CACHABLE;

        ptr = dmabuf_double_vmap(impl, cacheable, __FUNCTION__);
        dmabuf_dbg_buf("dmabuf %d:%d get kptr %#px with double range\n",
                       impl->heap_id, impl->buf_id, ptr);
    } else {
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 01, 0)
        ptr = dma_buf_vmap(dma_buf);
#else
        {
            struct iosys_map map;
            dma_buf_vmap(dma_buf, &map);
            ptr = map.vaddr;
        }
#endif
        dmabuf_dbg_buf("dmabuf %d:%d get kptr %#px\n",
                       impl->heap_id, impl->buf_id, ptr);
    }

    if (IS_ERR_OR_NULL(ptr)) {
        kmpp_loge_f("buf %px vmap failed ret %d\n", buf, PTR_ERR(ptr));
        ptr = NULL;
    }

    impl->kptr = ptr;

    return ptr;
}

rk_u64 kmpp_dmabuf_get_uptr(KmppDmaBuf buf)
{
    KmppDmaBufImpl *impl = (KmppDmaBufImpl *)buf;
    struct dma_buf *dma_buf;
    KmppDmaHeaps *heaps;
    struct file *file;
    rk_s32 size;
    rk_ul uptr;
    void *mm;

    if (!impl)
        return 0;

    if (impl->uptr)
        return impl->uptr;

    mm = kmpp_get_curr_mm();
    if (!mm)
        return 0;

    heaps = impl->heap->heaps;
    dma_buf = impl->dma_buf;
    file = dma_buf->file;
    size = impl->size;
    uptr = 0;

    dmabuf_dbg_heaps("dmabuf %px file %px size %d at mm %px\n",
                     dma_buf, file, size, mm);

    if (impl->map_size != impl->align_size) {
        rk_ul uptr0;

        do {
            uptr = vm_mmap(NULL, 0, 2 * size, PROT_READ | PROT_WRITE, MAP_SHARED, 0);
            if (IS_ERR_VALUE(uptr)) {
                kmpp_loge_f("vm_mmap anonymous failed ret %d\n", uptr);
                uptr = 0;
                break;
            }

            uptr0 = vm_mmap(file, uptr, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, 0);
            if (IS_ERR_VALUE(uptr0) || uptr0 != uptr) {
                kmpp_loge_f("vm_mmap size %d failed ret %d %d\n", size, uptr0, uptr);
                vm_munmap(uptr, 2 * size);
                uptr = 0;
                break;
            }

            uptr0 = vm_mmap(file, uptr + size, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, 0);
            if (IS_ERR_VALUE(uptr0) || uptr0 != uptr + size) {
                kmpp_loge_f("vm_mmap double size %d -> %d failed ret %d %d\n",
                            size, size * 2, uptr0, uptr);
                vm_munmap(uptr, 2 * size);
                uptr = 0;
                break;
            }
            break;
        } while (1);
    } else {
        uptr = vm_mmap(file, 0, size, PROT_READ | PROT_WRITE, MAP_SHARED, 0);
        if (IS_ERR_VALUE(uptr)) {
            kmpp_loge_f("vm_mmap size %d failed ret %d\n", size, uptr);
            uptr = 0;
        }
    }

    dmabuf_dbg_buf("dmabuf %d:%d get uptr %#lx\n", impl->heap_id, impl->buf_id, uptr);

    impl->uptr = (rk_u64)uptr;
    if (uptr)
        impl->mm = mm;

    kmpp_put_mm(mm);

    return impl->uptr;
}

rk_s32 kmpp_dmabuf_put_uptr(KmppDmaBuf buf)
{
    KmppDmaBufImpl *impl = (KmppDmaBufImpl *)buf;
    rk_s32 ret = rk_nok;

    if (!impl) {
        kmpp_loge_f("invalid NULL buf\n");
        return ret;
    }

    if (impl->uptr) {
        void *mm = kmpp_get_curr_mm();

        if (mm && mm == impl->mm) {
            vm_munmap((rk_ul)impl->uptr, impl->map_size);
            impl->uptr = 0;
            impl->mm = NULL;
            ret = rk_ok;
        }

        kmpp_put_mm(mm);
    }

    return ret;
}

rk_s32 kmpp_dmabuf_get_size(KmppDmaBuf buf)
{
    KmppDmaBufImpl *impl = (KmppDmaBufImpl *)buf;

    return impl ? impl->size : 0;
}

rk_u32 kmpp_dmabuf_get_flag(KmppDmaBuf buf)
{
    KmppDmaBufImpl *impl = (KmppDmaBufImpl *)buf;

    return impl ? impl->flag : 0;
}

rk_s32 kmpp_dmabuf_get_fd(KmppDmaBuf buf)
{
    KmppDmaBufImpl *impl = (KmppDmaBufImpl *)buf;

    if (!impl)
        return -1;

    if (impl->fd < 0) {
        impl->fd = dma_buf_fd(impl->dma_buf, O_CLOEXEC | O_RDWR);
        get_dma_buf(impl->dma_buf);
    }

    return impl->fd;
}

rk_s32 kmpp_dmabuf_get_iova(KmppDmaBuf buf, rk_u64 *iova, KmppDev *dev)
{
    KmppDmaBufImpl *impl = (KmppDmaBufImpl *)buf;
    KmppDmaBufIova *node;

    if (!buf || !iova || !dev) {
        kmpp_loge_f("invalid buf %px iova %px dev %px\n", buf, iova, dev);
        return rk_nok;
    }

    node = attach_iova_f(buf, dev, kmpp_dev_get_device(dev));
    if (!node)
        return rk_nok;

    dmabuf_dbg_buf("dmabuf %d get %s iova %#llx\n", impl->heap_id,
                   impl->buf_id, dev->name, node->iova);

    *iova = node->iova;
    return rk_ok;
}

rk_s32 kmpp_dmabuf_put_iova(KmppDmaBuf buf, rk_u64 iova, KmppDev *dev)
{
    KmppDmaBufImpl *impl = (KmppDmaBufImpl *)buf;
    KmppDmaBufIova *node;
    KmppDmaBufIova *pos, *n;

    if (!impl || iova == KMPP_INVALID_IOVA || !dev) {
        kmpp_loge_f("invalid buf %px iova %llx dev %px\n", buf, iova, dev);
        return rk_nok;
    }

    node = NULL;
    kmpp_spin_lock(impl->lock_iova);
    kmpp_list_for_each_entry_safe(pos, n, &impl->list_iova, KmppDmaBufIova, list) {
        if (pos->iova == iova && pos->dev == dev) {
            kmpp_list_del_init(&pos->list);
            node = pos;
            break;
        }
    }
    kmpp_spin_unlock(impl->lock_iova);

    if (!node) {
        kmpp_loge_f("not found buffer iova node %px iova %llx dev %px\n",
                    buf, iova, dev);
        return rk_nok;
    }

    dmabuf_dbg_buf("dmabuf %d:%d put %s iova %#llx\n", impl->heap_id,
                   impl->buf_id, dev->name, node->iova);

    detach_iova(node);
    return rk_ok;
}

rk_s32 kmpp_dmabuf_get_iova_by_device(KmppDmaBuf buf, rk_u64 *iova, void *device)
{
    KmppDmaBufImpl *impl = (KmppDmaBufImpl *)buf;
    KmppDmaBufIova *node;

    if (!buf || !iova || !device) {
        kmpp_loge_f("invalid buf %px iova %px dev %px\n", buf, iova, device);
        return rk_nok;
    }

    node = attach_iova_f(buf, NULL, device);
    if (!node)
        return rk_nok;

    dmabuf_dbg_buf("dmabuf %d:%d get %s iova %#llx\n", impl->heap_id,
                   impl->buf_id, kmpp_device_name(device), node->iova);

    *iova = node->iova;
    return rk_ok;
}

rk_s32 kmpp_dmabuf_put_iova_by_device(KmppDmaBuf buf, rk_u64 iova, void *device)
{
    KmppDmaBufImpl *impl = (KmppDmaBufImpl *)buf;
    KmppDmaBufIova *node;
    KmppDmaBufIova *pos, *n;

    if (!impl || iova == KMPP_INVALID_IOVA || !device) {
        kmpp_loge_f("invalid buf %px iova %llx device %px\n", buf, iova, device);
        return rk_nok;
    }

    node = NULL;
    kmpp_spin_lock(impl->lock_iova);
    kmpp_list_for_each_entry_safe(pos, n, &impl->list_iova, KmppDmaBufIova, list) {
        if (pos->iova == iova && pos->device == device) {
            kmpp_list_del_init(&pos->list);
            node = pos;
            break;
        }
    }
    kmpp_spin_unlock(impl->lock_iova);

    if (!node) {
        kmpp_loge_f("not found buffer iova node %px iova %llx device %px\n",
                    buf, iova, device);
        return rk_nok;
    }

    dmabuf_dbg_buf("dmabuf %d:%d put %s iova %#llx\n", impl->heap_id,
                   impl->buf_id, kmpp_device_name(device), node->iova);

    detach_iova(node);
    return rk_ok;
}

rk_u64 kmpp_invalid_iova(void)
{
    return (rk_u64)(~(dma_addr_t)0);
}

void kmpp_dmabuf_set_priv(KmppDmaBuf buf, void *priv)
{
    KmppDmaBufImpl *impl = (KmppDmaBufImpl *)buf;

    if (impl)
        impl->priv = priv;
}

void *kmpp_dmabuf_get_priv(KmppDmaBuf buf)
{
    KmppDmaBufImpl *impl = (KmppDmaBufImpl *)buf;

    return impl ? impl->priv : NULL;
}

rk_s32 kmpp_dmabuf_flush_for_cpu(KmppDmaBuf buf)
{
    KmppDmaBufImpl *impl = (KmppDmaBufImpl *)buf;

    if (!impl) {
        kmpp_loge_f("invalid NULL buf\n");
        return rk_nok;
    }

    dma_buf_begin_cpu_access(impl->dma_buf, DMA_FROM_DEVICE);

    return 0;
}

rk_s32 kmpp_dmabuf_flush_for_dev(KmppDmaBuf buf, KmppDev *dev)
{
    KmppDmaBufImpl *impl = (KmppDmaBufImpl *)buf;

    if (!impl) {
        kmpp_loge_f("invalid NULL buf\n");
        return rk_nok;
    }

    dma_buf_end_cpu_access(impl->dma_buf, DMA_TO_DEVICE);

    return 0;
}

#ifndef CONFIG_DMABUF_PARTIAL
#define dma_buf_begin_cpu_access_partial(dma_buf, dir, offset, size) \
    dma_buf_begin_cpu_access(dma_buf, dir)

#define dma_buf_end_cpu_access_partial(dma_buf, dir, offset, size) \
    dma_buf_end_cpu_access(dma_buf, dir)
#endif

rk_s32 kmpp_dmabuf_flush_for_cpu_partial(KmppDmaBuf buf, rk_u32 offset, rk_u32 size)
{
    KmppDmaBufImpl *impl = (KmppDmaBufImpl *)buf;
    rk_u32 _offset, _size;

    if (!impl) {
        kmpp_loge_f("invalid NULL buf\n");
        return rk_nok;
    }

    if (!impl->dma_buf->ops->begin_cpu_access_partial) {
        static rk_s32 once = 1;

        if (once && IS_ENABLED(CONFIG_DMABUF_PARTIAL)) {
            kmpp_logw("kernel not support cache partial sync, please enable the CONFIG_DMABUF_PARTIAL\n");
            once = 0;
        }
        return dma_buf_begin_cpu_access(impl->dma_buf, DMA_FROM_DEVICE);
    }

    _offset = KMPP_ALIGN_DOWN(offset, CACHE_LINE_SIZE);
    _size = KMPP_ALIGN(size + offset - _offset, CACHE_LINE_SIZE);

    dma_buf_begin_cpu_access_partial(impl->dma_buf, DMA_FROM_DEVICE, _offset, _size);

    return 0;
}

rk_s32 kmpp_dmabuf_flush_for_dev_partial(KmppDmaBuf buf, KmppDev *dev, rk_u32 offset, rk_u32 size)
{
    KmppDmaBufImpl *impl = (KmppDmaBufImpl *)buf;
    rk_u32 _offset, _size;

    if (!impl) {
        kmpp_loge_f("invalid NULL buf\n");
        return rk_nok;
    }

    if (!impl->dma_buf->ops->end_cpu_access_partial) {
        static rk_s32 once = 1;

        if (once && IS_ENABLED(CONFIG_DMABUF_PARTIAL)) {
            kmpp_logw("kernel not support cache partial sync, please enable the CONFIG_DMABUF_PARTIAL\n");
            once = 0;
        }
        return dma_buf_end_cpu_access(impl->dma_buf, DMA_TO_DEVICE);
    }

    _offset = KMPP_ALIGN_DOWN(offset, CACHE_LINE_SIZE);
    _size = KMPP_ALIGN(size + offset - _offset, CACHE_LINE_SIZE);

    dma_buf_end_cpu_access_partial(impl->dma_buf, DMA_TO_DEVICE, _offset, _size);

    return 0;
}

EXPORT_SYMBOL(kmpp_dmaheap_get);
EXPORT_SYMBOL(kmpp_dmaheap_get_by_name);
EXPORT_SYMBOL(kmpp_dmaheap_put);

EXPORT_SYMBOL(kmpp_dmabuf_alloc);
EXPORT_SYMBOL(kmpp_dmabuf_free);
EXPORT_SYMBOL(kmpp_dmabuf_import_fd);
EXPORT_SYMBOL(kmpp_dmabuf_import_ctx);

EXPORT_SYMBOL(kmpp_dmabuf_get_kptr);
EXPORT_SYMBOL(kmpp_dmabuf_get_uptr);
EXPORT_SYMBOL(kmpp_dmabuf_put_uptr);
EXPORT_SYMBOL(kmpp_dmabuf_get_size);
EXPORT_SYMBOL(kmpp_dmabuf_get_flag);
EXPORT_SYMBOL(kmpp_dmabuf_get_fd);
EXPORT_SYMBOL(kmpp_dmabuf_get_iova);
EXPORT_SYMBOL(kmpp_dmabuf_put_iova);
EXPORT_SYMBOL(kmpp_dmabuf_put_iova_by_device);
EXPORT_SYMBOL(kmpp_dmabuf_get_iova_by_device);
EXPORT_SYMBOL(kmpp_invalid_iova);

EXPORT_SYMBOL(kmpp_dmabuf_set_priv);
EXPORT_SYMBOL(kmpp_dmabuf_get_priv);

EXPORT_SYMBOL(kmpp_dmabuf_flush_for_cpu);
EXPORT_SYMBOL(kmpp_dmabuf_flush_for_dev);
EXPORT_SYMBOL(kmpp_dmabuf_flush_for_cpu_partial);
EXPORT_SYMBOL(kmpp_dmabuf_flush_for_dev_partial);

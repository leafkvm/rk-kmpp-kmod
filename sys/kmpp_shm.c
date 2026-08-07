/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define  MODULE_TAG "kmpp_shm"

#include <asm/ioctl.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/export.h>

#include "kmpp_list.h"
#include "rk-mpp-kobj.h"

#include "kmpp_osal.h"
#include "kmpp_sys.h"

#include "kmpp_sys_id.h"

#define SHM_DBG_FLOW                (0x00000001)
#define SHM_DBG_DETAIL              (0x00000002)
#define SHM_DBG_IOCTL               (0x00000004)
#define SHM_DBG_DUMP                (0x00000008)
#define SHM_DBG_POOL                (0x00000010)
#define SHM_DBG_LEAK                (0x00000020)
#define SHM_DBG_STATISTIC           (0x00000040)

#define shm_dbg(flag, fmt, ...)     kmpp_dbg(kmpp_shm_debug, flag, fmt, ## __VA_ARGS__)
#define shm_dbg_f(flag, fmt, ...)   kmpp_dbg_f(kmpp_shm_debug, flag, fmt, ## __VA_ARGS__)

#define shm_dbg_flow(fmt, ...)      shm_dbg_f(SHM_DBG_FLOW, fmt, ## __VA_ARGS__)
#define shm_dbg_detail(fmt, ...)    shm_dbg(SHM_DBG_DETAIL, fmt, ## __VA_ARGS__)
#define shm_dbg_ioctl(fmt, ...)     shm_dbg(SHM_DBG_IOCTL, fmt, ## __VA_ARGS__)
#define shm_dbg_pool(fmt, ...)      shm_dbg(SHM_DBG_POOL | SHM_DBG_DETAIL, fmt, ## __VA_ARGS__)
#define shm_dbg_leak(fmt, ...)      shm_dbg(SHM_DBG_LEAK, fmt, ## __VA_ARGS__)
#define shm_dbg_statistic(fmt, ...) shm_dbg(SHM_DBG_STATISTIC, fmt, ## __VA_ARGS__)

#define KMPP_SHM_IOC_MAGIC          'm'
#define KMPP_SHM_IOC_QUERY_INFO     _IOW(KMPP_SHM_IOC_MAGIC, 1, unsigned int)
#define KMPP_SHM_IOC_RELEASE_INFO   _IOW(KMPP_SHM_IOC_MAGIC, 2, unsigned int)
#define KMPP_SHM_IOC_GET_SHM        _IOW(KMPP_SHM_IOC_MAGIC, 3, unsigned int)
#define KMPP_SHM_IOC_PUT_SHM        _IOW(KMPP_SHM_IOC_MAGIC, 4, unsigned int)
#define KMPP_SHM_IOC_DUMP           _IOW(KMPP_SHM_IOC_MAGIC, 5, unsigned int)

#define KMPP_SHM_POOL_MAX_SLOT      10
#define KMPP_SHM_POOL_LARGE         ((KMPP_SHM_POOL_MAX_SLOT) - 1)
#define KMPP_SHM_POOL_SHIFT         ((KMPP_SHM_POOL_MAX_SLOT) - 2)

#define KMPP_SHM_POOL_MIN_SIZE      (PAGE_SIZE)
#define KMPP_SHM_POOL_MIN_SHIFT     (PAGE_SHIFT)
#define KMPP_SHM_POOL_MAX_SIZE      ((PAGE_SIZE) * (1 << KMPP_SHM_POOL_SHIFT))

typedef struct KmppShmList_t {
    kmpp_list_head  list;
    rk_u32          count;
} KmppShmList;

typedef struct KmppShmPool_t {
    KmppSpinlock    *lock;
    KmppFsDev       *file;
    KmppShmList     lists[KMPP_SHM_POOL_MAX_SLOT];
    kmpp_list_head  list_used;
    rk_u32          count_used;
    rk_u32          count_unused;
    rk_u32          size_used;
    rk_u32          size_unused;
    rk_u32          max_count;
    rk_u32          max_size;
} KmppShmPool;

/* kmpp share memory class */
typedef struct KmppShmClsImpl_t {
    KmppSpinlock    *lock;
    KmppCls         *cls;
    KmppDevFops     fops;
    kmpp_list_head  list_mgr;
} KmppShmClsImpl;

/* kmpp share memory manager (kmpp_shm_mgr) each file class has a manager */
typedef struct KmppShmMgr_t {
    KmppSpinlock    *lock;
    /* list to list_mgr in KmppShmClsImpl */
    kmpp_list_head  list_cls;
    /* list for list_mgr in KmppShmGrpRoot */
    kmpp_list_head  list_root;
    KmppFsDevMgr    *mgr;
    const rk_u8     *name;
} KmppShmMgr;

/* kmpp share memory group for buffer fast access */
typedef struct KmppShmGrpImpl_t {
    KmppSpinlock    *lock;
    KmppFsDev       *file;
    void            *root;
    KmppObjDef      def;

    /* list for list_shm in KmppShmGrpImpl */
    kmpp_list_head  list_shm;
    /* list for list_shm in KmppShmGrpImpl */
    kmpp_list_head  list_late;
    rk_s32          shm_count;
    /* objdef entry size */
    rk_s32          size;
    /* objdef name string offset from the share buffer base address */
    rk_s32          name_offset;
    /* objdef index in all the objdefs */
    rk_s32          index;
} KmppShmGrpImpl;

/* kmpp share memory group root each file handle can has a group root */
typedef struct KmppShmGrpRoot_t {
    KmppSpinlock    *lock;
    /* list to list_shm in KmppShmMgr */
    kmpp_list_head  list_mgr;
    /* list for list_file in KmppShmGrpImpl */
    kmpp_list_head  list_grp;

    /* device file infomation link */
    KmppFsDev       *file;
    KmppShmMgr      *mgr;

    /* object defines set info */
    KmppObjDefSet   *defs;
    KmppFsVm        *vm_trie;
    void            *buf;
    rk_s32          buf_size;

    /* session id for pid in system */
    rk_u32          sid;
    /* pid for find valid process share memory allocator */
    rk_s32          pid;

    /* share memory pool for recycle and reduce mmap overhead */
    KmppShmPool     pool;

    KmppShmGrpImpl  grp_shm;
    KmppShmGrpImpl  *grps;
} KmppShmGrpRoot;

typedef struct KmppShmImpl_t {
    KmppObjShm      ioc;

    /* reserve for userspace writing */
    rk_u64          upriv;
    rk_u64          ubase;

    /* share memory name offset from the share trie root */
    rk_u32          name_offset;
    rk_u32          align_size;

    /* kernel access address */
    void            *kpriv;
    void            *kbase;

    KmppFsVm        *vm_shm;
    KmppShmGrpImpl  *grp;

    /* list to list_shm in KmppShmGrpImpl */
    kmpp_list_head  list_grp;
    /* list to lists.list in KmppShmPool */
    kmpp_list_head  list_pool;
} KmppShmImpl;

static KmppShmClsImpl *kmpp_shm_cls = NULL;
static const rk_u8 kmpp_shm_name[] = "kmpp_shm";
static KmppEnvNode env_shm_debug = NULL;
static KmppEnvNode env_shm_show = NULL;
static KmppShmMgr *kmpp_shm_objs = NULL;
static rk_u32 kmpp_shm_root_sid = 0;
static rk_u32 kmpp_shm_debug = 0;

static inline rk_u32 size_to_pool_idx(rk_u32 size)
{
    rk_u32 tmp;

    if (size <= KMPP_SHM_POOL_MIN_SIZE)
        return 0;

    if (size > KMPP_SHM_POOL_MAX_SIZE)
        return KMPP_SHM_POOL_LARGE;

    tmp = 1 << (32 - __builtin_clz(size - 1));

    return __builtin_ctz(tmp) - KMPP_SHM_POOL_MIN_SHIFT;
}

static inline rk_u32 pool_idx_to_size(rk_u32 idx)
{
    if (idx < KMPP_SHM_POOL_LARGE)
        return KMPP_SHM_POOL_MIN_SIZE << idx;

    return 0;
}

static inline rk_s32 kmpp_shm_entry_offset(void)
{
    return sizeof(KmppShmImpl);
}

KmppShmImpl *create_shm(KmppFsDev *file, rk_u32 size, rk_u32 align_size)
{
    KmppShmImpl *impl;
    rk_s32 entry_offset = kmpp_shm_entry_offset();
    rk_s32 ret;

    impl = kmpp_malloc_share(align_size);
    if (!impl) {
        kmpp_loge_f("malloc KmppShm size %d failed\n", align_size);
        return NULL;
    }

    ret = kmpp_fs_vm_mmap(&impl->vm_shm, file, impl, align_size, KMPP_VM_PROT_RW);
    if (ret) {
        kmpp_loge_f("mmap %px size %d failed ret %d\n", impl, align_size, ret);
        kmpp_free_share(impl);
        return NULL;
    }

    KMPP_INIT_LIST_HEAD(&impl->list_grp);
    KMPP_INIT_LIST_HEAD(&impl->list_pool);
    impl->grp = NULL;
    impl->kpriv = NULL;
    impl->kbase = impl;
    impl->ubase = impl->vm_shm->uaddr;
    impl->name_offset = 0;
    impl->align_size = align_size;

    impl->ioc.kobj_uaddr = impl->ubase;
    impl->ioc.kobj_kaddr = (__u64)(uintptr_t)impl;

    shm_dbg_pool("get shm %px size %d:%d kaddr [%px:%px] uaddr [%#lx:%#llx]\n",
                 impl, size, align_size, impl->kbase, impl->kbase + entry_offset,
                 impl->ubase, impl->ubase + entry_offset);
    return impl;
}

const rk_u8 *grp_to_name(KmppShmGrpImpl *grp)
{
    KmppShmGrpRoot *root = grp->root;
    static const rk_u8 *name = "share_memory";

    if (grp->name_offset)
        return (const rk_u8 *)root->buf + grp->name_offset;

    return name;
}

rk_s32 attach_shm(KmppShmImpl *impl, KmppShmGrpImpl *grp)
{
    if (!impl)
        return rk_nok;

    impl->grp = grp;
    impl->upriv = 0;
    impl->kpriv = NULL;
    impl->name_offset = grp->name_offset;

    shm_dbg_pool("attach shm %px -> grp %s\n", impl, grp_to_name(grp));

    kmpp_spin_lock(grp->lock);
    kmpp_list_move_tail(&impl->list_grp, &grp->list_shm);
    kmpp_spin_unlock(grp->lock);

    return rk_ok;
}

rk_s32 release_shm(KmppShmImpl *impl)
{
    rk_s32 ret;

    if (!impl)
        return rk_nok;

    shm_dbg_pool("put shm %px size %d kaddr [%px:%px] uaddr [%#lx:%#llx]\n",
                 impl, impl->align_size, impl->kbase, impl->kbase + kmpp_shm_entry_offset(),
                 impl->ubase, impl->ubase + kmpp_shm_entry_offset());

    /* NOTE: when fd is closing do on munmap shm memory again */
    ret = kmpp_fs_vm_munmap(impl->vm_shm);
    if (ret)
        kmpp_loge_f("munmap %px failed ret %d\n", impl->vm_shm, ret);

    kmpp_memset(impl, 0, sizeof(*impl));

    kmpp_free_share(impl);

    return ret;
}

rk_s32 init_shm_pool(KmppShmPool *pool, KmppSpinlock *lock, KmppFsDev *file)
{
    rk_u32 i;

    if (!pool || !lock)
        return rk_nok;

    pool->lock = lock;
    pool->file = file;

    for (i = 0; i < KMPP_SHM_POOL_MAX_SLOT; i++) {
        KMPP_INIT_LIST_HEAD(&pool->lists[i].list);
        pool->lists[i].count = 0;
    }

    KMPP_INIT_LIST_HEAD(&pool->list_used);

    pool->count_used = 0;
    pool->count_unused = 0;
    pool->size_used = 0;
    pool->size_unused = 0;
    pool->max_count = 0;
    pool->max_size = 0;

    return rk_ok;
}

rk_s32 deinit_shm_pool(KmppShmPool *pool)
{
    KmppShmImpl *shm, *n;
    kmpp_list_head *list;
    KMPP_LIST_HEAD(list_local);
    rk_s32 i;

    if (!pool)
        return rk_nok;

    if (pool->count_used || pool->size_used) {
        kmpp_loge_f("shm pool has used memory total %d size %d\n",
                    pool->count_used, pool->size_used);

        list = &pool->list_used;
        kmpp_spin_lock(pool->lock);
        kmpp_list_for_each_entry_safe(shm, n, list, KmppShmImpl, list_pool) {
            kmpp_list_move(&shm->list_pool, &list_local);
            pool->count_used--;
            pool->size_used -= shm->align_size;
        }
        kmpp_spin_unlock(pool->lock);

        kmpp_list_for_each_entry_safe(shm, n, &list_local, KmppShmImpl, list_pool) {
            release_shm(shm);
        }

        if (pool->count_used || pool->size_used)
            kmpp_loge_f("shm pool has used memory total %d size %d after cleanup\n",
                        pool->count_used, pool->size_used);
    }

    for (i = 0; i < KMPP_SHM_POOL_MAX_SLOT; i++) {
        list = &pool->lists[i].list;

        kmpp_spin_lock(pool->lock);
        kmpp_list_for_each_entry_safe(shm, n, list, KmppShmImpl, list_pool) {
            kmpp_list_move(&shm->list_pool, &list_local);
            pool->lists[i].count--;
            pool->count_unused--;
            pool->size_unused -= shm->align_size;
        }
        kmpp_spin_unlock(pool->lock);
    }

    if (pool->count_unused || pool->size_unused) {
        kmpp_loge_f("shm pool has unused memory total %d size %d\n",
                    pool->count_unused, pool->size_unused);
    }

    kmpp_list_for_each_entry_safe(shm, n, &list_local, KmppShmImpl, list_pool) {
        release_shm(shm);
    }

    return rk_ok;
}

KmppShmImpl *get_shm_from_pool(KmppShmPool *pool, rk_u32 size)
{
    KmppShmImpl *shm = NULL;
    rk_u32 align_size;
    rk_u32 slot_size;
    rk_u32 idx;

    if (!pool || !size) {
        kmpp_loge_f("invalid param pool %px size %d\n", pool, size);
        return NULL;
    }

    align_size = PAGE_ALIGN(size);
    idx = size_to_pool_idx(align_size);
    slot_size = pool_idx_to_size(idx);
    if (slot_size)
        align_size = slot_size;

    kmpp_spin_lock(pool->lock);

    if (idx == KMPP_SHM_POOL_LARGE) {
        KmppShmImpl *pos, *n;

        kmpp_list_for_each_entry_safe(pos, n, &pool->lists[idx].list, KmppShmImpl, list_pool) {
            rk_u32 pos_size = pos->align_size;

            /* buffer is larger and the difference is smaller than 1/8 */
            if ((pos_size >= align_size) && ((pos_size - align_size) <= (pos_size >> 3))) {
                shm = pos;
                break;
            }
        }
    } else {
        shm = kmpp_list_first_entry_or_null(&pool->lists[idx].list, KmppShmImpl, list_pool);
    }

    if (shm) {
        rk_s32 entry_offset = kmpp_shm_entry_offset();

        shm_dbg_pool("get shm %px size %d:%d from unused %d [%d:%d:%d:%d]\n",
                     shm, size, align_size, idx,
                     pool->lists[0].count, pool->lists[1].count,
                     pool->lists[2].count, pool->lists[3].count);

        kmpp_list_del_init(&shm->list_pool);
        pool->lists[idx].count--;
        pool->count_unused--;
        pool->size_unused -= align_size;

        shm->grp = NULL;
        shm->kpriv = NULL;
        shm->name_offset = 0;
        kmpp_memset(shm->kbase + entry_offset, 0, size - entry_offset);
    } else {
        kmpp_spin_unlock(pool->lock);

        shm = create_shm(pool->file, size, align_size);
        if (!shm)
            return NULL;

        kmpp_spin_lock(pool->lock);
    }

    kmpp_assert(shm);

    kmpp_list_add_tail(&shm->list_pool, &pool->list_used);
    pool->count_used++;
    pool->size_used += align_size;
    shm_dbg_pool("add shm %px size %d to used [%d:%d|%d:%d]\n",
                 shm, align_size,
                 pool->count_unused, pool->size_unused,
                 pool->count_used, pool->size_used);
    if (pool->max_count < pool->count_used + pool->count_unused)
        pool->max_count = pool->count_used + pool->count_unused;
    if (pool->max_size < pool->size_used + pool->size_unused)
        pool->max_size = pool->size_used + pool->size_unused;

    kmpp_spin_unlock(pool->lock);

    return shm;
}

void put_shm_to_pool(KmppShmPool *pool, KmppShmImpl *shm)
{
    rk_u32 size = shm->align_size;
    rk_u32 idx = size_to_pool_idx(size);

    kmpp_spin_lock(pool->lock);
    kmpp_list_move_tail(&shm->list_pool, &pool->lists[idx].list);
    pool->lists[idx].count++;
    pool->count_used--;
    pool->count_unused++;
    pool->size_used -= size;
    pool->size_unused += size;
    shm_dbg_pool("put shm %px size %d to unused [%d:%d|%d:%d]\n",
                 shm, shm->align_size,
                 pool->count_unused, pool->size_unused,
                 pool->count_used, pool->size_used);
    kmpp_spin_unlock(pool->lock);
}

void init_shm_grp(KmppShmGrpImpl *grp, KmppShmGrpRoot *root, KmppFsDev *file)
{
    grp->lock = root->lock;
    grp->file = file;
    grp->root = root;
    KMPP_INIT_LIST_HEAD(&grp->list_shm);
    KMPP_INIT_LIST_HEAD(&grp->list_late);
}

rk_s32 kmpp_shm_open(KmppFsDev *file)
{
    KmppShmMgr *mgr = (KmppShmMgr *)file->drv_data;
    KmppShmGrpRoot *root = (KmppShmGrpRoot *)file->priv_data;
    KmppObjDefSet *defs = NULL;

    shm_dbg_flow("enter - %s\n", mgr->name);

    kmpp_objdefset_get(&defs, TRIE_ENTRY);

    if (!defs) {
        kmpp_loge_f("failed to get shared objdef set\n");
        return rk_nok;
    }

    kmpp_spinlock_assign(&root->lock, (void *)(root + 1), kmpp_spinlock_size());

    root->sid = kmpp_shm_root_sid++;
    root->pid = kmpp_getpid();

    KMPP_INIT_LIST_HEAD(&root->list_mgr);
    KMPP_INIT_LIST_HEAD(&root->list_grp);

    root->file = file;
    root->mgr = mgr;

    root->defs = defs;
    root->buf = kmpp_trie_get_node_root(defs->trie);
    root->buf_size = defs->buf_size;

    init_shm_pool(&root->pool, root->lock, file);

    {
        /* init shm group */
        KmppShmGrpImpl *grp = &root->grp_shm;

        init_shm_grp(grp, root, file);
        grp->def = NULL;
        grp->size = 0;
        grp->index = -1;
        grp->name_offset = 0;
    }

    if (defs->count) {
        KmppShmGrpImpl *grps = kmpp_calloc(defs->count * sizeof(KmppShmGrpImpl));
        KmppTrieInfo *info;
        rk_s32 i;

        if (!grps) {
            kmpp_loge_f("failed to alloc shm group array\n");
            return rk_nok;
        }

        root->grps = grps;
        info = kmpp_trie_get_info_first(defs->trie);

        for (i = 0; i < defs->count; i++) {
            KmppShmGrpImpl *grp = &grps[i];
            const rk_u8 *name = kmpp_trie_info_name(info);

            init_shm_grp(grp, root, file);
            grp->def = defs->defs[i];
            grp->size = kmpp_objdef_get_entry_size(grp->def);
            grp->index = i;
            grp->name_offset = name - (rk_u8 *)root->buf;

            info = kmpp_trie_get_info_next(defs->trie, info);
        }
    }

    if (kmpp_shm_cls) {
        kmpp_spin_lock(kmpp_shm_cls->lock);
        kmpp_list_add_tail(&root->list_mgr, &mgr->list_root);
        kmpp_spin_unlock(kmpp_shm_cls->lock);
    }

    shm_dbg_flow("leave - %s sid %d\n", mgr->name, root->sid);

    return rk_ok;
}

void deinit_shm_in_list(KmppShmGrpRoot *root, kmpp_list_head *list)
{
    KMPP_LIST_HEAD(list_local);
    KmppShmImpl *shm, *n;

    kmpp_spin_lock(root->lock);
    kmpp_list_for_each_entry_safe(shm, n, list, KmppShmImpl, list_grp) {
        kmpp_list_move(&shm->list_grp, &list_local);
    }
    kmpp_spin_unlock(root->lock);

    kmpp_list_for_each_entry_safe(shm, n, &list_local, KmppShmImpl, list_grp) {
        shm_dbg_leak("leak shm %px obj %px\n", shm, shm->kpriv);

        if (shm->kpriv) {
            kmpp_obj_put_f(shm->kpriv);
        } else {
            kmpp_shm_put(shm);
        }
    }
}

static rk_s32 kmpp_shm_grp_root_deinit(KmppShmGrpRoot *root)
{
    KmppObjDefSet *defs = root->defs;
    KmppShmMgr *mgr = root->mgr;
    KmppShmGrpImpl *grp;
    KmppShmImpl *shm, *n;

    kmpp_spin_lock(mgr->lock);
    kmpp_list_del_init(&root->list_mgr);
    kmpp_spin_unlock(mgr->lock);

    if (root->vm_trie) {
        kmpp_fs_vm_munmap(root->vm_trie);
        root->vm_trie = NULL;
    }

    if (defs) {
        KMPP_LIST_HEAD(list_late);
        rk_s32 i;

        /* move all late shm to local list for later deinit */
        for (i = 0; i < defs->count; i++) {
            grp = &root->grps[i];

            kmpp_spin_lock(root->lock);
            kmpp_list_for_each_entry_safe(shm, n, &grp->list_late, KmppShmImpl, list_grp) {
                kmpp_list_move(&shm->list_grp, &list_late);
                shm_dbg_leak("move late shm %px obj %px\n", shm, shm->kpriv);
            }
            kmpp_spin_unlock(root->lock);
        }

        /* deinit all auto deinit shm */
        for (i = 0; i < defs->count; i++)
            deinit_shm_in_list(root, &root->grps[i].list_shm);

        /* check late list for remaining shm */
        kmpp_list_for_each_entry_safe(shm, n, &list_late, KmppShmImpl, list_grp) {
            shm_dbg_leak("leak late shm %px obj %px\n", shm, shm->kpriv);

            if (shm->kpriv) {
                kmpp_obj_put_f(shm->kpriv);
            } else {
                kmpp_shm_put(shm);
            }
        }

        kmpp_objdefset_put(defs);
    }

    /* deinit default shm group */
    deinit_shm_in_list(root, &root->grp_shm.list_shm);

    /* deinit share memory pool and show usage */
    deinit_shm_pool(&root->pool);

    shm_dbg_statistic("shm pool %d:%d max usage: count %d size %d\n",
                      root->sid, root->pid, root->pool.max_count, root->pool.max_size);

    kmpp_free(root->grps);

    kmpp_spinlock_deinit(&root->lock);

    return rk_ok;
}

rk_s32 kmpp_shm_release(KmppFsDev *file)
{
    KmppShmMgr *mgr = (KmppShmMgr *)file->drv_data;
    KmppShmGrpRoot *root = (KmppShmGrpRoot *)file->priv_data;
    rk_s32 ret;

    shm_dbg_flow("enter - %s root %px\n", mgr->name, root);

    ret = kmpp_shm_grp_root_deinit(root);

    shm_dbg_flow("leave - %s\n", mgr->name);

    return ret;
}

static rk_s32 get_usr_str(rk_u8 *dst, rk_s32 size, void *src, const rk_u8 *log)
{
    rk_u64 name_uaddr;
    rk_s32 ret;

    ret = kmpp_copy_from_user(&name_uaddr, src, sizeof(name_uaddr));
    if (ret) {
        kmpp_loge_f("by %s copy userspace address %px fail ret %d\n", log, src, ret);
        return ret;
    }

    /* return zero for NULL address copy */
    if (!name_uaddr) {
        kmpp_snprintf(dst, size, "shm");
        return 0;
    }

    ret = kmpp_strncpy_from_user(dst, (void *)(uintptr_t)name_uaddr, size);
    if (ret <= 0) {
        kmpp_loge_f("by %s copy userspace string from %#llx len %d fail ret %d\n",
                    log, name_uaddr, size, ret);
        return ret;
    }

    return ret;
}

static KmppShmGrpImpl *get_shm_grp(KmppFsDev *file, const rk_u8 *name, const rk_u8 *log)
{
    KmppShmGrpRoot *root = (KmppShmGrpRoot *)file->priv_data;
    KmppObjDefSet *defs = root->defs;
    KmppTrieInfo *info;

    if (!name)
        return &root->grp_shm;

    if (!defs || !defs->trie) {
        kmpp_loge_f("by %s invalid grp root %px defs %px trie %px\n",
                    log, root, defs, defs ? defs->trie : NULL);
        return NULL;
    }

    info = kmpp_trie_get_info(defs->trie, name);
    if (!info) {
        kmpp_loge_f("by %s can not found valid objdef %s\n", log, name);
        return NULL;
    }

    if (info->index >= defs->count) {
        kmpp_loge_f("by %s invalid objdef index %d max %d\n",
                    log, info->index, defs->count);
        return NULL;
    }

    return &root->grps[info->index];
}

rk_s32 kmpp_shm_ioctl(KmppFsDev *file, rk_s32 cmd, void *arg)
{
    KmppShmMgr *mgr = (KmppShmMgr *)file->drv_data;
    rk_s32 ret = rk_nok;

    shm_dbg_flow("enter - %s cmd %#x arg %px\n", mgr->name, cmd, arg);

    switch (cmd) {
    case KMPP_SHM_IOC_QUERY_INFO : {
        KmppShmGrpRoot *root = (KmppShmGrpRoot *)file->priv_data;
        rk_u64 uaddr;

        if (!root->vm_trie) {
            rk_s32 buf_size = PAGE_ALIGN(root->buf_size);

            ret = kmpp_fs_vm_mmap(&root->vm_trie, file, root->buf,
                                  buf_size, KMPP_VM_PROT_RO);
            if (ret || !root->vm_trie) {
                kmpp_loge_f("vm_mmap fail ret %d %px\n", ret, root->vm_trie);
                break;
            }
        }

        uaddr = (rk_u64)root->vm_trie->uaddr;

        shm_dbg_ioctl("query obj trie root %#llx\n", uaddr);

        ret = kmpp_copy_to_user(arg, &uaddr, sizeof(uaddr));
        if (ret)
            kmpp_loge_f("KMPP_SHM_IOC_QUERY_INFO kmpp_copy_to_user fail ret %d\n", ret);
    } break;
    case KMPP_SHM_IOC_RELEASE_INFO : {
    } break;
    case KMPP_SHM_IOC_GET_SHM : {
        KmppObjIocArg ioc;
        rk_u32 i;

        ret = kmpp_copy_from_user(&ioc, arg, sizeof(ioc));
        if (ret) {
            kmpp_loge_f("KMPP_SHM_IOC_GET_SHM kmpp_copy_from_user KmppObjIocArg fail\n");
            break;
        }

        shm_dbg_ioctl("get shm %d\n", ioc.count);

        if (ioc.count > KMPP_SHM_IOC_MAX_COUNT) {
            kmpp_loge_f("KMPP_SHM_IOC_GET_SHM count %d over max %d\n",
                        ioc.count, KMPP_SHM_IOC_MAX_COUNT);
            break;
        }

        arg += sizeof(ioc);

        for (i = 0; i < ioc.count; i++, arg += sizeof(KmppShmPtr)) {
            rk_u8 name[64];
            KmppShmGrpImpl *grp = NULL;
            KmppShmPtr sptr;

            ret = get_usr_str(name, sizeof(name) - 1, arg, "KMPP_SHM_IOC_GET_SHM");
            if (ret < 0) {
                shm_dbg_ioctl("slot %d get shm ioc from %px failed ret %d\n", i, arg, ret);
                break;
            }

            shm_dbg_ioctl("slot %d get shm ioc name %s ret %d\n", i, name, ret);

            if (ret) {
                KmppObj obj = NULL;

                grp = get_shm_grp(file, name, "KMPP_SHM_IOC_GET_SHM");
                if (!grp) {
                    kmpp_loge_f("slot %d get_shm_grp %s failed ret %d\n", i, name, ret);
                    ret = rk_nok;
                    break;
                }

                ret = kmpp_obj_get_share_f(&obj, grp->def, file);
                if (ret || !obj)
                    kmpp_loge_f("slot %d kmpp_obj_get_share fail ret %d\n", i, ret);

                if (obj && (kmpp_shm_debug & SHM_DBG_DUMP))
                    kmpp_obj_dump(obj, "KMPP_SHM_IOC_GET_SHM");

                kmpp_obj_to_shmptr(obj, &sptr);
            } else {
                KmppShmReq shm_req;
                KmppShmPtr *ptr;
                KmppShm shm = NULL;
                rk_u32 shm_size;

                ret = kmpp_copy_from_user(&shm_req, arg, sizeof(shm_req));
                if (ret) {
                    kmpp_loge_f("slot %d copy shm request %px fail ret %d\n", i, arg, ret);
                    break;
                }

                shm_size = shm_req.shm_size;
                if (!shm_size) {
                    kmpp_loge_f("slot %d invalid shm size %d\n", i, shm_size);
                    ret = rk_nok;
                    break;
                }

                ret = kmpp_shm_get(&shm, file, NULL, shm_size);
                if (ret) {
                    kmpp_loge_f("slot %d kmpp_shm_get size %d fail ret %d\n",
                                i, shm_size, ret);
                    break;
                }

                ptr = kmpp_shm_to_shmptr(shm);
                if (!ptr) {
                    kmpp_loge_f("slot %d kmpp_shm_to_shmptr fail\n", i);
                    ret = rk_nok;
                    break;
                }

                sptr = *ptr;
            }

            shm_dbg_ioctl("slot %d get shm [u:k] %#llx :%#px\n", i, sptr.uaddr, sptr.kptr);

            if (kmpp_copy_to_user(arg, &sptr, sizeof(sptr))) {
                kmpp_loge_f("slot %d KMPP_SHM_IOC_GET_SHM kmpp_copy_to_user fail\n", i);
                ret = rk_nok;
            }
        }
    } break;
    case KMPP_SHM_IOC_PUT_SHM : {
        KmppObjIocArg ioc;
        rk_u32 i;

        ret = kmpp_copy_from_user(&ioc, arg, sizeof(ioc));
        if (ret) {
            kmpp_loge_f("KMPP_SHM_IOC_GET_SHM kmpp_copy_from_user KmppObjIocArg fail\n");
            break;
        }

        shm_dbg_ioctl("put shm %d\n", ioc.count);

        if (ioc.count > KMPP_SHM_IOC_MAX_COUNT) {
            kmpp_loge_f("KMPP_SHM_IOC_GET_SHM count %d over max %d\n",
                        ioc.count, KMPP_SHM_IOC_MAX_COUNT);
            break;
        }

        arg += sizeof(ioc);

        for (i = 0; i < ioc.count; i++, arg += sizeof(KmppShmPtr)) {
            KmppShmPtr sptr;
            KmppShmImpl *impl;

            ret = kmpp_copy_from_user(&sptr, arg, sizeof(sptr));
            if (ret) {
                kmpp_loge_f("slot %d KMPP_SHM_IOC_PUT_SHM kmpp_copy_from_user KmppShmPtr fail\n", i);
                break;
            }

            impl = kmpp_shm_from_shmptr_f(&sptr);
            if (!impl) {
                kmpp_loge_f("slot %d KMPP_SHM_IOC_PUT_SHM shm [u:k] %#llx:%#px check failed\n",
                            i, sptr.uaddr, sptr.kptr);
                break;
            }

            shm_dbg_ioctl("slot %d put shm k:%px u:%#llx\n", i, impl->kbase, impl->ubase);

            if (impl->kpriv) {
                if (kmpp_shm_debug & SHM_DBG_DUMP)
                    kmpp_obj_dump(impl->kpriv, "KMPP_SHM_IOC_PUT_SHM");

                ret = kmpp_obj_put_f(impl->kpriv);
            } else {
                ret = kmpp_shm_put(impl);
            }
        }
    } break;
    case KMPP_SHM_IOC_DUMP : {
        KmppObjShm ioc;

        ret = kmpp_copy_from_user(&ioc, arg, sizeof(ioc));
        if (ret) {
            kmpp_loge_f("KMPP_SHM_IOC_DUMP kmpp_copy_from_user fail\n");
        } else {
            KmppShmImpl *impl = (KmppShmImpl *)(uintptr_t)ioc.kobj_kaddr;

            if (!impl || impl != impl->kbase) {
                kmpp_loge_f("KMPP_SHM_IOC_DUMP invalid shm %px\n", impl);
                break;
            }

            if (impl->kpriv)
                ret = kmpp_obj_dump(impl->kpriv, "KMPP_SHM_IOC_DUMP");
        }
    } break;
    default : {
        kmpp_loge_f("KMPP_SHM_IOC_GET_SHM unknown cmd %x\n", cmd);
    } break;
    }

    shm_dbg_flow("leave - %s cmd %x\n", mgr->name, cmd);

    return ret;
}

rk_s32 kmpp_shm_read(KmppFsDev *file, rk_s32 offset, void **buf, rk_s32 *size)
{
    kmpp_objdef_dump_all();
    return 0;
}

static rk_s32 kmpp_shm_grp_root_size(void)
{
    return sizeof(KmppShmGrpRoot) + kmpp_spinlock_size();
}

rk_s32 kmpp_shm_mgr_put(KmppShmMgr *mgr)
{
    KmppShmGrpRoot *root, *n;
    KMPP_LIST_HEAD(list_root);

    if (!mgr) {
        kmpp_loge_f("invalid param mgr %px\n", mgr);
        return rk_nok;
    }

    if (kmpp_shm_cls) {
        kmpp_spin_lock(kmpp_shm_cls->lock);
        kmpp_list_del_init(&mgr->list_cls);
        kmpp_spin_unlock(kmpp_shm_cls->lock);
    } else {
        kmpp_list_del_init(&mgr->list_cls);
    }

    kmpp_spin_lock(mgr->lock);
    kmpp_list_for_each_entry_safe(root, n, &mgr->list_root, KmppShmGrpRoot, list_mgr) {
        kmpp_list_move_tail(&root->list_mgr, &list_root);
    }
    kmpp_spin_unlock(mgr->lock);

    kmpp_list_for_each_entry_safe(root, n, &list_root, KmppShmGrpRoot, list_mgr) {
        kmpp_shm_grp_root_deinit(root);
    }

    kmpp_spinlock_deinit(&mgr->lock);

    if (mgr->mgr) {
        kmpp_fs_dev_mgr_deinit(mgr->mgr);
        mgr->mgr = NULL;
    }

    kmpp_free(mgr);

    return rk_ok;
}

rk_s32 kmpp_shm_mgr_get(KmppShmMgr **mgr, const rk_u8 *name)
{
    KmppShmMgr *impl = NULL;
    KmppFsDevCfg cfg;
    rk_s32 lock_size = kmpp_spinlock_size();
    rk_s32 mgr_size;
    rk_s32 ret = rk_nok;

    if (!mgr || !name) {
        kmpp_loge_f("invalid param mgr %px name %s size %d\n", mgr, name);
        return ret;
    }

    *mgr = NULL;
    lock_size = kmpp_spinlock_size();
    mgr_size = kmpp_fs_dev_mgr_size(name);
    impl = kmpp_calloc(sizeof(*impl) + lock_size + mgr_size);
    if (!impl) {
        kmpp_loge_f("%s malloc size %d failed\n", name, mgr_size);
        goto done;
    }

    kmpp_spinlock_assign(&impl->lock, impl + 1, lock_size);

    cfg.cls = kmpp_shm_cls->cls;
    cfg.name = name;
    cfg.fops = &kmpp_shm_cls->fops;
    cfg.drv_data = impl;
    cfg.priv_size = kmpp_shm_grp_root_size();
    cfg.buf = (void *)((rk_u8 *)(impl + 1) + lock_size);
    cfg.size = mgr_size;

    ret = kmpp_fs_dev_mgr_init(&impl->mgr, &cfg);
    if (ret || !impl->mgr) {
        kmpp_loge_f("dev mgr %s init failed\n", name);
        goto done;
    } else {
        impl->name = cfg.name;

        KMPP_INIT_LIST_HEAD(&impl->list_cls);
        KMPP_INIT_LIST_HEAD(&impl->list_root);

        if (kmpp_shm_cls) {
            kmpp_spin_lock(kmpp_shm_cls->lock);
            kmpp_list_add_tail(&impl->list_cls, &kmpp_shm_cls->list_mgr);
            kmpp_spin_unlock(kmpp_shm_cls->lock);
        }

        ret = rk_ok;
    }

done:
    if (ret && impl) {
        kmpp_shm_mgr_put(impl);
        impl = NULL;
    }

    *mgr = impl;

    return ret;
}

static void kmpp_shm_show(KmppEnvNode env, void *data)
{
    KmppShmClsImpl *cls = kmpp_shm_cls;
    KmppShmMgr *mgr, *m;

    if (!cls)
        return;

    kmpp_spin_lock(cls->lock);

    kmpp_list_for_each_entry_safe(mgr, m, &cls->list_mgr, KmppShmMgr, list_cls) {
        KmppShmGrpRoot *root, *n;
        rk_s32 once = 1;

        kmpp_list_for_each_entry_safe(root, n, &mgr->list_root, KmppShmGrpRoot, list_mgr) {
            KmppShmImpl *shm, *o;
            KmppShmPool *pool;
            KmppShmList *list;
            rk_s32 i;

            if (once) {
                kmpp_env_log(env, "mgr %s:\n", mgr->name);
                once = 0;
            }

            kmpp_env_log(env, "  pid %d max count %d size %d\n",
                         root->pid, root->pool.max_count, root->pool.max_size);

            pool = &root->pool;

            kmpp_spin_lock(root->lock);

            kmpp_list_for_each_entry_safe(shm, o, &pool->list_used, KmppShmImpl, list_pool) {
                kmpp_env_log(env, "    %-10d [u:k] %lx:%px - used\n", shm->align_size,
                             shm->vm_shm->uaddr, shm->kbase);
            }

            for (i = 0; i < KMPP_SHM_POOL_MAX_SLOT; i++) {
                list = &pool->lists[i];

                if (!list->count)
                    continue;

                kmpp_list_for_each_entry_safe(shm, o, &list->list, KmppShmImpl, list_pool) {
                    kmpp_env_log(env, "    %-10d [u:k] %lx:%px - cached\n", shm->align_size,
                                 shm->vm_shm->uaddr, shm->kbase);
                }
            }

            kmpp_spin_unlock(root->lock);
        }
    }

    kmpp_spin_unlock(cls->lock);
}

static rk_s32 kmpp_shm_init(void)
{
    KmppEnvInfo env;
    rk_s32 lock_size = kmpp_spinlock_size();
    rk_s32 class_size = kmpp_class_size(kmpp_shm_name);
    rk_u8 *buf;

    if (kmpp_shm_cls) {
        kmpp_loge_f("kmpp_shm_cls is already inited\n");
        return rk_nok;
    }

    buf = kmpp_calloc(sizeof(KmppShmClsImpl) + lock_size + class_size);
    if (!buf) {
        kmpp_loge_f("kmpp_shm_cls malloc failed\n");
        return rk_nok;
    }

    kmpp_shm_cls = (KmppShmClsImpl *)buf;

    kmpp_spinlock_assign(&kmpp_shm_cls->lock, buf + sizeof(KmppShmClsImpl), lock_size);
    kmpp_class_assign(&kmpp_shm_cls->cls, kmpp_shm_name,
                      buf + sizeof(KmppShmClsImpl) + lock_size, class_size);

    kmpp_shm_cls->fops.open = kmpp_shm_open;
    kmpp_shm_cls->fops.release = kmpp_shm_release;
    kmpp_shm_cls->fops.ioctl = kmpp_shm_ioctl;
    kmpp_shm_cls->fops.read = kmpp_shm_read;

    KMPP_INIT_LIST_HEAD(&kmpp_shm_cls->list_mgr);

    env.type = KmppEnv_u32;
    env.readonly = 0;
    env.name = "kmpp_shm_debug";
    env.val = &kmpp_shm_debug;
    env.env_show = NULL;

    kmpp_env_add(kmpp_env_debug(), &env_shm_debug, &env);

    env.name = "shm_pool_info";
    env.readonly = 1;
    env.type = KmppEnv_user;
    env.val = NULL;
    env.env_show = kmpp_shm_show;

    kmpp_env_add(sys_get_env(), &env_shm_show, &env);

    kmpp_shm_mgr_get(&kmpp_shm_objs, "kmpp_objs");

    return rk_ok;
}

static rk_s32 kmpp_shm_deinit(void)
{
    KmppShmClsImpl *cls = KMPP_FETCH_AND(&kmpp_shm_cls, NULL);
    KmppShmMgr *mgr, *n;
    KMPP_LIST_HEAD(list);

    if (!cls)
        return rk_ok;

    kmpp_spin_lock(cls->lock);
    kmpp_list_for_each_entry_safe(mgr, n, &cls->list_mgr, KmppShmMgr, list_cls) {
        kmpp_list_move_tail(&mgr->list_cls, &list);
    }
    kmpp_spin_unlock(cls->lock);

    kmpp_list_for_each_entry_safe(mgr, n, &list, KmppShmMgr, list_cls) {
        kmpp_shm_mgr_put(mgr);
    }

    kmpp_spinlock_deinit(&cls->lock);
    kmpp_class_deinit(cls->cls);
    kmpp_free(cls);

    if (env_shm_debug) {
        kmpp_env_del(kmpp_env_debug(), env_shm_debug);
        env_shm_debug = NULL;
    }

    if (env_shm_show) {
        kmpp_env_del(sys_get_env(), env_shm_show);
        env_shm_show = NULL;
    }

    return rk_ok;
}

KMPP_MODULE_ADD(kmpp_sys, KMPP_SYS_SHM, shm, kmpp_shm_init, kmpp_shm_deinit);

rk_s32 kmpp_shm_add_trie_info(KmppTrie trie)
{
    rk_s32 val = sizeof(KmppShmImpl);

    kmpp_trie_add_info(trie, "__offset", &val, sizeof(val));

    /* add userspace private info position */
    val = offsetof(KmppShmImpl, upriv);
    kmpp_trie_add_info(trie, "__priv", &val, sizeof(val));

    /* add name offset from the objdef share root */
    val = offsetof(KmppShmImpl, name_offset);
    kmpp_trie_add_info(trie, "__name_offset", &val, sizeof(val));

    return rk_ok;
}

static inline KmppShmGrpRoot *get_grp_root(KmppShmGrp grp)
{
    KmppShmGrpImpl *impl = (KmppShmGrpImpl *)grp;
    KmppShmGrpRoot *root = impl ? (KmppShmGrpRoot *)impl->root : NULL;

    return root;
}

KmppFsDev *kmpp_shm_get_objs_file(void)
{
    KmppShmClsImpl *cls = kmpp_shm_cls;
    KmppShmMgr *mgr = kmpp_shm_objs;
    KmppFsDev *file = NULL;

    if (cls && mgr) {
        KmppShmGrpRoot *root, *n;
        rk_s32 pid = kmpp_getpid();

        kmpp_spin_lock(cls->lock);
        kmpp_list_for_each_entry_safe(root, n, &mgr->list_root, KmppShmGrpRoot, list_mgr) {
            if (root->pid == pid) {
                file = root->file;
                kmpp_logd_f("find root %px pid %d file %px\n", root, pid, file);
                break;
            }
        }
        /*
         * For fork process:
         * if cannot find the file, use parent pid to find again
         */
        if (!file) {
            pid = kmpp_getppid();
            kmpp_list_for_each_entry_safe(root, n, &mgr->list_root, KmppShmGrpRoot, list_mgr) {
                if (root->pid == pid) {
                    file = root->file;
                    kmpp_logd_f("find root %px ppid %d file %px\n", root, pid, file);
                    break;
                }
            }
        }
        kmpp_spin_unlock(cls->lock);
    }

    return file;
}

rk_s32 kmpp_shm_get(KmppShm *shm, KmppFsDev *file, const rk_u8 *name, rk_u32 size)
{
    KmppShmGrpImpl *grp = get_shm_grp(file, name, __FUNCTION__);;
    KmppShmGrpRoot *root;
    KmppShmImpl *impl;
    rk_s32 entry_size;

    if (!shm || !grp) {
        kmpp_loge_f("invalid param shm %px grp %px name %s\n", shm, grp, name);
        return rk_nok;
    }

    root = get_grp_root(grp);
    entry_size = (grp && grp->size) ? grp->size : size;
    entry_size += kmpp_shm_entry_offset();
    impl = get_shm_from_pool(&root->pool, entry_size);
    if (!impl) {
        kmpp_loge_f("get shm %s size %d from pool failed\n", name, entry_size);
        *shm = NULL;
        return rk_nok;
    }

    attach_shm(impl, grp);
    *shm = impl;

    return rk_ok;
}

rk_s32 kmpp_shm_put(KmppShm shm)
{
    KmppShmImpl *impl = (KmppShmImpl *)shm;
    KmppShmGrpImpl *grp = impl ? impl->grp : NULL;
    KmppShmGrpRoot *root = get_grp_root(grp);

    if (!impl || ! grp || !root) {
        kmpp_loge_f("invalid param shm %px grp %px root %px\n", impl, grp, root);
        return rk_nok;
    }

    kmpp_spin_lock(root->lock);
    kmpp_list_del_init(&impl->list_grp);
    kmpp_spin_unlock(root->lock);

    put_shm_to_pool(&root->pool, impl);

    return rk_ok;
}

rk_s32 kmpp_shm_set_late_deinit(KmppShm shm, rk_u32 enable)
{
    KmppShmImpl *impl = (KmppShmImpl *)shm;

    if (impl && impl->grp) {
        KmppShmGrpImpl *grp = impl->grp;
        kmpp_list_head *list = enable ? &grp->list_late : &grp->list_shm;

        kmpp_spin_lock(grp->lock);
        kmpp_list_move_tail(&impl->list_grp, list);
        kmpp_spin_unlock(grp->lock);

        return rk_ok;
    }

    return rk_nok;
}

rk_s32 kmpp_shm_dump(KmppShm shm, const rk_u8 *caller)
{
    KmppShmImpl *impl = (KmppShmImpl *)shm;

    if (!impl) {
        kmpp_loge_f("invalid param shm %px\n", impl);
        return rk_nok;
    }

    kmpp_logi("dump shm %px size %d at %s\n", impl, impl->align_size, caller);
    kmpp_logi("ioc [u:k] %#llx:%#llx\n", impl->ioc.kobj_uaddr, impl->ioc.kobj_kaddr);
    kmpp_logi("ubase %#llx uaddr %#llx upriv %#llx\n", impl->ubase,
              impl->ubase + kmpp_shm_entry_offset(), impl->upriv);
    kmpp_logi("kbase %#px kaddr %#px kpriv %#px\n", impl->kbase,
              impl->kbase + kmpp_shm_entry_offset(), impl->kpriv);

    return rk_ok;
}

rk_s32 kmpp_shm_check(KmppShm shm, const rk_u8 *caller)
{
    KmppShmImpl *impl = (KmppShmImpl *)shm;

    if (!impl) {
        kmpp_loge_f("invalid param shm %px at %s\n", impl, caller);
        return rk_nok;
    }

    if (impl != impl->kbase) {
        kmpp_loge_f("shm %px mismatch kbase %px at %s\n",
                    impl, impl->kbase, caller);
        return rk_nok;
    }

    if (impl->ubase != impl->ioc.kobj_uaddr) {
        kmpp_loge_f("shm %px mismatch ubase %llx - ioc ubase %llx at %s\n",
                    impl, impl->ubase, impl->ioc.kobj_uaddr, caller);
        return rk_nok;
    }

    return rk_ok;
}

KmppShm kmpp_shm_from_shmptr(KmppShmPtr *sptr, const rk_u8 *caller)
{
    if (sptr) {
        KmppShmImpl *impl = (KmppShmImpl *)sptr->kptr;

        if (!kmpp_shm_check(impl, caller))
            return impl;
    }

    return NULL;
}

KmppShmPtr *kmpp_shm_to_shmptr(KmppShm shm)
{
    KmppShmImpl *impl = (KmppShmImpl *)shm;

    shm_dbg_detail("set shm %px to shmptr [u:k] %#lx:%#llx\n",
                   impl, impl->ioc.kobj_uaddr, impl->ioc.kobj_kaddr);

    return impl ? (KmppShmPtr *)&impl->ioc : NULL;
}

void *kmpp_shm_get_kbase(KmppShm shm)
{
    KmppShmImpl *impl = (KmppShmImpl *)shm;

    return impl ? impl->kbase : NULL;
}

void *kmpp_shm_get_kaddr(KmppShm shm)
{
    KmppShmImpl *impl = (KmppShmImpl *)shm;

    return (impl && impl->kbase) ? (impl->kbase + kmpp_shm_entry_offset()) : NULL;
}

rk_ul kmpp_shm_get_ubase(KmppShm shm)
{
    KmppShmImpl *impl = (KmppShmImpl *)shm;

    return impl ? impl->ubase : 0;
}

rk_u64 kmpp_shm_get_uaddr(KmppShm shm)
{
    KmppShmImpl *impl = (KmppShmImpl *)shm;

    return impl && impl->kbase ? impl->ubase + kmpp_shm_entry_offset() : 0;
}

rk_s32 kmpp_shm_set_kpriv(KmppShm shm, void *kpriv)
{
    KmppShmImpl *impl = (KmppShmImpl *)shm;

    if (impl) {
        impl->kpriv = kpriv;
        return rk_ok;
    }

    return rk_nok;
}

void *kmpp_shm_get_kpriv(KmppShm shm)
{
    KmppShmImpl *impl = (KmppShmImpl *)shm;

    return impl ? impl->kpriv : NULL;
}

rk_u64 kmpp_shm_get_upriv(KmppShm shm)
{
    KmppShmImpl *impl = (KmppShmImpl *)shm;

    return impl ? (rk_u64)impl->upriv : 0;
}

rk_s32 kmpp_shm_get_entry_size(KmppShm shm)
{
    if (shm) {
        KmppShmImpl *impl = (KmppShmImpl *)shm;

        if (impl->grp)
            return impl->grp->size;
    }

    kmpp_loge_f("invalid param shm %px\n", shm);

    return 0;
}

EXPORT_SYMBOL(kmpp_shm_get);
EXPORT_SYMBOL(kmpp_shm_put);
EXPORT_SYMBOL(kmpp_shm_dump);
EXPORT_SYMBOL(kmpp_shm_check);

EXPORT_SYMBOL(kmpp_shm_to_shmptr);
EXPORT_SYMBOL(kmpp_shm_from_shmptr);

EXPORT_SYMBOL(kmpp_shm_get_kbase);
EXPORT_SYMBOL(kmpp_shm_get_kaddr);
EXPORT_SYMBOL(kmpp_shm_get_ubase);
EXPORT_SYMBOL(kmpp_shm_get_uaddr);

EXPORT_SYMBOL(kmpp_shm_set_kpriv);
EXPORT_SYMBOL(kmpp_shm_get_kpriv);
EXPORT_SYMBOL(kmpp_shm_get_upriv);
EXPORT_SYMBOL(kmpp_shm_get_entry_size);

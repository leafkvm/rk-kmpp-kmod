/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_class"

#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/of_platform.h>
#include <linux/version_compat_defs.h>
#include <linux/poll.h>

#include "rk-mpp-kobj.h"

#include "kmpp_macro.h"
#include "kmpp_atomic.h"
#include "kmpp_env.h"
#include "kmpp_mem.h"
#include "kmpp_log.h"
#include "kmpp_bitops.h"
#include "kmpp_mutex.h"
#include "kmpp_module.h"
#include "kmpp_osal_id.h"
#include "kmpp_dev_impl.h"
#include "kmpp_cls.h"
#include "kmpp_string.h"

#define KMPP_CLS_DBG_FLOW               (0x00000001)
#define KMPP_CLS_DBG_FOPS               (0x00000002)
#define KMPP_CLS_DBG_DEV                (0x00000004)
#define KMPP_CLS_DBG_VM                 (0x00000008)
#define KMPP_CLS_DBG_DETAIL             (0x00000010)

#define kmpp_cls_dbg(flag, fmt, ...)    kmpp_dbg(kmpp_cls_debug, flag, fmt, ## __VA_ARGS__)
#define kmpp_cls_dbg_f(flag, fmt, ...)  kmpp_dbg_f(kmpp_cls_debug, flag, fmt, ## __VA_ARGS__)

#define kmpp_cls_dbg_flow(fmt, ...)     kmpp_cls_dbg_f(KMPP_CLS_DBG_FLOW, fmt, ## __VA_ARGS__)
#define kmpp_cls_dbg_fops(fmt, ...)     kmpp_cls_dbg_f(KMPP_CLS_DBG_FOPS, fmt, ## __VA_ARGS__)
#define kmpp_cls_dbg_dev(fmt, ...)      kmpp_cls_dbg_f(KMPP_CLS_DBG_DEV, fmt, ## __VA_ARGS__)
#define kmpp_cls_dbg_vm(fmt, ...)       kmpp_cls_dbg_f(KMPP_CLS_DBG_VM, fmt, ## __VA_ARGS__)
#define kmpp_cls_dbg_detail(fmt, ...)   kmpp_cls_dbg_f(KMPP_CLS_DBG_DETAIL, fmt, ## __VA_ARGS__)

#define PENDING_NODE_ID_MIN             0
#define PENDING_NODE_ID_MAX             (BITS_PER_LONG)
#define PGOFF_PENDING_NODE_BASE         (64ul)
#define PGOFF_PENDING_NODE_END          (PGOFF_PENDING_NODE_BASE + PENDING_NODE_ID_MAX)

#define PENDING_NODE_START              ((PGOFF_PENDING_NODE_BASE) << PAGE_SHIFT)
#define TO_PENDING_NODE_IDX(x)          ((PENDING_NODE_START) + ((x) << PAGE_SHIFT))
#define TO_VM_MMAP_ID(x)                ((x - (PENDING_NODE_START)) >> PAGE_SHIFT)
#define PENDING_NODE_END                TO_PENDING_NODE_IDX(BITS_PER_LONG)

#define vm_to_node(x)                   container_of(x, KmppVmNode, vm)
#define fsdev_to_impl(x)                container_of(x, KmppFsDevImpl, dev)

//static rk_u32 kmpp_cls_debug = KMPP_CLS_DBG_FOPS | KMPP_CLS_DBG_FLOW;
static rk_u32 kmpp_cls_debug = 0;

typedef struct KmppFsDevImpl_t {
    /* export to external user */
    KmppFsDev               dev;

    /* vm management */
    rk_ul                   vm_id_bitmap;
    KmppMutex               *lock;
    kmpp_list_head          list_idle;
    kmpp_list_head          list_pending;
    kmpp_list_head          list_mapped;
    kmpp_list_head          list_failed;
} KmppFsDevImpl;

typedef struct KmppVmNode_t {
    KmppFsVm                vm;
    kmpp_list_head          list;
    KmppAtomic              *refcnt;

    rk_s32                  size_aligned;
    rk_s32                  pending_id;
} KmppVmNode;

static rk_s32 kmpp_class_srv_init(void)
{
    KmppEnvInfo info = {
        .type       = KmppEnv_u32,
        .readonly   = 0,
        .name       = "kmpp_cls_debug",
        .val        = &kmpp_cls_debug,
        .env_show   = NULL,
    };

    return kmpp_env_add(kmpp_env_debug(), NULL, &info);
}

KMPP_MODULE_ADD(kmpp_osal, KMPP_OSAL_CLASS, class,
                kmpp_class_srv_init, NULL);

rk_s32 kmpp_class_size(const rk_u8 *name)
{
    return KMPP_ALIGN(kmpp_strlen(name) + 1, sizeof(rk_u64)) + sizeof(KmppClsImpl);
}
EXPORT_SYMBOL(kmpp_class_size);

rk_s32 kmpp_class_init(KmppCls **cls, const rk_u8 *name)
{
    KmppClsImpl *impl = NULL;
    rk_s32 ret = rk_nok;

    if (!cls || !name) {
        kmpp_loge_f("invalid param cls %px name %pn\n", cls, name);
        return ret;
    }

    do {
        rk_s32 str_len = ALIGN(kmpp_strlen(name) + 1, sizeof(rk_u64));
        rk_u8 *str_buf;

        impl = kmpp_calloc(sizeof(*impl) + str_len);
        if (!impl) {
            kmpp_loge_f("alloc fs class impl size %d failed\n", sizeof(*impl) + str_len);
            break;
        }

        impl->need_free = 1;
        str_buf = (rk_u8 *)(impl + 1);
        kmpp_strcpy(str_buf, name);
        impl->cls.name = str_buf;
        impl->class = class_create(THIS_MODULE, str_buf);
        if (PTR_ERR_OR_ZERO(impl->class)) {
            impl->class = NULL;
            kmpp_loge_f("create kmpp class failed\n");
            break;
        }

        ret = rk_ok;
    } while (0);

    if (ret && impl) {
        kmpp_free(impl);
        impl = NULL;
    }

    *cls = &impl->cls;

    return rk_ok;
}
EXPORT_SYMBOL(kmpp_class_init);

rk_s32 kmpp_class_assign(KmppCls **cls, const rk_u8 *name, void *buf, rk_s32 size)
{
    KmppClsImpl *impl = NULL;
    rk_s32 require_size;
    rk_s32 ret = rk_nok;

    if (!cls || !name || !buf || size <= 0) {
        kmpp_loge_f("invalid param cls %px name %px buf %px size %d\n",
                    cls, name, buf, size);
        return ret;
    }

    require_size = kmpp_class_size(name);
    if (size < require_size) {
        kmpp_loge_f("invalid param size %d require size %d\n", size, require_size);
        return ret;
    }

    impl = buf;

    do {
        rk_u8 *str_buf = (rk_u8 *)(impl + 1);

        kmpp_strcpy(str_buf, name);
        impl->cls.name = str_buf;
        impl->class = class_create(THIS_MODULE, str_buf);
        if (PTR_ERR_OR_ZERO(impl->class)) {
            impl->class = NULL;
            kmpp_loge_f("create kmpp class failed\n");
            break;
        }

        ret = rk_ok;
    } while (0);

    *cls = ret ? NULL : &impl->cls;

    return ret;
}
EXPORT_SYMBOL(kmpp_class_assign);

rk_s32 kmpp_class_deinit(KmppCls *cls)
{
    if (cls) {
        KmppClsImpl *impl = cls_to_impl(cls);

        class_destroy(impl->class);

        if (impl->need_free)
            kmpp_free(impl);
    }

    return rk_ok;
}
EXPORT_SYMBOL(kmpp_class_deinit);

/* memory layout: KmppFsDevImpl is before struct cdev */
static int kmpp_fs_open(struct inode *inode, struct file *file)
{
    KmppFsDevMgr *mgr = ((void *)inode->i_cdev) - sizeof(*mgr);
    KmppFsDevImpl *impl = NULL;
    KmppFsDev *dev = NULL;
    rk_s32 lock_size = kmpp_mutex_size();
    rk_s32 ret = rk_nok;

    impl = kmpp_calloc(sizeof(KmppFsDevImpl) + lock_size + mgr->priv_size);
    if (!impl) {
        kmpp_loge_f("failed to create private_data size %d\n", mgr->priv_size);
        return rk_nok;
    }

    dev = &impl->dev;
    impl->vm_id_bitmap = -1;
    kmpp_mutex_assign(&impl->lock, (void *)(impl + 1), lock_size);
    KMPP_INIT_LIST_HEAD(&impl->list_idle);
    KMPP_INIT_LIST_HEAD(&impl->list_pending);
    KMPP_INIT_LIST_HEAD(&impl->list_mapped);

    dev->name = mgr->name;
    dev->mgr = mgr;
    dev->drv_data = mgr->drv_data;
    dev->file_data = file;
    dev->priv_data = (void *)((RK_U8 *)impl->lock + lock_size);
    dev->priv_size = mgr->priv_size;

    kmpp_cls_dbg_fops("%s mgr %px drv_data %px priv %px\n",
                      dev->name, dev->drv_data, dev->priv_data);

    if (mgr->fops.open) {
        ret = mgr->fops.open(dev);
        if (ret) {
            kmpp_free(impl);
            return ret;
        }
    }

    file->private_data = impl;

    return nonseekable_open(inode, file);
}

static int kmpp_fs_release(struct inode *inode, struct file *file)
{
    KmppFsDevImpl *impl = file->private_data;
    KmppFsDev *dev = &impl->dev;
    KmppFsDevMgr *mgr = dev->mgr;
    rk_s32 ret = rk_nok;

    file->private_data = NULL;

    kmpp_cls_dbg_fops("%s mgr %px drv_data %px priv %px\n", dev->name,
                      mgr, mgr->drv_data, dev->priv_data);

    if (mgr->fops.release)
        ret = mgr->fops.release(dev);

    {
        KmppVmNode *node, *tmp;
        KMPP_LIST_HEAD(list);

        kmpp_mutex_lock(impl->lock);
        kmpp_list_for_each_entry_safe(node, tmp, &impl->list_idle, KmppVmNode, list) {
            kmpp_list_move_tail(&node->list, &list);
        }
        kmpp_list_for_each_entry_safe(node, tmp, &impl->list_pending, KmppVmNode, list) {
            kmpp_list_move_tail(&node->list, &list);
        }
        kmpp_list_for_each_entry_safe(node, tmp, &impl->list_mapped, KmppVmNode, list) {
            kmpp_list_move_tail(&node->list, &list);
        }
        kmpp_mutex_unlock(impl->lock);

        kmpp_list_for_each_entry_safe(node, tmp, &list, KmppVmNode, list) {
            kmpp_fs_vm_munmap(&node->vm);
        }
    }

    kmpp_mutex_deinit(&impl->lock);

    kmpp_free(impl);

    return ret;
}

static long kmpp_fs_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    KmppFsDevImpl *impl = file->private_data;
    KmppFsDev *dev = &impl->dev;
    KmppFsDevMgr *mgr = dev->mgr;
    rk_s32 ret = rk_nok;

    kmpp_cls_dbg_fops("%s priv %px cmd %x arg %px\n",
                      dev->name, dev->priv_data, cmd, arg);

    if (mgr->fops.ioctl)
        ret = mgr->fops.ioctl(dev, cmd, (void *)arg);

    return ret;
}

static ssize_t kmpp_fs_read(struct file *file, char __user *buffer, size_t length, loff_t *offset)
{
    KmppFsDevImpl *impl = file->private_data;
    KmppFsDev *dev = &impl->dev;
    KmppFsDevMgr *mgr = dev->mgr;
    rk_s32 ret = rk_nok;

    kmpp_cls_dbg_fops("%s priv %px buffer %px length %d offset %d\n",
                      dev->name, dev->priv_data, buffer, length, (rk_s32)*offset);

    if (mgr->fops.read) {
        void *buf = NULL;
        rk_s32 len = 0;
        rk_s32 start = (offset) ? *offset : 0;

        ret = mgr->fops.read(dev, start, &buf, &len);
        if (!ret && buf && len > 0) {
            ret = copy_to_user(buffer, buf, len);
            if (!ret) {
                *offset += len;
            } else {
                kmpp_loge_f("copy_to_user %px -> %px len %d failed %d\n",
                            buf, buffer, len, ret);
            }
        }
    }

    return ret;
}

#define vm_node_unref_f(node) vm_node_unref(node, __FUNCTION__)
static rk_s32 vm_node_unref(KmppVmNode *node, const rk_u8 *func)
{
    if (!kmpp_atomic_dec_return(node->refcnt)) {
        KmppFsDevImpl *dev = fsdev_to_impl(node->vm.dev);

        kmpp_cls_dbg_vm("free node at %s\n", func);
        kmpp_mutex_lock(dev->lock);
        kmpp_list_del_init(&node->list);
        kmpp_mutex_unlock(dev->lock);
        kmpp_free(node);
        return 1;
    }

    kmpp_cls_dbg_vm("after dec return refcnt %d at %s\n",
                    kmpp_atomic_read(node->refcnt), func);

    return 0;
}

static void kmpp_fs_vm_open(struct vm_area_struct *vma)
{
    KmppVmNode *node = vma->vm_private_data;

    /*
     * Called when the vma is duplicated (vma split by partial munmap /
     * mprotect / mremap, or fork without VM_DONTCOPY). Each duplicated
     * vma gets its own vm_close, so each must hold its own reference,
     * otherwise the node refcnt underflows and the node is freed while
     * mappings still point to it.
     */
    if (node && node->vm.dev) {
        kmpp_atomic_inc_return(node->refcnt);
        kmpp_cls_dbg_vm("vma %px kaddr %px uaddr %#lx refcnt %d\n",
                        vma, node->vm.kaddr, node->vm.uaddr,
                        kmpp_atomic_read(node->refcnt));
    } else {
        kmpp_loge_f("invalid node %px dev %px\n", node, node ? node->vm.dev : NULL);
    }
}

static void kmpp_fs_vm_close(struct vm_area_struct *vma)
{
    KmppVmNode *node = KMPP_FETCH_AND(&vma->vm_private_data, NULL);

    if (node && node->vm.dev) {
        kmpp_cls_dbg_vm("vma %px kaddr %px uaddr %#lx refcnt %d\n",
                        vma, node->vm.kaddr, node->vm.uaddr,
                        kmpp_atomic_read(node->refcnt));

        vm_node_unref_f(node);
    } else {
        kmpp_loge_f("invalid node %px dev %px\n", node, node ? node->vm.dev : NULL);
    }
}

static const struct vm_operations_struct kmpp_fs_vm_ops = {
    .open   = kmpp_fs_vm_open,
    .close  = kmpp_fs_vm_close,
};

static int kmpp_fs_mmap(struct file *file, struct vm_area_struct *vma)
{
    KmppFsDevImpl *impl = file->private_data;
    KmppFsDev *dev = &impl->dev;
    KmppFsDevMgr *mgr = dev->mgr;
    KmppVmNode *node = NULL;
    KmppVmNode *tmp = NULL;
    rk_s32 pending_id = -1;
    rk_s32 ret = rk_nok;

    kmpp_cls_dbg_fops("%s priv %px vma [%#lx:%#lx] pgoff %#x\n", dev->name,
                      dev->priv_data, vma->vm_start, vma->vm_end, vma->vm_pgoff);

    /* setup vm_open and vm_close */
    vma->vm_ops = &kmpp_fs_vm_ops;

    if (vma->vm_pgoff < PGOFF_PENDING_NODE_BASE ||
        vma->vm_pgoff >= PGOFF_PENDING_NODE_END) {
        kmpp_loge_f("invalid pgoff %d not in range [%d:%d]\n", vma->vm_pgoff,
                    PGOFF_PENDING_NODE_BASE, PGOFF_PENDING_NODE_END);
        return rk_nok;
    }

    pending_id = vma->vm_pgoff - PGOFF_PENDING_NODE_BASE;

    kmpp_cls_dbg_vm("pending_id %d\n", pending_id);

    /* NOTE: clear pgoff */
    vma->vm_pgoff = 0;
    vma->vm_private_data = NULL;
    /*
     * VM_DONTCOPY: children must not inherit these mappings - uaddr
     * bookkeeping is per owner process and a forked child exiting or
     * execing would drop node references it never took.
     * VM_DONTEXPAND: block mremap growing into unbacked pages.
     */
    vm_flags_set(vma, VM_ACCOUNT | VM_DONTCOPY | VM_DONTEXPAND | VM_DONTDUMP);

    kmpp_mutex_lock(impl->lock);

    kmpp_list_for_each_entry_safe(node, tmp, &impl->list_pending, KmppVmNode, list) {
        if (node->pending_id == pending_id) {
            ret = remap_vmalloc_range(vma, node->vm.kaddr, 0);
            if (ret < 0) {
                kmpp_loge_f("remap_vmalloc_range %px failed ret %d\n",
                            node->vm.kaddr, ret);
                break;
            }

            node->vm.uaddr = vma->vm_start;
            vma->vm_private_data = node;
            kmpp_atomic_inc_return(node->refcnt);
            kmpp_list_move_tail(&node->list, &impl->list_mapped);

            kmpp_cls_dbg_vm("%s mmap - kaddr %px -> %#lx refcnt %d\n",
                            mgr->name, node->vm.kaddr, vma->vm_start,
                            kmpp_atomic_read(node->refcnt));

            ret = rk_ok;
            break;
        }
    }

    kmpp_set_bit(pending_id, &impl->vm_id_bitmap);
    if (ret)
        kmpp_list_del_init(&node->list);

    kmpp_mutex_unlock(impl->lock);

    kmpp_cls_dbg_fops("%s kaddr %px -> vma [%#lx:%#lx] ret %d\n", dev->name,
                      node->vm.kaddr, vma->vm_start, vma->vm_end, ret);

    return ret;
}

static unsigned int kmpp_fs_poll(struct file *file, poll_table *wait)
{
    KmppFsDevImpl *impl = file->private_data;
    KmppFsDev *dev = &impl->dev;
    KmppFsDevMgr *mgr = dev->mgr;
    rk_s32 ret = rk_nok;

    kmpp_cls_dbg_fops("%s priv %px\n", dev->name, dev->priv_data);

    if (mgr->fops.ioctl)
        ret = mgr->fops.poll(dev, (void*)wait);

    return ret;
}

static struct file_operations kmpp_fs_dev_fops = {
    .owner          = THIS_MODULE,
    .read           = kmpp_fs_read,
    .open           = kmpp_fs_open,
    .release        = kmpp_fs_release,
    .unlocked_ioctl = kmpp_fs_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl   = kmpp_fs_ioctl,
#endif
    .mmap           = kmpp_fs_mmap,
    .poll           = kmpp_fs_poll,
};

rk_s32 kmpp_fs_dev_mgr_size(const rk_u8 *name)
{
    return sizeof(KmppFsDevMgr) + sizeof(struct cdev) +
           KMPP_ALIGN(kmpp_strlen(name) + 1, sizeof(rk_u64));
}
EXPORT_SYMBOL(kmpp_fs_dev_mgr_size);

rk_s32 kmpp_fs_dev_mgr_init(KmppFsDevMgr **mgr, KmppFsDevCfg *cfg)
{
    KmppFsDevMgr *impl = NULL;
    KmppDevFops *fops = NULL;
    const rk_u8 *name = NULL;
    KmppCls *cls = NULL;
    rk_s32 total_size = 0;
    rk_u8 *name_buf = NULL;
    rk_s32 ret = rk_nok;

    if (!mgr || !cfg || !cfg->cls || !cfg->name) {
        kmpp_loge_f("can not init fs device with NULL class or NULL name\n");
        goto done;
    }

    cls = cfg->cls;
    name = cfg->name;
    fops = cfg->fops;

    total_size = kmpp_fs_dev_mgr_size(name);
    if (!cfg->buf || cfg->size < total_size) {
        impl = kmpp_calloc(total_size);
        if (!impl) {
            kmpp_loge_f("alloc dev fs size %d failed\n", total_size);
            goto done;
        }
        impl->need_free = 1;
    } else {
        impl = (KmppFsDevMgr *)cfg->buf;
    }

    impl->cls = cls_to_impl(cls)->class;
    impl->cdev_size = sizeof(struct cdev);
    impl->cdev = (struct cdev *)(impl + 1);

    if (fops)
        impl->fops = *fops;

    impl->drv_data = cfg->drv_data;
    impl->priv_size = cfg->priv_size;
    name_buf = (rk_u8 *)(impl->cdev + 1);
    cfg->name = name_buf;

    ret = alloc_chrdev_region(&impl->dev_id, 0, 1, name);
    if (ret) {
        kmpp_loge_f("alloc dev id failed\n");
        goto done;
    }

    kmpp_strcpy(name_buf, name);
    impl->name = name_buf;

    cdev_init(impl->cdev, &kmpp_fs_dev_fops);
    impl->cdev->owner = THIS_MODULE;

    ret = cdev_add(impl->cdev, impl->dev_id, 1);
    if (ret) {
        kmpp_loge_f("add cdev failed\n");
        goto done;
    }

    impl->dev = device_create(impl->cls, NULL, impl->dev_id, NULL, name_buf);
    if (PTR_ERR_OR_ZERO(impl->dev)) {
        kmpp_loge_f("device create failed\n");
        goto done;
    }

done:
    if (ret && impl) {
        kmpp_fs_dev_mgr_deinit(impl);
        impl = NULL;
    }

    *mgr = impl;

    kmpp_cls_dbg_flow("name %s ret %px\n", cfg->name, impl);

    return ret;
}
EXPORT_SYMBOL(kmpp_fs_dev_mgr_init);

rk_s32 kmpp_fs_dev_mgr_deinit(KmppFsDevMgr *mgr)
{
    if (mgr) {
        if (mgr->cdev) {
            cdev_del(mgr->cdev);
            mgr->cdev = NULL;
        }

        if (mgr->dev_id) {
            device_destroy(mgr->cls, mgr->dev_id);
            unregister_chrdev_region(mgr->dev_id, 1);
        }

        if (mgr->need_free)
            kmpp_free(mgr);
    }

    return rk_ok;
}
EXPORT_SYMBOL(kmpp_fs_dev_mgr_deinit);

rk_s32 kmpp_fs_vm_mmap(KmppFsVm **vm, KmppFsDev *dev, void *kaddr, rk_s32 size, rk_ul prot)
{
    KmppFsDevImpl *impl = (KmppFsDevImpl *)dev;
    KmppVmNode *node = NULL;
    rk_s32 pending_id;
    rk_ul uaddr = 0;
    rk_s32 ret = rk_nok;

    if (!vm || !dev || !kaddr || !size) {
        kmpp_loge_f("invalid param vm %px dev %px kaddr %px, size %d\n",
                    vm, dev, kaddr, size);
        return rk_nok;
    }

    kmpp_mutex_lock(impl->lock);

    pending_id = kmpp_find_first_bit(&impl->vm_id_bitmap, BITS_PER_LONG);
    if (pending_id >= BITS_PER_LONG) {
        kmpp_loge_f("no pending id\n");
        kmpp_mutex_unlock(impl->lock);
        goto done;
    }

    node = kmpp_list_first_entry_or_null(&impl->list_idle, KmppVmNode, list);
    if (!node) {
        node = kmpp_calloc(sizeof(KmppVmNode) + kmpp_atomic_size());
        if (node) {
            KMPP_INIT_LIST_HEAD(&node->list);
            kmpp_atomic_assign(&node->refcnt, (void *)(node + 1), kmpp_atomic_size());
        }
    }

    if (!node) {
        kmpp_loge_f("alloc vm node failed\n");
        kmpp_mutex_unlock(impl->lock);
        goto done;
    }

    kmpp_atomic_fetch_inc(node->refcnt);
    node->vm.dev = &impl->dev;
    node->vm.priv_data = NULL;
    node->vm.kaddr = kaddr;
    node->vm.size = size;
    node->vm.prot = prot;
    node->vm.uaddr = 0;
    node->size_aligned = PAGE_ALIGN(size);
    node->pending_id = pending_id;

    kmpp_clear_bit(pending_id, &impl->vm_id_bitmap);
    kmpp_list_move_tail(&node->list, &impl->list_pending);

    kmpp_mutex_unlock(impl->lock);

    uaddr = vm_mmap(dev->file_data, 0, node->size_aligned,
                    (prot == KMPP_VM_PROT_RO) ? PROT_READ : PROT_READ | PROT_WRITE,
                    MAP_SHARED, TO_PENDING_NODE_IDX(pending_id));

    if (IS_ERR_VALUE(uaddr)) {
        kmpp_loge_f("vm_mmap %d %s fail ret %ld\n", node->size_aligned,
                    node->vm.prot ? "rw" : "ro", uaddr);
        vm_node_unref_f(node);

        uaddr = 0;
        goto done;
    }

    node->vm.uaddr = uaddr;
    ret = rk_ok;

done:

    *vm = node ? &node->vm : NULL;

    return ret;
}
EXPORT_SYMBOL(kmpp_fs_vm_mmap);

rk_s32 kmpp_fs_vm_munmap(KmppFsVm *vm)
{
    KmppVmNode *node = vm_to_node(vm);
    rk_s32 ret = rk_ok;

    if (!vm || !vm->dev || !vm->kaddr || !vm->uaddr) {
        kmpp_loge_f("invalid param vm %px dev %px kaddr %px uaddr %#lx\n",
                    vm, vm ? vm->dev : NULL, vm ? vm->kaddr : NULL,
                    vm ? vm->uaddr : 0);
        return rk_nok;
    }

    node = vm_to_node(vm);

    if (!vm_node_unref_f(node)) {
        void *mm = kmpp_get_curr_mm();

        /* when the user process killed, the mm will be free earlier */
        if (mm) {
            ret = vm_munmap(vm->uaddr, PAGE_ALIGN(vm->size));
            if (ret)
                kmpp_loge_f("vm_munmap fail [%#lx:%d] ret %ld\n",
                            vm->uaddr, vm->size, ret);
            kmpp_put_mm(mm);
        }
    }

    return ret;
}
EXPORT_SYMBOL(kmpp_fs_vm_munmap);

rk_s32 kmpp_getpid(void)
{
    return task_tgid_vnr(current);
}
EXPORT_SYMBOL(kmpp_getpid);

rk_s32 kmpp_getppid(void)
{
    return task_ppid_nr(current);
}
EXPORT_SYMBOL(kmpp_getppid);

rk_s32 kmpp_gettid(void)
{
    return task_pid_nr(current);
}
EXPORT_SYMBOL(kmpp_gettid);

void *kmpp_get_curr_mm(void)
{
    return current ? get_task_mm(current) : NULL;
}
EXPORT_SYMBOL(kmpp_get_curr_mm);

void kmpp_put_mm(void *mm)
{
    if (mm)
        mmput(mm);
}
EXPORT_SYMBOL(kmpp_put_mm);

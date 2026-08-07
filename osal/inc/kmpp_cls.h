/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_CLS_H__
#define __KMPP_CLS_H__

#include "kmpp_dev.h"

#define KMPP_VM_PROT_RO     0x0
#define KMPP_VM_PROT_RW     0x1

typedef struct KmppCls_t {
    const rk_u8     *name;
} KmppCls;

typedef struct KmppFsDevMgr_t KmppFsDevMgr;

typedef struct KmppFsDev_t {
    /* device name */
    const rk_u8     *name;
    /* device manager pointer */
    KmppFsDevMgr    *mgr;
    /* device global data pointer registered on managner init */
    void            *drv_data;
    /* device local struct file pointer */
    void            *file_data;
    /* device local data pointer on file handle open */
    void            *priv_data;
    rk_s32          priv_size;
} KmppFsDev;

typedef struct KmppFsVm_t {
    KmppFsDev       *dev;
    /* input */
    void            *kaddr;
    rk_s32          size;
    rk_ul           prot;

    /* output */
    rk_ul           uaddr;
    /* vma */
    void            *priv_data;
} KmppFsVm;

typedef rk_s32 (*KmppFsOpen)(KmppFsDev *dev);
typedef rk_s32 (*KmppFsRelease)(KmppFsDev *dev);
typedef rk_s32 (*KmppFsIoctl)(KmppFsDev *dev, rk_s32 cmd, void *arg);
typedef rk_s32 (*KmppFsRead)(KmppFsDev *dev, rk_s32 offset, void **buf, rk_s32 *size);
typedef rk_s32 (*KmppFsPoll)(KmppFsDev *dev, void *poll_table);

typedef rk_s32 (*KmppFsVmMmap)(KmppFsDev *dev, KmppFsVm *vm);
typedef rk_s32 (*KmppFsVmOpen)(KmppFsDev *dev, KmppFsVm *vm);
typedef rk_s32 (*KmppFsVmClose)(KmppFsDev *dev, KmppFsVm *vm);

typedef struct KmppDevFops_t {
    KmppFsOpen      open;
    KmppFsRelease   release;
    KmppFsIoctl     ioctl;
    KmppFsRead      read;
    KmppFsPoll      poll;
} KmppDevFops;

typedef struct KmppFsDevCfg_t {
    KmppCls         *cls;
    const rk_u8     *name;      /* name will be update to mgr internal address on kmpp_fs_dev_mgr_init */
    KmppDevFops     *fops;

    /* for driver global data and file handle private data */
    void            *drv_data;
    rk_s32          priv_size;

    /* for buffer assign */
    void            *buf;
    rk_s32          size;
} KmppFsDevCfg;

rk_s32 kmpp_class_size(const rk_u8 *name);
rk_s32 kmpp_class_init(KmppCls **cls, const rk_u8 *name);
rk_s32 kmpp_class_assign(KmppCls **cls, const rk_u8 *name, void *buf, rk_s32 size);
rk_s32 kmpp_class_deinit(KmppCls *cls);

rk_s32 kmpp_fs_dev_mgr_size(const rk_u8 *name);
rk_s32 kmpp_fs_dev_mgr_init(KmppFsDevMgr **mgr, KmppFsDevCfg *cfg);
rk_s32 kmpp_fs_dev_mgr_deinit(KmppFsDevMgr *mgr);

rk_s32 kmpp_fs_vm_mmap(KmppFsVm **vm, KmppFsDev *dev, void *kaddr, rk_s32 size, rk_ul prot);
rk_s32 kmpp_fs_vm_munmap(KmppFsVm *vm);

rk_s32 kmpp_getpid(void);
rk_s32 kmpp_getppid(void);
rk_s32 kmpp_gettid(void);

void *kmpp_get_curr_mm(void);
void kmpp_put_mm(void *mm);

#endif /* __KMPP_CLS_H__ */
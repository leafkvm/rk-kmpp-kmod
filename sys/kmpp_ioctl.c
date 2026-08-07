/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_ioc"

#include <asm/ioctl.h>
#include <linux/mm.h>
#include <linux/mman.h>

#include "rk-mpp-kobj.h"
#include "kmpp_list.h"

#include "kmpp_cls.h"
#include "kmpp_env.h"
#include "kmpp_log.h"
#include "kmpp_mem.h"
#include "kmpp_module.h"
#include "kmpp_spinlock.h"
#include "kmpp_uaccess.h"

#include "kmpp_obj.h"
#include "kmpp_shm.h"
#include "kmpp_sys_id.h"
#include "kmpp_ioctl_impl.h"

#define IOC_DBG_FLOW                (0x00000001)
#define IOC_DBG_INFO                (0x00000002)
#define IOC_DBG_IOCTL               (0x00000004)
#define IOC_DBG_FILE                (0x00000008)

#define ioc_dbg(flag, fmt, ...)     kmpp_dbg(kmpp_ioc_debug, flag, fmt, ## __VA_ARGS__)

#define ioc_dbg_flow(fmt, ...)      ioc_dbg(IOC_DBG_FLOW, fmt, ## __VA_ARGS__)
#define ioc_dbg_info(fmt, ...)      ioc_dbg(IOC_DBG_INFO, fmt, ## __VA_ARGS__)
#define ioc_dbg_ioctl(fmt, ...)     ioc_dbg(IOC_DBG_IOCTL, fmt, ## __VA_ARGS__)
#define ioc_dbg_file(fmt, ...)      ioc_dbg(IOC_DBG_FILE, fmt, ## __VA_ARGS__)

#define KMPP_IOCTL_IOC_MAGIC        'i'
#define KMPP_IOCTL_IOC_QUERY_INFO   _IOW(KMPP_IOCTL_IOC_MAGIC, 1, unsigned int)
#define KMPP_IOCTL_IOC_PROC         _IOW(KMPP_IOCTL_IOC_MAGIC, 2, unsigned int)

#define get_ioc_srv(caller) \
    ({ \
        KmppIocSrv *__tmp; \
        if (srv_ioc) { \
            __tmp = srv_ioc; \
        } else { \
            kmpp_err_f("kmpp buf srv not init at %s : %s\n", __FUNCTION__, caller); \
            __tmp = NULL; \
        } \
        __tmp; \
    })

#define get_ioc_srv_f() get_ioc_srv(__FUNCTION__)

typedef struct KmppIocSrv_t {
    KmppSpinlock        *lock;

    /* /dev/kmpp_ioc device info */
    KmppCls             *cls;
    KmppDevFops         fops;
    KmppFsDevMgr        *mgr;
    KmppEnvNode         env;

    kmpp_list_head      list_file;

    rk_s32              file_id;
    rk_s32              file_cnt;
} KmppIocSrv;

typedef struct KmppIocMgr_t {
    kmpp_list_head      list_srv;
    KmppObjDefSet       *defs;

    /* objdef ioctl info trie buffer */
    KmppFsVm            *vm_trie;
    void                *buf;
    rk_s32              buf_size;

    rk_s32              file_id;
} KmppIocMgr;

static KmppIocSrv *srv_ioc = NULL;
static rk_u32 kmpp_ioc_debug = 0;
static const rk_u8 kmpp_ioc_name[] = "kmpp_ioctl"; /* /dev/kmpp_ioctl */

static rk_s32 kmpp_ioc_open(KmppFsDev *file)
{
    KmppIocSrv *srv = get_ioc_srv_f();
    KmppIocMgr *mgr = (KmppIocMgr *)file->priv_data;
    KmppObjDefSet *defs = NULL;

    KMPP_INIT_LIST_HEAD(&mgr->list_srv);

    kmpp_objdefset_get(&defs, TRIE_CTRL);

    if (!defs) {
        kmpp_loge_f("failed to get shared objdef set\n");
        return rk_nok;
    }

    mgr->defs = defs;
    mgr->buf = kmpp_trie_get_node_root(defs->trie);
    mgr->buf_size = defs->buf_size;

    kmpp_spin_lock(srv->lock);
    kmpp_list_add_tail(&mgr->list_srv, &srv->list_file);
    mgr->file_id = srv->file_id++;
    srv->file_cnt++;
    kmpp_spin_unlock(srv->lock);

    ioc_dbg_file("file %d opened cnt %d\n", mgr->file_id, srv->file_cnt);
    return rk_ok;
}

static rk_s32 kmpp_ioc_release(KmppFsDev *file)
{
    KmppIocSrv *srv = get_ioc_srv_f();
    KmppIocMgr *mgr = (KmppIocMgr *)file->priv_data;

    if (mgr->defs) {
        kmpp_objdefset_put(mgr->defs);
        mgr->defs = NULL;
    }

    kmpp_spin_lock(srv->lock);
    kmpp_list_del_init(&mgr->list_srv);
    srv->file_cnt--;
    kmpp_spin_unlock(srv->lock);

    if (mgr->vm_trie) {
        kmpp_fs_vm_munmap(mgr->vm_trie);
        mgr->vm_trie = NULL;
    }

    ioc_dbg_file("file %d closed cnt %d\n", mgr->file_id, srv->file_cnt);
    return rk_ok;
}

rk_s32 kmpp_ioc_proc(KmppFsDev *file, void *arg)
{
    KmppIocMgr *mgr = (KmppIocMgr *)file->priv_data;
    KmppObjDefSet *defs = mgr->defs;
    rk_u32 defs_count = defs->count;
    KmppObjIocArg ioc;
    rk_s32 cnt = 0;
    rk_s32 ret;
    rk_u32 i;

    ret = kmpp_copy_from_user(&ioc, arg, sizeof(ioc));
    if (ret) {
        kmpp_loge_f("copy ioc KmppShmPtr failed ret %d\n", ret);
        return rk_nok;
    }

    if (!ioc.count) {
        kmpp_loge_f("invalid ioc count %d\n", ioc.count);
        return rk_nok;
    }

    arg += sizeof(ioc);

    for (i = 0; i < ioc.count; i++, arg += sizeof(KmppObjShm)) {
        KmppShmPtr sptr;
        KmppIocImpl *impl;
        KmppShm shm;
        KmppObjDef def;

        ret = kmpp_copy_from_user(&sptr, arg, sizeof(sptr));
        if (ret) {
            kmpp_loge_f("slot %d ioctl copy_from_user KmppObjShm failed ret %d\n", i, ret);
            continue;
        }

        shm = kmpp_shm_from_shmptr_f(&sptr);
        if (!shm) {
            kmpp_loge_f("slot %d ioctl get shm from sptr [u:k] %llx : %llx fail\n",
                        i, sptr.uaddr, sptr.kaddr);
            continue;
        }

        impl = (KmppIocImpl *)kmpp_shm_get_kaddr(shm);
        if (!impl) {
            kmpp_loge_f("slot %d ioctl get impl from sptr [u:k] %llx : %llx fail\n",
                        i, sptr.uaddr, sptr.kaddr);
            continue;
        }

        /* set default return value to rk_nok */
        impl->ret = rk_nok;
        if (impl->def >= defs_count) {
            kmpp_loge_f("slot %d ioctl invalid def %d while max %d\n",
                        i, impl->def, defs_count);
            continue;
        }

        def = defs->defs[impl->def];
        if (!def) {
            kmpp_loge_f("slot %d ioctl objdef %d has no ioctl\n", i, impl->def);
            continue;
        }

        impl->ret = kmpp_objdef_ioctl(def, file, impl);
        ioc_dbg_info("objdef %d:%s ioctl %px ret %d\n", impl->def,
                     kmpp_objdef_get_name(def), arg, impl->ret);
        cnt++;
    }

    return cnt;
}

static rk_s32 kmpp_ioc_ioctl(KmppFsDev *file, rk_s32 cmd, void *arg)
{
    KmppIocMgr *mgr = (KmppIocMgr *)file->priv_data;
    rk_s32 ret = rk_nok;

    ioc_dbg_flow("file %d ioctl cmd %x arg %px enter\n", mgr->file_id, cmd, arg);

    if (!arg) {
        kmpp_loge_f("invalid NULL arg\n");
        return rk_nok;
    }

    switch (cmd) {
    case 0 :
    case KMPP_IOCTL_IOC_PROC : {
        ret = kmpp_ioc_proc(file, arg);
        ioc_dbg_ioctl("proc ioctl cmd %x ret %d\n", cmd, ret);
    } break;
    case KMPP_IOCTL_IOC_QUERY_INFO : {
        rk_u64 uaddr;

        if (!mgr->vm_trie) {
            rk_s32 buf_size = PAGE_ALIGN(mgr->buf_size);

            ret = kmpp_fs_vm_mmap(&mgr->vm_trie, file, mgr->buf,
                                  buf_size, KMPP_VM_PROT_RO);
            if (ret || !mgr->vm_trie) {
                kmpp_loge_f("vm_mmap fail ret %d %px\n", ret, mgr->vm_trie);
                break;
            }
        }

        uaddr = (rk_u64)mgr->vm_trie->uaddr;

        ioc_dbg_ioctl("query obj trie root %#llx\n", uaddr);

        ret = kmpp_copy_to_user(arg, &uaddr, sizeof(uaddr));
        if (ret)
            kmpp_loge_f("KMPP_SHM_IOC_QUERY_INFO kmpp_copy_to_user fail ret %d\n", ret);
    } break;
    default : {
        ioc_dbg_ioctl("invalid cmd %x\n", cmd);
    } break;
    }

    ioc_dbg_file("file %d ioctl cmd %x arg %px leave ret %d\n",
                 mgr->file_id, cmd, arg, ret);

    return ret;
}

static rk_s32 kmpp_ioc_read(KmppFsDev *file, rk_s32 offset, void **buf, rk_s32 *size)
{
    KmppIocMgr *mgr = (KmppIocMgr *)file->priv_data;
    KmppObjDefSet *defs = mgr->defs;

    if (defs && defs->count) {
        rk_u32 i;

        for (i = 0; i < defs->count; i++)
            kmpp_objdef_dump(defs->defs[i], OBJDEF_DUMP_IOCTL);
    }

    return rk_ok;
}

static rk_s32 kmpp_ioctl_init(void)
{
    KmppIocSrv *srv = srv_ioc;
    rk_s32 class_size;
    rk_s32 lock_size;
    rk_s32 mgr_size;
    rk_s32 size;

    if (srv)
        return rk_nok;

    class_size = kmpp_class_size(kmpp_ioc_name);
    lock_size = kmpp_spinlock_size();
    mgr_size = kmpp_fs_dev_mgr_size(kmpp_ioc_name);
    size = sizeof(KmppIocSrv) + lock_size + class_size + mgr_size;
    srv = kmpp_calloc_atomic(size);
    if (!srv) {
        kmpp_err_f("srv_ioc alloc size %d failed\n", size);
        return rk_nok;
    }

    kmpp_spinlock_assign(&srv->lock, (void *)(srv + 1), lock_size);
    kmpp_class_assign(&srv->cls, kmpp_ioc_name, (rk_u8 *)srv->lock + lock_size, class_size);

    srv->fops.open = kmpp_ioc_open;
    srv->fops.release = kmpp_ioc_release;
    srv->fops.ioctl = kmpp_ioc_ioctl;
    srv->fops.read = kmpp_ioc_read;

    {
        KmppFsDevCfg cfg;

        cfg.cls = srv->cls;
        cfg.name = kmpp_ioc_name;
        cfg.fops = &srv->fops;
        cfg.drv_data = srv;
        cfg.priv_size = sizeof(KmppIocMgr);
        cfg.buf = (rk_u8 *)srv->cls + class_size;
        cfg.size = mgr_size;

        kmpp_fs_dev_mgr_init(&srv->mgr, &cfg);
        if (!srv->mgr) {
            kmpp_err_f("kmpp_ioc mgr init failed\n");
            goto failed;
        }
    }

    KMPP_INIT_LIST_HEAD(&srv->list_file);

    kmpp_ioc_objdef_get();

    {
        KmppEnvInfo env;

        env.type = KmppEnv_u32;
        env.readonly = 0;
        env.name = "kmpp_ioc_debug";
        env.val = &kmpp_ioc_debug;
        env.env_show = NULL;

        kmpp_env_add(kmpp_env_debug(), &srv->env, &env);
    }

    srv_ioc = srv;

    return rk_ok;
failed:
    if (srv) {
        kmpp_fs_dev_mgr_deinit(srv->mgr);
        kmpp_class_deinit(srv->cls);
        kmpp_spinlock_deinit(&srv->lock);

        kmpp_free(srv);
    }

    return rk_nok;
}

static rk_s32 kmpp_ioctl_deinit(void)
{
    KmppIocSrv *srv = get_ioc_srv_f();

    if (!srv)
        return rk_nok;

    kmpp_fs_dev_mgr_deinit(srv->mgr);
    kmpp_class_deinit(srv->cls);
    kmpp_spinlock_deinit(&srv->lock);

    kmpp_ioc_objdef_put();

    kmpp_env_del(kmpp_env_debug(), srv->env);

    srv_ioc = NULL;
    kmpp_free(srv);

    return rk_ok;
}

KMPP_MODULE_ADD(kmpp_sys, KMPP_SYS_IOCTL, ioctl, kmpp_ioctl_init, kmpp_ioctl_deinit);

#define KMPP_OBJ_NAME               kmpp_ioc
#define KMPP_OBJ_INTF_TYPE          KmppIoc
#define KMPP_OBJ_IMPL_TYPE          KmppIocImpl
#define KMPP_OBJ_ENTRY_TABLE        KMPP_IOC_ENTRY_TABLE
#define KMPP_OBJ_ACCESS_DISABLE
#include "kmpp_obj_helper.h"

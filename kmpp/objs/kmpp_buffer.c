/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_buffer"

#include "kmpp_atomic.h"
#include "kmpp_cls.h"
#include "kmpp_env.h"
#include "kmpp_list.h"
#include "kmpp_log.h"
#include "kmpp_macro.h"
#include "kmpp_mem.h"
#include "kmpp_module.h"
#include "kmpp_string.h"
#include "kmpp_uaccess.h"

#include "kmpp_obj.h"
#include "kmpp_shm.h"
#include "kmpp_mod_id.h"
#include "kmpp_mem_pool.h"
#include "kmpp_buffer_impl.h"

#define BUF_DBG_FLOW                    (0x00000001)
#define BUF_DBG_INFO                    (0x00000002)
#define BUF_DBG_FILE                    (0x00000004)

#define buf_dbg(flag, fmt, ...)         kmpp_dbg(kmpp_buf_debug, flag, fmt, ## __VA_ARGS__)

#define buf_dbg_flow(fmt, ...)          buf_dbg(BUF_DBG_FLOW, fmt, ## __VA_ARGS__)
#define buf_dbg_info(fmt, ...)          buf_dbg(BUF_DBG_INFO, fmt, ## __VA_ARGS__)
#define buf_dbg_flow_info(fmt, ...)     buf_dbg(BUF_DBG_FLOW | BUF_DBG_INFO, fmt, ## __VA_ARGS__)
#define buf_dbg_file(fmt, ...)          buf_dbg(BUF_DBG_FILE, fmt, ## __VA_ARGS__)

#define get_buf_srv(caller) \
    ({ \
        KmppBufferSrv *__tmp; \
        if (srv_buf) { \
            __tmp = srv_buf; \
        } else { \
            kmpp_loge_f("kmpp buf srv not init at %s : %s\n", __FUNCTION__, caller); \
            __tmp = NULL; \
        } \
        __tmp; \
    })

#define get_buf_srv_f() get_buf_srv(__FUNCTION__)

typedef struct KmppBufferSrv_t {
    KmppSpinlock        *lock;

    /* /dev/kmpp_buf device info */
    KmppCls             *cls;
    KmppDevFops         fops;
    KmppFsDevMgr        *mgr;
    KmppEnvNode         env_debug;
    KmppEnvNode         env_info;

    /* list for list_srv in KmppBufGrpImpl */
    kmpp_list_head      list_grp;
    /* list for list_status in KmppBufferImpl on buffer init */
    kmpp_list_head      list_buf_init;
    /* list for list_status in KmppBufferImpl on buffer / buf grp deinit */
    kmpp_list_head      list_buf_deinit;

    rk_s32              buf_init_cnt;
    rk_s32              buf_deinit_cnt;

    rk_s32              grp_id;
    rk_s32              grp_cnt;
    rk_s32              buf_id;
    rk_s32              buf_cnt;

    /* default buffer group */
    KmppBufGrp          group_default;
    KmppBufGrpImpl      *grp;
} KmppBufferSrv;

static KmppBufferSrv *srv_buf = NULL;
static rk_u32 kmpp_buf_debug = 0;

static const rk_u8 *strof_buf_status(rk_u32 status)
{
    static const rk_u8 *strof_status[] = {
        [BUF_ST_NONE]               = "none",
        [BUF_ST_INIT]               = "init",
        [BUF_ST_INIT_TO_USED]       = "init->used",
        [BUF_ST_USED]               = "used",
        [BUF_ST_UNUSED]             = "unused",
        [BUF_ST_USED_TO_DEINIT]     = "on-deinit",
        [BUF_ST_DEINIT_AT_GRP]      = "deinit@grp",
        [BUF_ST_DEINIT_AT_SRV]      = "deinit@srv",
        [BUF_ST_DEINIT_AT_SRV + 1]  = "invalid",
    };

    if (status > BUF_ST_DEINIT_AT_SRV)
        status = BUF_ST_DEINIT_AT_SRV + 1;

    return strof_status[status];
}

static void kmpp_buf_show(KmppEnvNode env, void *data)
{
    KmppBufferSrv *srv = get_buf_srv_f();
    KmppBufGrpImpl *grp, *n;

    if (!srv)
        return;

    kmpp_spin_lock(srv->lock);

    kmpp_list_for_each_entry_safe(grp, n, &srv->list_grp, KmppBufGrpImpl, list_srv) {
        KmppBufferImpl *buf, *n;

        kmpp_spin_lock(grp->lock);
        kmpp_env_log(env, "---------------------------------"
                     " kmpp_buffer group %2d used %2d unused %2d "
                     "---------------------------------\n",
                     grp->grp_id, grp->count_used, grp->count_unused);
        kmpp_env_log(env, "|%-8s|%-10s|%-10s|%-18s|%-18s|%-8s|%-15s|%-10s|%-s\n",
                     "id", "status", "size", "kptr", "uaddr", "flag", "attach_device", "iova", "caller");
        if (grp->count_used) {
            kmpp_list_for_each_entry_safe(buf, n, &grp->list_used, KmppBufferImpl, list_status) {
                KmppBufIovaMap *map, *n;
                rk_u32 cnt = 0;

                kmpp_env_log(env, "|%-8d|%-10s|%-10d|0x%-16px|0x%-16llx|%-8d|",
                             buf->buf_uid, strof_buf_status(buf->status),
                             buf->size, buf->kptr, buf->uaddr, buf->cfg_int.flag);
                if (kmpp_list_empty(&buf->list_maps)) {
                    kmpp_env_log(env, "%26s|%-s\n", "", buf->caller);
                    continue;
                }
                kmpp_list_for_each_entry_safe(map, n, &buf->list_maps, KmppBufIovaMap, list) {
                    if (!cnt++)
                        kmpp_env_log(env, "%-15s|0x%-08x|%s\n",
                                     kmpp_device_name(map->device), (rk_u32)map->iova, buf->caller);
                    else
                        kmpp_env_log(env, "%76s|%-15s|0x%-08x|\n", "",
                                     kmpp_device_name(map->device), (rk_u32)map->iova);
                }
            }
        }

        if (grp->count_unused) {
            kmpp_list_for_each_entry_safe(buf, n, &grp->list_unused, KmppBufferImpl, list_status) {
                KmppBufIovaMap *map, *n;
                rk_u32 cnt = 0;

                kmpp_env_log(env, "|%-8d|%-10s|%-10d|0x%-16px|0x%-16llx|%-8d|",
                             buf->buf_uid, strof_buf_status(buf->status),
                             buf->size, buf->kptr, buf->uaddr, buf->cfg_int.flag);
                if (kmpp_list_empty(&buf->list_maps)) {
                    kmpp_env_log(env, "%26s|%-s\n", "", buf->caller);
                    continue;
                }
                kmpp_list_for_each_entry_safe(map, n, &buf->list_maps, KmppBufIovaMap, list) {
                    if (!cnt++)
                        kmpp_env_log(env, "%-15s|0x%-08x|%s\n",
                                     kmpp_device_name(map->device), (rk_u32)map->iova, buf->caller);
                    else
                        kmpp_env_log(env, "%76s|%-15s|0x%-08x|\n", "",
                                     kmpp_device_name(map->device), (rk_u32)map->iova);
                }
            }
        }
        kmpp_spin_unlock(grp->lock);
    }

    kmpp_spin_unlock(srv->lock);
}

static rk_s32 kmpp_buf_init(void)
{
    KmppBufferSrv *srv = srv_buf;
    rk_s32 lock_size;
    rk_s32 size;

    if (srv)
        return rk_ok;

    lock_size = kmpp_spinlock_size();
    size = sizeof(KmppBufferSrv) + lock_size;
    srv = kmpp_calloc_atomic(size);
    if (!srv) {
        kmpp_loge_f("srv_buf alloc size %d failed\n", size);
        return rk_nok;
    }

    kmpp_spinlock_assign(&srv->lock, (void *)(srv + 1), lock_size);

    KMPP_INIT_LIST_HEAD(&srv->list_grp);
    KMPP_INIT_LIST_HEAD(&srv->list_buf_init);
    KMPP_INIT_LIST_HEAD(&srv->list_buf_deinit);

    kmpp_buf_grp_objdef_get();
    kmpp_buf_grp_cfg_objdef_get();
    kmpp_buffer_objdef_get();
    kmpp_buf_cfg_objdef_get();

    {
        KmppEnvInfo env;

        env.type = KmppEnv_u32;
        env.readonly = 0;
        env.name = "kmpp_buf_debug";
        env.val = &kmpp_buf_debug;
        env.env_show = NULL;

        kmpp_env_add(kmpp_env_debug(), &srv->env_debug, &env);

        env.type = KmppEnv_user;
        env.readonly = 1;
        env.name = "buf_info";
        env.val = NULL;
        env.env_show = kmpp_buf_show;

        kmpp_env_add(kmpp_env_root(), &srv->env_info, &env);
    }

    srv_buf = srv;

    {   /* setup the default buffer group */
        KmppBufGrp group = NULL;

        kmpp_buf_grp_get(&group);
        kmpp_buf_grp_setup_f(group);

        srv->group_default = group;
        srv->grp = kmpp_obj_to_entry(group);
        srv->grp->is_default = 1;
    }

    return rk_ok;
}

static rk_s32 kmpp_buf_deinit(void)
{
    KmppBufferSrv *srv = get_buf_srv_f();
    KmppBufGrpImpl *grp, *n;

    if (!srv)
        return rk_nok;

    /* remove default group from the list */
    if (srv->group_default)
        kmpp_list_del_init(&srv->grp->list_srv);

    kmpp_list_for_each_entry_safe(grp, n, &srv->list_grp, KmppBufGrpImpl, list_srv) {
        kmpp_buf_grp_put(grp->obj);
    }

    if (srv->group_default) {
        kmpp_list_add_tail(&srv->grp->list_srv, &srv->list_grp);
        kmpp_buf_grp_put(srv->group_default);
    }

    srv->group_default = NULL;
    srv->grp = NULL;

    if (srv->grp_cnt)
        kmpp_logi_f("still have %d buffer group not released\n", srv->grp_cnt);

    if (srv->buf_cnt || srv->buf_init_cnt || srv->buf_deinit_cnt)
        kmpp_logi_f("still have %d : %d : %d buffer not released\n", srv->buf_cnt,
                    srv->buf_init_cnt, srv->buf_deinit_cnt);

    kmpp_spinlock_deinit(&srv->lock);

    kmpp_buffer_objdef_put();
    kmpp_buf_cfg_objdef_put();
    kmpp_buf_grp_objdef_put();
    kmpp_buf_grp_cfg_objdef_put();

    kmpp_env_del(kmpp_env_debug(), srv->env_debug);
    kmpp_env_del(kmpp_env_root(), srv->env_info);

    srv_buf = NULL;
    kmpp_free(srv);

    return rk_ok;
}

KMPP_MODULE_ADD(kmpp, KMPP_MOD_BUFFER, buffer, kmpp_buf_init, kmpp_buf_deinit);

rk_s32 kmpp_buf_grp_cfg_impl_init(void *entry, KmppObj obj, KmppFsDev *file, const rk_u8 *caller)
{
    KmppBufGrpCfgImpl *impl = (KmppBufGrpCfgImpl *)entry;

    impl->fd = -1;
    impl->flag = KMPP_DMAHEAP_FLAGS_CACHABLE;

    return rk_ok;
}

static rk_s32 kmpp_buf_grp_cfg_impl_dump(void *entry)
{
    KmppBufGrpCfgImpl *impl = (KmppBufGrpCfgImpl*)entry;

    if (!impl) {
        kmpp_loge_f("invalid param frame NULL\n");
        return rk_nok;
    }

    kmpp_logi("flag             %d\n", impl->flag);
    kmpp_logi("count            %d\n", impl->count);
    kmpp_logi("size             %d\n", impl->size);
    kmpp_logi("mode             %d\n", impl->mode);
    kmpp_logi("device           %#px\n", impl->device);
    kmpp_logi("allocator [u:k]  %#llx : %#llx\n", impl->allocator.uaddr, impl->allocator.kaddr);
    kmpp_logi("name [u:k]       %#llx : %#llx\n", impl->name.uaddr, impl->name.kaddr);

    return rk_ok;
}

#define KMPP_OBJ_NAME               kmpp_buf_grp_cfg
#define KMPP_OBJ_INTF_TYPE          KmppBufGrpCfg
#define KMPP_OBJ_IMPL_TYPE          KmppBufGrpCfgImpl
#define KMPP_OBJ_ENTRY_TABLE        KMPP_BUF_GRP_CFG_ENTRY_TABLE
#define KMPP_OBJ_FUNC_INIT          kmpp_buf_grp_cfg_impl_init
#define KMPP_OBJ_FUNC_DUMP          kmpp_buf_grp_cfg_impl_dump
#include "kmpp_obj_helper.h"

static void kmpp_buf_grp_cfg_sync(KmppBufGrpImpl *impl)
{
    impl->cfg_int.grp_id    = impl->grp_id;
    impl->cfg_int.used      = impl->count_used;
    impl->cfg_int.unused    = impl->count_unused;

    impl->cfg_usr->grp_id   = impl->grp_id;
    impl->cfg_usr->used     = impl->count_used;
    impl->cfg_usr->unused   = impl->count_unused;
}

static rk_s32 kmpp_buf_grp_impl_init(void *entry, KmppObj obj, KmppFsDev *file, const rk_u8 *caller)
{
    KmppBufGrpImpl *impl = (KmppBufGrpImpl *)entry;
    KmppBufferSrv *srv = get_buf_srv(caller);

    buf_dbg_flow("buf grp %px get enter at %s\n", impl, caller);

    if (!srv)
        return rk_nok;

    KMPP_INIT_LIST_HEAD(&impl->list_srv);
    KMPP_INIT_LIST_HEAD(&impl->list_used);
    KMPP_INIT_LIST_HEAD(&impl->list_unused);
    kmpp_spinlock_assign(&impl->lock, (void *)(impl + 1), kmpp_spinlock_size());

    if (file) {
        kmpp_buf_grp_cfg_get_share(&impl->cfg_ext, file);
    } else {
        kmpp_buf_grp_cfg_get(&impl->cfg_ext);
    }

    if (!impl->cfg_ext) {
        kmpp_loge_f("failed to create buffer group config for %px at %s\n", impl, caller);
        return rk_nok;
    }

    impl->cfg_usr = kmpp_obj_to_entry(impl->cfg_ext);
    impl->cfg_usr->grp_impl = impl;
    impl->cfg_int.grp_impl = impl;
    impl->obj = obj;
    kmpp_obj_to_shmptr(impl->cfg_ext, &impl->cfg);

    kmpp_spin_lock(srv->lock);
    kmpp_list_add_tail(&impl->list_srv, &srv->list_grp);
    impl->grp_id = srv->grp_id++;
    srv->grp_cnt++;
    buf_dbg_info("buf grp %d init total %d\n", impl->grp_id, srv->grp_cnt);
    kmpp_spin_unlock(srv->lock);

    buf_dbg_flow("buf grp %px - %d get leave at %s\n", impl, impl->grp_id, caller);

    return rk_ok;
}

static void kmpp_buf_grp_cleanup_unused(KmppBufferSrv *srv, KmppBufGrpImpl *impl, const rk_u8 *caller)
{
    KmppBufferImpl *buf, *n;
    KMPP_LIST_HEAD(list);
    rk_s32 count = 0;

    /* deinit all unused buffer */
    kmpp_spin_lock(impl->lock);
    kmpp_list_for_each_entry_safe(buf, n, &impl->list_unused, KmppBufferImpl, list_status) {
        kmpp_list_move_tail(&buf->list_status, &list);
        buf->status = BUF_ST_DEINIT_AT_GRP;
        buf->discard = 1;
        buf->grp = NULL;
        buf->lock = srv->lock;
        impl->count_unused--;
        count++;
    }
    kmpp_spin_unlock(impl->lock);

    if (count) {
        kmpp_list_for_each_entry_safe(buf, n, &list, KmppBufferImpl, list_status) {
            /* set ref_cnt to 1 for last no lock release step */
            buf->ref_cnt = 1;
            kmpp_obj_put_f(buf->obj);
        }
    }

    if (impl->count_unused) {
        kmpp_loge("buf grp %d still has %d unused buffer after cleanup at %s\n",
                  impl->grp_id, impl->count_unused, caller);
    }
}

static rk_s32 kmpp_buf_grp_impl_deinit(void *entry, KmppObj obj, const rk_u8 *caller)
{
    KmppBufGrpImpl *impl = (KmppBufGrpImpl *)entry;
    KmppBufferImpl *buf, *n;
    KmppBufferSrv *srv = get_buf_srv(caller);
    KMPP_LIST_HEAD(list);
    rk_s32 count;
    (void)obj;

    buf_dbg_flow("buf grp %d put enter at %s\n", impl->grp_id, caller);

    if (!srv)
        return rk_nok;

    kmpp_spin_lock(srv->lock);
    if (!kmpp_list_empty(&impl->list_srv)) {
        kmpp_list_del_init(&impl->list_srv);
        srv->grp_cnt--;
    }
    buf_dbg_info("buf grp %d deinit total %d\n", impl->grp_id, srv->grp_cnt);
    kmpp_spin_unlock(srv->lock);

    /* move all still used buffer to service list_buf_deinit and mark discard */
    count = 0;
    kmpp_spin_lock(impl->lock);
    kmpp_list_for_each_entry_safe(buf, n, &impl->list_used, KmppBufferImpl, list_status) {
        kmpp_list_move_tail(&buf->list_status, &list);
        buf->status = BUF_ST_USED_TO_DEINIT;
        buf->discard = 1;
        buf->grp = NULL;
        buf->lock = srv->lock;
        impl->count_used--;
        count++;
    }
    kmpp_spin_unlock(impl->lock);

    if (count) {
        kmpp_spin_lock(srv->lock);
        kmpp_list_for_each_entry_safe(buf, n, &list, KmppBufferImpl, list_status) {
            kmpp_list_move_tail(&buf->list_status, &srv->list_buf_deinit);
            buf->status = BUF_ST_DEINIT_AT_SRV;
            srv->buf_deinit_cnt++;
        }
        kmpp_spin_unlock(srv->lock);
    }

    /* deinit all unused buffer */
    kmpp_buf_grp_cleanup_unused(srv, impl, caller);

    if (impl->count_used || impl->count_unused) {
        kmpp_loge_f("buf grp %d still has %d used %d unused buffer\n",
                    impl->grp_id, impl->count_used, impl->count_unused);
    }

    if (impl->heap) {
        kmpp_dmaheap_put(impl->heap, caller);
        impl->heap = NULL;
    }

    if (impl->cfg_ext) {
        kmpp_buf_grp_cfg_put(impl->cfg_ext);
        impl->cfg_ext = NULL;
    }

    buf_dbg_flow("buf grp %d put leave at %s\n", impl->grp_id, caller);

    return rk_ok;
}

static rk_s32 kmpp_buf_grp_impl_dump(void *entry)
{
    KmppBufGrpImpl *impl = (KmppBufGrpImpl*)entry;

    if (!impl) {
        kmpp_loge_f("invalid param frame NULL\n");
        return rk_nok;
    }

    kmpp_logi("name             %s\n", impl->str_buf + impl->name_offset);
    if (impl->name_offset)
        kmpp_logi("allocator        %s\n", impl->str_buf);
    kmpp_logi("grp id           %d\n", impl->grp_id);

    return kmpp_buf_grp_cfg_impl_dump(&impl->cfg_int);
}

KmppShmPtr *kmpp_buf_grp_get_cfg_s(KmppBufGrp group)
{
    KmppBufGrpImpl *impl = kmpp_obj_to_entry(group);

    return impl ? &impl->cfg : NULL;
}

KmppBufGrpCfg kmpp_buf_grp_get_cfg_k(KmppBufGrp group)
{
    KmppBufGrpImpl *impl = kmpp_obj_to_entry(group);

    return impl ? impl->cfg.kptr : NULL;
}

rk_u64 kmpp_buf_grp_get_cfg_u(KmppBufGrp group)
{
    KmppBufGrpImpl *impl = kmpp_obj_to_entry(group);

    return impl ? impl->cfg.uaddr : 0;
}

rk_s32 kmpp_buf_grp_get_size(KmppBufGrpImpl *grp)
{
    return grp->cfg_int.size;
}

rk_s32 kmpp_buf_grp_get_count(KmppBufGrpImpl *grp)
{
    return grp->cfg_int.count;
}

rk_s32 kmpp_buf_grp_setup(KmppBufGrp group, const rk_u8 *caller)
{
    KmppBufferSrv *srv = get_buf_srv(caller);
    KmppBufGrpImpl *impl;
    KmppBufGrpCfgImpl *cfg;
    void *device;
    rk_s32 offset;

    if (!srv)
        return rk_nok;

    if (!group) {
        kmpp_loge_f("invalid param group %px caller %s\n", group, caller);
        return rk_nok;
    }

    impl = (KmppBufGrpImpl *)kmpp_obj_to_entry(group);

    buf_dbg_flow("buf grp %d setup enter at %s\n", impl->grp_id, caller);

    cfg = impl->cfg_usr;
    /* setup cfg_set parameters here */
    if (!cfg) {
        kmpp_loge_f("invalid param cfg_set %px caller %s\n", srv, cfg, caller);
        return rk_nok;
    }

    device = cfg->device;
    /* copy allocator name */
    offset = kmpp_strncpy_sptr(impl->str_buf, &cfg->allocator, BUF_GRP_STR_BUF_SIZE);

    /* copy buffer group name */
    if (offset) {
        impl->name_offset = offset + 1;
        impl->str_buf[offset] = '\0';

        kmpp_strncpy_sptr(impl->str_buf + offset + 1, &cfg->name,
                          BUF_GRP_STR_BUF_SIZE - offset - 1);
        buf_dbg_info("allocator %s - name %s\n", impl->str_buf, impl->str_buf + offset + 1);
    } else {
        impl->name_offset = 0;
        offset = kmpp_strncpy_sptr(impl->str_buf, &cfg->name,
                                   BUF_GRP_STR_BUF_SIZE - 1);
        buf_dbg_info("allocator na - name %s\n", impl->str_buf);
    }

    /*
     * three type of allocator:
     * name valid               - open heap by name
     * name NULL device NULL    - default dmaheap
     * name NULL device valid   - use device to allocate dmabuf in kmpp_dmabuf.c
     */
    if (impl->name_offset) {
        kmpp_dmaheap_get_by_name(&impl->heap, impl->str_buf, cfg->flag, caller);
        if (!impl->heap)
            kmpp_loge_f("open heap %s failed\n", impl->str_buf);
    }

    if (!impl->heap) {
        if (!device) {
            kmpp_dmaheap_get(&impl->heap, cfg->flag, caller);
            if (!impl->heap)
                kmpp_loge_f("open default heap failed\n");
        } else {
            kmpp_logi_f("use device %px %s to allocate dmabuf\n",
                        device, kmpp_device_name(device));
        }
    }

    if (!impl->heap && !device) {
        kmpp_loge_f("open heap %s device %px failed\n", impl->str_buf, device);
        return rk_nok;
    }

    kmpp_memcpy(&impl->cfg_int, cfg, sizeof(impl->cfg_int));

    kmpp_buf_grp_cfg_sync(impl);

    buf_dbg_info("buf grp %d setup mode %d size %d count %d\n", impl->grp_id,
                 impl->cfg_int.mode, impl->cfg_int.size, impl->cfg_int.count);

    buf_dbg_flow("buf grp %d setup leave at %s\n", impl->grp_id, caller);

    return rk_ok;
}

rk_s32 kmpp_buf_grp_reset(KmppBufGrp group, const rk_u8 *caller)
{
    KmppBufferSrv *srv = get_buf_srv(caller);
    KmppBufGrpImpl *impl;
    KmppBufferImpl *buf, *n;

    if (!srv)
        return rk_nok;

    if (!group) {
        kmpp_loge_f("invalid param group %px caller %s\n", group, caller);
        return rk_nok;
    }

    impl = (KmppBufGrpImpl *)kmpp_obj_to_entry(group);

    buf_dbg_flow("buf grp %d reset enter at %s\n", impl->grp_id, caller);

    /* mark all used buffer to be discarded */
    kmpp_spin_lock(impl->lock);
    kmpp_list_for_each_entry_safe(buf, n, &impl->list_used, KmppBufferImpl, list_status) {
        buf->discard = 1;
    }
    kmpp_spin_unlock(impl->lock);

    /* deinit all unused buffer */
    kmpp_buf_grp_cleanup_unused(srv, impl, caller);

    buf_dbg_flow("buf grp %d reset leave at %s\n", impl->grp_id, caller);

    return rk_ok;
}

#define shms_to_ctx(shm) shms_to_ctx_f(shm, __FUNCTION__)

static KmppObj shms_to_ctx_f(KmppShmPtr *shm, const rk_u8 *caller)
{
    KmppObj ctx = kmpp_shm_get_kpriv(shm[IOC_SHM_IN].kptr);

    if (!ctx)
        ctx = kmpp_shm_get_kpriv(shm[IOC_SHM_CTX].kptr);

    if (!ctx || kmpp_obj_check_f(ctx)) {
        kmpp_loge("%s invalid param [u:k] %llx:%px %llx:%px %llx:%px\n", caller,
                  shm[IOC_SHM_CTX].uaddr, shm[IOC_SHM_CTX].kptr,
                  shm[IOC_SHM_IN ].uaddr, shm[IOC_SHM_IN ].kptr,
                  shm[IOC_SHM_OUT].uaddr, shm[IOC_SHM_OUT].kptr);
        return NULL;
    }

    return ctx;
}

static rk_s32 kmpp_buf_grp_ioc_setup(KmppFsDev *file, KmppShmPtr *shm)
{
    KmppObj grp = shms_to_ctx(shm);
    rk_s32 ret = rk_nok;

    buf_dbg_flow("enter file %px grp %px\n", file, grp);

    if (grp) {
        ret = kmpp_buf_grp_setup_f(grp);
        if (ret)
            kmpp_loge_f("grp setup buf grp %px failed ret %d\n", grp, ret);
    }

    buf_dbg_flow("leave ret %d\n", ret);

    return ret;
}

static rk_s32 kmpp_buf_grp_ioc_reset(KmppFsDev *file, KmppShmPtr *shm)
{
    KmppObj grp = shms_to_ctx(shm);
    rk_s32 ret = rk_nok;

    buf_dbg_flow("enter file %px grp %px\n", file, grp);

    if (grp) {
        ret = kmpp_buf_grp_reset_f(grp);
        if (ret)
            kmpp_loge_f("grp reset buf grp %px failed ret %d\n", grp, ret);
    }

    buf_dbg_flow("leave ret %d\n", ret);

    return ret;
}

static KmppObjIoctl kmpp_buf_grp_ioctls[] = {
    KMPP_OBJ_IOCTL("setup",  kmpp_buf_grp_ioc_setup, "setup buffer group and enable config"),
    KMPP_OBJ_IOCTL("reset",  kmpp_buf_grp_ioc_reset, "reset buffer group and discard all buffers"),
};

static KmppObjIoctls kmpp_buf_grp_ioc = {
    .count = KMPP_ARRAY_ELEMS(kmpp_buf_grp_ioctls),
    .funcs = kmpp_buf_grp_ioctls,
};

#define KMPP_OBJ_NAME               kmpp_buf_grp
#define KMPP_OBJ_INTF_TYPE          KmppBufGrp
#define KMPP_OBJ_IMPL_TYPE          KmppBufGrpImpl
#define KMPP_OBJ_EXTRA_SIZE         kmpp_spinlock_size()
#define KMPP_OBJ_ENTRY_TABLE        KMPP_BUF_GRP_ENTRY_TABLE
#define KMPP_OBJ_FUNC_INIT          kmpp_buf_grp_impl_init
#define KMPP_OBJ_FUNC_DEINIT        kmpp_buf_grp_impl_deinit
#define KMPP_OBJ_FUNC_IOCTL         kmpp_buf_grp_ioc
#define KMPP_OBJ_FUNC_DUMP          kmpp_buf_grp_impl_dump
#include "kmpp_obj_helper.h"

static void kmpp_buf_cfg_sync(KmppBufferImpl *impl)
{
    /* update to both internal and external config for user access */
    impl->cfg_int.sptr.uaddr    = impl->uaddr;
    impl->cfg_int.sptr.kptr     = impl->kptr;
    impl->cfg_int.size          = impl->size;
    impl->cfg_int.grp_id        = impl->grp_id;
    impl->cfg_int.buf_gid       = impl->buf_gid;
    impl->cfg_int.buf_uid       = impl->buf_uid;
    impl->cfg_int.fd            = impl->cfg_usr->fd;

    impl->cfg_usr->sptr.uaddr   = impl->uaddr;
    impl->cfg_usr->sptr.kptr    = impl->kptr;
    impl->cfg_usr->size         = impl->size;
    impl->cfg_usr->grp_id       = impl->grp_id;
    impl->cfg_usr->buf_gid      = impl->buf_gid;
    impl->cfg_usr->buf_uid      = impl->buf_uid;
}

rk_s32 kmpp_buffer_impl_init(void *entry, KmppObj obj, KmppFsDev *file, const rk_u8 *caller)
{
    KmppBufferSrv *srv = get_buf_srv(caller);
    KmppBufferImpl *impl = (KmppBufferImpl *)entry;

    buf_dbg_flow("buf %px get enter at %s\n", impl, caller);

    if (!srv)
        return rk_nok;

    KMPP_INIT_LIST_HEAD(&impl->list_status);
    KMPP_INIT_LIST_HEAD(&impl->list_maps);
    kmpp_mutex_assign(&impl->mutex_maps, (void *)(impl + 1), kmpp_mutex_size());
    if (!impl->mutex_maps) {
        kmpp_loge_f("failed to create mutex for %px at %s\n", impl, caller);
        return rk_nok;
    }

    if (file) {
        kmpp_buf_cfg_get_share(&impl->cfg_ext, file);
    } else {
        kmpp_buf_cfg_get(&impl->cfg_ext);
    }

    if (!impl->cfg_ext) {
        kmpp_loge_f("failed to create buffer config for %px at %s\n", impl, caller);
        return rk_nok;
    }

    impl->cfg_usr = kmpp_obj_to_entry(impl->cfg_ext);
    impl->cfg_usr->buf_impl = impl;
    /* sync internal config and user config */
    kmpp_memcpy(&impl->cfg_int, impl->cfg_usr, sizeof(impl->cfg_int));
    kmpp_obj_to_shmptr(impl->cfg_ext, &impl->cfg);

    impl->grp = NULL;
    impl->obj = obj;
    impl->ref_cnt = 1;

    kmpp_spin_lock(srv->lock);
    kmpp_list_add_tail(&impl->list_status, &srv->list_buf_init);
    impl->status = BUF_ST_INIT;
    impl->buf_uid = srv->buf_id++;
    impl->lock = srv->lock;
    srv->buf_init_cnt++;
    KMPP_FETCH_ADD(&srv->buf_cnt, 1);
    buf_dbg_info("buf %d init total %d\n", impl->buf_uid, srv->buf_cnt);
    kmpp_spin_unlock(srv->lock);

    buf_dbg_flow("buf %px - %d get leave at %s\n", impl, impl->buf_uid, caller);

    return rk_ok;
}

static void buf_deinit(KmppBufferImpl *impl, const rk_u8 *caller)
{
    KmppBufIovaMap *map, *n;
    KMPP_LIST_HEAD(list);
    rk_s32 iova_count = 0;

    if (impl->cfg_ext)
        kmpp_buf_cfg_put(impl->cfg_ext);

    impl->cfg_ext = NULL;
    impl->cfg_usr = NULL;

    /* release all remaining iova map */
    kmpp_mutex_lock(impl->mutex_maps);
    kmpp_list_for_each_entry_safe(map, n, &impl->list_maps, KmppBufIovaMap, list) {
        kmpp_list_move_tail(&map->list, &list);
        iova_count++;
    }
    kmpp_mutex_unlock(impl->mutex_maps);

    if (iova_count) {
        kmpp_list_for_each_entry_safe(map, n, &list, KmppBufIovaMap, list) {
            if (map->mode == KMPP_BUF_MAP_BY_SYS_DEVICE)
                kmpp_dmabuf_put_iova_by_device(impl->buf, map->iova, map->device);
            else if (map->mode == KMPP_BUF_MAP_BY_OSAL_DEV)
                kmpp_dmabuf_put_iova(impl->buf, map->iova, (KmppDev *)map->device);
            else
                kmpp_loge_f("buf %d:[%d:%d] has invalid iova mode %ds\n",
                            impl->buf_uid, impl->grp_id, impl->buf_gid, map->mode);

            kmpp_free(map);
        }
    }

    if (impl->buf) {
        kmpp_dmabuf_free(impl->buf, caller);
        impl->buf = NULL;
    }

    if (impl->mutex_maps)
        kmpp_mutex_deinit(&impl->mutex_maps);

    impl->lock = NULL;

    buf_dbg_info("buf %d deinited at %s\n", impl->buf_uid, caller);
}

rk_s32 kmpp_buffer_impl_deinit(void *entry, KmppObj obj, const rk_u8 *caller)
{
    KmppBufferSrv *srv = get_buf_srv(caller);
    KmppBufferImpl *impl = (KmppBufferImpl *)entry;
    rk_s32 deinit = 0;
    rk_s32 buf_uid;
    rk_s32 ref_cnt;
    rk_s32 old;
    (void)obj;

    if (!impl) {
        kmpp_loge_f("invalid param buffer %px at %s\n", impl, caller);
        return rk_nok;
    }

    old = impl->ref_cnt;
    buf_uid = impl->buf_uid;
    ref_cnt = KMPP_SUB_FETCH(&impl->ref_cnt, 1);

    buf_dbg_flow("buf %d dec ref %d -> %d at %s\n",
                 buf_uid, old, ref_cnt, caller);

    if (ref_cnt < 0) {
        kmpp_loge_f("invalid negative ref_cnt %d buffer %px at %s\n",
                    ref_cnt, impl, caller);
        return rk_nok;
    }

    if (ref_cnt > 0)
        return rk_nok;

    if (!impl->lock) {
        kmpp_loge_f("buf %d:[%d:%d] has no lock but status grp at %s\n",
                    buf_uid, impl->grp_id, impl->buf_gid, caller);
        deinit = 1;
        kmpp_spin_lock(srv->lock);
        KMPP_FETCH_SUB(&srv->buf_cnt, 1);
        kmpp_spin_unlock(srv->lock);
    } else {
        KmppBufGrpImpl *grp;

        /* lock and handle buffer by status */
        kmpp_spin_lock(impl->lock);
        grp = impl->grp;
        /* deinit on valid group */
        switch (impl->status) {
        case BUF_ST_INIT : {
            kmpp_list_del_init(&impl->list_status);
            srv->buf_init_cnt--;
            KMPP_FETCH_SUB(&srv->buf_cnt, 1);
            buf_dbg_info("buf %d init -> n/a total %d\n", impl->buf_uid, srv->buf_cnt);
            deinit = 1;
        } break;
        case BUF_ST_USED : {
            if (!grp) {
                kmpp_logi_f("buf %d:[%d:%d] used but no grp at %s\n",
                            impl->buf_uid, impl->grp_id, impl->buf_gid, caller);
                break;
            }
            if (!grp->is_default && !impl->discard) {
                kmpp_list_move_tail(&impl->list_status, &grp->list_unused);
                impl->status = BUF_ST_UNUSED;
                grp->count_used--;
                grp->count_unused++;
                buf_dbg_info("buf %d used -> unused total %d\n", impl->buf_uid, srv->buf_cnt);
            } else {
                kmpp_list_del_init(&impl->list_status);
                impl->status = BUF_ST_NONE;
                grp->count_used--;
                KMPP_FETCH_SUB(&srv->buf_cnt, 1);
                deinit = 1;
                buf_dbg_info("buf %d used -> n/a total %d\n", impl->buf_uid, srv->buf_cnt);
            }
        } break;
        case BUF_ST_DEINIT_AT_GRP : {
            KMPP_FETCH_SUB(&srv->buf_cnt, 1);
            buf_dbg_info("buf %d deinit at grp -> n/a total %d\n", impl->buf_uid, srv->buf_cnt);
            deinit = 1;
        } break;
        case BUF_ST_DEINIT_AT_SRV : {
            kmpp_list_del_init(&impl->list_status);
            srv->buf_deinit_cnt--;
            KMPP_FETCH_SUB(&srv->buf_cnt, 1);
            buf_dbg_info("buf %d deinit at srv -> n/a total %d\n", impl->buf_uid, srv->buf_cnt);
            deinit = 1;
        } break;
        default : {
            kmpp_loge_f("invalid buf %d status %d at %s\n",
                        impl->buf_uid, impl->status, caller);
        } break;
        }

        if (grp) {
            if (deinit) {
                grp->buf_cnt--;
                impl->grp = NULL;
            }

            kmpp_buf_grp_cfg_sync(grp);
        }
        kmpp_spin_unlock(impl->lock);
    }

    if (deinit)
        buf_deinit(impl, caller);

    buf_dbg_flow("buf %d put -> %s leave at %s\n", buf_uid,
                 deinit ? "n/a" : "reuse", caller);

    return deinit ? rk_ok : rk_nok;
}

rk_s32 kmpp_buffer_inc_ref(KmppBuffer buffer, const rk_u8 *caller)
{
    KmppBufferImpl *impl = (KmppBufferImpl *)kmpp_obj_to_entry(buffer);
    rk_s32 old;

    if (!impl) {
        kmpp_loge_f("invalid param buffer %px at %s\n", buffer, caller);
        return rk_nok;
    }

    old = KMPP_FETCH_ADD(&impl->ref_cnt, 1);

    buf_dbg_flow_info("buf %d inc ref %d -> %d at %s\n",
                      impl->buf_uid, old, impl->ref_cnt, caller);

    return rk_ok;
}

rk_s32 kmpp_buffer_setup(KmppBuffer buffer, KmppFsDev *file, const rk_u8 *caller)
{
    KmppBufferSrv *srv = get_buf_srv(caller);
    KmppBufferImpl *impl;
    KmppBufCfgImpl *buf_cfg;
    KmppBufGrpImpl *grp = NULL;
    KmppDmaBuf dmabuf = NULL;
    rk_s32 size;

    if (!buffer) {
        kmpp_loge_f("invalid param buffer %px at %s\n", buffer, caller);
        return rk_nok;
    }

    impl = (KmppBufferImpl *)kmpp_obj_to_entry(buffer);

    buf_dbg_flow("buf %d setup enter at %s\n", impl->buf_uid, caller);

    buf_cfg = impl->cfg_usr;
    /*
     * The buffer setup flow:
     * 1. Check fd import from userspace
     * 2. Check struct dma_buf handle import from kernel
     * 3. Try user config group to alloc
     * 4. Try default group to alloc
     */

    /* get user config group or default group */
    {
        KmppObj obj = kmpp_obj_from_shmptr(&buf_cfg->group);

        if (obj) {
            if (kmpp_obj_check_f(obj)) {
                kmpp_loge_f("invalid buffer group %px object check at %s\n", obj, caller);
                return rk_nok;
            }

            grp = (KmppBufGrpImpl *)kmpp_obj_to_entry(obj);
        } else {
            /* if group is not set use default group */
            grp = NULL;
        }
    }

    if (!grp)
        grp = srv->grp;

    size = buf_cfg->size;

    /* step 1 - import userspace fd */
    if (buf_cfg->fd >= 0) {
        kmpp_dmabuf_import_fd(&dmabuf, buf_cfg->fd, buf_cfg->flag, caller);
        if (!dmabuf) {
            kmpp_loge_f("failed to import fd %d at %s\n", buf_cfg->fd, caller);
            return rk_nok;
        }
        goto done;
    }

    /* step 2 - import kernel struct dma_buf */
    if (buf_cfg->dmabuf) {
        kmpp_dmabuf_import_ctx(&dmabuf, buf_cfg->dmabuf, buf_cfg->flag, caller);
        if (!dmabuf) {
            kmpp_loge_f("failed to import dmabuf %px at %s\n", buf_cfg->dmabuf, caller);
            return rk_nok;
        }
        goto done;
    }

    /* step 3 - use buf grp to alloc */
    if (!size)
        size = kmpp_buf_grp_get_size(grp);

    if (!grp || !size) {
        kmpp_loge_f("invalid param grp %px size %d at %s\n", grp, size, caller);
        return rk_nok;
    }

    if (!grp->heap) {
        kmpp_loge_f("invalid param grp %px heap %px at %s\n", grp, grp->heap, caller);
        return rk_nok;
    }

    kmpp_dmabuf_alloc(&dmabuf, grp->heap, size, buf_cfg->flag, caller);
    if (!dmabuf) {
        kmpp_loge_f("failed to alloc dmabuf size %d at %s\n", size, caller);
        return rk_nok;
    }

done:
    kmpp_memcpy(&impl->cfg_int, buf_cfg, sizeof(impl->cfg_int));
    impl->buf = dmabuf;
    impl->size = kmpp_dmabuf_get_size(dmabuf);

    /* userspace call can get uaddr */
    if (file) {
        impl->uaddr = kmpp_dmabuf_get_uptr(dmabuf);
        buf_cfg->fd = kmpp_dmabuf_get_fd(dmabuf);
    }

    kmpp_buf_cfg_sync(impl);

    kmpp_spin_lock(srv->lock);
    kmpp_list_del_init(&impl->list_status);
    impl->status = BUF_ST_INIT_TO_USED;
    srv->buf_init_cnt--;
    kmpp_spin_unlock(srv->lock);

    kmpp_spin_lock(grp->lock);
    kmpp_list_add_tail(&impl->list_status, &grp->list_used);
    impl->status = BUF_ST_USED;
    impl->grp = grp;
    impl->lock = grp->lock;
    impl->grp_id = grp->grp_id;
    impl->buf_gid = grp->buf_id++;
    impl->caller = caller;
    grp->buf_cnt++;
    grp->count_used++;
    kmpp_buf_grp_cfg_sync(grp);
    buf_dbg_info("buf %d setup to grp %d:%d size %d [u:k] %#llx:%#px\n", impl->buf_uid,
                 impl->grp_id, impl->buf_gid, size, impl->uaddr, impl->kptr);
    kmpp_spin_unlock(grp->lock);

    buf_dbg_flow("buf %d [%d:%d] setup leave at %s\n",
                 impl->buf_uid, impl->grp_id, impl->buf_gid, caller);

    return rk_ok;
}

rk_s32 kmpp_buffer_impl_dump(void *entry)
{
    return rk_ok;
}

KmppShmPtr *kmpp_buffer_get_cfg_s(KmppBuffer buffer)
{
    KmppBufferImpl *impl = (KmppBufferImpl *)kmpp_obj_to_entry(buffer);

    return impl ? &impl->cfg : NULL;
}

KmppBufCfg kmpp_buffer_get_cfg_k(KmppBuffer buffer)
{
    KmppBufferImpl *impl = (KmppBufferImpl *)kmpp_obj_to_entry(buffer);

    return impl ? impl->cfg_ext : NULL;
}

rk_u64 kmpp_buffer_get_cfg_u(KmppBuffer buffer)
{
    KmppBufferImpl *impl = (KmppBufferImpl *)kmpp_obj_to_entry(buffer);

    return impl ? impl->cfg.uaddr : 0;
}

void *kmpp_buffer_get_kptr(KmppBuffer buffer)
{
    KmppBufferImpl *impl = (KmppBufferImpl *)kmpp_obj_to_entry(buffer);

    if (!impl)
        return NULL;

    if (impl->kptr)
        return impl->kptr;

    if (impl->buf)
        impl->kptr = kmpp_dmabuf_get_kptr(impl->buf);

    return impl->kptr;
}

rk_u64 kmpp_buffer_get_uptr(KmppBuffer buffer)
{
    KmppBufferImpl *impl = (KmppBufferImpl *)kmpp_obj_to_entry(buffer);

    if (!impl)
        return 0;

    if (impl->uaddr)
        return impl->uaddr;

    if (impl->buf)
        impl->uaddr = kmpp_dmabuf_get_uptr(impl->buf);

    return impl->uaddr;
}

rk_s32 kmpp_buffer_put_uptr(KmppBuffer buffer)
{
    KmppBufferImpl *impl = (KmppBufferImpl *)kmpp_obj_to_entry(buffer);

    if (!impl || !impl->buf) {
        kmpp_loge_f("invalid param buffer %px dmabuf %px\n",
                    impl, impl ? impl->buf : NULL);
        return rk_nok;
    }

    if (!kmpp_dmabuf_put_uptr(impl->buf)) {
        impl->uaddr = 0;
        return rk_ok;
    }

    return rk_nok;
}

rk_u32 kmpp_buffer_get_size(KmppBuffer buffer)
{
    KmppBufferImpl *impl = (KmppBufferImpl *)kmpp_obj_to_entry(buffer);

    return impl ? impl->size : 0;
}

KmppDmaBuf kmpp_buffer_get_dmabuf(KmppBuffer buffer)
{
    KmppBufferImpl *impl = (KmppBufferImpl *)kmpp_obj_to_entry(buffer);

    return impl ? impl->buf : NULL;
}

static rk_s32 kmpp_buffer_ioc_setup(KmppFsDev *file, KmppShmPtr *shm)
{
    KmppObj buf = shms_to_ctx(shm);
    rk_s32 ret = rk_nok;

    buf_dbg_flow("enter file %px buf %px\n", file, buf);

    if (buf) {
        ret = kmpp_buffer_setup_f(buf, file);
        if (ret)
            kmpp_loge_f("buf setup buffer %px failed ret %d\n", buf, ret);
    }

    buf_dbg_flow("leave ret %d\n", ret);

    return ret;
}

static rk_s32 kmpp_buffer_ioc_inc_ref(KmppFsDev *file, KmppShmPtr *shm)
{
    KmppObj buf = shms_to_ctx(shm);
    rk_s32 ret = rk_nok;

    buf_dbg_flow("enter file %px buf %px\n", file, buf);

    if (buf) {
        ret = kmpp_buffer_inc_ref_f(buf);
        if (ret)
            kmpp_loge_f("buf inc_ref buffer %px failed ret %d\n", buf, ret);
    }

    buf_dbg_flow("leave ret %d\n", ret);
    return ret;
}

static rk_s32 kmpp_buffer_ioc_flush(KmppFsDev *file, KmppShmPtr *shm)
{
    KmppObj buf = shms_to_ctx(shm);
    rk_s32 ret = rk_nok;

    buf_dbg_flow("enter file %px buf %px\n", file, buf);

    if (buf) {
        ret = kmpp_buffer_flush_for_device(buf, NULL);
        if (ret)
            kmpp_loge_f("buf cache flush buffer %px failed ret %d\n", buf, ret);
    }

    buf_dbg_flow("leave ret %d\n", ret);
    return ret;
}

static KmppObjIoctl kmpp_buffer_ioctls[] = {
    KMPP_OBJ_IOCTL("setup",     kmpp_buffer_ioc_setup,      "run buffer config and alloc or import dmabuf"),
    KMPP_OBJ_IOCTL("inc_ref",   kmpp_buffer_ioc_inc_ref,    "increase buffer reference count"),
    KMPP_OBJ_IOCTL("flush",     kmpp_buffer_ioc_flush,      "flush buffer cache to device"),
};

static KmppObjIoctls kmpp_buffer_ioc = {
    .count = KMPP_ARRAY_ELEMS(kmpp_buffer_ioctls),
    .funcs = kmpp_buffer_ioctls,
};

#define KMPP_OBJ_NAME               kmpp_buffer
#define KMPP_OBJ_INTF_TYPE          KmppBuffer
#define KMPP_OBJ_IMPL_TYPE          KmppBufferImpl
#define KMPP_OBJ_EXTRA_SIZE         kmpp_mutex_size()
#define KMPP_OBJ_ENTRY_TABLE        KMPP_BUFFER_ENTRY_TABLE
#define KMPP_OBJ_FUNC_INIT          kmpp_buffer_impl_init
#define KMPP_OBJ_FUNC_DEINIT        kmpp_buffer_impl_deinit
#define KMPP_OBJ_FUNC_IOCTL         kmpp_buffer_ioc
#define KMPP_OBJ_FUNC_DUMP          kmpp_buffer_impl_dump
#include "kmpp_obj_helper.h"

rk_s32 kmpp_buf_cfg_impl_init(void *entry, KmppObj obj, KmppFsDev *file, const rk_u8 *caller)
{
    KmppBufCfgImpl *impl = (KmppBufCfgImpl *)entry;

    impl->fd = -1;

    return rk_ok;
}

static rk_s32 kmpp_buf_cfg_impl_dump(void *entry)
{
    KmppBufCfgImpl *impl = (KmppBufCfgImpl*)entry;

    if (!impl) {
        kmpp_loge_f("invalid param frame NULL\n");
        return rk_nok;
    }

    kmpp_logi("addr [u:k]       %#llx:%#px\n", impl->sptr.uaddr, impl->sptr.kptr);
    kmpp_logi("size             %d\n", impl->size);
    kmpp_logi("offset           %d\n", impl->offset);
    kmpp_logi("flag             %d\n", impl->flag);
    kmpp_logi("fd               %d\n", impl->fd);
    kmpp_logi("index            %d\n", impl->index);

    return rk_ok;
}

#define KMPP_OBJ_NAME               kmpp_buf_cfg
#define KMPP_OBJ_INTF_TYPE          KmppBufCfg
#define KMPP_OBJ_IMPL_TYPE          KmppBufCfgImpl
#define KMPP_OBJ_ENTRY_TABLE        KMPP_BUF_CFG_ENTRY_TABLE
#define KMPP_OBJ_FUNC_INIT          kmpp_buf_cfg_impl_init
#define KMPP_OBJ_FUNC_DUMP          kmpp_buf_cfg_impl_dump
#define KMPP_OBJ_FUNC_EXPORT_ENABLE
#define KMPP_OBJ_SHARE_ENABLE
#include "kmpp_obj_helper.h"


rk_s32 kmpp_buffer_read(KmppBuffer buffer, rk_u32 offset, void *data, rk_s32 size, const rk_u8 *caller)
{
    return rk_nok;
}

rk_s32 kmpp_buffer_write(KmppBuffer buffer, rk_u32 offset, void *data, rk_s32 size, const rk_u8 *caller)
{
    return rk_nok;
}

rk_s32 kmpp_buffer_get_iova(KmppBuffer buffer, rk_u64 *iova, KmppDev *dev)
{
    return rk_nok;
}

rk_s32 kmpp_buffer_put_iova(KmppBuffer buffer, rk_u64 iova, KmppDev *dev)
{
    return rk_nok;
}

rk_s32 kmpp_buffer_get_iova_by_device(KmppBuffer buffer, rk_u64 *iova, void *device)
{
    KmppBufferImpl *impl = (KmppBufferImpl *)kmpp_obj_to_entry(buffer);
    rk_s32 ret = rk_nok;

    if (!iova) {
        kmpp_loge_f("invalid param iova %px\n", iova);
        return ret;
    }

    *iova = kmpp_invalid_iova();

    if (!impl || !impl->buf) {
        kmpp_loge_f("invalid param buffer %px dmabuf %px\n",
                    impl, impl ? impl->buf : NULL);
        return ret;
    }

    kmpp_mutex_lock(impl->mutex_maps);
    do {
        KmppBufIovaMap *map, *n;

        kmpp_list_for_each_entry_safe(map, n, &impl->list_maps, KmppBufIovaMap, list) {
            if (map->device == device) {
                *iova = map->iova;
                ret = rk_ok;
                goto done;
            }
        }

        map = kmpp_calloc_atomic(sizeof(*map));
        if (!map) {
            kmpp_loge_f("failed to create iova node\n");
            break;
        }

        ret = kmpp_dmabuf_get_iova_by_device(impl->buf, iova, device);
        if (ret) {
            kmpp_loge_f("failed to get iova from dev %s\n", kmpp_device_name(device));
            kmpp_free(map);
            break;
        }

        KMPP_INIT_LIST_HEAD(&map->list);
        kmpp_list_add_tail(&map->list, &impl->list_maps);
        map->buf = impl;
        map->iova = *iova;
        map->size = impl->size;
        map->mode = KMPP_BUF_MAP_BY_SYS_DEVICE;
        map->device = device;
    } while (0);

done:
    kmpp_mutex_unlock(impl->mutex_maps);

    buf_dbg_info("buf %d [%d:%d] get iova %llx from dev %s\n", impl->buf_uid,
                 impl->grp_id, impl->buf_gid, *iova, kmpp_device_name(device));

    return ret;
}

rk_s32 kmpp_buffer_put_iova_by_device(KmppBuffer buffer, rk_u64 iova, void *device)
{
    KmppBufferImpl *impl = (KmppBufferImpl *)kmpp_obj_to_entry(buffer);
    KmppBufIovaMap *map = NULL;
    rk_s32 ret = rk_nok;

    if (!impl || !impl->buf || iova == kmpp_invalid_iova()) {
        kmpp_loge_f("invalid param buffer %px dmabuf %px iova %#llx\n",
                    impl, impl ? impl->buf : NULL, iova);
        return ret;
    }

    kmpp_mutex_lock(impl->mutex_maps);

    do {
        KmppBufIovaMap *pos, *n;

        kmpp_list_for_each_entry_safe(pos, n, &impl->list_maps, KmppBufIovaMap, list) {
            if (pos->device == device && pos->iova == iova) {
                kmpp_list_del_init(&pos->list);
                map = pos;
                break;
            }
        }
    } while (0);

    kmpp_mutex_unlock(impl->mutex_maps);

    if (!map) {
        kmpp_loge_f("failed to find iova %#llx from dev %s\n",
                    iova, kmpp_device_name(device));
        return ret;
    }

    ret = kmpp_dmabuf_put_iova_by_device(impl->buf, iova, device);

    buf_dbg_info("buf %d [%d:%d] put iova %#llx from dev %s\n", impl->buf_uid,
                 impl->grp_id, impl->buf_gid, iova, kmpp_device_name(device));

    kmpp_free(map);

    return ret;
}

rk_s32 kmpp_buffer_flush_for_cpu(KmppBuffer buffer)
{
    KmppBufferImpl *impl = (KmppBufferImpl *)kmpp_obj_to_entry(buffer);

    if (!impl || !impl->buf) {
        kmpp_loge_f("invalid param buffer %px dmabuf %px\n",
                    impl, impl ? impl->buf : NULL);
        return rk_nok;
    }

    return kmpp_dmabuf_flush_for_cpu(impl->buf);
}

rk_s32 kmpp_buffer_flush_for_dev(KmppBuffer buffer, KmppDev *dev)
{
    KmppBufferImpl *impl = (KmppBufferImpl *)kmpp_obj_to_entry(buffer);

    if (!impl || !impl->buf) {
        kmpp_loge_f("invalid param buffer %px dmabuf %px\n",
                    impl, impl ? impl->buf : NULL);
        return rk_nok;
    }

    return kmpp_dmabuf_flush_for_dev(impl->buf, dev);
}

rk_s32 kmpp_buffer_flush_for_device(KmppBuffer buffer, void *device)
{
    KmppBufferImpl *impl = (KmppBufferImpl *)kmpp_obj_to_entry(buffer);

    if (!impl || !impl->buf) {
        kmpp_loge_f("invalid param buffer %px dmabuf %px\n",
                    impl, impl ? impl->buf : NULL);
        return rk_nok;
    }

    return kmpp_dmabuf_flush_for_dev(impl->buf, NULL);
}

rk_s32 kmpp_buffer_flush_for_cpu_partial(KmppBuffer buffer, rk_u32 offset, rk_u32 size)
{
    KmppBufferImpl *impl = (KmppBufferImpl *)kmpp_obj_to_entry(buffer);

    if (!impl || !impl->buf) {
        kmpp_loge_f("invalid param buffer %px dmabuf %px\n",
                    impl, impl ? impl->buf : NULL);
        return rk_nok;
    }

    return kmpp_dmabuf_flush_for_cpu_partial(impl->buf, offset, size);
}

rk_s32 kmpp_buffer_flush_for_dev_partial(KmppBuffer buffer, KmppDev *dev, rk_u32 offset, rk_u32 size)
{
    KmppBufferImpl *impl = (KmppBufferImpl *)kmpp_obj_to_entry(buffer);

    if (!impl || !impl->buf) {
        kmpp_loge_f("invalid param buffer %px dmabuf %px\n",
                    impl, impl ? impl->buf : NULL);
        return rk_nok;
    }

    return kmpp_dmabuf_flush_for_dev_partial(impl->buf, dev, offset, size);
}

rk_s32 kmpp_buffer_flush_for_device_partial(KmppBuffer buffer, void *device, rk_u32 offset, rk_u32 size)
{
    KmppBufferImpl *impl = (KmppBufferImpl *)kmpp_obj_to_entry(buffer);

    if (!impl || !impl->buf) {
        kmpp_loge_f("invalid param buffer %px dmabuf %px\n",
                    impl, impl ? impl->buf : NULL);
        return rk_nok;
    }

    return kmpp_dmabuf_flush_for_dev_partial(impl->buf, NULL, offset, size);
}

#include <linux/export.h>

EXPORT_SYMBOL(kmpp_buf_grp_get_cfg_s);
EXPORT_SYMBOL(kmpp_buf_grp_get_cfg_k);
EXPORT_SYMBOL(kmpp_buf_grp_get_cfg_u);
EXPORT_SYMBOL(kmpp_buf_grp_get_count);
EXPORT_SYMBOL(kmpp_buf_grp_setup);

EXPORT_SYMBOL(kmpp_buffer_inc_ref);
EXPORT_SYMBOL(kmpp_buffer_setup);
EXPORT_SYMBOL(kmpp_buffer_get_cfg_s);
EXPORT_SYMBOL(kmpp_buffer_get_cfg_k);
EXPORT_SYMBOL(kmpp_buffer_get_cfg_u);
EXPORT_SYMBOL(kmpp_buffer_get_kptr);
EXPORT_SYMBOL(kmpp_buffer_get_uptr);
EXPORT_SYMBOL(kmpp_buffer_put_uptr);
EXPORT_SYMBOL(kmpp_buffer_get_dmabuf);

EXPORT_SYMBOL(kmpp_buffer_read);
EXPORT_SYMBOL(kmpp_buffer_write);
EXPORT_SYMBOL(kmpp_buffer_get_iova);
EXPORT_SYMBOL(kmpp_buffer_put_iova);
EXPORT_SYMBOL(kmpp_buffer_get_iova_by_device);
EXPORT_SYMBOL(kmpp_buffer_put_iova_by_device);

EXPORT_SYMBOL(kmpp_buffer_flush_for_cpu);
EXPORT_SYMBOL(kmpp_buffer_flush_for_dev);
EXPORT_SYMBOL(kmpp_buffer_flush_for_device);
EXPORT_SYMBOL(kmpp_buffer_flush_for_cpu_partial);
EXPORT_SYMBOL(kmpp_buffer_flush_for_dev_partial);
EXPORT_SYMBOL(kmpp_buffer_flush_for_device_partial);

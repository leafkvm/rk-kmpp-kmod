/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_venc"

#include "kmpp_log.h"
#include "kmpp_macro.h"
#include "kmpp_module.h"

#include "kmpp_obj.h"

#include "kmpp_mod_id.h"
#include "kmpp_venc.h"
#include "kmpp_venc_ioctl.h"
#include "kmpp_venc_objs_impl.h"

#define VENC_DBG_FLOW                   (0x00000001)

#define venc_dbg(flag, fmt, ...)        kmpp_dbg_f(kmpp_venc_debug, flag, fmt, ## __VA_ARGS__)

#define venc_dbg_flow(fmt, ...)         venc_dbg(VENC_DBG_FLOW, fmt, ## __VA_ARGS__)

#define get_venc_srv(caller) \
    ({ \
        KmppVencSrv *__tmp; \
        if (srv_venc) { \
            __tmp = srv_venc; \
        } else { \
            kmpp_loge_f("kmpp venc srv not init at %s\n", caller); \
            __tmp = NULL; \
        } \
        __tmp; \
    })

#define get_venc_srv_f() get_venc_srv(__FUNCTION__)

typedef struct KmppVencSrv_t {
    KmppSpinlock        *lock;
    kmpp_list_head      list;
    KmppEnvNode         env;
} KmppVencSrv;

static KmppVencSrv *srv_venc = NULL;
static rk_u32 kmpp_venc_debug = 0;

static rk_s32 venc_obj_init(void *entry, KmppObj obj, KmppFsDev *file, const rk_u8 *caller)
{
    KmppVencImpl *impl = (KmppVencImpl *)kmpp_obj_to_priv(obj);
    KmppVencSrv *srv = get_venc_srv(caller);
    KmppShmPtr sptr;

    venc_dbg_flow("enter\n");

    if (!srv)
        return rk_nok;

    if (impl) {
        KMPP_INIT_LIST_HEAD(&impl->list_srv);

        kmpp_spin_lock(srv->lock);
        kmpp_list_add_tail(&impl->list_srv, &srv->list);
        kmpp_spin_unlock(srv->lock);
    }

    kmpp_obj_to_shmptr(obj, &sptr);

    venc_dbg_flow("leave\n");

    return rk_ok;
}

static rk_s32 venc_obj_deinit(void *entry, KmppObj obj, const rk_u8 *caller)
{
    KmppVencImpl *impl = (KmppVencImpl *)kmpp_obj_to_priv(obj);
    KmppVencSrv *srv = get_venc_srv(caller);
    KmppShmPtr sptr;

    venc_dbg_flow("enter\n");

    if (!srv)
        return rk_nok;

    kmpp_spin_lock(srv->lock);
    kmpp_list_del_init(&impl->list_srv);
    kmpp_spin_unlock(srv->lock);

    kmpp_obj_to_shmptr(obj, &sptr);

    venc_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 venc_impl_init(KmppVenc venc, KmppVencInitCfg cfg)
{
    venc_dbg_flow("enter\n");

    venc_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 venc_impl_deinit(KmppVenc venc)
{
    venc_dbg_flow("enter\n");

    venc_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 venc_impl_reset(KmppVenc venc)
{
    venc_dbg_flow("enter\n");

    venc_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 venc_impl_start(KmppVenc venc)
{
    venc_dbg_flow("enter\n");

    venc_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 venc_impl_stop(KmppVenc venc)
{
    venc_dbg_flow("enter\n");

    venc_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 venc_impl_suspend(KmppVenc venc)
{
    venc_dbg_flow("enter\n");

    venc_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 venc_impl_resume(KmppVenc venc)
{
    venc_dbg_flow("enter\n");

    venc_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 venc_impl_get_cfg(KmppVenc venc, KmppVencStCfg cfg)
{
    venc_dbg_flow("enter\n");

    venc_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 venc_impl_set_cfg(KmppVenc venc, KmppVencStCfg cfg)
{
    venc_dbg_flow("enter\n");

    venc_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 venc_impl_encode(KmppVenc venc, KmppPacket pkt, KmppFrame *frame)
{
    venc_dbg_flow("enter\n");

    venc_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 venc_impl_get_frm(KmppVenc venc, KmppFrame *frm)
{
    venc_dbg_flow("enter\n");

    venc_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 venc_impl_put_frm(KmppVenc venc, KmppFrame frm)
{
    venc_dbg_flow("enter\n");

    venc_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 venc_impl_get_pkt(KmppVenc venc, KmppPacket *pkt)
{
    venc_dbg_flow("enter\n");

    venc_dbg_flow("leave\n");

    return rk_ok;
}

rk_s32 venc_impl_put_pkt(KmppVenc venc, KmppPacket pkt)
{
    venc_dbg_flow("enter\n");

    venc_dbg_flow("leave\n");

    return rk_ok;
}

#define KMPP_OBJ_NAME               venc_impl
#define KMPP_OBJ_INTF_TYPE          KmppVenc
#define KMPP_OBJ_PRIV_SIZE          sizeof(KmppVencImpl)
#define KMPP_OBJ_FUNC_INIT          venc_obj_init
#define KMPP_OBJ_FUNC_DEINIT        venc_obj_deinit
#define KMPP_OBJ_FUNC_IOCTL         kmpp_venc_ioc_info
#include "kmpp_obj_helper.h"

static rk_s32 kmpp_venc_srv_init(void)
{
    KmppVencSrv *srv = srv_venc;
    rk_s32 lock_size;
    rk_s32 size;

    if (srv)
        return rk_ok;

    lock_size = kmpp_spinlock_size();
    size = sizeof(KmppVencSrv) + lock_size;
    srv = (KmppVencSrv *)kmpp_calloc_atomic(size);
    if (!srv) {
        kmpp_loge_f("failed to alloc venc srv size %d\n", size);
        return rk_nok;
    }

    kmpp_spinlock_assign(&srv->lock, (void *)(srv + 1), lock_size);

    KMPP_INIT_LIST_HEAD(&srv->list);

    venc_impl_objdef_get();

    {
        KmppEnvInfo env;

        env.type = KmppEnv_u32;
        env.readonly = 0;
        env.name = "kmpp_venc_debug";
        env.val = &kmpp_venc_debug;
        env.env_show = NULL;

        kmpp_env_add(kmpp_env_debug(), &srv->env, &env);
    }

    srv_venc = srv;

    return rk_ok;
}

static rk_s32 kmpp_venc_srv_deinit(void)
{
    KmppVencSrv *srv = srv_venc;

    if (!srv)
        return rk_nok;

    venc_impl_objdef_put();

    kmpp_env_del(kmpp_env_debug(), srv->env);

    kmpp_spinlock_deinit(&srv->lock);

    kmpp_free(srv);
    srv_venc = NULL;

    return rk_ok;
}

KMPP_MODULE_ADD(kmpp, KMPP_MOD_VENC, venc, kmpp_venc_srv_init, kmpp_venc_srv_deinit);

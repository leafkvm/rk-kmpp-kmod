/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_session"

#include "kmpp_osal.h"
#include "kmpp_module.h"
#include "kmpp_mod_id.h"
#include "kmpp_sys.h"

#include "mpp_common.h"
#include "kmpp_session_legacy.h"

typedef struct KmppSessionEntry_t {
    MppCtxType type;
    MppCodingType coding;
    rk_u32 client_type;
    void *device;

    KmppShmPtr req;
    /* readonly max request */
    rk_u32 req_max;
    /* valid request count */
    rk_u32 req_cnt;
    rk_u32 req_size;

    KmppShmPtr regs;
    rk_u32 regs_cnt;
    rk_u32 regs_size;
} KmppSessionEntry;

rk_s32 kmpp_session_setup(KmppSession obj, KmppSessionCfg *cfg)
{
    KmppSessionOld *session = (KmppSessionOld *)kmpp_obj_to_priv(obj);

    return kmpp_session_setup_legacy(session, cfg);
}

rk_s32 kmpp_session_is_running(KmppSession obj)
{
    KmppSessionOld *session = (KmppSessionOld *)kmpp_obj_to_priv(obj);

    return kmpp_session_is_running_legacy(session);
}

rk_s32 kmpp_session_run_task(KmppSession obj, void *param)
{
    KmppSessionOld *session = (KmppSessionOld *)kmpp_obj_to_priv(obj);

    return kmpp_session_run_task_legacy(session, param);
}

rk_s32 kmpp_session_add_req(KmppSession obj, void *reqs)
{
    KmppSessionOld *session = (KmppSessionOld *)kmpp_obj_to_priv(obj);

    return kmpp_session_add_req_legacy(session, reqs);
}

void *kmpp_session_get_dev(KmppSession obj)
{
    KmppSessionOld *session = (KmppSessionOld *)kmpp_obj_to_priv(obj);

    return kmpp_session_get_dev_legacy(session);
}

static rk_s32 kmpp_session_ioc_setup(KmppFsDev *file, KmppShmPtr *shm)
{
    return rk_ok;
}

static rk_s32 kmpp_session_ioc_send(KmppFsDev *file, KmppShmPtr *shm)
{
    return rk_ok;
}

static rk_s32 kmpp_session_ioc_recv(KmppFsDev *file, KmppShmPtr *shm)
{
    return rk_ok;
}

static KmppObjIoctl kmpp_session_ioctls[] = {
    KMPP_OBJ_IOCTL("setup", kmpp_session_ioc_setup, "setup kernel session with client type"),
    KMPP_OBJ_IOCTL("send",  kmpp_session_ioc_send, "send request to kernel session"),
    KMPP_OBJ_IOCTL("recv",  kmpp_session_ioc_recv, "receive response from kernel session"),
};

static KmppObjIoctls kmpp_session_ioc = {
    .count = KMPP_ARRAY_ELEMS(kmpp_session_ioctls),
    .funcs = kmpp_session_ioctls,
};

#define KMPP_SESSION_ENTRY_TABLE(prefix, ENTRY, STRCT, EHOOK, SHOOK, ALIAS) \
    ENTRY(prefix, u32,  MppCtxType,     type,           FLAG_NONE,  type) \
    ENTRY(prefix, u32,  MppCodingType,  coding,         FLAG_NONE,  coding) \
    ENTRY(prefix, u32,  rk_u32,         client_type,    FLAG_NONE,  client_type) \
    ENTRY(prefix, kptr, void *,         device,         FLAG_NONE,  device) \
    STRCT(prefix, shm,  KmppShmPtr,     regs,           FLAG_NONE,  regs)

rk_s32 kmpp_session_init(void *entry, KmppObj obj, KmppFsDev *file, const rk_u8 *caller)
{
    KmppSessionOld *session = (KmppSessionOld *)kmpp_obj_to_priv(obj);

    if (session)
        kmpp_session_init_legacy(session, obj);

    return rk_ok;
}

rk_s32 kmpp_session_deinit(void *entry, KmppObj obj, const rk_u8 *caller)
{
    KmppSessionOld *session = (KmppSessionOld *)kmpp_obj_to_priv(obj);

    if (session)
        return kmpp_session_deinit_legacy(session);

    return rk_ok;
}

#define KMPP_OBJ_NAME                   kmpp_session
#define KMPP_OBJ_INTF_TYPE              KmppSession
#define KMPP_OBJ_IMPL_TYPE              KmppSessionEntry
#define KMPP_OBJ_PRIV_SIZE              sizeof(struct mpp_session)
#define KMPP_OBJ_ENTRY_TABLE            KMPP_SESSION_ENTRY_TABLE
#define KMPP_OBJ_FUNC_INIT              kmpp_session_init
#define KMPP_OBJ_FUNC_DEINIT            kmpp_session_deinit
#define KMPP_OBJ_FUNC_IOCTL             kmpp_session_ioc
#include "kmpp_obj_helper.h"

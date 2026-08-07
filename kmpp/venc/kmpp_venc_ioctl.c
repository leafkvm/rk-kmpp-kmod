/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_venc_ioctl"

#include "kmpp_osal.h"
#include "kmpp_sys.h"

#include "kmpp_venc.h"

#define VENC_IOC_CTX(func) \
    static rk_s32 ioc_##func(KmppFsDev *file, KmppShmPtr *shm) \
    { \
        KmppObj ctx = kmpp_shm_get_kpriv(kmpp_shm_from_shmptr_f(&shm[IOC_SHM_CTX])); \
        if (!ctx || kmpp_obj_check_f(ctx)) { \
            kmpp_loge_f("invalid ctx %px\n", ctx); \
            return rk_nok; \
        } \
        return func(ctx); \
    }

#define VENC_IOC_IN(func) \
    static rk_s32 ioc_##func(KmppFsDev *file, KmppShmPtr *shm) \
    { \
        KmppObj ctx = kmpp_shm_get_kpriv(kmpp_shm_from_shmptr_f(&shm[IOC_SHM_CTX])); \
        KmppObj in = kmpp_shm_get_kpriv(kmpp_shm_from_shmptr_f(&shm[IOC_SHM_IN])); \
        if (!ctx || kmpp_obj_check_f(ctx)) { \
            kmpp_loge_f("invalid ctx %px\n", ctx); \
            return rk_nok; \
        } \
        return func(ctx, in); \
    }

#define VENC_IOC_OUT(func) \
    static rk_s32 ioc_##func(KmppFsDev *file, KmppShmPtr *shm) \
    { \
        KmppObj ctx = kmpp_shm_get_kpriv(kmpp_shm_from_shmptr_f(&shm[IOC_SHM_CTX])); \
        KmppObj out = NULL; \
        rk_s32 ret = rk_nok; \
        if (!ctx || kmpp_obj_check_f(ctx)) { \
            kmpp_loge_f("invalid ctx %px\n", ctx); \
            return ret; \
        } \
        ret = func(ctx, &out); \
        kmpp_obj_to_shmptr(out, &shm[IOC_SHM_OUT]); \
        return ret; \
    }

#define VENC_IOC_IN_OUT(func) \
    static rk_s32 ioc_##func(KmppFsDev *file, KmppShmPtr *shm) \
    { \
        KmppObj ctx = kmpp_shm_get_kpriv(kmpp_shm_from_shmptr_f(&shm[IOC_SHM_CTX])); \
        KmppObj in = kmpp_shm_get_kpriv(kmpp_shm_from_shmptr_f(&shm[IOC_SHM_IN])); \
        KmppObj out = NULL; \
        rk_s32 ret = rk_nok; \
        if (!ctx || kmpp_obj_check_f(ctx)) { \
            kmpp_loge_f("invalid ctx %px\n", ctx); \
            return ret; \
        } \
        ret = func(ctx, in, &out); \
        kmpp_obj_to_shmptr(out, &shm[IOC_SHM_OUT]); \
        return ret; \
    }

/* flow control function */
VENC_IOC_IN(venc_impl_init)
VENC_IOC_CTX(venc_impl_deinit)
VENC_IOC_CTX(venc_impl_reset)
VENC_IOC_CTX(venc_impl_start)
VENC_IOC_CTX(venc_impl_stop)
VENC_IOC_CTX(venc_impl_suspend)
VENC_IOC_CTX(venc_impl_resume)
VENC_IOC_IN(venc_impl_get_cfg)
VENC_IOC_IN(venc_impl_set_cfg)

/* data process function */
VENC_IOC_IN_OUT(venc_impl_encode)
VENC_IOC_OUT(venc_impl_get_frm)
VENC_IOC_IN(venc_impl_put_frm)
VENC_IOC_OUT(venc_impl_get_pkt)
VENC_IOC_IN(venc_impl_put_pkt)

static KmppObjIoctl kmpp_venc_ioctls[] = {
    KMPP_OBJ_IOCTL("init",      ioc_venc_impl_init,     "init encoder with one-shot unchanged config"),
    KMPP_OBJ_IOCTL("deinit",    ioc_venc_impl_deinit,   "deinit encoder and release all resources"),
    KMPP_OBJ_IOCTL("reset",     ioc_venc_impl_reset,    "reset and drop all unhandled input data"),
    KMPP_OBJ_IOCTL("start",     ioc_venc_impl_start,    "enable all static config and start data process"),
    KMPP_OBJ_IOCTL("stop",      ioc_venc_impl_stop,     "stop data process and ready for static config update"),
    KMPP_OBJ_IOCTL("suspend",   ioc_venc_impl_suspend,  "suspend data process"),
    KMPP_OBJ_IOCTL("resume",    ioc_venc_impl_resume,   "resume data process"),
    KMPP_OBJ_IOCTL("get_cfg",   ioc_venc_impl_get_cfg,  "get static config copy from encoder"),
    KMPP_OBJ_IOCTL("set_cfg",   ioc_venc_impl_set_cfg,  "set updated static config to encoder (only on stopped status)"),
    KMPP_OBJ_IOCTL("encode",    ioc_venc_impl_encode,   "put frame and get packet in one-shot"),
    KMPP_OBJ_IOCTL("get_frm",   ioc_venc_impl_get_frm,  "get idle frame buffer from encoder"),
    KMPP_OBJ_IOCTL("put_frm",   ioc_venc_impl_put_frm,  "put frame to encoder for encoding"),
    KMPP_OBJ_IOCTL("get_pkt",   ioc_venc_impl_get_pkt,  "get encoded packet from encoder"),
    KMPP_OBJ_IOCTL("put_pkt",   ioc_venc_impl_put_pkt,  "put finished packet to encoder"),
};

KmppObjIoctls kmpp_venc_ioc_info = {
    .count = KMPP_ARRAY_ELEMS(kmpp_venc_ioctls),
    .funcs = kmpp_venc_ioctls,
};

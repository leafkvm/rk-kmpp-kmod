/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_vdec_ioctl"

#include "kmpp_osal.h"
#include "kmpp_sys.h"

#include "kmpp_vdec.h"

#define VDEC_IOC_CTX(func) \
    static rk_s32 ioc_##func(KmppFsDev *file, KmppShmPtr *shm) \
    { \
        KmppObj ctx = kmpp_shm_get_kpriv(kmpp_shm_from_shmptr_f(&shm[IOC_SHM_CTX])); \
        if (!ctx || kmpp_obj_check_f(ctx)) { \
            kmpp_loge_f("invalid ctx %px\n", ctx); \
            return rk_nok; \
        } \
        return func(ctx); \
    }

#define VDEC_IOC_IN(func) \
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

#define VDEC_IOC_OUT(func) \
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

#define VDEC_IOC_IN_OUT(func) \
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
VDEC_IOC_IN(vdec_impl_init)
VDEC_IOC_CTX(vdec_impl_deinit)
VDEC_IOC_CTX(vdec_impl_reset)
VDEC_IOC_CTX(vdec_impl_start)
VDEC_IOC_CTX(vdec_impl_stop)
VDEC_IOC_CTX(vdec_impl_suspend)
VDEC_IOC_CTX(vdec_impl_resume)
VDEC_IOC_IN(vdec_impl_get_cfg)
VDEC_IOC_IN(vdec_impl_set_cfg)

/* data process function */
VDEC_IOC_IN_OUT(vdec_impl_decode)
VDEC_IOC_IN(vdec_impl_put_pkt)
VDEC_IOC_OUT(vdec_impl_get_frm)
VDEC_IOC_IN(vdec_impl_put_frm)

static KmppObjIoctl kmpp_vdec_ioctls[] = {
    KMPP_OBJ_IOCTL("init",      ioc_vdec_impl_init,     "init decoder with one-shot unchanged config"),
    KMPP_OBJ_IOCTL("deinit",    ioc_vdec_impl_deinit,   "deinit decoder and release all resources"),
    KMPP_OBJ_IOCTL("reset",     ioc_vdec_impl_reset,    "reset and drop all unhandled input data"),
    KMPP_OBJ_IOCTL("start",     ioc_vdec_impl_start,    "enable all static config and start data process"),
    KMPP_OBJ_IOCTL("stop",      ioc_vdec_impl_stop,     "stop data process and ready for static config update"),
    KMPP_OBJ_IOCTL("suspend",   ioc_vdec_impl_suspend,  "suspend data process (auto called on info change)"),
    KMPP_OBJ_IOCTL("resume",    ioc_vdec_impl_resume,   "resume data process (notify info change ready)"),
    KMPP_OBJ_IOCTL("get_cfg",   ioc_vdec_impl_get_cfg,  "get static config copy from decoder"),
    KMPP_OBJ_IOCTL("set_cfg",   ioc_vdec_impl_set_cfg,  "set updated static config to decoder (only on stopped status)"),
    KMPP_OBJ_IOCTL("decode",    ioc_vdec_impl_decode,   "put packet and get frame in one-shot"),
    KMPP_OBJ_IOCTL("put_pkt",   ioc_vdec_impl_put_pkt,  "put packet to decoder when pkt len is zero packet can be dropped"),
    KMPP_OBJ_IOCTL("get_frm",   ioc_vdec_impl_get_frm,  "get decoded frame from decoder"),
    KMPP_OBJ_IOCTL("put_frm",   ioc_vdec_impl_put_frm,  "return the finished decoded frame back to decoder"),
};

KmppObjIoctls kmpp_vdec_ioc_info = {
    .count = KMPP_ARRAY_ELEMS(kmpp_vdec_ioctls),
    .funcs = kmpp_vdec_ioctls,
};

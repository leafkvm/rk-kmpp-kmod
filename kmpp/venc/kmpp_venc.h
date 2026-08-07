/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_VENC_H__
#define __KMPP_VENC_H__

#include "kmpp_osal.h"

#include "kmpi_venc.h"

typedef void* KmppVenc;
typedef void* KmppVencFlow;

typedef void* KmppVencInst;
typedef void* VencRequest;
typedef void* VencTask;

typedef void* KmppVencHal;
typedef void* VencHalRequest;
typedef void* VencHalTask;

typedef struct KmppVencImpl_t {
    /* list to list in KmppVdecSrv */
    kmpp_list_head      list_srv;

    MppCodingType       coding;
    rk_u32              width;
    rk_u32              height;
} KmppVencImpl;

#define KMPP_OBJ_NAME               venc_impl
#define KMPP_OBJ_INTF_TYPE          KmppVenc
#include "kmpp_obj_func.h"

rk_s32 venc_impl_init(KmppVenc venc, KmppVencInitCfg cfg);
rk_s32 venc_impl_deinit(KmppVenc venc);
rk_s32 venc_impl_reset(KmppVenc venc);
rk_s32 venc_impl_start(KmppVenc venc);
rk_s32 venc_impl_stop(KmppVenc venc);
rk_s32 venc_impl_suspend(KmppVenc venc);
rk_s32 venc_impl_resume(KmppVenc venc);

rk_s32 venc_impl_get_cfg(KmppVenc venc, KmppVencStCfg cfg);
rk_s32 venc_impl_set_cfg(KmppVenc venc, KmppVencStCfg cfg);

rk_s32 venc_impl_encode(KmppVenc venc, KmppPacket pkt, KmppFrame *frame);
rk_s32 venc_impl_get_frm(KmppVenc venc, KmppFrame *frm);
rk_s32 venc_impl_put_frm(KmppVenc venc, KmppFrame frm);
rk_s32 venc_impl_get_pkt(KmppVenc venc, KmppPacket *pkt);
rk_s32 venc_impl_put_pkt(KmppVenc venc, KmppPacket pkt);

#endif /* __KMPP_VENC_H__ */

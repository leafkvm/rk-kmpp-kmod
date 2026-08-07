/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_MOD_ID_H__
#define __KMPP_MOD_ID_H__

typedef enum KmppModId_e {
    KMPP_MOD_BUFFER,
    KMPP_MOD_META,
    KMPP_MOD_FRAME,
    KMPP_MOD_PACKET,
    KMPP_MOD_VENC_OBJS,
    KMPP_MOD_VENC,
    KMPP_MOD_VDEC_OBJS,
    KMPP_MOD_VDEC,
    KMPP_MOD_VSP_OBJS,
    KMPP_MOD_RC_API,
    KMPP_MOD_HAL,
} KmppModId;

#endif /* __KMPP_MOD_ID_H__ */

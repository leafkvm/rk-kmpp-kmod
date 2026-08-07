/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#ifndef __H264E_SEI_H__
#define __H264E_SEI_H__

#include "kmpp_packet.h"

#ifdef __cplusplus
extern "C" {
#endif

MPP_RET h264e_sei_to_packet(KmppPacket packet, RK_S32 *len, RK_S32 type,
                            RK_U8 uuid[16], const void *data, RK_S32 size);
MPP_RET h264e_sei_recovery_point_to_packet(KmppPacket packet, RK_S32 *len,
                                           RK_U32 recovery_frame_cnt);

#ifdef __cplusplus
}
#endif

#endif /* __H264E_SEI_H__ */

/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_FRAME_STUB_H__
#define __KMPP_FRAME_STUB_H__

#include "kmpp_stub.h"
#include "kmpp_frame.h"

#define KMPP_OBJ_NAME           kmpp_frame
#define KMPP_OBJ_INTF_TYPE      KmppFrame
#define KMPP_OBJ_ENTRY_TABLE    KMPP_FRAME_ENTRY_TABLE

#include "kmpp_obj_func_stub.h"

#define kmpp_frame_dump_f(frame) kmpp_frame_dump(frame, __FUNCTION__)

rk_s32 kmpp_frame_has_meta(const KmppFrame frame)
STUB_FUNC_RET_ZERO(kmpp_frame_has_meta);

MPP_RET kmpp_frame_copy(KmppFrame dst, KmppFrame src)
STUB_FUNC_RET_ZERO(kmpp_frame_copy);

MPP_RET kmpp_frame_info_cmp(KmppFrame frame0, KmppFrame frame1)
STUB_FUNC_RET_ZERO(kmpp_frame_info_cmp);

rk_s32 kmpp_frame_get_fbc_offset(KmppFrame frame, rk_u32 *offset)
STUB_FUNC_RET_ZERO(kmpp_frame_get_fbc_offset);

rk_s32 kmpp_frame_get_fbc_stride(KmppFrame frame, rk_u32 *stride)
STUB_FUNC_RET_ZERO(kmpp_frame_get_fbc_stride);

#endif /*__KMPP_FRAME_STUB_H__*/

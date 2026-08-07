/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_UACCESSS_H__
#define __KMPP_UACCESSS_H__

#include "rk_type.h"

rk_ul kmpp_copy_from_user(void *dst, const void *src, rk_ul size);
rk_ul kmpp_copy_to_user(void *dst, const void *src, rk_ul size);

rk_ul kmpp_strncpy_from_user(void *dst, void *src, rk_ul size);
rk_ul kmpp_strnlen_user(void *str, rk_ul size);

rk_ul kmpp_strncpy_sptr(void *dst, KmppShmPtr *src, rk_ul size);
rk_ul kmpp_strnlen_sptr(KmppShmPtr *src, rk_ul size);

#endif /* __KMPP_UACCESSS_H__ */

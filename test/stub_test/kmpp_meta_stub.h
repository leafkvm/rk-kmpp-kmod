/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_META_STUB_H__
#define __KMPP_META_STUB_H__

#include "kmpp_stub.h"
#include "kmpp_meta.h"

rk_s32 kmpp_meta_get(KmppMeta *meta, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_meta_get);
rk_s32 kmpp_meta_get_share(KmppMeta *meta, KmppFsDev *file, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_meta_get_share);
rk_s32 kmpp_meta_put(KmppMeta meta, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_meta_put);
rk_s32 kmpp_meta_size(KmppMeta meta)
STUB_FUNC_RET_ZERO(kmpp_meta_size);
rk_s32 kmpp_meta_dump(KmppMeta meta)
STUB_FUNC_RET_ZERO(kmpp_meta_dump);

rk_s32 kmpp_meta_set_s32(KmppMeta meta, KmppMetaKey key, rk_s32 val)
STUB_FUNC_RET_ZERO(kmpp_meta_set_s32);
rk_s32 kmpp_meta_set_s64(KmppMeta meta, KmppMetaKey key, rk_s64 val)
STUB_FUNC_RET_ZERO(kmpp_meta_set_s64);
rk_s32 kmpp_meta_set_ptr(KmppMeta meta, KmppMetaKey key, void  *val)
STUB_FUNC_RET_ZERO(kmpp_meta_set_ptr);
rk_s32 kmpp_meta_get_s32(KmppMeta meta, KmppMetaKey key, rk_s32 *val)
STUB_FUNC_RET_ZERO(kmpp_meta_get_s32);
rk_s32 kmpp_meta_get_s64(KmppMeta meta, KmppMetaKey key, rk_s64 *val)
STUB_FUNC_RET_ZERO(kmpp_meta_get_s64);
rk_s32 kmpp_meta_get_ptr(KmppMeta meta, KmppMetaKey key, void  **val)
STUB_FUNC_RET_ZERO(kmpp_meta_get_ptr);
rk_s32 kmpp_meta_get_s32_d(KmppMeta meta, KmppMetaKey key, rk_s32 *val, rk_s32 def)
STUB_FUNC_RET_ZERO(kmpp_meta_get_s32_d);
rk_s32 kmpp_meta_get_s64_d(KmppMeta meta, KmppMetaKey key, rk_s64 *val, rk_s64 def)
STUB_FUNC_RET_ZERO(kmpp_meta_get_s64_d);
rk_s32 kmpp_meta_get_ptr_d(KmppMeta meta, KmppMetaKey key, void  **val, void *def)
STUB_FUNC_RET_ZERO(kmpp_meta_get_ptr_d);

rk_s32 kmpp_meta_set_obj(KmppMeta meta, KmppMetaKey key, KmppObj obj)
STUB_FUNC_RET_ZERO(kmpp_meta_set_obj);
rk_s32 kmpp_meta_get_obj(KmppMeta meta, KmppMetaKey key, KmppObj *obj)
STUB_FUNC_RET_ZERO(kmpp_meta_get_obj);
rk_s32 kmpp_meta_get_obj_d(KmppMeta meta, KmppMetaKey key, KmppObj *obj, KmppObj def)
STUB_FUNC_RET_ZERO(kmpp_meta_get_obj_d);
rk_s32 kmpp_meta_set_shm(KmppMeta meta, KmppMetaKey key, KmppShmPtr *sptr)
STUB_FUNC_RET_ZERO(kmpp_meta_set_shm);
rk_s32 kmpp_meta_get_shm(KmppMeta meta, KmppMetaKey key, KmppShmPtr *sptr)
STUB_FUNC_RET_ZERO(kmpp_meta_get_shm);
rk_s32 kmpp_meta_get_shm_d(KmppMeta meta, KmppMetaKey key, KmppShmPtr *sptr, KmppShmPtr *def)
STUB_FUNC_RET_ZERO(kmpp_meta_get_shm_d);

#endif /*__KMPP_META_STUB_H__*/

/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#include "kmpp_stub.h"

/* always define object common function */
#define KMPP_OBJ_FUNC_DEFINE_STUB(prefix) \
rk_s32 CONCAT_US(prefix, init)(void) \
STUB_FUNC_RET_ZERO(CONCAT_US(prefix, init)); \
rk_s32 CONCAT_US(prefix, deinit)(void) \
STUB_FUNC_RET_ZERO(CONCAT_US(prefix, deinit)); \
rk_s32 CONCAT_US(prefix, size)(void) \
STUB_FUNC_RET_ZERO(CONCAT_US(prefix, size)); \
rk_s32 CONCAT_US(prefix, get)(KMPP_OBJ_INTF_TYPE *p) \
STUB_FUNC_RET_ZERO(CONCAT_US(prefix, get)); \
rk_s32 CONCAT_US(prefix, get_share)(KMPP_OBJ_INTF_TYPE *p, KmppShmGrp grp) \
STUB_FUNC_RET_ZERO(CONCAT_US(prefix, get_share)); \
rk_s32 CONCAT_US(prefix, assign)(KMPP_OBJ_INTF_TYPE *p, void *buf, rk_s32 size) \
STUB_FUNC_RET_ZERO(CONCAT_US(prefix, assign)); \
rk_s32 CONCAT_US(prefix, put)(KMPP_OBJ_INTF_TYPE p) \
STUB_FUNC_RET_ZERO(CONCAT_US(prefix, put)); \
rk_s32 CONCAT_US(prefix, dump)(KMPP_OBJ_INTF_TYPE p, const rk_u8 *caller) \
STUB_FUNC_RET_ZERO(CONCAT_US(prefix, dump));

KMPP_OBJ_FUNC_DEFINE_STUB(KMPP_OBJ_NAME)
#undef KMPP_OBJ_FUNC_DEFINE_STUB

/* entry and hook access funcitons */
#ifdef KMPP_OBJ_ENTRY_TABLE
/* disable all hierarchy macro in header */
#define CFG_DEF_START(...)
#define CFG_DEF_END(...)
#define STRUCT_START(...)
#define STRUCT_END(...)

#define ENTRY_DECLARE(prefix, ftype, type, name, flag, ...) \
    rk_s32 CONCAT_US(prefix, set, __VA_ARGS__)(KMPP_OBJ_INTF_TYPE p, type val) \
    STUB_FUNC_RET_ZERO(CONCAT_US(prefix, set, __VA_ARGS__)); \
    rk_s32 CONCAT_US(prefix, get, __VA_ARGS__)(KMPP_OBJ_INTF_TYPE p, type* val) \
    STUB_FUNC_RET_ZERO(CONCAT_US(prefix, get, __VA_ARGS__)); \
    rk_s32 CONCAT_US(prefix, test, __VA_ARGS__)(KMPP_OBJ_INTF_TYPE p) \
    STUB_FUNC_RET_ZERO(CONCAT_US(prefix, test, __VA_ARGS__));

#define STRCT_DECLARE(prefix, ftype, type, name, flag, ...) \
    rk_s32 CONCAT_US(prefix, set, __VA_ARGS__)(KMPP_OBJ_INTF_TYPE p, type* val) \
    STUB_FUNC_RET_ZERO(CONCAT_US(prefix, set, __VA_ARGS__)); \
    rk_s32 CONCAT_US(prefix, get, __VA_ARGS__)(KMPP_OBJ_INTF_TYPE p, type* val) \
    STUB_FUNC_RET_ZERO(CONCAT_US(prefix, get, __VA_ARGS__)); \
    rk_s32 CONCAT_US(prefix, test, __VA_ARGS__)(KMPP_OBJ_INTF_TYPE p) \
    STUB_FUNC_RET_ZERO(CONCAT_US(prefix, test, __VA_ARGS__));

#define ALIAS_DECLARE(prefix, ftype, type, name, flag, ...)
KMPP_OBJ_ENTRY_TABLE(KMPP_OBJ_NAME, ENTRY_DECLARE, STRCT_DECLARE,
                     ENTRY_DECLARE, STRCT_DECLARE, ALIAS_DECLARE)

#undef ENTRY_DECLARE
#undef STRCT_DECLARE
#undef ALIAS_DECLARE

#undef CFG_DEF_START
#undef CFG_DEF_END
#undef STRUCT_START
#undef STRUCT_END

#endif

#undef KMPP_OBJ_NAME
#undef KMPP_OBJ_INTF_TYPE
#undef KMPP_OBJ_ENTRY_TABLE

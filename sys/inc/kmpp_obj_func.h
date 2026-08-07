/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include "kmpp_obj_macro.h"

#ifndef KMPP_OBJ_NAME
#error "KMPP_OBJ_NAME must be defined on using kmpp_obj_func.h"
#endif

#ifndef KMPP_OBJ_INTF_TYPE
#error "KMPP_OBJ_INTF_TYPE must be defined on using kmpp_obj_func.h"
#endif

/* always define object common function */
#define KMPP_OBJ_FUNC_DEFINE(prefix) \
    rk_s32 CONCAT_US(prefix, objdef, get)(void); \
    rk_s32 CONCAT_US(prefix, objdef, put)(void); \
    rk_s32 CONCAT_US(prefix, size)(void); \
    rk_s32 CONCAT_US(prefix, get)(KMPP_OBJ_INTF_TYPE *p); \
    rk_s32 CONCAT_US(prefix, get_share)(KMPP_OBJ_INTF_TYPE *p, KmppShmGrp grp); \
    rk_s32 CONCAT_US(prefix, assign)(KMPP_OBJ_INTF_TYPE *p, void *buf, rk_s32 size); \
    rk_s32 CONCAT_US(prefix, put)(KMPP_OBJ_INTF_TYPE p); \
    rk_s32 CONCAT_US(prefix, dump)(KMPP_OBJ_INTF_TYPE p, const rk_u8 *caller);

KMPP_OBJ_FUNC_DEFINE(KMPP_OBJ_NAME)
#undef KMPP_OBJ_FUNC_DEFINE

#if !defined(KMPP_OBJ_ACCESS_DISABLE)
/* entry and hook access funcitons */
#ifdef KMPP_OBJ_ENTRY_TABLE
/* disable all hierarchy macro in header */
#define CFG_DEF_START(...)
#define CFG_DEF_END(...)
#define STRUCT_START(...)
#define STRUCT_END(...)

#define ENTRY_DECLARE(prefix, ftype, type, name, flag, ...) \
    rk_s32 CONCAT_US(prefix, set, __VA_ARGS__)(KMPP_OBJ_INTF_TYPE p, type val); \
    rk_s32 CONCAT_US(prefix, get, __VA_ARGS__)(KMPP_OBJ_INTF_TYPE p, type* val); \
    rk_s32 CONCAT_US(prefix, test, __VA_ARGS__)(KMPP_OBJ_INTF_TYPE p);

#define STRCT_DECLARE(prefix, ftype, type, name, flag, ...) \
    rk_s32 CONCAT_US(prefix, set, __VA_ARGS__)(KMPP_OBJ_INTF_TYPE p, type* val); \
    rk_s32 CONCAT_US(prefix, get, __VA_ARGS__)(KMPP_OBJ_INTF_TYPE p, type* val); \
    rk_s32 CONCAT_US(prefix, test, __VA_ARGS__)(KMPP_OBJ_INTF_TYPE p);

#define ALIAS_DECLARE(prefix, ftype, type, name, flag, ...)

KMPP_OBJ_ENTRY_TABLE(KMPP_OBJ_NAME, ENTRY_DECLARE, STRCT_DECLARE,
                     ENTRY_DECLARE, STRCT_DECLARE, ALIAS_DECLARE)

#undef ENTRY_DECLARE
#undef ENTRY_TO_ALIAS
#undef STRCT_DECLARE

#undef CFG_DEF_START
#undef CFG_DEF_END
#undef STRUCT_START
#undef STRUCT_END

#endif /* KMPP_OBJ_ENTRY_TABLE */
#endif /* !KMPP_OBJ_ACCESS_DISABLE */

#undef KMPP_OBJ_NAME
#undef KMPP_OBJ_INTF_TYPE
#undef KMPP_OBJ_ENTRY_TABLE
#undef KMPP_OBJ_ACCESS_DISABLE

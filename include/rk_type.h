/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __RK_TYPE_H__
#define __RK_TYPE_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

typedef unsigned char           RK_U8;
typedef unsigned short          RK_U16;
typedef unsigned int            RK_U32;
typedef unsigned long           RK_ULONG;
typedef RK_ULONG                RK_UL;
typedef unsigned long long int  RK_U64;

typedef signed char             RK_S8;
typedef signed short            RK_S16;
typedef signed int              RK_S32;
typedef signed long             RK_LONG;
typedef RK_LONG                 RK_SL;
typedef signed long long int    RK_S64;

typedef float                   RK_FLOAT;
typedef double                  RK_DOUBLE;

typedef unsigned long           RK_SIZE_T;
typedef unsigned int            RK_HANDLE;

typedef unsigned char           rk_u8;
typedef unsigned short          rk_u16;
typedef unsigned int            rk_u32;
typedef unsigned long           rk_ulong;
typedef rk_ulong                rk_ul;
typedef unsigned long long int  rk_u64;

typedef signed char             rk_s8;
typedef signed short            rk_s16;
typedef signed int              rk_s32;
typedef signed long             rk_long;
typedef rk_long                 rk_sl;
typedef signed long long int    rk_s64;

typedef float                   rk_float;
typedef double                  rk_double;

typedef unsigned long           rk_size_t;
typedef unsigned int            rk_handle;

typedef enum {
    RK_FALSE = 0,
    RK_TRUE  = 1,
} RK_BOOL;

typedef enum {
    rk_false = 0,
    rk_true  = 1,
} rk_bool;

#ifndef NULL
#define NULL                    0L
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned long           size_t;
#endif

#ifndef _SSIZE_T
#define _SSIZE_T
typedef long                    ssize_t;
#endif

#define RK_RET                  RK_S32
#define RK_NULL                 0L
#define RK_OK                   0
#define RK_NOK                  (-1)
#define RK_SUCCESS              0
#define RK_FAILURE              (-1)

#define RK_VOID                 void
#define RK_UNUSED(x)            ((void)((x)))

#define rk_ret                  rk_s32
#define rk_null                 0L
#define rk_ok                   0
#define rk_nok                  (-1)
#define rk_success              0
#define rk_failure              (-1)

#define rk_void                 void
#define rk_unused(x)            ((void)((x)))

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#include "rk_defs.h"
#include "rk_err_def.h"

#endif /* __RK_TYPE_H__ */

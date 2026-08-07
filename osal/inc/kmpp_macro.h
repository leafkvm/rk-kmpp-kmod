/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_MACRO_H__
#define __KMPP_MACRO_H__

#define KMPP_ABS(x)             ((x) < (0) ? -(x) : (x))

#define KMPP_MAX(a, b)          ((a) > (b) ? (a) : (b))
#define KMPP_MAX3(a, b, c)      KMPP_MAX(KMPP_MAX(a,b),c)
#define KMPP_MAX4(a, b, c, d)   KMPP_MAX((a), KMPP_MAX3((b), (c), (d)))

#define KMPP_MIN(a,b)           ((a) > (b) ? (b) : (a))
#define KMPP_MIN3(a,b,c)        KMPP_MIN(KMPP_MIN(a,b),c)
#define KMPP_MIN4(a, b, c, d)   KMPP_MIN((a), KMPP_MIN3((b), (c), (d)))

#define KMPP_DIV(a, b)          ((b) ? (a) / (b) : (a))

#define KMPP_CLIP3(l, h, v)     ((v) < (l) ? (l) : ((v) > (h) ? (h) : (v)))
#define KMPP_SIGN(a)            ((a) < (0) ? (-1) : (1))
#define KMPP_DIV_SIGN(a, b)     (((a) + (KMPP_SIGN(a) * (b)) / 2) / (b))

#define KMPP_SWAP(type, a, b)   do {type SWAP_tmp = b; b = a; a = SWAP_tmp;} while(0)
#define KMPP_ARRAY_ELEMS(a)     (sizeof(a) / sizeof((a)[0]))
#define KMPP_ALIGN(x, a)        (((x)+(a)-1)&~((a)-1))
#define KMPP_ALIGN_DOWN(x, a)   ((x)&~((a)-1))
#define KMPP_ALIGN_GEN(x, a)    (((x)+(a)-1)/(a)*(a))
#define KMPP_VSWAP(a, b)        { a ^= b; b ^= a; a ^= b; }

#define ARG_T(t)                t
#define ARG_N(a,b,c,d,N,...)    N
#define ARG_N_HELPER(...)       ARG_T(ARG_N(__VA_ARGS__))
#define COUNT_ARG(...)          ARG_N_HELPER(__VA_ARGS__,4,3,2,1,0)

#define KMPP_OFFSET_OF(TYPE, MEMBER)  ((size_t) &((TYPE *)0)->MEMBER)

#define KMPP_CONTAINER_OF(ptr, type, member) \
    ({ \
        const typeof(((type *)0)->member) *__mptr = (ptr); \
        (type *)((char *)__mptr - KMPP_OFFSET_OF(type, member)); \
    })

#endif /* __KMPP_MACRO_H__ */

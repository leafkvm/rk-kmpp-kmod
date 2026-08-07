/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_MEM_POOL_H__
#define __KMPP_MEM_POOL_H__

#include "rk_type.h"

typedef void* KmppMemPool;

#define kmpp_mem_get_pool_f(name, size, max_cnt, flag) \
    kmpp_mem_get_pool(name, size, max_cnt, flag, __FUNCTION__)

#define kmpp_mem_put_pool_f(pool)       kmpp_mem_put_pool(pool, __FUNCTION__)
#define kmpp_mem_pool_get_f(pool)       kmpp_mem_pool_get(pool, __FUNCTION__)
#define kmpp_mem_pool_put_f(pool, p)    kmpp_mem_pool_put(pool, p, __FUNCTION__)
#define kmpp_mem_pool_dump_f(pool)      kmpp_mem_pool_dump(pool, __FUNCTION__)
#define kmpp_mem_pool_dump_all_f()      kmpp_mem_pool_dump_all(__FUNCTION__)

KmppMemPool kmpp_mem_get_pool(const rk_u8 *name, rk_s32 size, rk_u32 max_cnt, rk_u32 flag, const rk_u8 *caller);
void kmpp_mem_put_pool(KmppMemPool pool, const rk_u8 *caller);
void *kmpp_mem_pool_get(KmppMemPool pool, const rk_u8 *caller);
void kmpp_mem_pool_put(KmppMemPool pool, void *p, const rk_u8 *caller);
void kmpp_mem_pool_dump(KmppMemPool pool, const rk_u8 *caller);
void kmpp_mem_pool_dump_all(const rk_u8 *caller);

#endif /*__KMPP_MEM_POOL_H__*/

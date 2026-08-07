/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_MEM_H__
#define __KMPP_MEM_H__

#include "rk_type.h"

#define KmppGfpNormal 0
#define KmppGfpAtomic 1

#define kmpp_malloc(size) \
    osal_kmalloc_f(__FUNCTION__, size, KmppGfpNormal)
#define kmpp_calloc(size) \
    osal_kcalloc_f(__FUNCTION__, size, KmppGfpNormal)
#define kmpp_realloc(ptr, oldsize, newsize) \
    osal_krealloc_f(__FUNCTION__, ptr, oldsize, newsize, KmppGfpNormal)

#define kmpp_malloc_atomic(size) \
    osal_kmalloc_f(__FUNCTION__, size, KmppGfpAtomic)
#define kmpp_calloc_atomic(size) \
    osal_kcalloc_f(__FUNCTION__, size, KmppGfpAtomic)
#define kmpp_realloc_atomic(ptr, oldsize, newsize) \
    osal_krealloc_f(__FUNCTION__, ptr, oldsize, newsize, KmppGfpAtomic)

#define kmpp_free(ptr) \
    do { if(ptr) osal_kfree_f(__FUNCTION__, ptr); ptr = NULL; } while (0)

/* memory for userspace */
#define kmpp_malloc_share(size) \
    osal_malloc_share_f(__FUNCTION__, size)
#define kmpp_free_share(ptr) \
   do { if(ptr) osal_kfree_share_f(__FUNCTION__, ptr); ptr = NULL; } while (0)

void *osal_kmalloc_f(const char *func, rk_u32 size, rk_u32 gfp);
void *osal_kcalloc_f(const char *func, rk_u32 size, rk_u32 gfp);
void *osal_krealloc_f(const char *func, void *ptr, rk_u32 oldsize, rk_u32 newsize, rk_u32 gfp);
void osal_kfree_f(const char *func, void *ptr);

/* share alloc do not set zero and do not have debug segment */
void *osal_malloc_share_f(const char *func, rk_u32 size);
void osal_kfree_share_f(const char *func, void *ptr);

/* callback for delay register on kmpp_env_init */
void kmpp_mem_env_init(void);

#endif /* __KMPP_MEM_H__ */

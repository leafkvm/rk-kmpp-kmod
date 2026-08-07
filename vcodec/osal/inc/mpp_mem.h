/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#ifndef __MPP_MEM_H__
#define __MPP_MEM_H__

#include "rk_err_def.h"
#include "kmpp_mem.h"

#define mpp_malloc(type, count)  \
    (type*)kmpp_malloc(sizeof(type) * (count))

#define mpp_malloc_size(type, size)  \
    (type*)kmpp_malloc(size)

#define mpp_calloc(type, count)  \
    (type*)kmpp_calloc(sizeof(type) * (count))

#define mpp_calloc_size(type, size)  \
    (type*)kmpp_calloc(size)

#define mpp_free(ptr)   kmpp_free(ptr)
#define MPP_FREE(ptr)   kmpp_free(ptr)

#endif /*__MPP_MEM_H__*/

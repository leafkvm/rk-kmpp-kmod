/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef KMPP_TEST_H
#define KMPP_TEST_H

#include <linux/module.h>
#include <linux/kernel.h>

#include "kmpp_osal.h"
#include "kmpp_sys.h"

#define TEST_DISABLE        1
#define TEST_DETAIL         2
#define TEST_DUMP           4

#define is_flag_set(val, flag)   (val && (*val & flag))

#define test_detail(fmt, ...) \
    do { \
        if (is_flag_set(flag, TEST_DETAIL)) \
            kmpp_logi(fmt, ##__VA_ARGS__); \
    } while (0)

typedef struct KmppTest_t {
    const rk_u8     *name;
    void            *ctx;
    rk_u32          *flag;
    rk_s32          (*func)(const rk_u8 *name, rk_u32 *flag, void *ctx);
} KmppTest;

#endif /* KMPP_TEST_H */

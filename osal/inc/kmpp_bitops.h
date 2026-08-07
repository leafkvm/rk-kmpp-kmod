/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_BITOPS_H__
#define __KMPP_BITOPS_H__

#include "rk_type.h"

void kmpp_set_bit(rk_s32 nr, rk_ul *addr);
void kmpp_clear_bit(rk_s32 nr, rk_ul *addr);
rk_s32 kmpp_test_bit(rk_s32 nr, rk_ul *addr);
rk_s32 kmpp_test_and_set_bit(rk_s32 nr, rk_ul *addr);
rk_s32 kmpp_test_and_clear_bit(rk_s32 nr, rk_ul *addr);
rk_s32 kmpp_test_and_change_bit(rk_s32 nr, rk_ul *addr);

rk_s32 kmpp_find_first_bit(rk_ul *addr, rk_s32 size);
rk_s32 kmpp_find_next_bit(rk_ul *addr, rk_s32 size, rk_s32 start);
rk_s32 kmpp_find_last_bit(rk_ul *addr, rk_s32 size);
rk_s32 kmpp_find_first_zero_bit(rk_ul *addr, rk_s32 size);
rk_s32 kmpp_find_next_zero_bit(rk_ul *addr, rk_s32 size, rk_s32 start);

#ifdef __ANDROID__
#undef __always_inline
#define __always_inline inline
#else
#ifndef __always_inline
#define __always_inline inline __attribute__((always_inline))
#endif
#endif

static __always_inline rk_u32 kmpp_ffs32(rk_u32 word)
{
    rk_u32 num = 0;

    if ((word & 0xffff) == 0) {
        num += 16;
        word >>= 16;
    }
    if ((word & 0xff) == 0) {
        num += 8;
        word >>= 8;
    }
    if ((word & 0xf) == 0) {
        num += 4;
        word >>= 4;
    }
    if ((word & 0x3) == 0) {
        num += 2;
        word >>= 2;
    }
    if ((word & 0x1) == 0)
        num += 1;

    return num;
}

static __always_inline rk_u32 kmpp_ffs64(rk_u64 word)
{
    rk_u32 num = 0;

    if ((word & 0xffffffff) == 0) {
        num += 32;
        word >>= 32;
    }
    if ((word & 0xffff) == 0) {
        num += 16;
        word >>= 16;
    }
    if ((word & 0xff) == 0) {
        num += 8;
        word >>= 8;
    }
    if ((word & 0xf) == 0) {
        num += 4;
        word >>= 4;
    }
    if ((word & 0x3) == 0) {
        num += 2;
        word >>= 2;
    }
    if ((word & 0x1) == 0)
        num += 1;

    return num;
}

#endif /* __KMPP_BITOPS_H__ */

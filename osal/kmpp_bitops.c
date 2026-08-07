/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/bitmap.h>
#include <linux/kernel.h>

#include "kmpp_bitops.h"

void kmpp_set_bit(rk_s32 nr, rk_ul *addr)
{
    set_bit(nr, (unsigned long *)addr);
}
EXPORT_SYMBOL(kmpp_set_bit);

void kmpp_clear_bit(rk_s32 nr, rk_ul *addr)
{
    clear_bit(nr, (unsigned long *)addr);
}
EXPORT_SYMBOL(kmpp_clear_bit);

rk_s32 kmpp_test_bit(rk_s32 nr, rk_ul *addr)
{
    return test_bit(nr, (unsigned long *)addr);
}
EXPORT_SYMBOL(kmpp_test_bit);

rk_s32 kmpp_test_and_set_bit(rk_s32 nr, rk_ul *addr)
{
    return test_and_set_bit(nr, (unsigned long *)addr);
}
EXPORT_SYMBOL(kmpp_test_and_set_bit);

rk_s32 kmpp_test_and_clear_bit(rk_s32 nr, rk_ul *addr)
{
    return test_and_clear_bit(nr, (unsigned long *)addr);
}
EXPORT_SYMBOL(kmpp_test_and_clear_bit);

rk_s32 kmpp_test_and_change_bit(rk_s32 nr, rk_ul *addr)
{
    return test_and_change_bit(nr, (unsigned long *)addr);
}
EXPORT_SYMBOL(kmpp_test_and_change_bit);

rk_s32 kmpp_find_first_bit(rk_ul *addr, rk_s32 size)
{
    return find_first_bit((unsigned long *)addr, size);
}
EXPORT_SYMBOL(kmpp_find_first_bit);

rk_s32 kmpp_find_next_bit(rk_ul *addr, rk_s32 size, rk_s32 start)
{
    return find_next_bit((unsigned long *)addr, size, start);
}
EXPORT_SYMBOL(kmpp_find_next_bit);

rk_s32 kmpp_find_last_bit(rk_ul *addr, rk_s32 size)
{
    return find_last_bit((unsigned long *)addr, size);
}
EXPORT_SYMBOL(kmpp_find_last_bit);

rk_s32 kmpp_find_first_zero_bit(rk_ul *addr, rk_s32 size)
{
    return find_first_zero_bit((unsigned long *)addr, size);
}
EXPORT_SYMBOL(kmpp_find_first_zero_bit);

rk_s32 kmpp_find_next_zero_bit(rk_ul *addr, rk_s32 size, rk_s32 start)
{
    return find_next_zero_bit((unsigned long *)addr, size, start);
}
EXPORT_SYMBOL(kmpp_find_next_zero_bit);

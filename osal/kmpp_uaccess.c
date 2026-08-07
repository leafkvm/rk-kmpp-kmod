/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_uaccess"

#include <linux/version.h>
#include <linux/uaccess.h>

#include "kmpp_uaccess.h"

static rk_s32 kmpp_strncpy(char *dst, const char *src, rk_ul size)
{
    rk_s32 len = strlen(src);

    if (len >= size) {
        memcpy(dst, src, size - 1);
        len = size - 1;
    } else {
        memcpy(dst, src, len);
        dst[len] = '\0';
    }

    return len;
}

rk_ul kmpp_copy_from_user(void *dst, const void *src, rk_ul size)
{
    if (access_ok(src, size))
        return copy_from_user(dst, src, size);

    memcpy(dst, src, size);
    return 0;
}
EXPORT_SYMBOL(kmpp_copy_from_user);

rk_ul kmpp_copy_to_user(void *dst, const void *src, rk_ul size)
{
    if (access_ok(dst, size))
        return copy_to_user(dst, src, size);

    memcpy(dst, src, size);
    return 0;
}
EXPORT_SYMBOL(kmpp_copy_to_user);

rk_ul kmpp_strncpy_from_user(void *dst, void *src, rk_ul size)
{
    if (access_ok(src, size))
        return strncpy_from_user(dst, src, size);

    return kmpp_strncpy(dst, src, size);
}
EXPORT_SYMBOL(kmpp_strncpy_from_user);

rk_ul kmpp_strnlen_user(void *str, rk_ul size)
{
    if (access_ok(str, size))
        return strnlen_user(str, size);

    return strnlen(str, size);
}
EXPORT_SYMBOL(kmpp_strnlen_user);

rk_ul kmpp_strncpy_sptr(void *dst, KmppShmPtr *src, rk_ul size)
{
    if (!dst || !src || (!src->kptr && !src->uptr) || !size)
        return 0;

    if (src->kptr)
        return kmpp_strncpy(dst, src->kptr, size);

    if (src->uptr)
        return kmpp_strncpy_from_user(dst, src->uptr, size);

    return 0;
}
EXPORT_SYMBOL(kmpp_strncpy_sptr);

rk_ul kmpp_strnlen_sptr(KmppShmPtr *src, rk_ul size)
{
    if (!src || (!src->kptr && !src->uptr) || !size)
        return 0;

    if (src->kptr)
        return strnlen(src->kptr, size);

    if (src->uptr)
        return kmpp_strnlen_user(src->uptr, size);

    return 0;
}
EXPORT_SYMBOL(kmpp_strnlen_sptr);

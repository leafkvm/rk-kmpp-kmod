/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/version.h>

#include "kmpp_string.h"

rk_u8 *kmpp_strcpy(rk_u8 *dest, const rk_u8 *src)
{
    return strcpy(dest, src);
}
EXPORT_SYMBOL(kmpp_strcpy);

rk_u8 *kmpp_strncpy(rk_u8 *dest, const rk_u8 *src, rk_s32 count)
{
    return strncpy(dest, src, count);
}
EXPORT_SYMBOL(kmpp_strncpy);

rk_s32 kmpp_strlcpy(rk_u8 *dest, const rk_u8 *src, rk_s32 size)
{
    return strlcpy(dest, src, size);
}
EXPORT_SYMBOL(kmpp_strlcpy);

rk_u8 *kmpp_strcat(rk_u8 *dest, const rk_u8 *src)
{
    return strcat(dest, src);
}
EXPORT_SYMBOL(kmpp_strcat);

rk_u8 *kmpp_strncat(rk_u8 *dest, const rk_u8 *src, rk_s32 count)
{
    return strncat(dest, src, count);
}
EXPORT_SYMBOL(kmpp_strncat);

rk_s32 kmpp_strlcat(rk_u8 *dest, const rk_u8 *src, rk_s32 count)
{
    return strlcat(dest, src, count);
}
EXPORT_SYMBOL(kmpp_strlcat);

rk_s32 kmpp_strcmp(const rk_u8 *cs, const rk_u8 *ct)
{
    return strcmp(cs, ct);
}
EXPORT_SYMBOL(kmpp_strcmp);

rk_s32 kmpp_strncmp(const rk_u8 *cs, const rk_u8 *ct, rk_s32 count)
{
    return strncmp(cs, ct, count);
}
EXPORT_SYMBOL(kmpp_strncmp);

rk_s32 kmpp_strnicmp(const rk_u8 *s1, const rk_u8 *s2, rk_s32 len)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
    return strnicmp(s1, s2, len);
#endif
    return 0;
}
EXPORT_SYMBOL(kmpp_strnicmp);

rk_s32 kmpp_strcasecmp(const rk_u8 *s1, const rk_u8 *s2)
{
    return strcasecmp(s1, s2);
}
EXPORT_SYMBOL(kmpp_strcasecmp);

rk_s32 kmpp_strncasecmp(const rk_u8 *s1, const rk_u8 *s2, rk_s32 n)
{
    return strncasecmp(s1, s2, n);
}
EXPORT_SYMBOL(kmpp_strncasecmp);

rk_u8 *kmpp_strchr(const rk_u8 *s, rk_s32 c)
{
    return strchr(s, c);
}
EXPORT_SYMBOL(kmpp_strchr);

rk_u8 *kmpp_strnchr(const rk_u8 *s, rk_s32 count, rk_s32 c)
{
    return strnchr(s, count, c);
}
EXPORT_SYMBOL(kmpp_strnchr);

rk_u8 *kmpp_strrchr(const rk_u8 *s, rk_s32 c)
{
    return strrchr(s, c);
}
EXPORT_SYMBOL(kmpp_strrchr);

rk_u8 *kmpp_strstr(const rk_u8 *s1, const rk_u8 *s2)
{
    return strstr(s1, s2);
}
EXPORT_SYMBOL(kmpp_strstr);

rk_u8 *kmpp_strnstr(const rk_u8 *s1, const rk_u8 *s2, rk_s32 len)
{
    return strnstr(s1, s2, len);
}
EXPORT_SYMBOL(kmpp_strnstr);

rk_s32 kmpp_strlen(const rk_u8 *s)
{
    return strlen(s);
}
EXPORT_SYMBOL(kmpp_strlen);

rk_s32 kmpp_strnlen(const rk_u8 *s, rk_s32 count)
{
    return strnlen(s, count);
}
EXPORT_SYMBOL(kmpp_strnlen);

rk_u8 *kmpp_strpbrk(const rk_u8 *cs, const rk_u8 *ct)
{
    return strpbrk(cs, ct);
}
EXPORT_SYMBOL(kmpp_strpbrk);

rk_u8 *kmpp_strsep(rk_u8 **s, const rk_u8 *ct)
{
    return (rk_u8 *)strsep((char **)s, (const char *)ct);
}
EXPORT_SYMBOL(kmpp_strsep);

rk_s32 kmpp_strspn(const rk_u8 *s, const rk_u8 *accept)
{
    return strspn(s, accept);
}
EXPORT_SYMBOL(kmpp_strspn);

rk_s32 kmpp_strcspn(const rk_u8 *s, const rk_u8 *reject)
{
    return strcspn(s, reject);
}
EXPORT_SYMBOL(kmpp_strcspn);

rk_void *kmpp_memset(rk_void *str, rk_s32 c, rk_s32 count)
{
    return memset(str, c, count);
}
EXPORT_SYMBOL(kmpp_memset);

rk_void *kmpp_memcpy(rk_void *dest, const rk_void *src, rk_s32 count)
{
    return memcpy(dest, src, count);
}
EXPORT_SYMBOL(kmpp_memcpy);

rk_void *kmpp_memmove(rk_void *dest, const rk_void *src, rk_s32 count)
{
    return memmove(dest, src, count);
}
EXPORT_SYMBOL(kmpp_memmove);

rk_void *kmpp_memscan(rk_void *addr, rk_s32 c, rk_s32 size)
{
    return memscan(addr, c, size);
}
EXPORT_SYMBOL(kmpp_memscan);

rk_s32 kmpp_memcmp(const rk_void *cs, const rk_void *ct, rk_s32 count)
{
    return memcmp(cs, ct, count);
}
EXPORT_SYMBOL(kmpp_memcmp);

rk_void *kmpp_memchr(const rk_void *s, RK_S32 c, RK_S32 n)
{
    return memchr(s, c, n);
}
EXPORT_SYMBOL(kmpp_memchr);

rk_void *kmpp_memchr_inv(const rk_void *start, RK_S32 c, RK_S32 bytes)
{
    return memchr_inv(start, c, bytes);
}
EXPORT_SYMBOL(kmpp_memchr_inv);

rk_u64 kmpp_strtoull(const rk_u8 *cp, rk_u8 **endp, rk_u32 base)
{
    return simple_strtoull((const char *)cp, (char **)endp, base);
}
EXPORT_SYMBOL(kmpp_strtoull);

rk_ulong kmpp_strtoul(const rk_u8 *cp, rk_u8 **endp, rk_u32 base)
{
    return simple_strtoul((const char *)cp, (char **)endp, base);
}
EXPORT_SYMBOL(kmpp_strtoul);

rk_long kmpp_strtol(const rk_u8 *cp, rk_u8 **endp, rk_u32 base)
{
    return simple_strtol((const char *)cp, (char **)endp, base);
}
EXPORT_SYMBOL(kmpp_strtol);

rk_s64 kmpp_strtoll(const rk_u8 *cp, rk_u8 **endp, rk_u32 base)
{
    return simple_strtoll((const char *)cp, (char **)endp, base);
}
EXPORT_SYMBOL(kmpp_strtoll);

rk_s32 kmpp_snprintf(rk_u8 *buf, rk_s32 size, const rk_u8 *fmt, ...)
{
    va_list args;
    rk_s32 i;

    va_start(args, fmt);
    i = vsnprintf(buf, size, fmt, args);
    va_end(args);

    return i;
}
EXPORT_SYMBOL(kmpp_snprintf);

rk_s32 kmpp_scnprintf(rk_u8 *buf, rk_s32 size, const rk_u8 *fmt, ...)
{
    va_list args;
    rk_s32 i;

    va_start(args, fmt);
    i = vscnprintf(buf, size, fmt, args);
    va_end(args);

    return i;
}
EXPORT_SYMBOL(kmpp_scnprintf);

rk_s32 kmpp_sprintf(rk_u8 *buf, const rk_u8 *fmt, ...)
{
    va_list args;
    rk_s32 i;

    va_start(args, fmt);
    i = vsnprintf(buf, INT_MAX, fmt, args);
    va_end(args);

    return i;
}
EXPORT_SYMBOL(kmpp_sprintf);

rk_s32 kmpp_sscanf(const rk_u8 *buf, const rk_u8 *fmt, ...)
{
    va_list args;
    rk_s32 i;

    va_start(args, fmt);
    i = vsscanf(buf, fmt, args);
    va_end(args);

    return i;
}
EXPORT_SYMBOL(kmpp_sscanf);

rk_s32 kmpp_vsnprintf(rk_u8 *str, rk_s32 size, const rk_u8 *fmt, va_list args)
{
    return vsnprintf(str, size, fmt, args);
}
EXPORT_SYMBOL(kmpp_vsnprintf);

/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_STRING_H__
#define __KMPP_STRING_H__

#include "rk_type.h"

rk_u8 *kmpp_strcpy(rk_u8 *s1, const rk_u8 *s2);
rk_u8 *kmpp_strncpy(rk_u8 *s1, const rk_u8 *s2, rk_s32 size);
rk_s32 kmpp_strlcpy(rk_u8 *s1, const rk_u8 *s2, rk_s32 size);
rk_u8 *kmpp_strcat(rk_u8 *s1, const rk_u8 *s2);
rk_u8 *kmpp_strncat(rk_u8 *s1, const rk_u8 *s2, rk_s32 size);
rk_s32 kmpp_strlcat(rk_u8 *s1, const rk_u8 *s2, rk_s32 size);
rk_s32 kmpp_strcmp(const rk_u8 *s1, const rk_u8 *s2);
rk_s32 kmpp_strncmp(const rk_u8 *s1, const rk_u8 *s2, rk_s32 size);
rk_s32 kmpp_strnicmp(const rk_u8 *s1, const rk_u8 *s2, rk_s32 size);
rk_s32 kmpp_strcasecmp(const rk_u8 *s1, const rk_u8 *s2);
rk_s32 kmpp_strncasecmp(const rk_u8 *s1, const rk_u8 *s2, rk_s32 n);
rk_u8 *kmpp_strchr(const rk_u8 *s, rk_s32 n);
rk_u8 *kmpp_strnchr(const rk_u8 *s, rk_s32 count, rk_s32 c);
rk_u8 *kmpp_strrchr(const rk_u8 *s, rk_s32 c);
rk_u8 *kmpp_strstr(const rk_u8 *s1, const rk_u8 *s2);
rk_u8 *kmpp_strnstr(const rk_u8 *s1, const rk_u8 *s2, rk_s32 n);
rk_s32 kmpp_strlen(const rk_u8 *s);
rk_s32 kmpp_strnlen(const rk_u8 *s, rk_s32 size);
rk_u8 *kmpp_strpbrk(const rk_u8 *s1, const rk_u8 *s2);
rk_u8 *kmpp_strsep(rk_u8 **s, const rk_u8 *ct);
rk_s32 kmpp_strspn(const rk_u8 *s, const rk_u8 *accept);
rk_s32 kmpp_strcspn(const rk_u8 *s, const rk_u8 *reject);
rk_void *kmpp_memset(rk_void *str, rk_s32 c, rk_s32 count);
rk_void *kmpp_memcpy(rk_void *s1, const rk_void *s2, rk_s32 count);
rk_void *kmpp_memmove(rk_void *s1, const rk_void *s2, rk_s32 count);
rk_void *kmpp_memscan(rk_void *addr, rk_s32 c, rk_s32 size);
rk_s32 kmpp_memcmp(const rk_void *cs, const rk_void *ct, rk_s32 count);
rk_void *kmpp_memchr(const rk_void *s, rk_s32 c, rk_s32 n);
rk_void *kmpp_memchr_inv(const rk_void *s, rk_s32 c, rk_s32 n);

rk_u64 kmpp_strtoull(const rk_u8 *cp, rk_u8 **endp, rk_u32 base);
rk_ulong kmpp_strtoul(const rk_u8 *cp, rk_u8 **endp, rk_u32 base);
rk_long kmpp_strtol(const rk_u8 *cp, rk_u8 **endp, rk_u32 base);
rk_s64 kmpp_strtoll(const rk_u8 *cp, rk_u8 **endp, rk_u32 base);
rk_s32 kmpp_snprintf(rk_u8 *buf, rk_s32 size, const rk_u8 *fmt, ...);
rk_s32 kmpp_scnprintf(rk_u8 *buf, rk_s32 size, const rk_u8 *fmt, ...);
rk_s32 kmpp_sprintf(rk_u8 *buf, const rk_u8 *fmt, ...);
rk_s32 kmpp_sscanf(const rk_u8 *buf, const rk_u8 *fmt, ...);

#define STR_BUF_INIT(len) \
    rk_u8 __str_buf[len]; \
    rk_s32 __str_len = 0; \
    rk_s32 __str_size = len - 1;

#define STR_BUF_INIT_ZERO(len) \
    rk_u8 __str_buf[len] = {0}; \
    rk_s32 __str_len = 0; \
    rk_s32 __str_size = len - 1;

#define STR_BUF_INIT_PTR(buf, len) \
    rk_u8 *__str_buf = buf; \
    rk_s32 __str_len = 0; \
    rk_s32 __str_size = len - 1;

#define STR_BUF_BUSE() \
    ({ __str_buf; })

#define STR_BUF_VAL(pos) \
    ({ __str_buf[pos]; })

#define STR_BUF_POS() \
    ({ __str_len; })

/* add more data but not update string buffer end position */
#define STR_BUF_ADD(fmt, ...) \
    ({ \
        kmpp_snprintf(&__str_buf[__str_len], \
                      __str_size - __str_len, \
                      fmt, ##__VA_ARGS__); \
    })

/* add more data and update position */
#define STR_BUF_APPEND(fmt, ...) \
    ({ \
        rk_s32 __len; \
        __len = kmpp_snprintf(&__str_buf[__str_len], \
                              __str_size - __str_len, \
                              fmt, ##__VA_ARGS__); \
        __str_len += __len; \
        __len; \
    })

/* add more data with condition and update position */
#define STR_BUF_APPEND_C(cond, fmt, ...) \
    ({ \
        rk_s32 __len = 0; \
        if (cond) { \
            __len = kmpp_snprintf(&__str_buf[__str_len], \
                                  __str_size - __str_len, \
                                  fmt, ##__VA_ARGS__); \
            __str_len += __len; \
        } \
        __len; \
    })

#define STR_BUF_SHOW(func) \
    do { \
        func("%s\n", __str_buf); \
    } while (0)

#define STR_BUF_SHOW_C(cond, func) \
    do { \
        if (cond) \
            func("%s\n", __str_buf); \
    } while (0)

#endif /* __KMPP_STRING_H__ */
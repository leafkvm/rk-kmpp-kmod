/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_buffer_test"

#include "kmpp_test.h"

#include "kmpp_buffer.h"

rk_u32 kmpp_buffer_flag;
module_param(kmpp_buffer_flag, uint, 0644);
MODULE_PARM_DESC(kmpp_buffer_flag, "kmpp buffer test flag");

static const rk_u8 *test_stages[] = {
    "get buf grp",
    "get buf grp cfg",
    "set buf grp cfg",
    "setup buf grp",
    "get buffer",
    "get buffer cfg",
    "set buffer cfg",
    "setup buffer",
    "get buffer_kptr",
    "wrap buffer access",
    "inc buffer ref",
    "put buffer"
};

static rk_s32 kmpp_buffer_test(const rk_u8 *name, rk_u32 *flag, void *ctx)
{
    KmppShmPtr sptr;
    KmppBufGrp grp = NULL;
    KmppBufGrpCfg grp_cfg = NULL;
    KmppBuffer buf = NULL;
    KmppBufCfg buf_cfg = NULL;
    void *kptr;
    const rk_u8 *str;
    rk_s32 size = PAGE_SIZE;
    rk_s32 len = 128;
    rk_s32 ret = rk_nok;
    rk_s32 str_step = 0;
    rk_s32 str_max;
    rk_s32 i;

    str_max = 0;
    for (i = 0; i < KMPP_ARRAY_ELEMS(test_stages); i++) {
        rk_s32 tmp = kmpp_strlen(test_stages[i]);

        if (tmp > str_max)
            str_max = tmp;
    }

    str = test_stages[str_step++];

    ret = kmpp_buf_grp_get(&grp);
    if (ret) {
        kmpp_loge("%-*s failed\n", str_max, str);
        goto done;
    }

    test_detail("%-*s - success\n", str_max, str);
    str = test_stages[str_step++];

    grp_cfg = kmpp_buf_grp_get_cfg_k(grp);
    if (!grp_cfg) {
        kmpp_loge("%-*s failed\n", str_max, str);
        ret = rk_nok;
        goto done;
    }

    test_detail("%-*s - success\n", str_max, str);
    str = test_stages[str_step++];

    sptr.kptr = "rk dma heap";
    sptr.uaddr = 0;
    ret |= kmpp_buf_grp_cfg_set_allocator(grp_cfg, &sptr);
    sptr.kptr = "test";
    sptr.uaddr = 0;
    ret |= kmpp_buf_grp_cfg_set_name(grp_cfg, &sptr);
    ret |= kmpp_buf_grp_cfg_set_count(grp_cfg, 16);
    ret |= kmpp_buf_grp_cfg_set_size(grp_cfg, size);
    if (ret) {
        kmpp_loge("%-*s failed\n", str_max, str);
        goto done;
    }

    test_detail("%-*s - success\n", str_max, str);
    str = test_stages[str_step++];

    ret = kmpp_buf_grp_setup_f(grp);
    if (ret) {
        kmpp_loge("%-*s failed\n", str_max, str);
        goto done;
    }

    test_detail("%-*s - success\n", str_max, str);
    str = test_stages[str_step++];

    if (is_flag_set(flag, TEST_DUMP))
        kmpp_buf_grp_dump(grp, __FUNCTION__);

    ret = kmpp_buffer_get(&buf);
    if (ret) {
        kmpp_loge("%-*s failed\n", str_max, str);
        goto done;
    }

    test_detail("%-*s - success\n", str_max, str);
    str = test_stages[str_step++];

    buf_cfg = kmpp_buffer_get_cfg_k(buf);
    if (!buf_cfg) {
        kmpp_loge("%-*s failed\n", str_max, str);
        ret = rk_nok;
        goto done;
    }

    test_detail("%-*s - success\n", str_max, str);
    str = test_stages[str_step++];

    sptr.kptr = grp;
    sptr.uaddr = 0;
    ret |= kmpp_buf_cfg_set_group(buf_cfg, &sptr);
    ret |= kmpp_buf_cfg_set_size(buf_cfg, size);
    ret |= kmpp_buf_cfg_set_offset(buf_cfg, 1024);
    ret |= kmpp_buf_cfg_set_flag(buf_cfg, KMPP_DMABUF_FLAGS_DUP_MAP | KMPP_DMABUF_FLAGS_DUP_IOVA);
    ret |= kmpp_buf_cfg_set_index(buf_cfg, 1);
    if (ret) {
        kmpp_loge("%-*s failed\n", str_max, str);
        goto done;
    }

    test_detail("%-*s - success\n", str_max, str);
    str = test_stages[str_step++];

    ret = kmpp_buffer_setup_f(buf, NULL);
    if (ret) {
        kmpp_loge("%-*s failed\n", str_max, str);
        goto done;
    }

    test_detail("%-*s - success\n", str_max, str);
    str = test_stages[str_step++];

    kptr = kmpp_buffer_get_kptr(buf);
    if (!kptr) {
        kmpp_loge("%-*s failed\n", str_max, str);
        ret = rk_nok;
        goto done;
    }

    test_detail("%-*s - success\n", str_max, str);
    str = test_stages[str_step++];

    {
        rk_u8 *p = kptr;
        rk_u8 mark0 = 0xff;
        rk_u8 mark1 = 0x80;
        rk_s32 has_err = 0;

        kmpp_memset(p, 0xff, len);
        kmpp_memset(p + size + len, 0x80, len);

        for (i = 0; i < len; i++) {
            if (p[i + size] != mark0) {
                kmpp_loge("mark 0 warp failed\n");
                has_err = 1;
                break;
            }
        }

        for (i = len; i < len * 2; i++) {
            if (p[i] != mark1) {
                kmpp_loge("mark 1 warp failed\n");
                has_err = 1;
                break;
            }
        }

        if (has_err) {
            kmpp_loge("%-*s failed\n", str_max, str);
            ret = rk_nok;
            goto done;
        }

        test_detail("%-*s - success\n", str_max, str);
        str = test_stages[str_step++];
    }

    ret = kmpp_buffer_inc_ref_f(buf);
    if (ret) {
        kmpp_loge("%-*s failed\n", str_max, str);
        goto done;
    }

    test_detail("%-*s - success\n", str_max, str);
    str = test_stages[str_step++];

    ret = kmpp_buffer_put(buf);
    if (ret) {
        kmpp_loge("%-*s failed\n", str_max, str);
        goto done;
    }

    test_detail("%-*s - success\n", str_max, str);

done:
    if (buf) {
        kmpp_buffer_put(buf);
        buf = NULL;
    }

    if (grp) {
        kmpp_buf_grp_put(grp);
        grp = NULL;
    }

    return ret;
}

KmppTest test_kmpp_buffer = {
    .name = "kmpp buffer",
    .ctx = NULL,
    .flag = &kmpp_buffer_flag,
    .func = kmpp_buffer_test,
};

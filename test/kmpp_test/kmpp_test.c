/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_test"

#include "kmpp_test.h"

extern KmppTest test_kmpp_objdef_dump;
extern KmppTest test_kmpp_obj_macro;
extern KmppTest test_kmpp_frame;
extern KmppTest test_kmpp_venc_cfg;
extern KmppTest test_kmpp_buffer;

static KmppTest *tests[] = {
    &test_kmpp_objdef_dump,
    &test_kmpp_obj_macro,
    &test_kmpp_frame,
    &test_kmpp_venc_cfg,
    &test_kmpp_buffer,
};

static KmppWorker *test_worker = NULL;
static KmppWork *test_work = NULL;

rk_s32 kmpp_test_func(void *param)
{
    rk_s32 ret = rk_ok;
    rk_s32 i;

    for (i = 0; i < ARRAY_SIZE(tests); i++) {
        KmppTest *test = tests[i];
        const rk_u8 *name = test->name;
        rk_u32 *flag = test->flag;
        void *ctx = test->ctx;

        if (is_flag_set(flag, TEST_DISABLE)) {
            kmpp_logi("test %-16s disabled\n", name);
            continue;
        }

        ret = test->func(name, flag, ctx);
        if (ret) {
            kmpp_logi("test %-16s failed ret %d\n", name, ret);
            goto done;
        }
        kmpp_logi("test %-16s success\n", name);
    }

done:
    kmpp_logi("done %s\n", ret ? "failed" : "success");
    return ret;
}

void kmpp_test_prepare(void)
{
    kmpp_logi("kmpp test start\n");
}

void kmpp_test_finish(void)
{
    kmpp_logi("kmpp test end\n");
}

int kmpp_test_init(void)
{
    kmpp_test_prepare();

    kmpp_worker_init(&test_worker, "kmpp test");
    kmpp_work_init(&test_work, kmpp_test_func, NULL);

    kmpp_work_queue(test_worker, test_work);

    return 0;
}

void kmpp_test_exit(void)
{
    kmpp_worker_deinit(&test_worker);
    kmpp_work_deinit(&test_work);

    kmpp_test_finish();
}

module_init(kmpp_test_init);
module_exit(kmpp_test_exit);

MODULE_AUTHOR("rockchip");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_VERSION("1.0");

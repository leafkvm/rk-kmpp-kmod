/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_dec_test"

#include <linux/module.h>

#include "rk_mpi_cmd.h"
#include "mpp_buffer.h"

#include "kmpp_osal.h"
#include "kmpi_vdec.h"

#include "kmpp_obj.h"
#include "kmpp_vdec_objs.h"

#include "kmpp_frame.h"
#include "kmpp_packet.h"
#include "kmpp_buffer.h"

static rk_u32 frame_num = 10;
static rk_s32 width = 1280;
static rk_s32 height = 720;
static MppCodingType type = MPP_VIDEO_CodingAVC;

module_param(frame_num, uint, 0644);
module_param(width, int, 0644);
module_param(height, int, 0644);
module_param(type, uint, 0644);

#include "vdec_init_deinit.c"

static rk_s32 setup_cfg(KmppCtx ctx)
{
    KmppVdecCfg cfg = NULL;
    rk_s32 ret;

    ret = kmpp_vdec_cfg_get(&cfg);
    if (ret) {
        kmpp_loge("kmpp_vdec_cfg_get failed %d\n", ret);
        return ret;
    }

    ret = kmpp_vdec_get_cfg(ctx, cfg);
    if (ret)
        kmpp_loge("kmpp_vdec_get_cfg failed %d\n", ret);

    ret = kmpp_vdec_set_cfg(ctx, cfg);
    if (ret)
        kmpp_loge("kmpp_vdec_set_cfg failed %d\n", ret);

    ret = kmpp_vdec_cfg_put(cfg);
    if (ret)
        kmpp_loge("kmpp_vdec_cfg_put failed %d\n", ret);

    return ret;
}

rk_s32 vdec_test_single_chan(const rk_u8 *name, rk_u32 flag)
{
    KmppCtx ctx = NULL;
    KmppVdecInitCfg cfg;
    rk_u32 frame_count = 0;
    rk_s32 ret = rk_nok;

    kmpp_vdec_init_cfg_get(&cfg);

    kmpp_obj_set_u32(cfg, "base:coding", MPP_VIDEO_CodingAVC);
    /* setup input packet buffer size 128K */
    kmpp_obj_set_u32(cfg, "input:pkt_buf_size", 128 * 1024);

    /* create decoder by KmppVdecInitCfg */
    ret = kmpp_vdec_init(&ctx, cfg);
    if (ret) {
        kmpp_err_f("kmpp_vdec_init failed ret %d\n", ret);
        goto done;
    }
    kmpp_vdec_init_cfg_put(cfg);

    /* setup detail decoder static config */
    ret = setup_cfg(ctx);
    if (ret) {
        kmpp_err_f("setup_cfg failed ret %d\n", ret);
        goto done;
    }

    /* start decoder and prepare to put frame / get packet */
    kmpp_vdec_start(ctx, NULL);

    while (frame_count < frame_num) {
        KmppFrame frame = NULL;
        KmppPacket packet = NULL;
        KmppShmPtr sptr = {0};

        kmpp_packet_get(&packet);

        kmpp_packet_set_data(packet, &sptr);
        kmpp_packet_set_size(packet, 4096);
        kmpp_packet_set_pos(packet, &sptr);
        kmpp_packet_set_length(packet, 0);

        kmpp_vdec_decode(ctx, packet, &frame);

        if (frame)
            kmpp_frame_put(frame);

        kmpp_packet_put(packet);

        frame_count++;
    }

done:
    if (ctx) {
        kmpp_vdec_stop(ctx, NULL);
        kmpp_vdec_deinit(ctx, NULL);
        ctx = NULL;
    }

    return ret;
}

typedef struct KmppVdecTest_t {
    const rk_u8 *name;
    rk_u32 flag;
    rk_s32 (*func)(const rk_u8 *name, rk_u32 flag);
} KmppVdecTest;

static KmppWorker *test_worker = NULL;
static KmppWork *test_work = NULL;
static rk_s32 test_ret = rk_ok;
static KmppVdecTest vdec_tests[] = {
    {
        "init / deinit",
        0,
        vdec_test_init_deinit,
    },
    {
        "single channel",
        0,
        vdec_test_single_chan,
    },
};

void kmpi_dec_test_prepare(void)
{
    kmpp_logi("start\n");
}

rk_s32 kmpi_dec_test_run(void *data)
{
    rk_s32 ret = rk_ok;
    rk_u32 i;
    (void) data;

    for (i = 0; i < KMPP_ARRAY_ELEMS(vdec_tests); i++) {
        const rk_u8 *name = vdec_tests[i].name;
        rk_u32 flag = vdec_tests[i].flag;

        ret = vdec_tests[i].func(name, flag);
        if (ret) {
            kmpp_logi("test %-16s failed ret %d\n", name, ret);
            break;
        }
        kmpp_logi("test %-16s success\n", name);
    }

    test_ret = ret;

    return ret;
}

void kmpi_dec_test_finish(void)
{
    kmpp_logi("end %s\n", test_ret ? "failed" : "success");
}

int kmpi_dec_test_init(void)
{
    kmpi_dec_test_prepare();

    kmpp_worker_init(&test_worker, "kmpp dec test");
    kmpp_work_init(&test_work, kmpi_dec_test_run, NULL);

    kmpp_work_queue(test_worker, test_work);

    return 0;
}

void kmpi_dec_test_exit(void)
{
    kmpp_worker_deinit(&test_worker);
    kmpp_work_deinit(&test_work);

    kmpi_dec_test_finish();
}

module_init(kmpi_dec_test_init);
module_exit(kmpi_dec_test_exit);

MODULE_AUTHOR("rockchip");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_VERSION("1.0");

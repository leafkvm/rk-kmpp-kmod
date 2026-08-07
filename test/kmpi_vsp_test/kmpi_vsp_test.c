/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#include <linux/module.h>

#include "kmpp_frame.h"
#include "kmpp_meta.h"
#include "mpp_buffer.h"
#include "kmpp_buffer.h"

#include "kmpp_osal.h"
#include "kmpp_sys.h"
#include "kmpi_vsp.h"

static RK_U32 frame_num = 10;
module_param(frame_num, uint, 0644);

#define PP_TEST_WIDTH   1280
#define PP_TEST_HEIGHT  720

typedef struct TestVspData {
    rk_s32 width;
    rk_s32 height;
    rk_s32 hor_stride;
    rk_s32 ver_stride;
    rk_s32 fmt;
    rk_s32 buf_size;

    MppBuffer buf_frm;
    MppBuffer buf_md;
    MppBuffer buf_od;
    KmppVspRtOut out;

    KmppShmPtr zero;
} TestVspData;

#define CHK(func, ...)  \
    ret = (func)(__VA_ARGS__); \
    if (ret) { \
        kmpp_err_f("%s failed ret %d\n", #func, ret); \
        return; \
    }

void vsp_data_init(TestVspData *data)
{
    data->width = PP_TEST_WIDTH;
    data->height = PP_TEST_HEIGHT;
    data->hor_stride = PP_TEST_WIDTH;
    data->ver_stride = PP_TEST_HEIGHT;
    data->fmt = MPP_FMT_YUV420SP;
    data->buf_size = data->hor_stride * data->ver_stride * 3 / 2;

    mpp_buffer_get(NULL, &data->buf_frm, data->buf_size);
    mpp_buffer_get(NULL, &data->buf_md, data->buf_size);
    mpp_buffer_get(NULL, &data->buf_od, data->buf_size);
    kmpp_vsp_pp_rt_out_get(&data->out);

    data->zero.uaddr = 0;
    data->zero.kaddr = 0;
}

void vsp_data_deinit(TestVspData *data)
{
    if (data->buf_frm) {
        mpp_buffer_put(data->buf_frm);
        data->buf_frm = NULL;
    }
    if (data->buf_md) {
        mpp_buffer_put(data->buf_md);
        data->buf_md = NULL;
    }
    if (data->buf_od) {
        mpp_buffer_put(data->buf_od);
        data->buf_od = NULL;
    }
    if (data->out) {
        kmpp_vsp_pp_rt_out_put(data->out);
        data->out = NULL;
    }
}

void vsp_test(TestVspData *data)
{
    KmppCtx ctx = NULL;
    KmppVspInitCfg init_cfg = NULL;
    KmppVspRtCfg rt_cfg = NULL;
    RK_U32 frame_count = 0;
    KmppShmPtr name;
    rk_s32 ret;

    /* 1. init vsp ctx by init_cfg */
    ret = kmpp_vsp_init_cfg_get(&init_cfg);
    if (ret) {
        kmpp_err_f("kmpp_vsp_init_cfg_get failed ret %d\n", ret);
        return;
    }

    name.uptr = 0;
    name.kptr = "vepu500_pp";

    ret |= kmpp_vsp_init_cfg_set_name(init_cfg, &name);
    ret |= kmpp_vsp_init_cfg_set_max_width(init_cfg, 4096);
    ret |= kmpp_vsp_init_cfg_set_max_height(init_cfg, 2048);
    if (ret) {
        kmpp_err_f("kmpp_vsp_init_cfg_set failed ret %d\n", ret);
        return;
    }

    ret = kmpp_vsp_init(&ctx, init_cfg);
    if (ret) {
        kmpp_err_f("kmpp_vsp_init failed ret %d\n", ret);
        return;
    }

    ret = kmpp_vsp_init_cfg_put(init_cfg);
    if (ret) {
        kmpp_err_f("kmpp_vsp_init_cfg_put failed ret %d\n", ret);
        return;
    }

    /* 2. setup static / dynamic config */
    /*
     * NOTE: here use get_rt_cfg to get one-shot data
     * which will be freed after set_rt_cfg atuomatically
     */
    ret = kmpp_vsp_get_rt_cfg(ctx, &rt_cfg, NULL);
    if (ret) {
        kmpp_err_f("kmpp_vsp_pp_rt_cfg_get failed ret %d\n", ret);
        return;
    }

    ret |= kmpp_obj_set_s32(rt_cfg, "width", data->width);
    ret |= kmpp_obj_set_s32(rt_cfg, "height", data->height);
    ret |= kmpp_obj_set_s32(rt_cfg, "od:enable", 1);
    ret |= kmpp_obj_set_s32(rt_cfg, "md:enable", 1);
    ret |= kmpp_obj_set_s32(rt_cfg, "down_scale_en", 1);
    if (ret) {
        kmpp_err_f("kmpp_obj_get_s32 failed ret %d\n", ret);
        return;
    }

    ret = kmpp_vsp_set_rt_cfg(ctx, rt_cfg);
    if (ret) {
        kmpp_err_f("mpp_vsp_cfg_setup failed ret %d\n", ret);
        return;
    }

    /* 3. setup data process and only dynamic config can be setup after start */
    ret = kmpp_vsp_start(ctx, NULL);
    if (ret) {
        kmpp_err_f("kmpp_vsp_start failed ret %d\n", ret);
        return;
    }

    /* 4. process input data by kmpp_vsp_proc */
    while (frame_count++ < frame_num) {
        KmppFrame frame = NULL;
        KmppFrame out = NULL;
        KmppMeta meta = NULL;
        KmppShmPtr meta_sptr;
        KmppShmPtr sptr;

        kmpp_frame_get(&frame);
        kmpp_frame_set_width(frame, data->width);
        kmpp_frame_set_height(frame, data->height);
        kmpp_frame_set_hor_stride(frame, data->hor_stride);
        kmpp_frame_set_ver_stride(frame, data->ver_stride);
        kmpp_frame_set_fmt(frame, data->fmt);
        mpp_buffer_to_shmptr(data->buf_frm, &sptr);
        kmpp_frame_set_buffer(frame, &sptr);

        kmpp_frame_get_meta(frame, &meta_sptr);
        meta = meta_sptr.kptr;

        if (meta) {
            KmppShmPtr buf_sptr;

            buf_sptr.uaddr = 0;
            buf_sptr.kptr = data->buf_md;

            kmpp_meta_set_shm(meta, KEY_PP_MD_BUF, &buf_sptr);

            buf_sptr.kptr = data->buf_od;
            kmpp_meta_set_shm(meta, KEY_PP_OD_BUF, &buf_sptr);

            buf_sptr.kptr = data->out;
            kmpp_meta_set_shm(meta, KEY_PP_OUT, &buf_sptr);
        }

        /* block wait for task done */
        kmpp_vsp_proc(ctx, frame, &out);

        if (out) {
            if (out != frame) {
                kmpp_err_f("get mismatch frame in %px - out %px\n", frame, out);
            } else {
                /* NOTE: frame == out */
                kmpp_frame_get_meta(frame, &meta_sptr);
                meta = meta_sptr.kptr;

                if (meta) {
                    KmppShmPtr out;

                    {   /* Verify md buf only. No practical use */
                        out.uaddr = 0;
                        out.kaddr = 0;

                        kmpp_meta_get_shm(meta, KEY_PP_MD_BUF, &out);

                        if (out.kptr != data->buf_md) {
                            kmpp_loge_f("mismatch md buf %px - %px\n", out.kptr, data->buf_md);
                        }
                    }

                    {   /* Verify od buf only. No practical use */
                        out.uaddr = 0;
                        out.kaddr = 0;

                        kmpp_meta_get_shm(meta, KEY_PP_OD_BUF, &out);

                        if (out.kptr != data->buf_od) {
                            kmpp_loge_f("mismatch od buf %px - %px\n", out.kptr, data->buf_od);
                        }
                    }

                    kmpp_meta_get_shm_d(meta, KEY_PP_OUT, &out, &data->zero);

                    if (out.kptr && frame_count == frame_num) {
                        kmpp_vsp_pp_rt_out_dump(out.kptr, "last dump");
                    }
                }
            }
        }

        kmpp_frame_put(frame);
    }

    /* 5. stop data process flow */
    kmpp_vsp_stop(ctx, NULL);

    /* 6. deinit vsp ctx */
    kmpp_vsp_deinit(ctx, NULL);
}

rk_s32 kmpi_vsp_test_func(void *param)
{
    TestVspData data;

    vsp_data_init(&data);

    vsp_test(&data);

    vsp_data_deinit(&data);

    return rk_ok;
}

static KmppWorker *test_worker = NULL;
static KmppWork *test_work = NULL;

void kmpi_vsp_test_prepare(void)
{
    kmpp_logi("kmpp vsp test start\n");
}

void kmpi_vsp_test_finish(void)
{
    kmpp_logi("kmpp vsp test end\n");
}

int kmpi_vsp_test_init(void)
{
    kmpi_vsp_test_prepare();

    kmpp_worker_init(&test_worker, "kmpp vsp test");
    kmpp_work_init(&test_work, kmpi_vsp_test_func, NULL);

    kmpp_work_queue(test_worker, test_work);

    return 0;
}

void kmpi_vsp_test_exit(void)
{
    kmpp_worker_deinit(&test_worker);
    kmpp_work_deinit(&test_work);

    kmpi_vsp_test_finish();
}

module_init(kmpi_vsp_test_init);
module_exit(kmpi_vsp_test_exit);

MODULE_AUTHOR("rockchip");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_VERSION("1.0");

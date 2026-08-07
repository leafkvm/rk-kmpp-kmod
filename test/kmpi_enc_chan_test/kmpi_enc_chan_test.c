/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_enc_chan_test"

#include <linux/module.h>

#include "rk_mpi_cmd.h"
#include "mpp_buffer.h"
#include "mpp_rc_api.h"

#include "kmpp_osal.h"
#include "kmpi_venc.h"

#include "kmpp_obj.h"
#include "kmpp_venc_objs.h"

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

static rk_s32 setup_st_cfg(KmppChanId chan_id)
{
    KmppVencStCfg cfg = NULL;
    rk_u32 hor_stride = width;
    rk_u32 ver_stride = height;
    rk_s32 fps_in_flex = 0;
    rk_s32 fps_in_den = 1;
    rk_s32 fps_in_num = 25;
    rk_s32 fps_out_flex = 0;
    rk_s32 fps_out_den = 1;
    rk_s32 fps_out_num = 25;
    rk_s32 bps = 0;
    rk_s32 bps_max = 0;
    rk_s32 bps_min = 0;
    rk_s32 rc_mode = MPP_ENC_RC_MODE_CBR;
    rk_u32 gop_len = 60;
    rk_s32 ret;

    ret = kmpp_venc_st_cfg_get(&cfg);
    if (ret) {
        kmpp_loge("kmpp_venc_st_cfg_get failed %d\n", ret);
        return ret;
    }

    if (!bps)
        bps = width * height / 8 * (fps_out_num / fps_out_den);

    kmpp_obj_set_s32(cfg, "prep:width", width);
    kmpp_obj_set_s32(cfg, "prep:height", height);
    kmpp_obj_set_s32(cfg, "prep:hor_stride", hor_stride);
    kmpp_obj_set_s32(cfg, "prep:ver_stride", ver_stride);
    kmpp_obj_set_s32(cfg, "prep:format", 0);
    kmpp_obj_set_s32(cfg, "rc:mode", rc_mode);

    /* fix input / output frame rate */
    kmpp_obj_set_s32(cfg, "rc:fps_in_flex", fps_in_flex);
    kmpp_obj_set_s32(cfg, "rc:fps_in_num", fps_in_num);
    kmpp_obj_set_s32(cfg, "rc:fps_in_denorm", fps_in_den);
    kmpp_obj_set_s32(cfg, "rc:fps_out_flex", fps_out_flex);
    kmpp_obj_set_s32(cfg, "rc:fps_out_num", fps_out_num);
    kmpp_obj_set_s32(cfg, "rc:fps_out_denorm", fps_out_den);
    kmpp_obj_set_s32(cfg, "rc:gop", gop_len ? gop_len : fps_out_num * 2);

    /* drop frame or not when bitrate overflow */
    kmpp_obj_set_u32(cfg, "rc:drop_mode", MPP_ENC_RC_DROP_FRM_DISABLED);
    kmpp_obj_set_u32(cfg, "rc:drop_thd", 20);    /* 20% of max bps */
    kmpp_obj_set_u32(cfg, "rc:drop_gap", 1);    /* Do not continuous drop frame */

    /* setup bitrate for different rc_mode */
    kmpp_obj_set_s32(cfg, "rc:bps_target", bps);
    switch (rc_mode) {
    case MPP_ENC_RC_MODE_FIXQP: {
        /* do not setup bitrate on FIXQP mode */
    } break;
    case MPP_ENC_RC_MODE_CBR: {
        /* CBR mode has narrow bound */
        kmpp_obj_set_s32(cfg, "rc:bps_max", bps_max ? bps_max : bps * 17 / 16);
        kmpp_obj_set_s32(cfg, "rc:bps_min", bps_min ? bps_min : bps * 15 / 16);
    } break;
    case MPP_ENC_RC_MODE_VBR:
    case MPP_ENC_RC_MODE_AVBR:
    case MPP_ENC_RC_MODE_SMTRC: {
        /* VBR mode has wide bound */
        kmpp_obj_set_s32(cfg, "rc:bps_max", bps_max ? bps_max : bps * 17 / 16);
        kmpp_obj_set_s32(cfg, "rc:bps_min", bps_min ? bps_min : bps * 1 / 16);
    } break;
    default: {
        /* default use CBR mode */
        kmpp_obj_set_s32(cfg, "rc:bps_max", bps_max ? bps_max : bps * 17 / 16);
        kmpp_obj_set_s32(cfg, "rc:bps_min", bps_min ? bps_min : bps * 15 / 16);
    } break;
    }

    /* setup qp for different codec and rc_mode */
    switch (type) {
    case MPP_VIDEO_CodingAVC:
    case MPP_VIDEO_CodingHEVC: {
        switch (rc_mode) {
        case MPP_ENC_RC_MODE_FIXQP: {
            kmpp_obj_set_s32(cfg, "rc:qp_init", 20);
            kmpp_obj_set_s32(cfg, "rc:qp_max", 20);
            kmpp_obj_set_s32(cfg, "rc:qp_min", 20);
            kmpp_obj_set_s32(cfg, "rc:qp_max_i", 20);
            kmpp_obj_set_s32(cfg, "rc:qp_min_i", 20);
            kmpp_obj_set_s32(cfg, "rc:qp_ip", 2);
        } break;
        case MPP_ENC_RC_MODE_CBR:
        case MPP_ENC_RC_MODE_VBR:
        case MPP_ENC_RC_MODE_AVBR: {
            kmpp_obj_set_s32(cfg, "rc:qp_init", 26);
            kmpp_obj_set_s32(cfg, "rc:qp_max", 51);
            kmpp_obj_set_s32(cfg, "rc:qp_min", 10);
            kmpp_obj_set_s32(cfg, "rc:qp_max_i", 51);
            kmpp_obj_set_s32(cfg, "rc:qp_min_i", 10);
            kmpp_obj_set_s32(cfg, "rc:qp_ip", 2);
        } break;
        default: {
            kmpp_loge_f("unsupport encoder rc mode %d\n", rc_mode);
        } break;
        }
    }
    break;
    case MPP_VIDEO_CodingVP8: {
        /* vp8 only setup base qp range */
        kmpp_obj_set_s32(cfg, "rc:qp_init", 40);
        kmpp_obj_set_s32(cfg, "rc:qp_max", 127);
        kmpp_obj_set_s32(cfg, "rc:qp_min", 0);
        kmpp_obj_set_s32(cfg, "rc:qp_max_i", 127);
        kmpp_obj_set_s32(cfg, "rc:qp_min_i", 0);
        kmpp_obj_set_s32(cfg, "rc:qp_ip", 6);
    } break;
    case MPP_VIDEO_CodingMJPEG: {
        /* jpeg use special codec config to control qtable */
        kmpp_obj_set_s32(cfg, "jpeg:q_factor", 80);
        kmpp_obj_set_s32(cfg, "jpeg:qf_max", 99);
        kmpp_obj_set_s32(cfg, "jpeg:qf_min", 1);
    } break;
    default: {
    } break;
    }

    /* setup codec  */
    kmpp_obj_set_s32(cfg, "codec:type", type);
    switch (type) {
    case MPP_VIDEO_CodingAVC: {
        /*
         * H.264 profile_idc parameter
         * 66  - Baseline profile
         * 77  - Main profile
         * 100 - High profile
         */
        kmpp_obj_set_s32(cfg, "h264:profile", 100);

        /*
         * H.264 level_idc parameter
         * 10 / 11 / 12 / 13    - qcif@15fps / cif@7.5fps / cif@15fps / cif@30fps
         * 20 / 21 / 22         - cif@30fps / half-D1@@25fps / D1@12.5fps
         * 30 / 31 / 32         - D1@25fps / 720p@30fps / 720p@60fps
         * 40 / 41 / 42         - 1080p@30fps / 1080p@30fps / 1080p@60fps
         * 50 / 51 / 52         - 4K@30fps
         */
        kmpp_obj_set_s32(cfg, "h264:level", 40);
        kmpp_obj_set_s32(cfg, "h264:cabac_en", 1);
        kmpp_obj_set_s32(cfg, "h264:cabac_idc", 0);
        kmpp_obj_set_s32(cfg, "h264:trans8x8", 1);
    } break;
    case MPP_VIDEO_CodingHEVC:
    case MPP_VIDEO_CodingMJPEG:
    case MPP_VIDEO_CodingVP8: {
    } break;
    default: {
        kmpp_loge_f("unsupport encoder coding type %d\n", type);
    } break;
    }

    ret = kmpp_venc_chan_ctrl(chan_id, MPP_ENC_SET_CFG, cfg);
    if (ret)
        kmpp_loge("kmpp_venc_chan_ctrl MPP_ENC_SET_CFG failed %d\n", ret);

    ret = kmpp_venc_st_cfg_put(cfg);
    if (ret)
        kmpp_loge("kmpp_venc_st_cfg_put failed %d\n", ret);

    {
        RcApiBrief rc_api_brief;

        rc_api_brief.type = type;
        rc_api_brief.name = "smart";

        ret = kmpp_venc_chan_ctrl(chan_id, MPP_ENC_SET_RC_API_CURRENT, &rc_api_brief);
        if (ret)
            kmpp_loge("mpi control enc set rc api failed ret %d\n", ret);
    }

    return ret;
}

rk_s32 kmpi_enc_chan_test_func(void *param)
{
    KmppChanId chan_id = -1;
    KmppVencInitCfg cfg;
    MppBuffer buffer;
    KmppShmPtr sptr;
    rk_u32 frame_count = 0;
    rk_s32 ret = rk_nok;

    kmpp_venc_init_cfg_get(&cfg);

    kmpp_venc_init_cfg_set_coding(cfg, type);
    kmpp_venc_init_cfg_set_ntfy_mode(cfg, 0);

    /* create encoder by KmppVencInitCfg */
    ret = kmpp_venc_chan_init(&chan_id, cfg);
    if (ret) {
        kmpp_err_f("kmpp_venc_chan_init failed ret %d\n", ret);
        goto done;
    }
    kmpp_venc_init_cfg_put(cfg);

    /* setup detail encoder static config */
    ret = setup_st_cfg(chan_id);
    if (ret) {
        kmpp_err_f("setup_st_cfg failed ret %d\n", ret);
        goto done;
    }

    /* start encoder and prepare to put frame / get packet */
    kmpp_venc_chan_start(chan_id, NULL);

    while (frame_count < frame_num) {
        KmppFrame frame = NULL;
        KmppPacket packet = NULL;
        rk_u32 hor_stride = width;
        rk_u32 ver_stride = height;
        rk_u32 size = width * height * 3 / 2;

        mpp_buffer_get(NULL, &buffer, size);

        kmpp_frame_get(&frame);

        kmpp_frame_set_width(frame, width);
        kmpp_frame_set_height(frame, height);
        kmpp_frame_set_hor_stride(frame, hor_stride);
        kmpp_frame_set_ver_stride(frame, ver_stride);
        kmpp_frame_set_fmt(frame, MPP_FMT_YUV420SP);

        mpp_buffer_to_shmptr(buffer, &sptr);
        kmpp_frame_set_buffer(frame, &sptr);

        mpp_buffer_put(buffer);

        /* put frame to encoder */
        kmpp_venc_chan_put_frm(chan_id, frame);

        /* get frame from encoder */
        kmpp_venc_chan_get_pkt(chan_id, &packet);

        if (packet) {
            rk_u32 len;

            kmpp_packet_get_length(packet, &len);
            kmpp_packet_put(packet);
            kmpp_logi("frm %d get stream size %d\n", frame_count, len);
        }
        kmpp_frame_put(frame);

        frame_count++;
    }

done:
    if (chan_id >= 0) {
        kmpp_venc_chan_stop(chan_id, NULL);
        kmpp_venc_chan_deinit(chan_id, NULL);
    }

    return ret;
}

static KmppWorker *test_worker = NULL;
static KmppWork *test_work = NULL;

void kmpi_enc_chan_test_prepare(void)
{
    kmpp_logi("start\n");
}

void kmpi_enc_chan_test_finish(void)
{
    kmpp_logi("end\n");
}

int kmpi_enc_chan_test_init(void)
{
    kmpi_enc_chan_test_prepare();

    kmpp_worker_init(&test_worker, "kmpp enc test");
    kmpp_work_init(&test_work, kmpi_enc_chan_test_func, NULL);

    kmpp_work_queue(test_worker, test_work);

    return 0;
}

void kmpi_enc_chan_test_exit(void)
{
    kmpp_worker_deinit(&test_worker);
    kmpp_work_deinit(&test_work);

    kmpi_enc_chan_test_finish();
}

module_init(kmpi_enc_chan_test_init);
module_exit(kmpi_enc_chan_test_exit);

MODULE_AUTHOR("rockchip");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_VERSION("1.0");

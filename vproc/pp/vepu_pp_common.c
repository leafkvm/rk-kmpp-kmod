/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#include "kmpp_frame.h"
#include "vepu_pp_common.h"

static vepu_pp_hw_fmt_cfg pp_hw_yuv_fmt_cfg[MPP_FMT_YUV_BUTT] = {
    /* MPP_FMT_YUV420SP */
    {
        .format = VEPU_PP_HW_FMT_YUV420SP,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_YUV420SP_10BIT */
    {
        .format = VEPU_PP_HW_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_YUV422SP */
    {
        .format = VEPU_PP_HW_FMT_YUV422SP,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_YUV422SP_10BIT */
    {
        .format = VEPU_PP_HW_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_YUV420P */
    {
        .format = VEPU_PP_HW_FMT_YUV420P,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_YUV420SP_VU */
    {
        .format = VEPU_PP_HW_FMT_YUV420SP,
        .alpha_swap = 0,
        .rbuv_swap = 1,
        .src_endian = 0,
    },
    /* MPP_FMT_YUV422P */
    {
        .format = VEPU_PP_HW_FMT_YUV422P,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_YUV422SP_VU */
    {
        .format = VEPU_PP_HW_FMT_YUV422SP,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_YUV422_YUYV */
    {
        .format = VEPU_PP_HW_FMT_YUYV422,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_YUV422_YVYU */
    {
        .format = VEPU_PP_HW_FMT_YUYV422,
        .alpha_swap = 0,
        .rbuv_swap = 1,
        .src_endian = 0,
    },
    /* MPP_FMT_YUV422_UYVY */
    {
        .format = VEPU_PP_HW_FMT_UYVY422,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_YUV422_VYUY */
    {
        .format = VEPU_PP_HW_FMT_UYVY422,
        .alpha_swap = 0,
        .rbuv_swap = 1,
        .src_endian = 0,
    },
    /* MPP_FMT_YUV400 */
    {
        .format = VEPU_PP_HW_FMT_YUV400,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_YUV440SP */
    {
        .format = VEPU_PP_HW_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_YUV411SP */
    {
        .format = VEPU_PP_HW_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_YUV444SP */
    {
        .format = VEPU_PP_HW_FMT_YUV444SP,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_YUV444P */
    {
        .format = VEPU_PP_HW_FMT_YUV444P,
        .alpha_swap = 0,
        .rbuv_swap = 1,
        .src_endian = 0,
    },
    /* MPP_FMT_AYUV2BPP */
    {
        .format = VEPU_PP_HW_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_AYUV1BPP */
    {
        .format = VEPU_PP_HW_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
};

static vepu_pp_hw_fmt_cfg pp_hw_rgb_fmt_cfg[MPP_FMT_RGB_BUTT - MPP_FRAME_FMT_RGB] = {
    /* MPP_FMT_RGB565 */
    {
        .format = VEPU_PP_HW_FMT_RGB565,
        .alpha_swap = 0,
        .rbuv_swap = 1,
        .src_endian = 1,
    },
    /* MPP_FMT_BGR565 */
    {
        .format = VEPU_PP_HW_FMT_RGB565,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 1,
    },
    /* MPP_FMT_RGB555 */
    {
        .format = VEPU_PP_HW_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_BGR555 */
    {
        .format = VEPU_PP_HW_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_RGB444 */
    {
        .format = VEPU_PP_HW_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_BGR444 */
    {
        .format = VEPU_PP_HW_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_RGB888 */
    {
        .format = VEPU_PP_HW_FMT_RGB888,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_BGR888 */
    {
        .format = VEPU_PP_HW_FMT_RGB888,
        .alpha_swap = 0,
        .rbuv_swap = 1,
        .src_endian = 0,
    },
    /* MPP_FMT_RGB101010 */
    {
        .format = VEPU_PP_HW_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_BGR101010 */
    {
        .format = VEPU_PP_HW_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_ARGB8888 */
    {
        .format = VEPU_PP_HW_FMT_BGRA8888,
        .alpha_swap = 1,
        .rbuv_swap = 1,
        .src_endian = 0,
    },
    /* MPP_FMT_ABGR8888 */
    {
        .format = VEPU_PP_HW_FMT_BGRA8888,
        .alpha_swap = 1,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_BGRA8888 */
    {
        .format = VEPU_PP_HW_FMT_BGRA8888,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_RGBA8888 */
    {
        .format = VEPU_PP_HW_FMT_BGRA8888,
        .alpha_swap = 0,
        .rbuv_swap = 1,
        .src_endian = 0,
    },
    /* MPP_FMT_ARGB4444 */
    {
        .format = VEPU_PP_HW_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
    /* MPP_FMT_ARGB1555 */
    {
        .format = VEPU_PP_HW_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_endian = 0,
    },
};

void vepu_pp_transform_hw_fmt_cfg(vepu_pp_hw_fmt_cfg *cfg, MppFrameFormat format)
{
    vepu_pp_hw_fmt_cfg *fmt = NULL;

    format &= MPP_FRAME_FMT_MASK;

    if (MPP_FRAME_FMT_IS_YUV(format))
        fmt = &pp_hw_yuv_fmt_cfg[format - MPP_FRAME_FMT_YUV];
    else if (MPP_FRAME_FMT_IS_RGB(format))
        fmt = &pp_hw_rgb_fmt_cfg[format - MPP_FRAME_FMT_RGB];
    else {
        kmpp_memset(cfg, 0, sizeof(*cfg));
        cfg->format = VEPU_PP_HW_FMT_BUTT;
    }

    if (fmt && fmt->format != VEPU_PP_HW_FMT_BUTT)
        kmpp_memcpy(cfg, fmt, sizeof(*cfg));
    else {
        kmpp_loge_f("unsupport frame format %x\n", format);
        cfg->format = VEPU_PP_HW_FMT_BUTT;
    }
}
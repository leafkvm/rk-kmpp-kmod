/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG  "vepu541_common"

#include "kmpp_string.h"

#include "kmpp_log.h"
#include "mpp_mem.h"
#include "mpp_maths.h"

#include "vepu541_common.h"

static const RK_S32 zeros[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static VepuFmtCfg vepu5xx_yuv_cfg[MPP_FMT_YUV_BUTT] = {
    { /* MPP_FMT_YUV420SP */
        .format = VEPU541_FMT_YUV420SP,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_YUV420SP_10BIT */
        .format = VEPU540C_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_YUV422SP */
        .format = VEPU541_FMT_YUV422SP,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_YUV422SP_10BIT */
        .format = VEPU540C_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_YUV420P */
        .format = VEPU541_FMT_YUV420P,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_YUV420SP_VU   */
        .format = VEPU541_FMT_YUV420SP,
        .alpha_swap = 0,
        .rbuv_swap = 1,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_YUV422P */
        .format = VEPU541_FMT_YUV422P,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_YUV422SP_VU */
        .format = VEPU541_FMT_YUV422SP,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_YUV422_YUYV */
        .format = VEPU541_FMT_YUYV422,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_YUV422_YVYU */
        .format = VEPU541_FMT_YUYV422,
        .alpha_swap = 0,
        .rbuv_swap = 1,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_YUV422_UYVY */
        .format = VEPU541_FMT_UYVY422,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_YUV422_VYUY */
        .format = VEPU541_FMT_UYVY422,
        .alpha_swap = 0,
        .rbuv_swap = 1,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_YUV400 */
        .format = VEPU540_FMT_YUV400,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_YUV440SP */
        .format = VEPU540C_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_YUV411SP */
        .format = VEPU540C_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_YUV444SP */
        .format = VEPU540C_FMT_YUV444SP,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },

    { /* MPP_FMT_YUV444P */
        .format = VEPU540C_FMT_YUV444P,
        .alpha_swap = 0,
        .rbuv_swap = 1,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },

    { /* MPP_FMT_YUV444SP_10BIT */
        .format = VEPU540C_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },

    { /* MPP_FMT_AYUV2BPP */
        .format = VEPU540C_FMT_AYUV2BPP,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },

    { /* MPP_FMT_AYUV1BPP */
        .format = VEPU500_FMT_AYUV1BPP,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
};

static VepuFmtCfg vepu5xx_rgb_cfg[MPP_FMT_RGB_BUTT - MPP_FRAME_FMT_RGB] = {
    { /* MPP_FMT_RGB565 */
        .format = VEPU541_FMT_BGR565,
        .alpha_swap = 0,
        .rbuv_swap = 1,
        .src_range = 0,
        .src_endian = 1,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_BGR565 */
        .format = VEPU541_FMT_BGR565,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 1,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_RGB555 */
        .format = VEPU540C_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_BGR555 */
        .format = VEPU540C_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_RGB444 */
        .format = VEPU540C_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_BGR444 */
        .format = VEPU540C_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_RGB888 */
        .format = VEPU541_FMT_BGR888,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_BGR888 */
        .format = VEPU541_FMT_BGR888,
        .alpha_swap = 0,
        .rbuv_swap = 1,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_RGB101010 */
        .format = VEPU540C_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_BGR101010 */
        .format = VEPU540C_FMT_BUTT,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_ARGB8888 */
        .format = VEPU541_FMT_BGRA8888,
        .alpha_swap = 1,
        .rbuv_swap = 1,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_ABGR8888 */
        .format = VEPU541_FMT_BGRA8888,
        .alpha_swap = 1,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_BGRA8888 */
        .format = VEPU541_FMT_BGRA8888,
        .alpha_swap = 0,
        .rbuv_swap = 0,
        .src_range = 0,
        .src_endian = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_RGBA8888 */
        .format = VEPU541_FMT_BGRA8888,
        .alpha_swap = 0,
        .rbuv_swap = 1,
        .src_endian = 0,
        .src_range = 0,
        .weight = zeros,
        .offset = zeros,
    },

    { /* MPP_FMT_ARGB4444 */
        .format = VEPU540C_FMT_ARGB4444,
        .alpha_swap = 0,
        .rbuv_swap = 1,
        .src_endian = 0,
        .src_range = 0,
        .weight = zeros,
        .offset = zeros,
    },
    { /* MPP_FMT_ARGB1555 */
        .format = VEPU541_FMT_ARGB1555,
        .alpha_swap = 0,
        .rbuv_swap = 1,
        .src_endian = 0,
        .src_range = 0,
        .weight = zeros,
        .offset = zeros,
    },
};

MPP_RET vepu5xx_set_fmt(VepuFmtCfg * cfg, MppFrameFormat format)
{
    VepuFmtCfg *fmt = NULL;
    MPP_RET ret = MPP_OK;

    format &= MPP_FRAME_FMT_MASK;

    if (MPP_FRAME_FMT_IS_YUV(format))
        fmt = &vepu5xx_yuv_cfg[format - MPP_FRAME_FMT_YUV];
    else if (MPP_FRAME_FMT_IS_RGB(format))
        fmt = &vepu5xx_rgb_cfg[format - MPP_FRAME_FMT_RGB];
    else {
        kmpp_memset(cfg, 0, sizeof(*cfg));
        cfg->format = VEPU540C_FMT_BUTT;
    }

    if (fmt && fmt->format != VEPU540C_FMT_BUTT)
        kmpp_memcpy(cfg, fmt, sizeof(*cfg));
    else {
        kmpp_loge_f("unsupport frame format %x\n", format);
        cfg->format = VEPU540C_FMT_BUTT;
        ret = MPP_NOK;
    }

    return ret;
}

MPP_RET vepu5xx_check_osd_buffer_size(rk_u32 lt_x, rk_u32 lt_y, rk_u32 rb_x, rk_u32 rb_y,
                                      rk_u32 stride, VepuFmt format, rk_u32 buf_size)
{
    rk_u32 width = rb_x - lt_x + 1;
    rk_u32 height = rb_y - lt_y + 1;
    rk_u32 need_stride;
    rk_u32 need_size;

    switch (format) {
    case VEPU541_FMT_BGRA8888: {
        need_stride = width * 4;
    } break;
    case VEPU541_FMT_ARGB1555:
    case VEPU540C_FMT_ARGB4444: {
        need_stride = width  * 2;
    } break;
    case VEPU540C_FMT_AYUV2BPP: {
        need_stride = width / 4;
    } break;
    case VEPU500_FMT_AYUV1BPP: {
        need_stride = width / 8;
    } break;
    default:
        return MPP_NOK;
    }

    if (stride < need_stride) {
        kmpp_loge_f("osd stride lt[%d:%d] rb[%d:%d] fmt %d stride not enough %d < %d\n",
                    lt_x, lt_y, rb_x, rb_y, format, stride, need_stride);
        return MPP_NOK;
    }

    /* hardware access buffer is aligned to 128 bits */
    need_size = MPP_ALIGN(stride * height, 16);
    if (buf_size < need_size) {
        kmpp_loge_f("osd buffer lt[%d:%d] rb[%d:%d] fmt %d stride %d size %d < %d\n",
                    lt_x, lt_y, rb_x, rb_y, format, stride, buf_size, need_size);
        return MPP_NOK;
    }

    return MPP_OK;
}

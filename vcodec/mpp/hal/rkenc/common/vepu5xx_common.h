/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#ifndef __VEPU5XX_COMMON_H__
#define __VEPU5XX_COMMON_H__

#define HWID_VEPU58X                (0x50603312)
#define HWID_VEPU540C               (0x50603313)

typedef enum VepuFmt_e {
    VEPU541_FMT_BGRA8888,   // 0
    VEPU541_FMT_BGR888, // 1
    VEPU541_FMT_BGR565, // 2
    VEPU541_FMT_ARGB1555,   // 3
    VEPU541_FMT_YUV422SP,   // 4
    VEPU541_FMT_YUV422P,    // 5
    VEPU541_FMT_YUV420SP,   // 6
    VEPU541_FMT_YUV420P,    // 7
    VEPU541_FMT_YUYV422,    // 8
    VEPU541_FMT_UYVY422,    // 9
    VEPU541_FMT_BUTT,   // 10

    /* vepu540 add YUV400 support */
    VEPU540_FMT_YUV400 = VEPU541_FMT_BUTT,  // 10
    VEPU540C_FMT_AYUV2BPP,
    VEPU540C_FMT_YUV444SP,
    VEPU540C_FMT_YUV444P,
    VEPU540C_FMT_ARGB4444,      //[15:0] : ARGB
    VEPU540C_FMT_ARGB2BPP,  // 15
    VEPU500_FMT_AYUV1BPP = VEPU540C_FMT_ARGB2BPP,   //15
    VEPU540C_FMT_BUTT,  // 16
} VepuFmt;

typedef struct VepuFmtCfg_t {
    VepuFmt format;
    RK_U32 alpha_swap;
    RK_U32 rbuv_swap;
    RK_U32 src_range;
    RK_U32 src_endian;
    const RK_S32 *weight;
    const RK_S32 *offset;
} VepuFmtCfg;

MPP_RET vepu5xx_set_fmt(VepuFmtCfg * cfg, MppFrameFormat format);
MPP_RET vepu5xx_check_osd_buffer_size(rk_u32 lt_x, rk_u32 lt_y, rk_u32 rb_x, rk_u32 rb_y,
                                      rk_u32 stride, VepuFmt format, rk_u32 buf_size);

#endif /* __VEPU5XX_H__ */

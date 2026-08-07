/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_FRAME_IMPL_H__
#define __KMPP_FRAME_IMPL_H__

#include "kmpp_frame.h"
#include "legacy_rockit.h"

typedef struct KmppFrameImpl_t {
    const char *name;

    /*
     * dimension parameter for display
     */
    RK_U32 width;
    RK_U32 height;
    RK_U32 hor_stride;
    RK_U32 ver_stride;
    RK_U32 hor_stride_pixel;
    RK_U32 offset_x;
    RK_U32 offset_y;

    /*
     * poc - picture order count
     */
    RK_U32 poc;
    /*
     * pts - display time stamp
     * dts - decode time stamp
     */
    RK_S64 pts;
    RK_S64 dts;

    /*
        * eos - end of stream
        * info_change - set when buffer resized or frame infomation changed
        */
    RK_U32 eos;
    MppFrameColorRange color_range;
    MppFrameColorPrimaries color_primaries;
    MppFrameColorTransferCharacteristic color_trc;

    /**
     * YUV colorspace type.
     * It must be accessed using av_frame_get_colorspace() and
     * av_frame_set_colorspace().
     * - encoding: Set by user
     * - decoding: Set by libavcodec
     */
    MppFrameColorSpace colorspace;
    MppFrameChromaLocation chroma_location;

    MppFrameFormat fmt;

    MppFrameRational sar;

    /*
     * buffer information
     * NOTE: buf_size only access internally
     */
    KmppShmPtr buffer;
    size_t buf_size;
    RK_U32 buf_fd;
    /*
     * frame buffer compression (FBC) information
     *
     * NOTE: some constraint on fbc data
     * 1. FBC config need two addresses but only one buffer.
     *    The second address should be represented by base + offset form.
     * 2. FBC has header address and payload address
     *    Both addresses should be 4K aligned.
     * 3. The header section size is default defined by:
     *    header size = aligned(aligned(width, 16) * aligned(height, 16) / 16, 4096)
     * 4. The stride in header section is defined by:
     *    stride = aligned(width, 16)
     */
    RK_U32 fbc_offset;
    RK_U32 is_gray;

    KmppShmPtr meta;
} KmppFrameImpl;

#endif /* __KMPP_FRAME_IMPL_H__ */

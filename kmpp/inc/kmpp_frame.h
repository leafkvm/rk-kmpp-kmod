/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_FRAME_H__
#define __KMPP_FRAME_H__

#include "kmpi_defs.h"

/*
 * bit definition for mode flag in KmppFrame
 */
/* progressive frame */
#define MPP_FRAME_FLAG_FRAME            (0x00000000)
/* top field only */
#define MPP_FRAME_FLAG_TOP_FIELD        (0x00000001)
/* bottom field only */
#define MPP_FRAME_FLAG_BOT_FIELD        (0x00000002)
/* paired field */
#define MPP_FRAME_FLAG_PAIRED_FIELD     (MPP_FRAME_FLAG_TOP_FIELD|MPP_FRAME_FLAG_BOT_FIELD)
/* paired field with field order of top first */
#define MPP_FRAME_FLAG_TOP_FIRST        (0x00000004)
/* paired field with field order of bottom first */
#define MPP_FRAME_FLAG_BOT_FIRST        (0x00000008)
/* paired field with unknown field order (MBAFF) */
#define MPP_FRAME_FLAG_DEINTERLACED     (MPP_FRAME_FLAG_TOP_FIRST|MPP_FRAME_FLAG_BOT_FIRST)
#define MPP_FRAME_FLAG_FIELD_ORDER_MASK (0x0000000C)
// for multiview stream
#define MPP_FRAME_FLAG_VIEW_ID_MASK     (0x000000f0)

#define MPP_FRAME_FLAG_IEP_DEI_MASK     (0x00000f00)
#define MPP_FRAME_FLAG_IEP_DEI_I2O1     (0x00000100)
#define MPP_FRAME_FLAG_IEP_DEI_I4O2     (0x00000200)
#define MPP_FRAME_FLAG_IEP_DEI_I4O1     (0x00000300)

/*
 * MPEG vs JPEG YUV range.
 */
typedef enum {
    MPP_FRAME_RANGE_UNSPECIFIED = 0,
    MPP_FRAME_RANGE_MPEG        = 1,    ///< the normal 219*2^(n-8) "MPEG" YUV ranges
    MPP_FRAME_RANGE_JPEG        = 2,    ///< the normal     2^n-1   "JPEG" YUV ranges
    MPP_FRAME_RANGE_NB,                 ///< Not part of ABI
} MppFrameColorRange;

typedef enum {
    MPP_FRAME_CHROMA_DOWN_SAMPLE_MODE_NONE,
    MPP_FRAME_CHORMA_DOWN_SAMPLE_MODE_AVERAGE,
    MPP_FRAME_CHORMA_DOWN_SAMPLE_MODE_DISCARD,
} MppFrameChromaDownSampleMode;

typedef enum {
    MPP_FRAME_VIDEO_FMT_COMPONEMT   = 0,
    MPP_FRAME_VIDEO_FMT_PAL         = 1,
    MPP_FRAME_VIDEO_FMT_NTSC        = 2,
    MPP_FRAME_VIDEO_FMT_SECAM       = 3,
    MPP_FRAME_VIDEO_FMT_MAC         = 4,
    MPP_FRAME_VIDEO_FMT_UNSPECIFIED = 5,
    MPP_FRAME_VIDEO_FMT_RESERVED0   = 6,
    MPP_FRAME_VIDEO_FMT_RESERVED1   = 7,
} MppFrameVideoFormat;

/*
 * Chromaticity coordinates of the source primaries.
 */
typedef enum {
    MPP_FRAME_PRI_RESERVED0     = 0,
    MPP_FRAME_PRI_BT709         = 1,    ///< also ITU-R BT1361 / IEC 61966-2-4 / SMPTE RP177 Annex B
    MPP_FRAME_PRI_UNSPECIFIED   = 2,
    MPP_FRAME_PRI_RESERVED      = 3,
    MPP_FRAME_PRI_BT470M        = 4,    ///< also FCC Title 47 Code of Federal Regulations 73.682 (a)(20)

    MPP_FRAME_PRI_BT470BG       = 5,    ///< also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM
    MPP_FRAME_PRI_SMPTE170M     = 6,    ///< also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC
    MPP_FRAME_PRI_SMPTE240M     = 7,    ///< functionally identical to above
    MPP_FRAME_PRI_FILM          = 8,    ///< colour filters using Illuminant C
    MPP_FRAME_PRI_BT2020        = 9,    ///< ITU-R BT2020
    MPP_FRAME_PRI_SMPTEST428_1  = 10,   ///< SMPTE ST 428-1 (CIE 1931 XYZ)
    MPP_FRAME_PRI_SMPTE431      = 11,   ///< SMPTE ST 431-2 (2011) / DCI P3
    MPP_FRAME_PRI_SMPTE432      = 12,   ///< SMPTE ST 432-1 (2010) / P3 D65 / Display P3
    MPP_FRAME_PRI_JEDEC_P22     = 22,   ///< JEDEC P22 phosphors
    MPP_FRAME_PRI_NB,                   ///< Not part of ABI
} MppFrameColorPrimaries;

/*
 * Color Transfer Characteristic.
 */
typedef enum {
    MPP_FRAME_TRC_RESERVED0    = 0,
    MPP_FRAME_TRC_BT709        = 1,     ///< also ITU-R BT1361
    MPP_FRAME_TRC_UNSPECIFIED  = 2,
    MPP_FRAME_TRC_RESERVED     = 3,
    MPP_FRAME_TRC_GAMMA22      = 4,     ///< also ITU-R BT470M / ITU-R BT1700 625 PAL & SECAM
    MPP_FRAME_TRC_GAMMA28      = 5,     ///< also ITU-R BT470BG
    MPP_FRAME_TRC_SMPTE170M    = 6,     ///< also ITU-R BT601-6 525 or 625 / ITU-R BT1358 525 or 625 / ITU-R BT1700 NTSC
    MPP_FRAME_TRC_SMPTE240M    = 7,
    MPP_FRAME_TRC_LINEAR       = 8,     ///< "Linear transfer characteristics"
    MPP_FRAME_TRC_LOG          = 9,     ///< "Logarithmic transfer characteristic (100:1 range)"
    MPP_FRAME_TRC_LOG_SQRT     = 10,    ///< "Logarithmic transfer characteristic (100 * Sqrt(10) : 1 range)"
    MPP_FRAME_TRC_IEC61966_2_4 = 11,    ///< IEC 61966-2-4
    MPP_FRAME_TRC_BT1361_ECG   = 12,    ///< ITU-R BT1361 Extended Colour Gamut
    MPP_FRAME_TRC_IEC61966_2_1 = 13,    ///< IEC 61966-2-1 (sRGB or sYCC)
    MPP_FRAME_TRC_BT2020_10    = 14,    ///< ITU-R BT2020 for 10 bit system
    MPP_FRAME_TRC_BT2020_12    = 15,    ///< ITU-R BT2020 for 12 bit system
    MPP_FRAME_TRC_SMPTEST2084  = 16,    ///< SMPTE ST 2084 for 10-, 12-, 14- and 16-bit systems
    MPP_FRAME_TRC_SMPTEST428_1 = 17,    ///< SMPTE ST 428-1
    MPP_FRAME_TRC_ARIB_STD_B67 = 18,    ///< ARIB STD-B67, known as "Hybrid log-gamma"
    MPP_FRAME_TRC_NB,                   ///< Not part of ABI
} MppFrameColorTransferCharacteristic;

/*
 * YUV colorspace type.
 */
typedef enum {
    MPP_FRAME_SPC_RGB         = 0,      ///< order of coefficients is actually GBR, also IEC 61966-2-1 (sRGB)
    MPP_FRAME_SPC_BT709       = 1,      ///< also ITU-R BT1361 / IEC 61966-2-4 xvYCC709 / SMPTE RP177 Annex B
    MPP_FRAME_SPC_UNSPECIFIED = 2,
    MPP_FRAME_SPC_RESERVED    = 3,
    MPP_FRAME_SPC_FCC         = 4,      ///< FCC Title 47 Code of Federal Regulations 73.682 (a)(20)
    MPP_FRAME_SPC_BT470BG     = 5,      ///< also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM / IEC 61966-2-4 xvYCC601
    MPP_FRAME_SPC_SMPTE170M   = 6,      ///< also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC / functionally identical to above
    MPP_FRAME_SPC_SMPTE240M   = 7,
    MPP_FRAME_SPC_YCOCG       = 8,      ///< Used by Dirac / VC-2 and H.264 FRext, see ITU-T SG16
    MPP_FRAME_SPC_BT2020_NCL  = 9,      ///< ITU-R BT2020 non-constant luminance system
    MPP_FRAME_SPC_BT2020_CL   = 10,     ///< ITU-R BT2020 constant luminance system
    MPP_FRAME_SPC_SMPTE2085   = 11,     ///< SMPTE 2085, Y'D'zD'x
    MPP_FRAME_SPC_CHROMA_DERIVED_NCL = 12,  ///< Chromaticity-derived non-constant luminance system
    MPP_FRAME_SPC_CHROMA_DERIVED_CL = 13,   ///< Chromaticity-derived constant luminance system
    MPP_FRAME_SPC_ICTCP       = 14,     ///< ITU-R BT.2100-0, ICtCp
    MPP_FRAME_SPC_NB,                   ///< Not part of ABI
} MppFrameColorSpace;

/*
 * Location of chroma samples.
 *
 * Illustration showing the location of the first (top left) chroma sample of the
 * image, the left shows only luma, the right
 * shows the location of the chroma sample, the 2 could be imagined to overlay
 * each other but are drawn separately due to limitations of ASCII
 *
 *                1st 2nd       1st 2nd horizontal luma sample positions
 *                 v   v         v   v
 *                 ______        ______
 *1st luma line > |X   X ...    |3 4 X ...     X are luma samples,
 *                |             |1 2           1-6 are possible chroma positions
 *2nd luma line > |X   X ...    |5 6 X ...     0 is undefined/unknown position
 */
typedef enum {
    MPP_CHROMA_LOC_UNSPECIFIED = 0,
    MPP_CHROMA_LOC_LEFT        = 1,     ///< mpeg2/4 4:2:0, h264 default for 4:2:0
    MPP_CHROMA_LOC_CENTER      = 2,     ///< mpeg1 4:2:0, jpeg 4:2:0, h263 4:2:0
    MPP_CHROMA_LOC_TOPLEFT     = 3,     ///< ITU-R 601, SMPTE 274M 296M S314M(DV 4:1:1), mpeg2 4:2:2
    MPP_CHROMA_LOC_TOP         = 4,
    MPP_CHROMA_LOC_BOTTOMLEFT  = 5,
    MPP_CHROMA_LOC_BOTTOM      = 6,
    MPP_CHROMA_LOC_NB,                  ///< Not part of ABI
} MppFrameChromaLocation;

typedef enum {
    MPP_CHROMA_UNSPECIFIED,
    MPP_CHROMA_400,
    MPP_CHROMA_410,
    MPP_CHROMA_411,
    MPP_CHROMA_420,
    MPP_CHROMA_422,
    MPP_CHROMA_440,
    MPP_CHROMA_444,
} MppFrameChromaFormat;

#define MPP_FRAME_FMT_MASK          (0x000fffff)

#define MPP_FRAME_FMT_COLOR_MASK    (0x000f0000)
#define MPP_FRAME_FMT_YUV           (0x00000000)
#define MPP_FRAME_FMT_RGB           (0x00010000)

#define MPP_FRAME_FBC_MASK          (0x00f00000)
#define MPP_FRAME_FBC_NONE          (0x00000000)

#define MPP_FRAME_TILE_FLAG         (0x02000000)
/*
 * AFBC_V1 is for ISP output.
 * It has default payload offset to be calculated * from width and height:
 * Payload offset = MPP_ALIGN(MPP_ALIGN(width, 16) * MPP_ALIGN(height, 16) / 16, SZ_4K)
 */
#define MPP_FRAME_FBC_AFBC_V1       (0x00100000)
/*
 * AFBC_V2 is for video decoder output.
 * It stores payload offset in first 32-bit in header address
 * Payload offset is always set to zero.
 */
#define MPP_FRAME_FBC_AFBC_V2       (0x00200000)
/*
 * RKFBC is for video decoder output.
 * It has default header stride to be calculated * from width and height:
 * Header stride = MPP_ALIGN(width, 64) / 64 * 16
 */
#define MPP_FRAME_FBC_RKFBC         (0x00400000)

#define MPP_FRAME_FMT_LE_MASK       (0x01000000)

#define MPP_FRAME_FMT_IS_YUV(fmt)   (((fmt & MPP_FRAME_FMT_COLOR_MASK) == MPP_FRAME_FMT_YUV) && \
                                     ((fmt & MPP_FRAME_FMT_MASK) < MPP_FMT_YUV_BUTT))
#define MPP_FRAME_FMT_IS_RGB(fmt)   (((fmt & MPP_FRAME_FMT_COLOR_MASK) == MPP_FRAME_FMT_RGB) && \
                                     ((fmt & MPP_FRAME_FMT_MASK) < MPP_FMT_RGB_BUTT))

/*
 * For MPP_FRAME_FBC_AFBC_V1 the 16byte aligned stride is used.
 */
#define MPP_FRAME_FMT_IS_FBC(fmt)   (fmt & MPP_FRAME_FBC_MASK)

#define MPP_FRAME_FMT_IS_LE(fmt)    ((fmt & MPP_FRAME_FMT_LE_MASK) == MPP_FRAME_FMT_LE_MASK)
#define MPP_FRAME_FMT_IS_BE(fmt)    ((fmt & MPP_FRAME_FMT_LE_MASK) == 0)

#define MPP_FRAME_FMT_IS_TILE(fmt)  (fmt & MPP_FRAME_TILE_FLAG)

/* mpp color format index definition */
typedef enum {
    MPP_FMT_YUV420SP        = (MPP_FRAME_FMT_YUV + 0),  /* YYYY... UV... (NV12)     */
    /*
        * A rockchip specific pixel format, without gap between pixel aganist
        * the P010_10LE/P010_10BE
        */
    MPP_FMT_YUV420SP_10BIT  = (MPP_FRAME_FMT_YUV + 1),
    MPP_FMT_YUV422SP        = (MPP_FRAME_FMT_YUV + 2),  /* YYYY... UVUV... (NV16)   */
    MPP_FMT_YUV422SP_10BIT  = (MPP_FRAME_FMT_YUV + 3),  ///< Not part of ABI
    MPP_FMT_YUV420P         = (MPP_FRAME_FMT_YUV + 4),  /* YYYY... U...V...  (I420) */
    MPP_FMT_YUV420SP_VU     = (MPP_FRAME_FMT_YUV + 5),  /* YYYY... VUVUVU... (NV21) */
    MPP_FMT_YUV422P         = (MPP_FRAME_FMT_YUV + 6),  /* YYYY... UU...VV...(422P) */
    MPP_FMT_YUV422SP_VU     = (MPP_FRAME_FMT_YUV + 7),  /* YYYY... VUVUVU... (NV61) */
    MPP_FMT_YUV422_YUYV     = (MPP_FRAME_FMT_YUV + 8),  /* YUYVYUYV... (YUY2)       */
    MPP_FMT_YUV422_YVYU     = (MPP_FRAME_FMT_YUV + 9),  /* YVYUYVYU... (YVY2)       */
    MPP_FMT_YUV422_UYVY     = (MPP_FRAME_FMT_YUV + 10), /* UYVYUYVY... (UYVY)       */
    MPP_FMT_YUV422_VYUY     = (MPP_FRAME_FMT_YUV + 11), /* VYUYVYUY... (VYUY)       */
    MPP_FMT_YUV400          = (MPP_FRAME_FMT_YUV + 12), /* YYYY...                  */
    MPP_FMT_YUV440SP        = (MPP_FRAME_FMT_YUV + 13), /* YYYY... UVUV...          */
    MPP_FMT_YUV411SP        = (MPP_FRAME_FMT_YUV + 14), /* YYYY... UV...            */
    MPP_FMT_YUV444SP        = (MPP_FRAME_FMT_YUV + 15), /* YYYY... UVUVUVUV...      */
    MPP_FMT_YUV444P         = (MPP_FRAME_FMT_YUV + 16), /* YYYY... UVUVUVUV...      */
    MPP_FMT_YUV444SP_10BIT  = (MPP_FRAME_FMT_YUV + 17),
    MPP_FMT_AYUV2BPP        = (MPP_FRAME_FMT_YUV + 18),
    MPP_FMT_AYUV1BPP        = (MPP_FRAME_FMT_YUV + 19),
    MPP_FMT_YUV_BUTT,

    MPP_FMT_RGB565          = (MPP_FRAME_FMT_RGB + 0),  /* 16-bit RGB               */
    MPP_FMT_BGR565          = (MPP_FRAME_FMT_RGB + 1),  /* 16-bit RGB               */
    MPP_FMT_RGB555          = (MPP_FRAME_FMT_RGB + 2),  /* 15-bit RGB               */
    MPP_FMT_BGR555          = (MPP_FRAME_FMT_RGB + 3),  /* 15-bit RGB               */
    MPP_FMT_RGB444          = (MPP_FRAME_FMT_RGB + 4),  /* 12-bit RGB               */
    MPP_FMT_BGR444          = (MPP_FRAME_FMT_RGB + 5),  /* 12-bit RGB               */
    MPP_FMT_RGB888          = (MPP_FRAME_FMT_RGB + 6),  /* 24-bit RGB               */
    MPP_FMT_BGR888          = (MPP_FRAME_FMT_RGB + 7),  /* 24-bit RGB               */
    MPP_FMT_RGB101010       = (MPP_FRAME_FMT_RGB + 8),  /* 30-bit RGB               */
    MPP_FMT_BGR101010       = (MPP_FRAME_FMT_RGB + 9),  /* 30-bit RGB               */
    MPP_FMT_ARGB8888        = (MPP_FRAME_FMT_RGB + 10), /* 32-bit RGB               */
    MPP_FMT_ABGR8888        = (MPP_FRAME_FMT_RGB + 11), /* 32-bit RGB               */
    MPP_FMT_BGRA8888        = (MPP_FRAME_FMT_RGB + 12), /* 32-bit RGB               */
    MPP_FMT_RGBA8888        = (MPP_FRAME_FMT_RGB + 13), /* 32-bit RGB               */
    MPP_FMT_ARGB4444        = (MPP_FRAME_FMT_RGB + 14), /* 16-bit RGB               */
    MPP_FMT_ARGB1555        = (MPP_FRAME_FMT_RGB + 15), /* 2-bit RGB               */
    MPP_FMT_RGB_BUTT,

    MPP_FMT_BUTT,
} MppFrameFormat;

/**
 * Rational number (pair of numerator and denominator).
 */
typedef struct MppFrameRational {
    rk_s32 num; ///< Numerator
    rk_s32 den; ///< Denominator
} MppFrameRational;

typedef struct MppFrameMasteringDisplayMetadata {
    rk_u16 display_primaries[3][2];
    rk_u16 white_point[2];
    rk_u32 max_luminance;
    rk_u32 min_luminance;
} MppFrameMasteringDisplayMetadata;

typedef struct MppFrameContentLightMetadata {
    rk_u16 MaxCLL;
    rk_u16 MaxFALL;
} MppFrameContentLightMetadata;

typedef enum {
    MPP_FRAME_ERR_UNKNOW           = 0x0001,
    MPP_FRAME_ERR_UNSUPPORT        = 0x0002,
} MPP_FRAME_ERR;

typedef void* KmppRoi;
typedef void* KmppOsd;
typedef void* KmppPpInfo;

#define KMPP_FRAME_ENTRY_TABLE(prefix, ENTRY, STRCT, EHOOK, SHOOK, ALIAS) \
    ENTRY(prefix, u32,  rk_u32,             width,              FLAG_NONE,  width) \
    ENTRY(prefix, u32,  rk_u32,             height,             FLAG_NONE,  height) \
    ENTRY(prefix, u32,  rk_u32,             hor_stride,         FLAG_NONE,  hor_stride) \
    ENTRY(prefix, u32,  rk_u32,             ver_stride,         FLAG_NONE,  ver_stride) \
    ENTRY(prefix, u32,  rk_u32,             hor_stride_pixel,   FLAG_NONE,  hor_stride_pixel) \
    ENTRY(prefix, u32,  rk_u32,             offset_x,           FLAG_NONE,  offset_x) \
    ENTRY(prefix, u32,  rk_u32,             offset_y,           FLAG_NONE,  offset_y) \
    ENTRY(prefix, u32,  rk_u32,             poc,                FLAG_NONE,  poc) \
    ENTRY(prefix, s64,  rk_s64,             pts,                FLAG_NONE,  pts) \
    ENTRY(prefix, s64,  rk_s64,             dts,                FLAG_NONE,  dts) \
    ENTRY(prefix, u32,  rk_u32,             eos,                FLAG_NONE,  eos) \
    ENTRY(prefix, u32,  rk_u32,             color_range,        FLAG_NONE,  color_range) \
    ENTRY(prefix, u32,  rk_u32,             color_primaries,    FLAG_NONE,  color_primaries) \
    ENTRY(prefix, u32,  rk_u32,             color_trc,          FLAG_NONE,  color_trc) \
    ENTRY(prefix, u32,  rk_u32,             colorspace,         FLAG_NONE,  colorspace) \
    ENTRY(prefix, u32,  rk_u32,             chroma_location,    FLAG_NONE,  chroma_location) \
    ENTRY(prefix, u32,  rk_u32,             fmt,                FLAG_NONE,  fmt) \
    ENTRY(prefix, u32,  rk_u32,             buf_size,           FLAG_NONE,  buf_size) \
    ENTRY(prefix, u32,  rk_u32,             buf_fd,             FLAG_NONE,  buf_fd) \
    ENTRY(prefix, u32,  rk_u32,             is_gray,            FLAG_NONE,  is_gray) \
    STRCT(prefix, shm,  KmppShmPtr,         meta,               FLAG_NONE,  meta) \
    STRCT(prefix, st,   MppFrameRational,   sar,                FLAG_NONE,  sar) \
    SHOOK(prefix, shm,  KmppShmPtr,         buffer,             FLAG_NONE,  buffer)

#define KMPP_OBJ_NAME           kmpp_frame
#define KMPP_OBJ_INTF_TYPE      KmppFrame
#define KMPP_OBJ_ENTRY_TABLE    KMPP_FRAME_ENTRY_TABLE
#include "kmpp_obj_func.h"

#define kmpp_frame_dump_f(frame) kmpp_frame_dump(frame, __FUNCTION__)

rk_s32 kmpp_frame_has_buffer(const KmppFrame frame);
rk_s32 kmpp_frame_has_meta(const KmppFrame frame);
rk_s32 kmpp_frame_copy(KmppFrame dst, KmppFrame src);
rk_s32 kmpp_frame_info_cmp(KmppFrame frame0, KmppFrame frame1);
rk_s32 kmpp_frame_get_fbc_offset(KmppFrame frame, rk_u32 *offset);
rk_s32 kmpp_frame_get_fbc_stride(KmppFrame frame, rk_u32 *stride);

#endif /*__KMPP_FRAME_H__*/

/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_META_H__
#define __KMPP_META_H__

#include "kmpp_sys_defs.h"

#define FOURCC_META(a, b, c, d) ((RK_U32)(a) << 24  | \
                                ((RK_U32)(b) << 16) | \
                                ((RK_U32)(c) << 8)  | \
                                ((RK_U32)(d) << 0))

/*
 * Mpp Metadata definition
 *
 * Metadata is for expandable information transmision in mpp.
 */
typedef enum KmppMetaKey_e {
    /* data flow key */
    KEY_INPUT_FRAME             = FOURCC_META('i', 'f', 'r', 'm'),
    KEY_INPUT_PACKET            = FOURCC_META('i', 'p', 'k', 't'),
    KEY_OUTPUT_FRAME            = FOURCC_META('o', 'f', 'r', 'm'),
    KEY_OUTPUT_PACKET           = FOURCC_META('o', 'p', 'k', 't'),
    /* output motion information for motion detection */
    KEY_MOTION_INFO             = FOURCC_META('m', 'v', 'i', 'f'),
    KEY_HDR_INFO                = FOURCC_META('h', 'd', 'r', ' '),
    KEY_HDR_META_OFFSET         = FOURCC_META('h', 'd', 'r', 'o'),
    KEY_HDR_META_SIZE           = FOURCC_META('h', 'd', 'r', 'l'),

    /* flow control key */
    KEY_INPUT_BLOCK             = FOURCC_META('i', 'b', 'l', 'k'),
    KEY_OUTPUT_BLOCK            = FOURCC_META('o', 'b', 'l', 'k'),
    KEY_INPUT_IDR_REQ           = FOURCC_META('i', 'i', 'd', 'r'),   /* input idr frame request flag */
    KEY_OUTPUT_INTRA            = FOURCC_META('o', 'i', 'd', 'r'),   /* output intra frame indicator */

    /* mpp_frame / mpp_packet meta data info key */
    KEY_TEMPORAL_ID             = FOURCC_META('t', 'l', 'i', 'd'),
    KEY_LONG_REF_IDX            = FOURCC_META('l', 't', 'i', 'd'),
    KEY_ENC_AVERAGE_QP          = FOURCC_META('a', 'v', 'g', 'q'),
    KEY_ENC_START_QP            = FOURCC_META('s', 't', 'r', 'q'),
    KEY_ENC_BPS_RT              = FOURCC_META('r', 't', 'b', 'r'),   /* realtime bps */
    KEY_ROI_DATA                = FOURCC_META('r', 'o', 'i', ' '),
    KEY_JPEG_ROI_DATA           = FOURCC_META('j', 'r', 'o', 'i'),
    KEY_OSD_DATA                = FOURCC_META('o', 's', 'd', ' '),
    KEY_OSD_DATA2               = FOURCC_META('o', 's', 'd', '2'),
    KEY_OSD_DATA3               = FOURCC_META('o', 's', 'd', '3'),
    KEY_USER_DATA               = FOURCC_META('u', 's', 'r', 'd'),
    KEY_USER_DATAS              = FOURCC_META('u', 'r', 'd', 's'),

    /* num of inter different size predicted block */
    KEY_LVL64_INTER_NUM         = FOURCC_META('l', '6', '4', 'p'),
    KEY_LVL32_INTER_NUM         = FOURCC_META('l', '3', '2', 'p'),
    KEY_LVL16_INTER_NUM         = FOURCC_META('l', '1', '6', 'p'),
    KEY_LVL8_INTER_NUM          = FOURCC_META('l', '8', 'p', ' '),
    /* num of intra different size predicted block */
    KEY_LVL32_INTRA_NUM         = FOURCC_META('l', '3', '2', 'i'),
    KEY_LVL16_INTRA_NUM         = FOURCC_META('l', '1', '6', 'i'),
    KEY_LVL8_INTRA_NUM          = FOURCC_META('l', '8', 'i', ' '),
    KEY_LVL4_INTRA_NUM          = FOURCC_META('l', '4', 'i', ' '),
    /* output P skip frame indicator */
    KEY_OUTPUT_PSKIP            = FOURCC_META('o', 'p', 's', 'p'),
    /* input P skip frame request */
    KEY_INPUT_PSKIP             = FOURCC_META('i', 'p', 's', 'p'),
    KEY_INPUT_PSKIP_NUM         = FOURCC_META('i', 'p', 's', 'n'),
    KEY_ENC_SSE                 = FOURCC_META('e', 's', 's', 'e'),

    /*
     * For vepu580 roi buffer config mode
     * The encoder roi structure is so complex that we should provide a buffer
     * tunnel for externl user to config encoder hardware by direct sending
     * roi data buffer.
     * This way can reduce the config parsing and roi buffer data generating
     * overhead in mpp.
     */
    KEY_ROI_DATA2               = FOURCC_META('r', 'o', 'i', '2'),

    /*
     * qpmap for rv1109/1126 encoder qpmap config
     * Input data is a MppBuffer which contains an array of 16bit Vepu541RoiCfg.
     * And each 16bit represents a 16x16 block qp info.
     *
     * H.264 - 16x16 block qp is arranged in raster order:
     * each value is a 16bit data
     * 00 01 02 03 04 05 06 07 -> 00 01 02 03 04 05 06 07
     * 10 11 12 13 14 15 16 17    10 11 12 13 14 15 16 17
     * 20 21 22 23 24 25 26 27    20 21 22 23 24 25 26 27
     * 30 31 32 33 34 35 36 37    30 31 32 33 34 35 36 37
     *
     * H.265 - 16x16 block qp is reorder to 64x64/32x32 ctu order then 64x64 / 32x32 ctu raster order
     * 64x64 ctu
     * 00 01 02 03 04 05 06 07 -> 00 01 02 03 10 11 12 13 20 21 22 23 30 31 32 33 04 05 06 07 14 15 16 17 24 25 26 27 34 35 36 37
     * 10 11 12 13 14 15 16 17
     * 20 21 22 23 24 25 26 27
     * 30 31 32 33 34 35 36 37
     * 32x32 ctu
     * 00 01 02 03 04 05 06 07 -> 00 01 10 11 02 03 12 13 04 05 14 15 06 07 16 17
     * 10 11 12 13 14 15 16 17    20 21 30 31 22 23 32 33 24 25 34 35 26 27 36 37
     * 20 21 22 23 24 25 26 27
     * 30 31 32 33 34 35 36 37
     */
    KEY_QPMAP0                  = FOURCC_META('e', 'q', 'm', '0'),

    /*
     * shared memory buffer for object dectection flag from NPU, for rv1126b
     * Picture width is aligned to 16, each 16x16 block is 8bit data.
     * H.264: 16x16 block is arranged in raster order.
     * H.265: 16x16 block is reordered to 32x32 ctu order then 32x32 ctu raster order
     */
    KEY_NPU_SOBJ_FLAG           = FOURCC_META('n', 'p', 'u', 's'),
    /* userspace object dectection flag from NPU, for rk3588 and rk3576 */
    KEY_NPU_UOBJ_FLAG           = FOURCC_META('n', 'p', 'u', 'u'),
    /* foreground object area from NPU, for rv1126b */
    KEY_NPU_OBJ_FG_AREA         = FOURCC_META('n', 'o', 'f', 'a'),

    /* frame long-term reference frame operation */
    KEY_ENC_MARK_LTR            = FOURCC_META('m', 'l', 't', 'r'),
    KEY_ENC_USE_LTR             = FOURCC_META('u', 'l', 't', 'r'),

    /* MLVEC specified encoder feature  */
    KEY_ENC_FRAME_QP            = FOURCC_META('f', 'r', 'm', 'q'),
    KEY_ENC_BASE_LAYER_PID      = FOURCC_META('b', 'p', 'i', 'd'),

    /* Thumbnail info for decoder output frame */
    KEY_DEC_TBN_EN              = FOURCC_META('t', 'b', 'e', 'n'),
    KEY_DEC_TBN_Y_OFFSET        = FOURCC_META('t', 'b', 'y', 'o'),
    KEY_DEC_TBN_UV_OFFSET       = FOURCC_META('t', 'b', 'c', 'o'),
    /* combo frame */
    KEY_COMBO_FRAME             = FOURCC_META('c', 'f', 'r', 'm'),
    KEY_CHANNEL_ID              = FOURCC_META('c', 'h', 'a', 'n'),

    /* Preprocess (pp) operation metat data */
    /* Motion Detection output buffer */
    KEY_PP_MD_BUF               = FOURCC_META('m', 'd', 'b', 'f'),
    /* Occlusion Detection output buffer */
    KEY_PP_OD_BUF               = FOURCC_META('o', 'd', 'b', 'f'),
    /* pp output data */
    KEY_PP_OUT                  = FOURCC_META('o', 'p', 'p', ' '),

    /* AE parameters from ISP module */
    /* sensor exposure time */
    KEY_EXP_TIME                = FOURCC_META('e', 'x', 'p', 't'),
    /* sensor analog gain */
    KEY_ANALOG_GAIN             = FOURCC_META('s', 'a', 'g', 'n'),
    /* sensor digital gain */
    KEY_DIGITAL_GAIN            = FOURCC_META('s', 'd', 'g', 'n'),
    /* ISP digital gain */
    KEY_ISP_DGAIN               = FOURCC_META('i', 'd', 'g', 'n')
} KmppMetaKey;

#define kmpp_meta_get_f(meta)               kmpp_meta_get(meta, __FUNCTION__)
#define kmpp_meta_get_share_f(meta, file)   kmpp_meta_get_share(meta, file, __FUNCTION__)
#define kmpp_meta_put_f(meta)               kmpp_meta_put(meta, __FUNCTION__)

rk_s32 kmpp_meta_get(KmppMeta *meta, const rk_u8 *caller);
rk_s32 kmpp_meta_get_share(KmppMeta *meta, KmppFsDev *file, const rk_u8 *caller);
rk_s32 kmpp_meta_put(KmppMeta meta, const rk_u8 *caller);
rk_s32 kmpp_meta_size(KmppMeta meta);
rk_s32 kmpp_meta_dump(KmppMeta meta);

rk_s32 kmpp_meta_set_s32(KmppMeta meta, KmppMetaKey key, rk_s32 val);
rk_s32 kmpp_meta_set_s64(KmppMeta meta, KmppMetaKey key, rk_s64 val);
rk_s32 kmpp_meta_set_ptr(KmppMeta meta, KmppMetaKey key, void  *val);
rk_s32 kmpp_meta_get_s32(KmppMeta meta, KmppMetaKey key, rk_s32 *val);
rk_s32 kmpp_meta_get_s64(KmppMeta meta, KmppMetaKey key, rk_s64 *val);
rk_s32 kmpp_meta_get_ptr(KmppMeta meta, KmppMetaKey key, void  **val);
rk_s32 kmpp_meta_get_s32_d(KmppMeta meta, KmppMetaKey key, rk_s32 *val, rk_s32 def);
rk_s32 kmpp_meta_get_s64_d(KmppMeta meta, KmppMetaKey key, rk_s64 *val, rk_s64 def);
rk_s32 kmpp_meta_get_ptr_d(KmppMeta meta, KmppMetaKey key, void  **val, void *def);

rk_s32 kmpp_meta_set_obj(KmppMeta meta, KmppMetaKey key, KmppObj obj);
rk_s32 kmpp_meta_get_obj(KmppMeta meta, KmppMetaKey key, KmppObj *obj);
rk_s32 kmpp_meta_get_obj_d(KmppMeta meta, KmppMetaKey key, KmppObj *obj, KmppObj def);
rk_s32 kmpp_meta_set_shm(KmppMeta meta, KmppMetaKey key, KmppShmPtr *sptr);
rk_s32 kmpp_meta_get_shm(KmppMeta meta, KmppMetaKey key, KmppShmPtr *sptr);
rk_s32 kmpp_meta_get_shm_d(KmppMeta meta, KmppMetaKey key, KmppShmPtr *sptr, KmppShmPtr *def);

#endif /*__KMPP_META_H__*/

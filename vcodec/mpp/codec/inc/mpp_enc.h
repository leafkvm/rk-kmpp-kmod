/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#ifndef __MPP_ENC_H__
#define __MPP_ENC_H__

#include "rk_err_def.h"
#include "rk_mpi_cmd.h"

#include "kmpp_env.h"

#include "kmpp_venc_objs.h"
#include "kmpp_venc_objs_impl.h"

/*
 * Configure of encoder is separated into four parts.
 *
 * 1. Rate control parameter
 *    This is quality and bitrate request from user.
 *    For controller only
 *
 * 2. Data source KmppFrame parameter
 *    This is data source buffer information.
 *    For both controller and hal
 *
 * 3. Video codec infomation
 *    This is user custormized stream information.
 *    For hal only
 *
 * 4. Extra parameter
 *    including:
 *    PreP  : encoder Preprocess configuration
 *    ROI   : Region Of Interest
 *    OSD   : On Screen Display
 *    MD    : Motion Detection
 *    extra : SEI for h.264 / Exif for mjpeg
 *    For hal only
 *
 * The module transcation flow is as follows:
 *
 *                 +                      +
 *     User        |      Mpi/Mpp         |         EncImpl
 *                 |                      |            Hal
 *                 |                      |
 * +----------+    |    +---------+       |       +------------+
 * |          |    |    |         +-----RcCfg----->            |
 * |  RcCfg   +--------->         |       |       | EncImpl |
 * |          |    |    |         |   +-Frame----->            |
 * +----------+    |    |         |   |   |       +---+-----^--+
 *                 |    |         |   |   |           |     |
 *                 |    |         |   |   |           |     |
 * +----------+    |    |         |   |   |        syntax   |
 * |          |    |    |         |   |   |           |     |
 * | KmppFrame+--------->  MppEnc +---+   |           |   result
 * |          |    |    |         |   |   |           |     |
 * +----------+    |    |         |   |   |           |     |
 *                 |    |         |   |   |       +---v-----+--+
 *                 |    |         |   +-Frame----->            |
 * +----------+    |    |         |       |       |            |
 * |          |    |    |         +---CodecCfg---->    Hal     |
 * | CodecCfg +--------->         |       |       |            |
 * |          |    |    |         <-----Extra----->            |
 * +----------+    |    +---------+       |       +------------+
 *                 |                      |
 *                 |                      |
 *                 +                      +
 *
 * The function call flow is shown below:
 *
 *  mpi                      mpp_enc         controller                  hal
 *   +                          +                 +                       +
 *   |                          |                 |                       |
 *   |                          |                 |                       |
 *   +----------init------------>                 |                       |
 *   |                          |                 |                       |
 *   |                          |                 |                       |
 *   |         PrepCfg          |                 |                       |
 *   +---------control---------->     PrepCfg     |                       |
 *   |                          +-----control----->                       |
 *   |                          |                 |        PrepCfg        |
 *   |                          +--------------------------control-------->
 *   |                          |                 |                    allocate
 *   |                          |                 |                     buffer
 *   |                          |                 |                       |
 *   |          RcCfg           |                 |                       |
 *   +---------control---------->      RcCfg      |                       |
 *   |                          +-----control----->                       |
 *   |                          |              rc_init                    |
 *   |                          |                 |                       |
 *   |                          |                 |                       |
 *   |         CodecCfg         |                 |                       |
 *   +---------control---------->                 |        CodecCfg       |
 *   |                          +--------------------------control-------->
 *   |                          |                 |                    generate
 *   |                          |                 |                    sps/pps
 *   |                          |                 |     Get extra info    |
 *   |                          +--------------------------control-------->
 *   |      Get extra info      |                 |                       |
 *   +---------control---------->                 |                       |
 *   |                          |                 |                       |
 *   |                          |                 |                       |
 *   |         ROICfg           |                 |                       |
 *   +---------control---------->                 |        ROICfg         |
 *   |                          +--------------------------control-------->
 *   |                          |                 |                       |
 *   |         OSDCfg           |                 |                       |
 *   +---------control---------->                 |        OSDCfg         |
 *   |                          +--------------------------control-------->
 *   |                          |                 |                       |
 *   |          MDCfg           |                 |                       |
 *   +---------control---------->                 |         MDCfg         |
 *   |                          +--------------------------control-------->
 *   |                          |                 |                       |
 *   |      Set extra info      |                 |                       |
 *   +---------control---------->                 |     Set extra info    |
 *   |                          +--------------------------control-------->
 *   |                          |                 |                       |
 *   |           task           |                 |                       |
 *   +----------encode---------->      task       |                       |
 *   |                          +-----encode------>                       |
 *   |                          |              encode                     |
 *   |                          |                 |        syntax         |
 *   |                          +--------------------------gen_reg-------->
 *   |                          |                 |                       |
 *   |                          |                 |                       |
 *   |                          +---------------------------start--------->
 *   |                          |                 |                       |
 *   |                          |                 |                       |
 *   |                          +---------------------------wait---------->
 *   |                          |                 |                       |
 *   |                          |    callback     |                       |
 *   |                          +----------------->                       |
 *   +--OSD-MD--encode---------->                 |                       |
 *   |             .            |                 |                       |
 *   |             .            |                 |                       |
 *   |             .            |                 |                       |
 *   +--OSD-MD--encode---------->                 |                       |
 *   |                          |                 |                       |
 *   +----------deinit---------->                 |                       |
 *   +                          +                 +                       +
 */

typedef void *MppEnc;

typedef struct MppEncInitCfg_t {
    MppCodingType   coding;
    RK_S32          online;
    RK_U32          buf_size;
    RK_U32          max_strm_cnt;
    RK_U32          ref_buf_shared;
    RK_U32          smart_en;
    RK_U32          max_width;
    RK_U32          max_height;
    struct hal_shared_buf *shared_buf;
    RK_U32          qpmap_en;
    RK_S32          chan_id;
    RK_U32          only_smartp;
    RK_U32          tmvp_enable;
    RK_U32          ntfy_mode;
    RK_S32          input_timeout;
    RK_S32          output_timeout;
} MppEncInitCfg;

#ifdef __cplusplus
extern "C" {
#endif

MPP_RET mpp_enc_init(MppEnc * ctx, KmppVencInitCfgImpl *cfg);
MPP_RET mpp_enc_deinit(MppEnc ctx);

MPP_RET mpp_enc_start(MppEnc ctx);
MPP_RET mpp_enc_stop(MppEnc ctx);
MPP_RET mpp_enc_run_task(MppEnc ctx, RK_S64 pts, RK_S64 dts);
MPP_RET mpp_enc_online_task_failed(MppEnc ctx);
RK_S32 mpp_enc_check_hw_running(MppEnc ctx);
RK_S32 mpp_enc_unbind_jpeg_task(MppEnc ctx);
rk_bool mpp_enc_check_is_int_process(MppEnc ctx);
MPP_RET mpp_enc_set_online_mode(MppEnc ctx, RK_U32 mode);

MPP_RET mpp_enc_control(MppEnc ctx, MpiCmd cmd, void *param);
MPP_RET mpp_enc_notify(MppEnc ctx, RK_U32 flag);
MPP_RET mpp_enc_reset(MppEnc ctx);
MPP_RET mpp_enc_oneframe(MppEnc ctx, KmppFrame frame, KmppPacket *packet);
MPP_RET mpp_enc_cfg_reg(MppEnc ctx, KmppFrame frame);
MPP_RET mpp_enc_hw_start(MppEnc ctx, MppEnc jpeg_ctx);

MPP_RET mpp_enc_int_process(MppEnc ctx, MppEnc jpeg_ctx, KmppPacket *packet,
                            KmppPacket *jpeg_packet);
void mpp_enc_slice_info(MppEnc ctx, void *param, KmppPacket *packet);
MPP_RET mpp_enc_register_chl(MppEnc ctx, void *func, RK_S32 chan_id);
void    mpp_enc_proc_debug(KmppEnvNode node, MppEnc ctx, RK_U32 chl_id);
void mpp_enc_deinit_frame(MppEnc ctx);
void mpp_enc_pkt_full_inc(MppEnc ctx);
MPP_RET mpp_enc_force_pskip(MppEnc ctx, KmppFrame frame, KmppPacket *packet);
RK_S32 mpp_enc_get_fps_out(MppEnc ctx);
KmppVencNtfy mpp_enc_get_notify(MppEnc ctx);

#ifdef __cplusplus
}
#endif

#endif /*__MPP_ENC_H__*/

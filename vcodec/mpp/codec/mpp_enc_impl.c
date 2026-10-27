/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "mpp_enc"

#include "kmpp_string.h"
#include "kmpp_uaccess.h"
#include "kmpp_macro.h"

#include "kmpp_time.h"
#include "mpp_maths.h"
#include "mpp_mem.h"
#include "mpp_buffer_impl.h"
#include "mpp_service.h"

#include "kmpp_frame.h"
#include "kmpp_packet_impl.h"
#include "mpp_enc_ref.h"
#include "mpp_enc_refs_setup.h"
#include "mpp_enc_debug.h"
#include "mpp_enc_impl.h"
#include "mpp_2str.h"
#include "mpp_enc.h"

#include "kmpp_obj.h"
#include "kmpp_shm.h"
#include "kmpp_buffer.h"
#include "kmpp_venc_objs_impl.h"
#include "kmpp_meta.h"

#include "h264_syntax.h"
#include "jpege_syntax.h"

typedef union EncTaskWait_u {
    RK_U32 val;
    struct {
        RK_U32 enc_frm_in : 1; // 0x0001 MPP_ENC_NOTIFY_FRAME_ENQUEUE
        RK_U32 reserv0002 : 1; // 0x0002
        RK_U32 reserv0004 : 1; // 0x0004
        RK_U32 enc_pkt_out : 1; // 0x0008 MPP_ENC_NOTIFY_PACKET_ENQUEUE

        RK_U32 reserv0010 : 1; // 0x0010
        RK_U32 reserv0020 : 1; // 0x0020
        RK_U32 reserv0040 : 1; // 0x0040
        RK_U32 reserv0080 : 1; // 0x0080

        RK_U32 reserv0100 : 1; // 0x0100
        RK_U32 reserv0200 : 1; // 0x0200
        RK_U32 reserv0400 : 1; // 0x0400
        RK_U32 reserv0800 : 1; // 0x0800

        RK_U32 reserv1000 : 1; // 0x1000
        RK_U32 reserv2000 : 1; // 0x2000
        RK_U32 reserv4000 : 1; // 0x4000
        RK_U32 reserv8000 : 1; // 0x8000
    };
} EncTaskWait;

/* encoder internal work flow */
typedef union EncTaskStatus_u {
    RK_U32 val;
    struct {
        RK_U32 task_in_rdy : 1;
        RK_U32 task_out_rdy : 1;

        RK_U32 frm_pkt_rdy : 1;

        RK_U32 hal_task_reset_rdy : 1; // reset hal task to start
        RK_U32 rc_check_frm_drop : 1; // rc  stage
        RK_U32 pkt_buf_rdy : 1; // prepare pkt buf

        RK_U32 enc_start : 1; // enc stage
        RK_U32 refs_force_update : 1; // enc stage
        RK_U32 low_delay_again : 1; // enc stage low delay output again

        RK_U32 enc_backup : 1; // enc stage
        RK_U32 enc_restore : 1; // reenc flow start point
        RK_U32 enc_proc_dpb : 1; // enc stage
        RK_U32 rc_frm_start : 1; // rc  stage
        RK_U32 check_type_reenc : 1; // flow checkpoint if reenc -> enc_restore
        RK_U32 enc_proc_hal : 1; // enc stage
        RK_U32 hal_get_task : 1; // hal stage
        RK_U32 rc_hal_start : 1; // rc  stage
        RK_U32 hal_gen_reg : 1; // hal stage
        RK_U32 hal_start : 1; // hal stage
        RK_U32 hal_wait : 1; // hal stage NOTE: special in low delay mode
        RK_U32 rc_hal_end : 1; // rc  stage
        RK_U32 hal_ret_task : 1; // hal stage
        RK_U32 enc_update_hal : 1; // enc stage
        RK_U32 rc_frm_end : 1; // rc  stage
        RK_U32 rc_reenc : 1; // need reenc
    };
} EncTaskStatus;

typedef struct EncTask_t {
    HalTaskHnd hnd;

    RK_S32 seq_idx;
    EncTaskStatus status;
    EncTaskWait wait;
    HalTaskInfo info;
} EncTask;

static RK_U8 uuid_version[16] = {
    0x3d, 0x07, 0x6d, 0x45, 0x73, 0x0f, 0x41, 0xa8,
    0xb1, 0xc4, 0x25, 0xd7, 0x97, 0x6b, 0xf1, 0xac,
};

static RK_U8 uuid_rc_cfg[16] = {
    0xd7, 0xdc, 0x03, 0xc3, 0xc5, 0x6f, 0x40, 0xe0,
    0x8e, 0xa9, 0x17, 0x1a, 0xd2, 0xef, 0x5e, 0x23,
};

static RK_U8 uuid_usr_data[16] = {
    0xfe, 0x39, 0xac, 0x4c, 0x4a, 0x8e, 0x4b, 0x4b,
    0x85, 0xd9, 0xb2, 0xa2, 0x4f, 0xa1, 0x19, 0x5b,
};

RK_U8 uuid_refresh_cfg[16] = {
    0x72, 0x65, 0x63, 0x6f, 0x76, 0x65, 0x72, 0x79,
    0x5f, 0x70, 0x6f, 0x69, 0x6e, 0x74, 0x00, 0x00
};

#if 0
static RK_U8 uuid_debug_info[16] = {
    0x57, 0x68, 0x97, 0x80, 0xe7, 0x0c, 0x4b, 0x65,
    0xa9, 0x06, 0xae, 0x29, 0x94, 0x11, 0xcd, 0x9a
};
#endif
static void reset_hal_enc_task(HalEncTask *task)
{
    kmpp_memset(task, 0, sizeof(*task));
}

static void reset_enc_rc_task(EncRcTask *task)
{
    kmpp_memset(task, 0, sizeof(*task));
}

static void reset_enc_task(MppEncImpl *enc)
{
    enc->packet = NULL;
    enc->frame = NULL;

    enc->frm_buf = NULL;
    enc->pkt_buf = NULL;

    /* NOTE: clear add_by flags */
    enc->hdr_status.val = enc->hdr_status.ready;
}

static void update_hal_info_fps(MppEncImpl *enc)
{
    RK_S32 time_diff = ((RK_S32)(enc->time_end - enc->time_base) / 1000);

    enc->real_fps = (enc->frame_count * 1000 * 10) / time_diff;
    enc->time_base = enc->time_end;
    enc->frame_count = 0;
}

static RK_S32 mpp_enc_get_real_fps(MppEncImpl *enc)
{
    RK_S32 time_diff = ((RK_S32)(kmpp_time_us() - enc->time_end) / 1000);

    if (time_diff > 2000)
        enc->real_fps = 0;

    return enc->real_fps;
}

static void check_hal_task_pkt_len(HalEncTask *task, const char *reason)
{
    RK_U32 task_length = task->length;
    RK_U32 packet_length;

    kmpp_packet_get_length(task->packet, &packet_length);
    if (task_length != packet_length) {
        kmpp_loge_f("%s check failed: task length is not match to packet length %d vs %d\n",
                    reason, task_length, packet_length);
    }
}

static void enc_set_rc_updated(MppEncImpl *enc, const char *log_str)
{
    if (!enc->rc_status.rc_api_user_cfg) {
        enc->rc_status.rc_api_user_cfg = 1;
        if (log_str)
            enc_dbg_cfg("rc cfg updated for %s\n", log_str);
    }
}

static void enc_set_resend_hdr(MppEncImpl *enc, const char *log_str)
{
    if (enc->coding != MPP_VIDEO_CodingAVC && enc->coding != MPP_VIDEO_CodingHEVC)
        return;

    if (!(enc->frm_cfg.force_flag & ENC_FORCE_IDR)) {
        enc->frm_cfg.force_flag |= ENC_FORCE_IDR;
        if (log_str)
            enc_dbg_cfg("resend header and force idr for %s\n", log_str);
    }
    enc->hdr_status.val = 0;
}

static void show_prep_update(MppEncPrepCfg *cfg, MppEncPrepCfg *set)
{
    rk_u8 str[256];
    rk_s32 len;
    rk_s32 base;

    len = kmpp_snprintf(str, sizeof(str) - 1, "set prep cfg ");
    base = len;

    if (set->width != cfg->width || set->height != cfg->height) {
        len += kmpp_snprintf(str + len, sizeof(str) - len - 1,
                             "w:h [%d:%d] -> [%d:%d] ",
                             cfg->width, cfg->height, set->width, set->height);
    }

    if (set->hor_stride != cfg->hor_stride || set->ver_stride != cfg->ver_stride) {
        len += kmpp_snprintf(str + len, sizeof(str) - len - 1,
                             "stride [%d:%d] -> [%d:%d] ",
                             cfg->hor_stride, cfg->ver_stride,
                             set->hor_stride, set->ver_stride);
    }

    if (set->format != cfg->format) {
        len += kmpp_snprintf(str + len, sizeof(str) - len - 1,
                             "fmt %d -> %d ", cfg->format, set->format);
    }

    if (set->rotation != cfg->rotation) {
        len += kmpp_snprintf(str + len, sizeof(str) - len - 1,
                             "rotate %d -> %d ", cfg->rotation, set->rotation);
    }

    if (set->mirroring != cfg->mirroring) {
        len += kmpp_snprintf(str + len, sizeof(str) - len - 1,
                             "mirror %d -> %d ", cfg->mirroring, set->mirroring);
    }

    if (len > base)
        enc_dbg_cfg("%s\n", str);

    kmpp_logi("set prep cfg w:h [%d:%d] stride [%d:%d] fmt %d rotate %d mirror %d\n",
              set->width, set->height, set->hor_stride, set->ver_stride,
              set->format, set->rotation, set->mirroring);
}

static void proc_base_cfg(MppEncImpl *enc)
{
    MppEncBaseCfg *set = &enc->set->base;
    MppEncBaseCfg *cfg = &enc->cfg->base;
    const rk_u8 *str_smt = "smart";
    const rk_u8 *str_default = "default";
    const rk_u8 *rc_name = NULL;
    static KmppEntry *tbl_smt1 = NULL;
    static KmppEntry *tbl_smt3 = NULL;
    static rk_s32 once = 1;
    rk_s32 smart_en;

    cfg->coding = set->coding;
    cfg->low_delay = set->low_delay;

    if (!tbl_smt1 && once) {
        tbl_smt1 = query_mpp_enc_cfg_entry("base:smt1_en");
        tbl_smt3 = query_mpp_enc_cfg_entry("base:smt3_en");

        if (!tbl_smt1 || !tbl_smt3)
            kmpp_loge("failed to get smt1_en and smt3_en table\n");

        once = 0;
    }

    if (!kmpp_obj_tbl_test(enc->set_obj, tbl_smt1) &&
        !kmpp_obj_tbl_test(enc->set_obj, tbl_smt3))
        return;

    if (set->smt1_en && set->smt3_en)
        set->smt3_en = 0;

    kmpp_logd("dynamically switch smt1_en %d -> %d smt3_en %d -> %d\n",
              cfg->smt1_en, set->smt1_en, cfg->smt3_en, set->smt3_en);

    cfg->smt1_en = set->smt1_en;
    cfg->smt3_en = set->smt3_en;

    smart_en = cfg->smt1_en || (cfg->smt3_en && enc->cfg->rc.rc_mode == MPP_ENC_RC_MODE_SMTRC);
    rc_name = smart_en ? str_smt : str_default;
    if (kmpp_strcmp(rc_name, enc->rc_brief.name)) {
        enc->rc_brief.name = rc_name;
        enc->rc_status.rc_api_user_cfg = 1;
        enc->rc_status.rc_api_updated = 1;
    }

    /* use change_smt_mode to mpp_enc_hal_prepare */
    cfg->change_smt_mode = 1;
    enc_set_resend_hdr(enc, "change smart mode");
}

static void proc_prep_cfg(MppEncImpl *enc)
{
    MppEncPrepCfg *cfg = &enc->cfg->prep;
    MppEncPrepCfg *set = &enc->set->prep;
    rk_u32 flag = *mpp_enc_cfg_prep_change(enc->set);
    rk_u32 change_res = 0;
    rk_u32 change_rot = 0;
    rk_u32 restore = 0;
    rk_s32 mirroring;
    rk_s32 rotation;

    enc_dbg_cfg("prep cfg change 0x%x\n", flag);

    if (!flag)
        return;

    {  /* chekc input and format change */
        static KmppEntry *tbl_input = NULL;
        static KmppEntry *tbl_format = NULL;
        static KmppEntry *tbl_rotation = NULL;
        static rk_s32 once = 1;

        if (!tbl_input && once) {
            tbl_input = query_mpp_enc_cfg_entry("prep:width");
            tbl_format = query_mpp_enc_cfg_entry("prep:format");
            tbl_rotation = query_mpp_enc_cfg_entry("prep:rotation");

            if (!tbl_input || !tbl_format || !tbl_rotation)
                kmpp_loge("failed to get input and format table\n");

            once = 0;
        }

        if (kmpp_obj_tbl_test(enc->set_obj, tbl_input) ||
            kmpp_obj_tbl_test(enc->set_obj, tbl_format)) {
            change_res = 1;
        }
        if (kmpp_obj_tbl_test(enc->set_obj, tbl_input) ||
            kmpp_obj_tbl_test(enc->set_obj, tbl_rotation)) {
            change_rot = 1;
        }
    }

    if (set->rotation_ext >= MPP_ENC_ROT_BUTT || set->rotation_ext < 0 ||
        set->mirroring_ext < 0 || set->flip < 0) {
        kmpp_loge("invalid rotation %d mirroring %d flip %d\n",
                  set->rotation_ext, set->mirroring_ext, set->flip);
        kmpp_loge("restore rotation %d mirroring %d flip %d\n",
                  cfg->rotation_ext, cfg->mirroring_ext, cfg->flip);
        set->rotation_ext = cfg->rotation_ext;
        set->mirroring_ext = cfg->mirroring_ext;
        set->flip = cfg->flip;
    }

    /*
     * For unifying the encoder's params used by CFG_SET and CFG_GET command,
     * there is distinction between user's set and set in hal.
     * User can externally set rotation_ext, mirroring_ext and flip,
     * which should be transformed to mirroring and rotation in hal.
     */
    rotation = set->rotation_ext;
    mirroring = set->mirroring_ext;

    if (set->flip) {
        mirroring = !mirroring;
        rotation += MPP_ENC_ROT_180;
        rotation &= MPP_ENC_ROT_270;
    }

    set->mirroring = mirroring;
    set->rotation = rotation;

    if (MPP_FRAME_FMT_IS_FBC(set->format) && (set->mirroring || set->rotation)) {
        kmpp_loge("conflict fbc input format %x with mirror %d or rotaion %d\n",
                  set->format, set->mirroring, set->rotation);
        kmpp_loge("disable mirror and rotation\n");
        set->mirroring = 0;
        set->rotation = 0;
    }

    if (set->range >= MPP_FRAME_RANGE_NB ||
        set->color >= MPP_FRAME_SPC_NB ||
        set->colorprim >= MPP_FRAME_PRI_NB ||
        set->colortrc >= MPP_FRAME_TRC_NB) {
        kmpp_loge("invalid color range %d colorspace %d primaries %d transfer characteristic %d\n",
                  set->range, set->color, set->colorprim, set->colortrc);
        kmpp_loge("restore color range %d colorspace %d primaries %d transfer characteristic %d\n",
                  cfg->range, cfg->color, cfg->colorprim, cfg->colortrc);
        set->range = cfg->range;
        set->color = cfg->color;
        set->colorprim = cfg->colorprim;
        set->colortrc = cfg->colortrc;
    }

    if (set->rotation == MPP_ENC_ROT_90 || set->rotation == MPP_ENC_ROT_270) {
        set->height = set->width_set;
        set->width = set->height_set;

        if (set->height > set->hor_stride || set->width > set->ver_stride)
            restore = 1;
    } else {
        set->width = set->width_set;
        set->height = set->height_set;

        if (set->width > set->hor_stride || set->height > set->ver_stride)
            restore = 1;
    }

    if (restore) {
        kmpp_loge("invalid set w:h [%d:%d] stride [%d:%d] rotation %d\n",
                  set->width, set->height, set->hor_stride, set->ver_stride, set->rotation);
        kmpp_loge("restore cfg w:h [%d:%d] stride [%d:%d] rotation %d\n",
                  cfg->width, cfg->height, cfg->hor_stride, cfg->ver_stride, cfg->rotation);
        set->width = cfg->width;
        set->height = cfg->height;
        set->width_set = cfg->width;
        set->height_set = cfg->height;
        set->hor_stride = cfg->hor_stride;
        set->ver_stride = cfg->ver_stride;
        set->rotation = cfg->rotation;
    }

    if (!set->max_height || !set->max_width) {
        set->max_width = set->width;
        set->max_height = set->height;
    }

    show_prep_update(cfg, set);
    kmpp_memcpy(cfg, set, sizeof(*cfg));
    enc_set_resend_hdr(enc, "change prep cfg");

    /* NOTE: only set change res flag and NOT clear */
    if (change_res) {
        if (enc->online)
            mpp_dev_chnl_control(enc->dev, MPP_CMD_VEPU_CLEAR_ONLINE_TASK, NULL);
        cfg->change_res = 1;
    }
}

#define RC_CHANGE_MODE      (1 << 0)
#define RC_CHANGE_BPS       (1 << 1)
#define RC_CHANGE_FPS_IN    (1 << 2)
#define RC_CHANGE_FPS_OUT   (1 << 3)
#define RC_CHANGE_GOP       (1 << 4)

static void show_rc_update(MppEncRcCfg *cfg, MppEncRcCfg *set, rk_u32 change)
{
    rk_u8 str[256];
    rk_s32 len;
    rk_s32 base;

    len = kmpp_snprintf(str, sizeof(str) - 1, "set rc cfg ");
    base = len;

    if (change & RC_CHANGE_MODE) {
        len += kmpp_snprintf(str + len, sizeof(str) - len - 1, "mode %s -> %s ",
                             strof_rc_mode(cfg->rc_mode), strof_rc_mode(set->rc_mode));
    }

    if (change & RC_CHANGE_BPS) {
        len += kmpp_snprintf(str + len, sizeof(str) - len - 1,
                             "bps [%d:%d:%d] -> [%d:%d:%d] ",
                             cfg->bps_min, cfg->bps_target, cfg->bps_max,
                             set->bps_min, set->bps_target, set->bps_max);
    }

    if (len > base) {
        enc_dbg_cfg("%s\n", str);
        len = base;
    }

    if (change & RC_CHANGE_FPS_IN) {
        len += kmpp_snprintf(str + len, sizeof(str) - len - 1,
                             "fps in %s [%d:%d] -> %s [%d:%d] ",
                             cfg->fps_in_flex ? "flex" : "fix",
                             cfg->fps_in_num, cfg->fps_in_denom,
                             set->fps_in_flex ? "flex" : "fix",
                             set->fps_in_num, set->fps_in_denom);
    }

    if (change & RC_CHANGE_FPS_OUT) {
        len += kmpp_snprintf(str + len, sizeof(str) - len - 1,
                             "fps out %s [%d:%d] -> %s [%d:%d] ",
                             cfg->fps_out_flex ? "flex" : "fix",
                             cfg->fps_out_num, cfg->fps_out_denom,
                             set->fps_out_flex ? "flex" : "fix",
                             set->fps_out_num, set->fps_out_denom);
    }

    if (change & RC_CHANGE_GOP) {
        len += kmpp_snprintf(str + len, sizeof(str) - len - 1,
                             "gop %d -> %d ", cfg->gop, set->gop);
    }

    if (len > base)
        enc_dbg_cfg("%s\n", str);

    if (!change)
        return;

    kmpp_logi("set rc %s bps [%d:%d:%d] fps %s [%d:%d] -> %s [%d:%d] gop %d\n",
              strof_rc_mode(set->rc_mode),
              set->bps_min, set->bps_target, set->bps_max,
              set->fps_in_flex ? "flex" : "fix",
              set->fps_in_num, set->fps_in_denom,
              set->fps_out_flex ? "flex" : "fix",
              set->fps_out_num, set->fps_out_denom,
              set->gop);
}

static MPP_RET mpp_enc_proc_ref_kcfg(MppEncImpl *enc, KmppShmPtr *shm)
{
    MppEncCfgSet *cfg = enc->cfg;
    MppEncRefCfg ref_cfg = NULL;
    MPP_RET ret;

    if (!enc || !shm) {
        kmpp_loge_f("invalid param enc %px shm %px\n", enc, shm);
        return MPP_ERR_NULL_PTR;
    }

    ret = mpp_enc_ref_get_by_shm(&ref_cfg, shm);
    if (ret) {
        kmpp_loge_f("failed to create ref cfg from shm ret %d\n", ret);
        return ret;
    }

    ret = mpp_enc_ref_cfg_check(ref_cfg);
    if (ret) {
        kmpp_loge_f("ref cfg check failed ret %d\n", ret);
        goto done;
    }

    ret = mpp_enc_refs_set_cfg(enc->refs, ref_cfg);
    if (ret) {
        kmpp_loge_f("failed to set ref cfg to refs ret %d\n", ret);
        goto done;
    }

    if (mpp_enc_refs_update_hdr(enc->refs))
        enc->hdr_status.val = 0;

    if (cfg->ref_cfg && cfg->ref_cfg != mpp_enc_ref_default())
        mpp_enc_ref_cfg_deinit(&cfg->ref_cfg);

    cfg->ref_cfg = ref_cfg;
    ref_cfg = NULL;
    enc_set_rc_updated(enc, "ref cfg from user kernel");

done:
    if (ref_cfg)
        mpp_enc_ref_cfg_deinit(&ref_cfg);

    return ret;
}

static void proc_rc_cfg(MppEncImpl *enc)
{
    MppEncRcCfg *cfg = &enc->cfg->rc;
    MppEncRcCfg *set = &enc->set->rc;
    rk_u32 flag = *mpp_enc_cfg_rc_change(enc->set);
    const rk_u8 *str_smt = "smart";
    const rk_u8 *str_default = "default";
    rk_u32 change = 0;
    rk_s32 i;

    enc_dbg_cfg("rc cfg change 0x%x\n", flag);

    if (!flag)
        return;

    /* 1. check input rc config for invalid value and revert to valid value */
    if (set->rc_priority >= MPP_ENC_RC_PRIORITY_BUTT) {
        kmpp_loge("invalid rc_priority %d should be in range [%d:%d]\n",
                  set->rc_priority, MPP_ENC_RC_BY_BITRATE_FIRST, MPP_ENC_RC_PRIORITY_BUTT);
        kmpp_loge("restore rc_priority to %d\n", cfg->rc_priority);
        set->rc_priority = cfg->rc_priority;
    }

    if (set->super_mode >= MPP_ENC_RC_SUPER_FRM_BUTT) {
        kmpp_loge("invalid super_mode %d should be in range [%d:%d]\n",
                  set->super_mode, MPP_ENC_RC_SUPER_FRM_NONE, MPP_ENC_RC_SUPER_FRM_BUTT);
        kmpp_loge("restore super_mode to %d\n", cfg->super_mode);
        set->super_mode = cfg->super_mode;
    }

    if (set->debreath_en && set->debre_strength > 35) {
        kmpp_loge("invalid debre_strength %d should be in range [%d:%d]\n",
                  set->debre_strength, 0, 35, cfg->debre_strength);
        kmpp_loge("restore debre_strength to %d\n", cfg->debre_strength);
        set->debre_strength = cfg->debre_strength;
    }

    if (set->rc_mode >= MPP_ENC_RC_MODE_BUTT) {
        kmpp_loge("invalid rc mode %d must be in range [0:%d]\n",
                  set->rc_mode, MPP_ENC_RC_MODE_BUTT - 1);

        for (i = 0; i < MPP_ENC_RC_MODE_BUTT; i++)
            kmpp_loge("%d - %s\n", i, strof_rc_mode(i));

        kmpp_loge("restore rc mode to %d\n", cfg->rc_mode);
        set->rc_mode = cfg->rc_mode;
    }

    if (set->quality >= MPP_ENC_RC_QUALITY_BUTT) {
        kmpp_loge("invalid quality %d must be in ranget [0:%d]\n",
                  set->quality, MPP_ENC_RC_QUALITY_BUTT - 1);
        kmpp_loge("0 - MPP_ENC_RC_QUALITY_WORST\n");
        kmpp_loge("1 - MPP_ENC_RC_QUALITY_BAD\n");
        kmpp_loge("2 - MPP_ENC_RC_QUALITY_MEDIUM\n");
        kmpp_loge("3 - MPP_ENC_RC_QUALITY_BETTER\n");
        kmpp_loge("4 - MPP_ENC_RC_QUALITY_BEST\n");
        kmpp_loge("5 - MPP_ENC_RC_QUALITY_CQP\n");
        kmpp_loge("6 - MPP_ENC_RC_QUALITY_AQ_ONLY\n");
        kmpp_loge("restore quality to %d\n", cfg->quality);
        set->quality = cfg->quality;
    }

    if (set->rc_mode != MPP_ENC_RC_MODE_FIXQP) {
        if ((set->bps_target >= 100 * SZ_1M || set->bps_target <= 1 * SZ_1K) ||
            (set->bps_max >= 100 * SZ_1M    || set->bps_max <= 1 * SZ_1K) ||
            (set->bps_min >= 100 * SZ_1M    || set->bps_min <= 1 * SZ_1K)) {
            kmpp_loge("invalid bit per second (bps) %d [%d:%d] out of range 1K~100M\n",
                      set->bps_target, set->bps_min, set->bps_max);
            kmpp_loge("restore bps to %d [%d:%d]\n",
                      cfg->bps_target, cfg->bps_min, cfg->bps_max);
            set->bps_target = cfg->bps_target;
            set->bps_max = cfg->bps_max;
            set->bps_min = cfg->bps_min;
        }
    }

    if (set->ref_cfg) {
        KmppShmPtr *ref_kcfg = NULL;
        MPP_RET ret;

        ref_kcfg = (KmppShmPtr *)set->ref_cfg;
        ret = mpp_enc_proc_ref_kcfg(enc, ref_kcfg);
        if (ret)
            kmpp_loge("mpp_enc_proc_ref_kcfg failed ret %d\n", ret);

        kmpp_memset(&set->ref_cfg, 0, sizeof(set->ref_cfg));
    }

    // if I frame min/max is not set use normal case
    if (set->qp_min_i <= 0)
        set->qp_min_i = set->qp_min;
    if (set->qp_max_i <= 0)
        set->qp_max_i = set->qp_max;
    if (set->qp_min < 0 || set->qp_max < 0 ||
        set->qp_min > set->qp_max || set->qp_min_i < 0 ||
        set->qp_max_i < 0 || set->qp_min_i > set->qp_max_i ||
        (set->qp_init > 0 && (set->qp_init > set->qp_max_i ||
                              set->qp_init < set->qp_min_i))) {
        kmpp_loge("invalid qp range: init %d i [%d:%d] p [%d:%d]\n",
                  set->qp_init, set->qp_min_i, set->qp_max_i,
                  set->qp_min, set->qp_max);

        kmpp_loge("restore qp range: init %d i [%d:%d] p [%d:%d]\n",
                  set->qp_init, set->qp_min_i, set->qp_max_i,
                  set->qp_min, set->qp_max);

        set->qp_init = cfg->qp_init;
        set->qp_min_i = cfg->qp_min_i;
        set->qp_max_i = cfg->qp_max_i;
        set->qp_min = cfg->qp_min;
        set->qp_max = cfg->qp_max;
    }

    if (KMPP_ABS(set->qp_delta_ip) > 8) {
        kmpp_loge("invalid qp delta ip %d restore to %d\n",
                  set->qp_delta_ip, cfg->qp_delta_ip);
        set->qp_delta_ip = cfg->qp_delta_ip;
    }
    if (KMPP_ABS(set->qp_delta_vi) > 6) {
        kmpp_loge("invalid qp delta vi %d restore to %d\n",
                  set->qp_delta_vi, cfg->qp_delta_vi);
        set->qp_delta_vi = cfg->qp_delta_vi;
    }
    if (set->qp_max_step < 0) {
        kmpp_loge("invalid qp max step %d restore to %d\n",
                  set->qp_max_step, cfg->qp_max_step);
        set->qp_max_step = cfg->qp_max_step;
    }
    if (set->stats_time && set->stats_time > 60) {
        kmpp_loge("warning: bitrate statistic time %d is larger than 60s\n",
                  set->stats_time);
    }

    /* 2. check rc cfg done now check rc cfg change */
    if (set->rc_mode != cfg->rc_mode)
        change |= RC_CHANGE_MODE;
    if (set->bps_target != cfg->bps_target ||
        set->bps_max != cfg->bps_max ||
        set->bps_min != cfg->bps_min)
        change |= RC_CHANGE_BPS;
    if (set->fps_in_num != cfg->fps_in_num ||
        set->fps_in_denom != cfg->fps_in_denom ||
        set->fps_in_flex != cfg->fps_in_flex)
        change |= RC_CHANGE_FPS_IN;
    if (set->fps_out_num != cfg->fps_out_num ||
        set->fps_out_denom != cfg->fps_out_denom ||
        set->fps_out_flex != cfg->fps_out_flex)
        change |= RC_CHANGE_FPS_OUT;
    if (set->gop != cfg->gop)
        change |= RC_CHANGE_GOP;

    if (change & (RC_CHANGE_MODE | RC_CHANGE_FPS_IN | RC_CHANGE_FPS_OUT | RC_CHANGE_GOP))
        enc_set_resend_hdr(enc, "change rc_mode/fps/gop");

    /* NOTE: some rc change no need to (quality) */
    if (flag)
        enc_set_rc_updated(enc, "rc cfg changed");

    if (change)
        show_rc_update(cfg, set, change);
    kmpp_memcpy(cfg, set, sizeof(*cfg));

    if (change & RC_CHANGE_GOP)
        mpp_enc_refs_set_rc_igop(enc->refs, cfg->gop);

    /* smart v3 have two rc mode(cbr and smtrc), need switch rc_api */
    if (enc->cfg->base.smt3_en) {
        if (cfg->rc_mode == MPP_ENC_RC_MODE_CBR && kmpp_strcmp(enc->rc_brief.name, str_default)) {
            enc->rc_brief.name = str_default;
            enc->rc_status.rc_api_user_cfg = 1;
            enc->rc_status.rc_api_updated = 1;
        } else if (cfg->rc_mode == MPP_ENC_RC_MODE_SMTRC && kmpp_strcmp(enc->rc_brief.name, str_smt)) {
            enc->rc_brief.name = str_smt;
            enc->rc_status.rc_api_user_cfg = 1;
            enc->rc_status.rc_api_updated = 1;
        }
    }
}

static void proc_split_cfg(MppEncSliceSplit *cfg, MppEncSliceSplit *set)
{
    if (set->split_mode >= MPP_ENC_SPLIT_MODE_BUTT) {
        kmpp_loge("invalid split mode %d should be in range [%d:%d]\n",
                  set->split_mode, MPP_ENC_SPLIT_NONE, MPP_ENC_SPLIT_MODE_BUTT);
        kmpp_loge("restore split mode %d\n", cfg->split_mode);
        set->split_mode = cfg->split_mode;
    }

    if (!set->split_mode) {
        set->split_arg = 0;
        set->split_out = 0;
    }

    kmpp_memcpy(cfg, set, sizeof(*cfg));
}

static void proc_hw_cfg(MppEncImpl *enc)
{
    MppEncHwCfg *cfg = &enc->cfg->hw;
    MppEncHwCfg *set = &enc->set->hw;
    rk_u32 flag = *mpp_enc_cfg_hw_change(enc->set);

    enc_dbg_cfg("hw cfg change 0x%x\n", flag);

    if (!flag)
        return;

    if (set->qp_delta_row < 0 || set->qp_delta_row_i < 0) {
        kmpp_loge("invalid hw qp delta row [%d:%d] restore to [%d:%d]\n",
                  set->qp_delta_row_i, set->qp_delta_row,
                  cfg->qp_delta_row_i, cfg->qp_delta_row);
        set->qp_delta_row = cfg->qp_delta_row;
        set->qp_delta_row_i = cfg->qp_delta_row_i;
    }

    if (set->qbias_en < 0) {
        kmpp_loge("invalid hw qbias_en %d restore to %d\n",
                  set->qbias_en, cfg->qbias_en);
        set->qbias_en = cfg->qbias_en;
    }

    if (set->qbias_i < 0 || set->qbias_p < 0) {
        kmpp_loge("invalid hw qp bias [%d:%d] restore to [%d:%d]\n",
                  set->qbias_i, set->qbias_p, cfg->qbias_i, cfg->qbias_p);
        set->qbias_i = cfg->qbias_i;
    }

    if (set->flt_str_i < 0 || set->flt_str_p < 0) {
        kmpp_loge("invalid filter strength [%d:%d] restore to [%d:%d]\n",
                  set->flt_str_i, set->flt_str_p,
                  cfg->flt_str_i, cfg->flt_str_p);
        set->flt_str_i = cfg->flt_str_i;
    }

    kmpp_memcpy(cfg, set, sizeof(*cfg));
}

static void proc_tune_cfg(MppEncImpl *enc)
{
    MppEncFineTuneCfg *cfg = &enc->cfg->tune;
    MppEncFineTuneCfg *set = &enc->set->tune;
    rk_u32 flag = *mpp_enc_cfg_tune_change(enc->set);

    enc_dbg_cfg("tune cfg change 0x%x\n", flag);

    if (!flag)
        return;

    if (set->scene_mode < MPP_ENC_SCENE_MODE_DEFAULT ||
        set->scene_mode >= MPP_ENC_SCENE_MODE_BUTT) {
        kmpp_loge("invalid scene mode %d not in range [%d:%d]\n", set->scene_mode,
                  MPP_ENC_SCENE_MODE_DEFAULT, MPP_ENC_SCENE_MODE_BUTT - 1);
        kmpp_loge("restore scene mode %d\n", cfg->scene_mode);
        set->scene_mode = cfg->scene_mode;
    }

    if (set->motion_static_switch_enable < 0 || set->motion_static_switch_enable > 1) {
        kmpp_loge("invalid motion static switch enable not in range [%d : %d]\n", 0, 1);
        kmpp_loge("restore motion static switch enable %d\n", cfg->motion_static_switch_enable);
        set->motion_static_switch_enable = cfg->motion_static_switch_enable;
    }

    if (set->deblur_str < 0 || set->deblur_str > 7) {
        kmpp_loge("invalid deblur strength not in range [0 : 7]\n");
        kmpp_loge("restore deblur strength %d\n", cfg->deblur_str);
        set->deblur_str = cfg->deblur_str;
    }

    if (set->atr_str_i < 0 || set->atr_str_i > 3) {
        kmpp_loge("invalid anti ring strength of I frame not in range [0 : 3]\n");
        kmpp_loge("restore anti ring strength of I frame %d\n", cfg->atr_str_i);
        set->atr_str_i = cfg->atr_str_i;
    }

    if (set->atr_str_p < 0 || set->atr_str_p > 3) {
        kmpp_loge("invalid anti ring strength of P frame not in range [0 : 3]\n");
        kmpp_loge("restore anti ring strength of P frame %d\n", cfg->atr_str_p);
        set->atr_str_p = cfg->atr_str_p;
    }

    if (set->atl_str < 0 || set->atl_str > 3) {
        kmpp_loge("invalid anti line strength not in range [0 : 3]\n");
        kmpp_loge("restore anti line strength %d\n", cfg->atl_str);
        set->atl_str = cfg->atl_str;
    }

    if (set->lambda_idx_p < 0 || set->lambda_idx_p > 8) {
        kmpp_loge("invalid lambda idx not in range [0 : 8]\n");
        kmpp_loge("restore lambda idx %d\n", cfg->lambda_idx_p);
        set->lambda_idx_p = cfg->lambda_idx_p;
    }

    if (set->lambda_idx_i < 0 || set->lambda_idx_i > 8) {
        kmpp_loge("invalid I frame lambda idx not in range [0 : 8]\n");
        kmpp_loge("restore I frame lambda idx %d\n", cfg->lambda_idx_i);
        set->lambda_idx_i = cfg->lambda_idx_i;
    }

    if (set->atf_str < 0 || set->atf_str > 3) {
        kmpp_loge("invalid anti flick strength not in range [0 : 3]\n");
        kmpp_loge("restore anti flick strength %d\n", cfg->atf_str);
        set->atf_str = cfg->atf_str;
    }

    if (set->lgt_chg_lvl < 0 || set->lgt_chg_lvl > 7) {
        kmpp_loge("invalid lgt_chg_lvl %d not in range [0, 7]\n", set->lgt_chg_lvl);
        kmpp_loge("restore lgt_chg_lvl %d\n", cfg->lgt_chg_lvl);
        set->lgt_chg_lvl = cfg->lgt_chg_lvl;
    }

    if (set->static_frm_num < 0 || set->static_frm_num > 7) {
        kmpp_loge("invalid static_frm_num %d not in range [0, 7]\n", set->static_frm_num);
        kmpp_loge("restore static_frm_num %d\n", cfg->static_frm_num);
        set->static_frm_num = cfg->static_frm_num;
    }

    if (set->madp16_th < 0 || set->madp16_th > 63) {
        kmpp_loge("invalid madp16_th %d not in range [0, 255]\n", set->madp16_th);
        kmpp_loge("restore madp16_th %d\n", cfg->madp16_th);
        set->madp16_th = cfg->madp16_th;
    }

    if (!(set->skip16_wgt == 0 || (set->skip16_wgt >= 3 && set->skip16_wgt <= 8))) {
        kmpp_loge("invalid skip16_wgt %d not in range [3, 8] or 0\n", set->skip16_wgt);
        kmpp_loge("restore skip16_wgt %d\n", cfg->skip16_wgt);
        set->skip16_wgt = cfg->skip16_wgt;
    }

    if (!(set->skip32_wgt == 0 || (set->skip32_wgt >= 3 && set->skip32_wgt <= 8))) {
        kmpp_loge("invalid skip32_wgt %d not in range [3, 8] or 0\n", set->skip32_wgt);
        kmpp_loge("restore skip32_wgt %d\n", cfg->skip32_wgt);
        set->skip32_wgt = cfg->skip32_wgt;
    }

    set->bmap_qpmin_i = mpp_clip(set->bmap_qpmin_i, 1, 51);
    set->bmap_qpmin_p = mpp_clip(set->bmap_qpmin_p, 1, 51);
    set->bmap_qpmax_i = mpp_clip(set->bmap_qpmax_i, 1, 51);
    set->bmap_qpmax_p = mpp_clip(set->bmap_qpmax_p, 1, 51);
    set->min_bg_fqp = mpp_clip(set->min_bg_fqp, 1, 51);
    set->max_bg_fqp = mpp_clip(set->max_bg_fqp, 1, 51);
    set->min_fg_fqp = mpp_clip(set->min_fg_fqp, 1, 51);
    set->max_fg_fqp = mpp_clip(set->max_fg_fqp, 1, 51);
    set->bg_delta_qp_i = mpp_clip(set->bg_delta_qp_i, -32, 32);
    set->bg_delta_qp_p = mpp_clip(set->bg_delta_qp_p, -32, 32);
    set->fg_delta_qp_i = mpp_clip(set->fg_delta_qp_i, -32, 32);
    set->fg_delta_qp_p = mpp_clip(set->fg_delta_qp_p, -32, 32);
    set->fg_area = mpp_clip(set->fg_area, -1, 100);

    if (set->scene_mode != cfg->scene_mode ||
        set->motion_static_switch_enable != cfg->motion_static_switch_enable ||
        set->deblur_str != cfg->deblur_str)
        enc_set_rc_updated(enc, "tune cfg changed");

    kmpp_memcpy(cfg, set, sizeof(*cfg));
}

static void proc_h264_cfg(MppEncImpl *enc)
{
    MppEncH264Cfg *cfg = &enc->cfg->h264;
    MppEncH264Cfg *set = &enc->set->h264;
    rk_u32 flag = *mpp_enc_cfg_h264_change(enc->set);
    rk_s32 profile = set->profile;
    rk_s32 entropy_coding_mode = set->entropy_coding_mode_ex;
    rk_s32 cabac_init_idc = set->cabac_init_idc_ex;
    rk_s32 transform8x8_mode = set->transform8x8_mode_ex;
    rk_s32 disable_cabac = (H264_PROFILE_FREXT_CAVLC444 == profile ||
                            H264_PROFILE_BASELINE == profile ||
                            H264_PROFILE_EXTENDED == profile);

    enc_dbg_cfg("h264 cfg change 0x%x\n", flag);

    if (enc->cfg->rc.refresh_en) {
        RK_U32 mb_rows;

        if (!enc->cfg->rc.refresh_mode)
            mb_rows = MPP_ALIGN(enc->cfg->prep.height, 16) / 16;
        else
            mb_rows = MPP_ALIGN(enc->cfg->prep.width, 16) / 16;

        enc->cfg->rc.refresh_length = (mb_rows + enc->cfg->rc.refresh_num - 1) /
                                      enc->cfg->rc.refresh_num;
        if (enc->cfg->rc.gop < enc->cfg->rc.refresh_length)
            enc->cfg->rc.refresh_length = enc->cfg->rc.gop;
    }

    if (!flag)
        return;

    /* 1. check input h264 config */
    if (disable_cabac) {
        if (entropy_coding_mode) {
            kmpp_loge("Warning: invalid cabac_en %d for h264 profile %d, set to 0.\n",
                      entropy_coding_mode, profile);
            entropy_coding_mode = 0;
        }

        if (cabac_init_idc >= 0) {
            kmpp_loge("Warning: invalid cabac_init_idc %d for h264 profile %d, set to -1.\n",
                      cabac_init_idc, profile);

            cabac_init_idc = -1;
        }
    }

    if (profile < H264_PROFILE_HIGH && transform8x8_mode) {
        kmpp_loge("Warning: invalid transform8x8_mode %d for h264 profile %d, set to 0.\n",
                  transform8x8_mode, profile);

        transform8x8_mode = 0;
    }

    set->entropy_coding_mode = entropy_coding_mode;
    set->cabac_init_idc = cabac_init_idc;
    set->transform8x8_mode = transform8x8_mode;

    kmpp_memcpy(cfg, set, sizeof(*cfg));
    enc_set_resend_hdr(enc, "h264 cfg changed");
}

static void proc_h265_cfg(MppEncImpl *enc)
{
    MppEncH265Cfg *cfg = &enc->cfg->h265;
    MppEncH265Cfg *set = &enc->set->h265;
    rk_u32 flag = *mpp_enc_cfg_h265_change(enc->set);

    enc_dbg_cfg("h265 cfg change 0x%x\n", flag);

    if (enc->cfg->rc.refresh_en) {
        RK_U32 mb_rows;

        if (!enc->cfg->rc.refresh_mode)
            mb_rows = MPP_ALIGN(enc->cfg->prep.height, 32) / 32;
        else
            mb_rows = MPP_ALIGN(enc->cfg->prep.width, 32) / 32;

        enc->cfg->rc.refresh_length = (mb_rows + enc->cfg->rc.refresh_num - 1) /
                                      enc->cfg->rc.refresh_num;
        if (enc->cfg->rc.gop < enc->cfg->rc.refresh_length)
            enc->cfg->rc.refresh_length = enc->cfg->rc.gop;
    }

    if (!flag)
        return;

    if (set->trans_cfg.cb_qp_offset != set->trans_cfg.cr_qp_offset) {
        kmpp_loge("h265 cr_qp_offset %d MUST equal to cb_qp_offset %d. FORCE to same value\n",
                  set->trans_cfg.cb_qp_offset, set->trans_cfg.cr_qp_offset);

        set->trans_cfg.cr_qp_offset = set->trans_cfg.cb_qp_offset;
    }

    if (set->trans_cfg.diff_cu_qp_delta_depth > 2 || set->trans_cfg.diff_cu_qp_delta_depth < 0) {
        kmpp_loge("h265 diff_cu_qp_delta_depth must be in [0, 2] restore to 1\n");
        set->trans_cfg.diff_cu_qp_delta_depth = 1;
    }

    kmpp_memcpy(cfg, set, sizeof(*cfg));
    enc_set_resend_hdr(enc, "h265 cfg changed");
}

static void proc_jpeg_cfg(MppEncImpl *enc)
{
    MppEncRcCfg *rc_cfg = &enc->cfg->rc;
    MppEncRcCfg *rc_set = &enc->set->rc;
    MppEncJpegCfg *cfg = &enc->cfg->jpeg;
    MppEncJpegCfg *set = &enc->set->jpeg;
    rk_u32 flag = *mpp_enc_cfg_jpeg_change(enc->set);
    rk_u32 update_rc = 0;
    static KmppEntry *tbl_quant = NULL;
    static KmppEntry *tbl_qfactor = NULL;
    static KmppEntry *tbl_qtable = NULL;
    static rk_s32 once = 1;

    if (once) {
        tbl_quant = query_mpp_enc_cfg_entry("jpeg:quant");
        tbl_qfactor = query_mpp_enc_cfg_entry("jpeg:q_factor");
        tbl_qtable = query_mpp_enc_cfg_entry("jpeg:qtable_y");

        if (!tbl_quant || !tbl_qfactor || !tbl_qtable)
            kmpp_loge("failed to get jpeg update flag\n");

        once = 0;
    }

    enc_dbg_cfg("jpeg cfg change 0x%x\n", flag);

    /* no update and current q mode is valid */
    if (!flag && cfg->q_mode)
        return;

    /* check input change then check validity */
    if (kmpp_obj_tbl_test(enc->set_obj, tbl_quant)) {
        /* quant is set */
        if (set->quant < 0 || set->quant > 10) {
            kmpp_loge("invalid quant %d not in range [0, 10] restore to 10 (best quality)\n", set->quant);
            set->quant = 10;
        }

        rc_set->rc_mode = MPP_ENC_RC_MODE_FIXQP;
        update_rc = 1;
        set->q_mode = JPEG_QUANT;
        kmpp_logi("set jpeg quant %d\n", set->quant);
    } else if (kmpp_obj_tbl_test(enc->set_obj, tbl_qfactor)) {
        rk_u32 fixqp = rc_cfg->rc_mode == MPP_ENC_RC_MODE_FIXQP;

        if (set->q_factor < 1 || set->q_factor > 99) {
            kmpp_loge("invalid q_factor %d out of range [1, 99] restore to 80\n", set->q_factor);
            set->q_factor = 80;
        }
        if (set->qf_min < 1 || set->qf_min > 99) {
            kmpp_loge_c(!fixqp, "invalid qf_min %d out of range [1, 99] restore to 1\n", set->qf_min);
            set->qf_min = 1;
        }
        if (set->qf_max < 1 || set->qf_max > 99) {
            kmpp_loge_c(!fixqp, "invalid qf_max %d out of range [1, 99] restore to 99\n", set->qf_max);
            set->qf_max = 99;
        }

        /* generate q_factor */
        set->q_mode = JPEG_QFACTOR;
        if (fixqp)
            kmpp_logi("set jpeg fix qfactor %d\n", set->q_factor);
        else
            kmpp_logi("set jpeg qfactor %d min:max [%d:%d]\n", set->q_factor, set->qf_min, set->qf_max);
    } else if (kmpp_obj_tbl_test(enc->set_obj, tbl_qtable)) {
        /* check qtable direct update */
        rk_s32 i;

        rc_set->rc_mode = MPP_ENC_RC_MODE_FIXQP;
        update_rc = 1;
        set->q_mode = JPEG_QTABLE;
        kmpp_logi("set jpeg qtable y:\n");
        for (i = 0; i < 8; i++)
            kmpp_logi("%2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d\n",
                      set->qtable_y[i * 8 + 0], set->qtable_y[i * 8 + 1],
                      set->qtable_y[i * 8 + 2], set->qtable_y[i * 8 + 3],
                      set->qtable_y[i * 8 + 4], set->qtable_y[i * 8 + 5],
                      set->qtable_y[i * 8 + 6], set->qtable_y[i * 8 + 7]);

        kmpp_logi("set jpeg qtable u:\n");
        for (i = 0; i < 8; i++)
            kmpp_logi("%2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d\n",
                      set->qtable_u[i * 8 + 0], set->qtable_u[i * 8 + 1],
                      set->qtable_u[i * 8 + 2], set->qtable_u[i * 8 + 3],
                      set->qtable_u[i * 8 + 4], set->qtable_u[i * 8 + 5],
                      set->qtable_u[i * 8 + 6], set->qtable_u[i * 8 + 7]);

        kmpp_logi("set jpeg qtable v:\n");
        for (i = 0; i < 8; i++)
            kmpp_logi("%2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d\n",
                      set->qtable_v[i * 8 + 0], set->qtable_v[i * 8 + 1],
                      set->qtable_v[i * 8 + 2], set->qtable_v[i * 8 + 3],
                      set->qtable_v[i * 8 + 4], set->qtable_v[i * 8 + 5],
                      set->qtable_v[i * 8 + 6], set->qtable_v[i * 8 + 7]);
    }

    if (!cfg->q_mode && !set->q_mode) {
        set->q_mode = JPEG_QUANT;
        set->quant = 10;
        rc_set->rc_mode = MPP_ENC_RC_MODE_FIXQP;
        update_rc = 1;

        kmpp_loge("set jpeg default quant 10\n");
    }
    kmpp_memcpy(cfg, set, sizeof(*cfg));

    if (update_rc)
        rc_cfg->rc_mode = rc_set->rc_mode;
    cfg->update = 1;
}

static void proc_vp8_cfg(MppEncImpl *enc)
{
}

static MPP_RET mpp_enc_proc_user_data(MppEncImpl *enc, void *param)
{
    MppEncUserData *user_data = (MppEncUserData *)param;
    RK_U32 i = 0;

    if (!enc->rb_userdata.free_cnt) {
        kmpp_loge_f("user data is overflow");
        return MPP_NOK;
    }

    i = enc->rb_userdata.write_pos % MAX_USRDATA_CNT;

    if (user_data->len > MAX_USRDATA_LEN) {
        kmpp_loge_f("usr data is bigger than %d byte len %d", MAX_USRDATA_LEN, user_data->len);
        return MPP_NOK;
    }

    if (kmpp_copy_from_user(&enc->rb_userdata.data[i], user_data->pdata,
                            user_data->len))
        return -EFAULT;
    enc->rb_userdata.len[i] = user_data->len;
    enc->rb_userdata.write_pos = i + 1;
    enc->rb_userdata.free_cnt--;
    return MPP_OK;
}

static MPP_RET mpp_enc_proc_ref_cfg(MppEncImpl *enc, void *param)
{
    MppEncRefCfg src = NULL;
    MppEncCfgSet *cfg = enc->cfg;
    MppEncRefCfg dst = cfg->ref_cfg;
    MppEncRefParam *ref_p = (MppEncRefParam *)param;
    MPP_RET ret = MPP_OK;

    if (ref_p->cfg_mode && !src)
        mpp_enc_ref_cfg_init(&src);

    if (!dst) {
        mpp_enc_ref_cfg_init(&dst);
        cfg->ref_cfg = dst;
    }

    switch (ref_p->cfg_mode) {
    case REF_IPPP: {
        src = mpp_enc_ref_default();
    } break;
    case REF_TSVC1:
    case REF_TSVC2:
    case REF_TSVC3: {
        mpi_enc_gen_ref_cfg(src, ref_p->cfg_mode);
    } break;
    case REF_VI: {
        mpi_enc_gen_smart_gop_ref_cfg(src, ref_p);
    } break;
    case REF_HIR_SKIP: {
        mpi_enc_gen_hir_skip_ref(src, ref_p);
    } break;
    default: {
        kmpp_loge_f("ref param_error");
    } break;
    }
    if (src) {
        ret = mpp_enc_ref_cfg_copy(dst, src);
        if (ret)
            kmpp_loge_f("failed to copy ref cfg ret %d\n", ret);

        ret = mpp_enc_refs_set_cfg(enc->refs, dst);
        if (ret)
            kmpp_loge_f("failed to set ref cfg ret %d\n", ret);

        if (mpp_enc_refs_update_hdr(enc->refs))
            enc->hdr_status.val = 0;

        if (ref_p->cfg_mode && src)
            mpp_enc_ref_cfg_deinit(&src);
    }
    kmpp_memcpy(&cfg->ref_param, param, sizeof(MppEncRefParam));

    return ret;
}

MPP_RET mpp_enc_proc_cfg(MppEncImpl *enc, MpiCmd cmd, void *param)
{
    MppEncCfgSet *cfg = enc->cfg;
    MPP_RET ret = MPP_OK;

    switch (cmd) {
    case MPP_ENC_SET_CFG: {
        KmppObj in_obj = (KmppObj)param;
        MppEncCfgSet *set = enc->set;

        /* update all element to temporal storage */
        ret = kmpp_obj_update(enc->set_obj, in_obj);
        if (ret) {
            kmpp_loge_f("failed to update set_obj\n");
            break;
        }

        /* check all date status in set_obj */
        /* 1. base cfg -> check smt mode */
        proc_base_cfg(enc);

        /* 2. prep cfg -> chech and show resolution update */
        proc_prep_cfg(enc);

        /* 3. rc cfg -> check and show bps update */
        proc_rc_cfg(enc);

        /* 4. split cfg -> check with restore */
        proc_split_cfg(&cfg->split, &set->split);

        /* 5. hw cfg -> check with restore */
        proc_hw_cfg(enc);

        /* 6. tune cfg -> check with restore */
        proc_tune_cfg(enc);

        /* 7. codec config */
        switch (enc->coding) {
        case MPP_VIDEO_CodingAVC : {
            proc_h264_cfg(enc);
        } break;
        case MPP_VIDEO_CodingHEVC : {
            proc_h265_cfg(enc);
        } break;
        case MPP_VIDEO_CodingMJPEG : {
            proc_jpeg_cfg(enc);
        } break;
        case MPP_VIDEO_CodingVP8 : {
            proc_vp8_cfg(enc);
        } break;
        default : {
            kmpp_loge_f("unsupport coding %d\n", enc->coding);
        } break;
        }

        if (enc->cfg->rc.refresh_en)
            mpp_enc_refs_set_refresh_length(enc->refs, enc->cfg->rc.refresh_length);

        ret = enc_impl_proc_cfg(enc->impl, cmd, (void *)set);
    } break;
    case MPP_ENC_SET_IDR_FRAME: {
        enc->frm_cfg.force_idr++;
        /* resend header on idr request */
        enc_set_resend_hdr(enc, "idr frame request");
    } break;
    case MPP_ENC_GET_HDR_SYNC:
    case MPP_ENC_GET_EXTRA_INFO: {
        /*
             * NOTE: get stream header should use user's KmppPacket
             * If we provide internal KmppPacket to external user
             * we do not known when the buffer usage is finished.
             * So encoder always write its header to external buffer
             * which is provided by user.
             */
        if (!enc->hdr_status.ready) {
            enc_impl_gen_hdr(enc->impl, enc->hdr_pkt);
            kmpp_packet_get_length(enc->hdr_pkt, &enc->hdr_len);
            enc->hdr_status.ready = 1;
        }

        if (cmd == MPP_ENC_GET_EXTRA_INFO) {
            kmpp_loge_f("Please use MPP_ENC_GET_HDR_SYNC instead of unsafe MPP_ENC_GET_EXTRA_INFO\n");
            kmpp_loge_f("NOTE: MPP_ENC_GET_HDR_SYNC needs KmppPacket input\n");

            *(KmppPacket *)param = enc->hdr_pkt;
        } else
            kmpp_packet_copy((KmppPacket)param, enc->hdr_pkt);

        enc->hdr_status.added_by_ctrl = 1;
    } break;
    case MPP_ENC_PRE_ALLOC_BUFF: {
        /* deprecated control */
        kmpp_logi_f("deprecated MPP_ENC_PRE_ALLOC_BUFF control\n");
    } break;
    case MPP_ENC_GET_RC_API_ALL : {
        RcApiQueryAll *query = (RcApiQueryAll *)param;

        rc_brief_get_all(query);
    } break;
    case MPP_ENC_GET_RC_API_BY_TYPE : {
        RcApiQueryType *query = (RcApiQueryType *)param;

        rc_brief_get_by_type(query);
    } break;
    case MPP_ENC_SET_RC_API_CFG : {
        const RcApi *api = (const RcApi *)param;

        if (api)
            rc_api_register(api);
        else
            kmpp_loge_f("NULL RcApi pointer passed to rc_api_register\n");
    } break;
    case MPP_ENC_GET_RC_API_CURRENT : {
        RcApiBrief *dst = (RcApiBrief *)param;

        *dst = enc->rc_brief;
    } break;
    case MPP_ENC_SET_RC_API_CURRENT : {
        RcApiBrief *src = (RcApiBrief *)param;

        if (src->type == enc->coding) {
            enc->rc_brief = *src;
            enc->rc_status.rc_api_user_cfg = 1;
            enc->rc_status.rc_api_updated = 1;
        } else {
            kmpp_loge_f("rc api type %d not match coding %d\n",
                        src->type, enc->coding);
        }
    } break;
    case MPP_ENC_SET_HEADER_MODE: {
        if (param) {
            MppEncHeaderMode mode = *((MppEncHeaderMode *)param);

            if (mode < MPP_ENC_HEADER_MODE_BUTT) {
                enc->hdr_mode = mode;
                enc_dbg_ctrl("header mode set to %d\n", mode);
            } else {
                kmpp_loge_f("invalid header mode %d\n", mode);
                ret = MPP_NOK;
            }
        } else {
            kmpp_loge_f("invalid NULL ptr on setting header mode\n");
            ret = MPP_NOK;
        }
    } break;
    case MPP_ENC_SET_SEI_CFG: {
        if (param) {
            MppEncSeiMode mode = *((MppEncSeiMode *)param);

            if (mode <= MPP_ENC_SEI_MODE_ONE_FRAME) {
                enc->sei_mode = mode;
                enc_dbg_ctrl("sei mode set to %d\n", mode);
            } else {
                kmpp_loge_f("invalid sei mode %d\n", mode);
                ret = MPP_NOK;
            }
        } else {
            kmpp_loge_f("invalid NULL ptr on setting header mode\n");
            ret = MPP_NOK;
        }
    } break;
    case MPP_ENC_SET_REF_CFG: {
        ret = mpp_enc_proc_ref_cfg(enc, param);
        enc_set_rc_updated(enc, "ref cfg update");
    } break;
    case MPP_ENC_SET_OSD_DATA_CFG: {
        kmpp_memcpy(&cfg->osd, param, sizeof(cfg->osd));
    } break;
    case MPP_ENC_SET_ROI_CFG: {
        kmpp_memcpy(&cfg->roi, param, sizeof(cfg->roi));
    } break;
    case MPP_ENC_SET_JPEG_ROI_CFG: {
        kmpp_memcpy(&cfg->jpeg_roi, param, sizeof(cfg->jpeg_roi));
    } break;
    case MPP_ENC_SET_USERDATA: {
        ret = mpp_enc_proc_user_data(enc, param);
    } break;
    default: {
        ret = enc_impl_proc_cfg(enc->impl, cmd, param);
    } break;
    }

    return ret;
}

static void update_rc_cfg_log(MppEncImpl *impl, const char *fmt, ...)
{
    RK_S32 size = impl->rc_cfg_size;
    RK_S32 length = impl->rc_cfg_length;
    char *base = impl->rc_cfg_info + length;

    va_list args;
    va_start(args, fmt);

    length += kmpp_snprintf(base, size - length, fmt, args);
    if (length >= size)
        kmpp_logi_f("rc cfg log is full\n");

    impl->rc_cfg_length = length;

    va_end(args);
}

static void update_user_datas(MppEncImpl *enc, KmppPacket packet,
                              HalEncTask *hal_task)
{
    if (enc->rb_userdata.free_cnt >= MAX_USRDATA_CNT)
        return;

    while (enc->rb_userdata.free_cnt < MAX_USRDATA_CNT) {
        RK_U32 i = enc->rb_userdata.read_pos % MAX_USRDATA_CNT;
        RK_S32 length = 0;

        enc_impl_add_prefix(enc->impl, packet, &length, uuid_usr_data,
                            &enc->rb_userdata.data[i],
                            enc->rb_userdata.len[i]);

        hal_task->sei_length += length;
        hal_task->length += length;
        enc->rb_userdata.free_cnt++;
        enc->rb_userdata.read_pos = i + 1;
    }
}

MPP_RET mpp_enc_proc_rc_update(MppEncImpl *enc)
{
    MPP_RET ret = MPP_OK;

    // check and update rate control api
    if (!enc->rc_status.rc_api_inited || enc->rc_status.rc_api_updated) {
        RcApiBrief *brief = &enc->rc_brief;

        if (enc->rc_ctx) {
            enc_dbg_rc_flow("rc %s deinit\n", rc_api_name(enc->rc_ctx));
            rc_deinit(enc->rc_ctx);
            enc->rc_ctx = NULL;
        }

        /* NOTE: default name is NULL */
        ret = rc_init(&enc->rc_ctx, enc->coding, &brief->name);
        if (ret)
            kmpp_loge("enc %p fail to init rc %s\n", enc, brief->name);
        else
            enc->rc_status.rc_api_inited = 1;

        enc_dbg_rc_flow("rc %s init ret %d\n", rc_api_name(enc->rc_ctx), ret);
        enc->rc_status.rc_api_updated = 0;

        enc->rc_cfg_length = 0;
        update_rc_cfg_log(enc, "%s:", brief->name);
        enc->rc_cfg_pos = enc->rc_cfg_length;
    }

    /* check and update rate control config */
    if (enc->rc_status.rc_api_user_cfg) {
        MppEncCfgSet *set = enc->set;
        MppEncCfgSet *cfg = enc->cfg;
        MppEncRcMode rc_mode = cfg->rc.rc_mode;

        enc_dbg_rc_flow("rc %s update cfg start\n", rc_api_name(enc->rc_ctx));

        ret = rc_update_usr_cfg(enc->rc_ctx, enc->cfg_obj);
        /*
         * leafkvm: count rc model inits (rc_model_init -> bits_model_init for
         * the AVBR/CBR model); every one after the first is a re-init by a
         * runtime cfg change, exported per packet as enc_rc_reinit.
         */
        enc->rc_init_cnt++;
        *mpp_enc_cfg_rc_change(cfg) = 0;

        enc_dbg_rc_flow("rc %s update cfg done\n", rc_api_name(enc->rc_ctx));
        enc->rc_status.rc_api_user_cfg = 0;

        enc->rc_cfg_length = enc->rc_cfg_pos;
        enc->gop_mode = cfg->rc.gop_mode;

        /* The following scenario requires setting qpmap_en to 1 */
        if (!cfg->base.smt1_en && cfg->base.smt3_en &&
            (rc_mode == MPP_ENC_RC_MODE_CBR || rc_mode == MPP_ENC_RC_MODE_SMTRC ||
             rc_mode == MPP_ENC_RC_MODE_SE)) {
            set->tune.qpmap_en = 1;
            cfg->tune.qpmap_en = 1;
        }
    }

    return ret;
}

#define ENC_RUN_FUNC2(func, ctx, task, enc, ret)                               \
    ret = func(ctx, task);                                                 \
    if (ret) {                                                             \
        kmpp_loge_f("enc %p " #func ":%-4d failed return %d", enc,         \
            __LINE__, ret);                                        \
        goto TASK_DONE;                                                \
    }

#define ENC_RUN_FUNC3(func, ctx, task, jpeg_task, enc, ret)                    \
    ret = func(ctx, task, jpeg_task);                                      \
    if (ret) {                                                             \
        kmpp_loge_f("enc %p " #func ":%-4d failed return %d", enc,         \
            __LINE__, ret);                                        \
        goto TASK_DONE;                                                \
    }

static MPP_RET mpp_enc_check_frm_pkt(MppEncImpl *enc)
{
    MPP_RET ret = MPP_OK;

    enc->frm_buf = NULL;
    if (!enc->packet) {
        ret = kmpp_packet_get(&enc->packet);
        if (ret)
            return ret;
    }

    if (enc->frame) {
        KmppShmPtr sptr;
        MppBuffer frm_buf;
        RK_S64 pts, dts;
        rk_u32 eos = 0;

        kmpp_frame_get_buffer(enc->frame, &sptr);
        frm_buf = kmpp_obj_from_shmptr(&sptr);
        kmpp_frame_get_pts(enc->frame, &pts);
        kmpp_frame_get_dts(enc->frame, &dts);

        enc->task_pts = pts;
        enc->frm_buf = frm_buf;

        kmpp_packet_set_pts(enc->packet, pts);
        kmpp_packet_set_dts(enc->packet, dts);

        kmpp_frame_get_eos(enc->frame, &eos);
        if (eos)
            kmpp_packet_set_eos(enc->packet);
        else
            kmpp_packet_clr_eos(enc->packet);
    }

    if (enc->online)
        return enc->frame ? MPP_OK : MPP_NOK;

    return (!enc->frame || !enc->frm_buf) ? MPP_NOK : MPP_OK;
}

MPP_RET mpp_enc_alloc_output_from_bufpool(MppEncImpl *enc)
{
    MPP_RET ret = mpp_enc_check_frm_pkt(enc);

    if (ret)
        return ret;

    kmpp_packet_set_length(enc->packet, 0);
    if (!enc->pkt_buf) {
        /* NOTE: set buffer w * h * 1.5 to avoid buffer overflow */
        MppEncCfgSet *cfg = enc->cfg;
        RK_U32 width = MPP_ALIGN(cfg->prep.width, 16);
        RK_U32 height = MPP_ALIGN(cfg->prep.height, 16);
        RK_U32 size = (enc->coding == MPP_VIDEO_CodingMJPEG) ?
                      (width * height * 3 / 2) :
                      (width * height);
        KmppPacketImpl *pkt = (KmppPacketImpl *)kmpp_obj_to_entry(enc->packet);
        MppBuffer buffer = NULL;
        KmppShmPtr sptr;

        kmpp_assert(size);
        mpp_buffer_get(NULL, &buffer, size);

        kmpp_assert(buffer);
        enc->pkt_buf = &pkt->buf;
        pkt->buf.buf = buffer;
        pkt->buf.start_offset = 0;
        pkt->buf.size = mpp_buffer_get_size(buffer);
        pkt->data.kptr = mpp_buffer_get_ptr(buffer);
        pkt->pos.kptr = pkt->data.kptr;
        pkt->size = mpp_buffer_get_size(buffer);
        pkt->length = 0;
        mpp_buffer_to_shmptr(buffer, &sptr);
        kmpp_packet_set_buffer(enc->packet, &sptr);

        enc_dbg_detail("create output pkt %p buf %p\n", enc->packet,
                       buffer);
    } else {
        KmppShmPtr pos;
        RK_S32 length;

        kmpp_packet_get_pos(enc->packet, &pos);
        kmpp_packet_get_length(enc->packet, &length);
        enc_dbg_detail("output to pkt %p buf %p pos %p length %d\n",
                       enc->packet, enc->pkt_buf, pos.kptr, length);
    }
    return MPP_OK;
}

static MPP_RET mpp_enc_alloc_packet_with_ring_buf(KmppPacket *packet, RingBufPool *pool,
                                                  rk_u32 align, size_t min_size)
{
    MPP_RET ret = MPP_OK;
    KmppPacketImpl *impl = NULL;

    if (!packet) {
        kmpp_loge_f("invalid NULL input\n");
        return MPP_ERR_NULL_PTR;
    }

    ret = kmpp_packet_get(packet);
    if (ret) {
        kmpp_loge_f("kmpp_packet_get failed ret %d\n", ret);
        return MPP_ERR_MALLOC;
    }

    impl = (KmppPacketImpl *)kmpp_obj_to_entry(*packet);
    if (min_size)
        min_size = (min_size + SZ_1K) & (SZ_1K - 1);

    if (pool) {
        if (ring_buf_get_free(pool, &impl->buf, align, min_size, 1)) {
            kmpp_packet_put(*packet);
            *packet = NULL;
            return MPP_ERR_MALLOC;
        }
    }

    if (pool->buf_base)
        impl->data.kptr = impl->pos.kptr = pool->buf_base + impl->buf.start_offset;

    impl->size = impl->buf.size;
    impl->buf.ring_pool = pool;

    return ret;
}

MPP_RET mpp_enc_ringbuf_pool_init(MppEncImpl *enc, RK_U32 size)
{
    MPP_RET ret = MPP_OK;

    ring_buf_pool_get(&enc->ring_pool);

    if (!size)
        return ring_buf_pool_setup(enc->ring_pool, NULL, 0);

    if (!enc->ring_pool || !enc->ring_pool->init_done) {
        MppBuffer buffer = NULL;

        size = MPP_MAX(size, SZ_16K);
        size = MPP_ALIGN(size, SZ_4K);
        if (enc->shared_buf && enc->shared_buf->stream_buf) {
            buffer = enc->shared_buf->stream_buf;
        } else {
            mpp_ring_buffer_get(NULL, &buffer, size);
            if (!buffer) {
                kmpp_loge_f("ring buf get mpp_buf fail\n");
                return MPP_NOK;
            }
        }
        ret = ring_buf_pool_setup(enc->ring_pool, buffer, enc->max_strm_cnt);
        if (ret) {
            kmpp_loge_f("ring buffer %d init failed\n", size);
            mpp_buffer_put(buffer);
            return ret;
        }
        enc->ring_buf_size = size;
    }

    return ret;
}

MPP_RET mpp_enc_alloc_output_from_ringbuf(MppEncImpl *enc)
{
    MPP_RET ret = MPP_NOK;

    if (enc->ring_pool && !enc->ring_pool->init_done) {
        MppEncCfgSet *cfg = enc->cfg;
        RK_U32 width = MPP_ALIGN(cfg->prep.width, 16);
        RK_U32 height = MPP_ALIGN(cfg->prep.height, 16);
        RK_U32 size = (enc->coding == MPP_VIDEO_CodingMJPEG) ?
                      (width * height) :
                      (width * height / 2);

        if (!enc->ring_buf_size)
            enc->ring_buf_size = size;

        ret = mpp_enc_ringbuf_pool_init(enc, enc->ring_buf_size);
    }
    ret = mpp_enc_alloc_packet_with_ring_buf(&enc->packet, enc->ring_pool, 128, 0);
    if (ret) {
        if (ret == MPP_ERR_NULL_PTR)
            enc->pkt_fail_cnt++ ;
        else
            enc->ringbuf_fail_cnt++;
        return ret;
    }
    {
        KmppPacketImpl *pkt = (KmppPacketImpl *)kmpp_obj_to_entry(enc->packet);
        enc->pkt_buf = &pkt->buf;
    }
    ret = mpp_enc_check_frm_pkt(enc);

    return ret;
}

static MPP_RET mpp_enc_alloc_output(MppEncImpl *enc)
{
    MPP_RET ret = MPP_OK;

#ifdef USE_RING_BUF
    ret = mpp_enc_alloc_output_from_ringbuf(enc);
#else
    ret = mpp_enc_alloc_output_from_bufpool(enc);
#endif
    return ret;
}

static MPP_RET mpp_enc_proc_two_pass(MppEncImpl *enc, EncTask *task)
{
    MPP_RET ret = MPP_OK;

    if (mpp_enc_refs_next_frm_is_intra(enc->refs)) {
        EncRcTask *rc_task = &enc->rc_task;
        EncFrmStatus frm_bak = rc_task->frm;
        EncRcTaskInfo rc_info = rc_task->info;
        EncCpbStatus *cpb = &rc_task->cpb;
        EncFrmStatus *frm = &rc_task->frm;
        HalEncTask *hal_task = &task->info.enc;
        EncImpl impl = enc->impl;
        MppEncHal hal = enc->enc_hal;
        KmppPacket packet = hal_task->packet;
        RK_S32 task_len = hal_task->length;
        RK_S32 hw_len = hal_task->hw_length;
        RK_S32 pkt_len;

        kmpp_packet_get_length(packet, &pkt_len);
        enc_dbg_detail("task %d two pass mode enter\n", frm->seq_idx);
        rc_task->info = enc->rc_info_prev;

        enc_dbg_detail("task %d enc proc dpb\n", frm->seq_idx);
        mpp_enc_refs_get_cpb_pass1(enc->refs, cpb);

        enc_dbg_frm_status("frm %d start ***********************************\n", cpb->curr.seq_idx);
        ENC_RUN_FUNC2(enc_impl_proc_dpb, impl, hal_task, enc, ret);

        enc_dbg_detail("task %d enc proc hal\n", frm->seq_idx);
        ENC_RUN_FUNC2(enc_impl_proc_hal, impl, hal_task, enc, ret);

        enc_dbg_detail("task %d hal get task\n", frm->seq_idx);
        ENC_RUN_FUNC2(mpp_enc_hal_get_task, hal, hal_task, enc, ret);

        enc_dbg_detail("task %d hal generate reg\n", frm->seq_idx);
        ENC_RUN_FUNC2(mpp_enc_hal_gen_regs, hal, hal_task, enc, ret);

        enc_dbg_detail("task %d hal start\n", frm->seq_idx);
        ENC_RUN_FUNC3(mpp_enc_hal_start, hal, hal_task, NULL, enc,
                      ret);

        enc_dbg_detail("task %d hal wait\n", frm->seq_idx);
        ENC_RUN_FUNC2(mpp_enc_hal_wait, hal, hal_task, enc, ret);

        //enc_dbg_detail("task %d hal ret task\n", frm->seq_idx);
        ENC_RUN_FUNC3(mpp_enc_hal_ret_task, hal, hal_task, NULL, enc,
                      ret);

        //recover status & packet
        kmpp_packet_set_length(packet, pkt_len);
        hal_task->hw_length = hw_len;
        hal_task->length = task_len;

        *frm = frm_bak;
        rc_task->info = rc_info;

        enc_dbg_detail("task %d two pass mode leave\n", frm->seq_idx);
    }
TASK_DONE:
    return ret;
}

static void mpp_enc_rc_info_backup(MppEncImpl *enc)
{
    if (enc->online)
        return;

    enc->rc_info_prev = enc->rc_task.info;
}

static MPP_RET mpp_enc_normal_cfg(MppEncImpl *enc, EncTask *task)
{
    MppEncCfgSet *cfg = enc->cfg;
    EncImpl impl = enc->impl;
    MppEncHal hal = enc->enc_hal;
    EncRcTask *rc_task = &enc->rc_task;
    MppEncHeaderStatus *hdr_status = &enc->hdr_status;
    EncCpbStatus *cpb = &rc_task->cpb;
    EncFrmStatus *frm = &rc_task->frm;
    HalEncTask *hal_task = &task->info.enc;
    KmppPacket packet = hal_task->packet;
    MppEncRefFrmUsrCfg *frm_cfg = &enc->frm_cfg;
    MPP_RET ret = MPP_OK;

    rc_task->info.scene_mode = (RK_S32)cfg->tune.scene_mode;
    rc_task->info.last_scene_mode = enc->last_scene_mode;
    enc->last_scene_mode = rc_task->info.scene_mode;

    if (!enc->online && cfg->rc.debreath_en) {
        if (!frm_cfg->force_flag) {
            ret = mpp_enc_proc_two_pass(enc, task);
            if (ret)
                return ret;
        }
    }
    enc_dbg_detail("task %d enc proc dpb\n", frm->seq_idx);
    mpp_enc_refs_get_cpb(enc->refs, cpb);

    enc_dbg_frm_status("frm %d start ***********************************\n",
                       cpb->curr.seq_idx);
    ENC_RUN_FUNC2(enc_impl_proc_dpb, impl, hal_task, enc, ret);

    enc_dbg_frm_status("frm %d compare\n", cpb->curr.seq_idx);
    enc_dbg_frm_status("seq_idx      %d vs %d\n", frm->seq_idx,
                       cpb->curr.seq_idx);
    enc_dbg_frm_status("is_idr       %d vs %d\n", frm->is_idr,
                       cpb->curr.is_idr);
    enc_dbg_frm_status("is_intra     %d vs %d\n", frm->is_intra,
                       cpb->curr.is_intra);
    enc_dbg_frm_status("is_non_ref   %d vs %d\n", frm->is_non_ref,
                       cpb->curr.is_non_ref);
    enc_dbg_frm_status("is_lt_ref    %d vs %d\n", frm->is_lt_ref,
                       cpb->curr.is_lt_ref);
    enc_dbg_frm_status("lt_idx       %d vs %d\n", frm->lt_idx,
                       cpb->curr.lt_idx);
    enc_dbg_frm_status("temporal_id  %d vs %d\n", frm->temporal_id,
                       cpb->curr.temporal_id);
    enc_dbg_frm_status("frm %d done  ***********************************\n",
                       cpb->curr.seq_idx);

    enc_dbg_detail("task %d rc frame start\n", frm->seq_idx);
    ENC_RUN_FUNC2(rc_frm_start, enc->rc_ctx, rc_task, enc, ret);
    enc_dbg_detail("task %d rc frame start ok\n", frm->seq_idx);

    /* generate header before hardware stream */
    if (enc->hdr_mode == MPP_ENC_HEADER_MODE_EACH_IDR && frm->is_intra &&
        !hdr_status->added_by_change && !hdr_status->added_by_ctrl &&
        !hdr_status->added_by_mode) {
        enc_dbg_detail("task %d IDR header length %d\n", frm->seq_idx,
                       enc->hdr_len);

        kmpp_packet_append(packet, enc->hdr_pkt);

        hal_task->header_length = enc->hdr_len;
        hal_task->length += enc->hdr_len;
        hdr_status->added_by_mode = 1;
    }
    /* check for header adding */
    check_hal_task_pkt_len(hal_task, "header adding");

    /* Add all prefix info before encoding */
    if (frm->is_idr && enc->sei_mode >= MPP_ENC_SEI_MODE_ONE_SEQ) {
        RK_S32 length = 0;

        enc_impl_add_prefix(impl, packet, &length, uuid_version,
                            enc->version_info, enc->version_length);

        hal_task->sei_length += length;
        hal_task->length += length;

        length = 0;
        enc_impl_add_prefix(impl, packet, &length, uuid_rc_cfg,
                            enc->rc_cfg_info, enc->rc_cfg_length);

        hal_task->sei_length += length;
        hal_task->length += length;
    }

    if (frm->is_i_recovery && enc->cfg->rc.refresh_en) {
        RK_S32 length = 0;

        enc_impl_add_prefix(impl, packet, &length, uuid_refresh_cfg,
                            &enc->cfg->rc.refresh_length, 0);
        hal_task->sei_length += length;
        hal_task->length += length;
    }

    update_user_datas(enc, packet, hal_task);

    /* check for user data adding */
    check_hal_task_pkt_len(hal_task, "user data adding");

    enc_dbg_detail("task %d enc proc hal\n", frm->seq_idx);
    ENC_RUN_FUNC2(enc_impl_proc_hal, impl, hal_task, enc, ret);

    enc_dbg_detail("task %d hal get task\n", frm->seq_idx);
    ENC_RUN_FUNC2(mpp_enc_hal_get_task, hal, hal_task, enc, ret);

    enc_dbg_detail("task %d rc hal start\n", frm->seq_idx);
    ENC_RUN_FUNC2(rc_hal_start, enc->rc_ctx, rc_task, enc, ret);

    enc_dbg_detail("task %d hal generate reg\n", frm->seq_idx);
    ENC_RUN_FUNC2(mpp_enc_hal_gen_regs, hal, hal_task, enc, ret);

TASK_DONE:
    if (ret)
        enc->cfg_fail_cnt++;
    return ret;
}

static MPP_RET mpp_enc_end(MppEncImpl *enc, EncTask *task, EncTask *jpeg_task)
{
    MppEncHal hal = enc->enc_hal;
    EncRcTask *rc_task = &enc->rc_task;
    EncFrmStatus *frm = &rc_task->frm;
    HalEncTask *hal_task = &task->info.enc;
    HalEncTask *jpeg_hal_task = NULL;
    MPP_RET ret = MPP_OK;

    if (jpeg_task)
        jpeg_hal_task = &jpeg_task->info.enc;

    enc_dbg_detail("task %d rc hal end\n", frm->seq_idx);
    ENC_RUN_FUNC2(rc_hal_end, enc->rc_ctx, rc_task, enc, ret);

    enc_dbg_detail("task %d hal ret task\n", frm->seq_idx);
    ret = mpp_enc_hal_ret_task(hal, hal_task, jpeg_hal_task);
    if (ret)
        goto TASK_DONE;

    enc_dbg_detail("task %d rc frame check reenc\n", frm->seq_idx);
    ENC_RUN_FUNC2(rc_frm_check_reenc, enc->rc_ctx, rc_task, enc, ret);

TASK_DONE:
    return ret;
}

static void mpp_enc_clr_rc_cb_info(EncRcTask *rc_task)
{
    EncRcTaskInfo *hal_rc_ret = (EncRcTaskInfo *) &rc_task->info;
    RK_S32          bit_target = hal_rc_ret->bit_target;
    RK_S32          bit_max = hal_rc_ret->bit_max;
    RK_S32          bit_min = hal_rc_ret->bit_min;
    RK_S32          quality_target = hal_rc_ret->quality_target;
    RK_S32          quality_max = hal_rc_ret->quality_max;
    RK_S32          quality_min = hal_rc_ret->quality_min;
    kmpp_memset(hal_rc_ret, 0, sizeof(EncRcTaskInfo));
    hal_rc_ret->bit_target = bit_target;
    hal_rc_ret->bit_max = bit_max;
    hal_rc_ret->bit_min = bit_min;
    hal_rc_ret->quality_target =  quality_target;
    hal_rc_ret->quality_max = quality_max;
    hal_rc_ret->quality_min = quality_min;
    return;
}

static MPP_RET mpp_enc_reenc_simple(MppEncImpl *enc, EncTask *task)
{
    //    MppEncImpl *enc = (MppEncImpl *)mpp->mEnc;
    MppEncHal hal = enc->enc_hal;
    EncRcTask *rc_task = &enc->rc_task;
    EncFrmStatus *frm = &rc_task->frm;
    HalEncTask *hal_task = &task->info.enc;

    MPP_RET ret = MPP_OK;
    enc_dbg_func("enter\n");

    mpp_enc_clr_rc_cb_info(rc_task);
    enc_dbg_detail("task %d enc proc hal\n", frm->seq_idx);
    ENC_RUN_FUNC2(enc_impl_proc_hal, enc->impl, hal_task, enc, ret);

    enc_dbg_detail("task %d hal get task\n", frm->seq_idx);
    ENC_RUN_FUNC2(mpp_enc_hal_get_task, hal, hal_task, enc, ret);

    enc_dbg_detail("task %d rc hal start\n", frm->seq_idx);
    ENC_RUN_FUNC2(rc_hal_start, enc->rc_ctx, rc_task, enc, ret);

    enc_dbg_detail("task %d hal generate reg\n", frm->seq_idx);
    ENC_RUN_FUNC2(mpp_enc_hal_gen_regs, hal, hal_task, enc, ret);

    enc_dbg_detail("task %d reenc %d times %d\n", frm->seq_idx,
                   frm->reencode, frm->reencode_times);
    enc_dbg_func("leave\n");

TASK_DONE:
    return ret;
}

static MPP_RET mpp_enc_reenc_drop(MppEncImpl *enc, EncTask *task)
{
    //    MppEncImpl *enc = (MppEncImpl *)mpp->mEnc;
    EncRcTask *rc_task = &enc->rc_task;
    EncRcTaskInfo *info = &rc_task->info;
    EncFrmStatus *frm = &rc_task->frm;
    HalEncTask *hal_task = &task->info.enc;
    MPP_RET ret = MPP_OK;

    enc_dbg_func("enter\n");
    mpp_enc_refs_rollback(enc->refs);

    info->bit_real = hal_task->length;
    info->quality_real = info->quality_target;

    enc_dbg_detail("task %d rc frame end\n", frm->seq_idx);
    ENC_RUN_FUNC2(rc_frm_end, enc->rc_ctx, rc_task, enc, ret);
    enc->qp_out = rc_task->qp_out;

TASK_DONE:
    enc_dbg_func("leave\n");
    return ret;
}

static MPP_RET mpp_enc_reenc_force_pskip(MppEncImpl *enc, EncTask *task)
{
    //    MppEncImpl *enc = (MppEncImpl *)mpp->mEnc;
    EncImpl impl = enc->impl;
    MppEncRefFrmUsrCfg *frm_cfg = &enc->frm_cfg;
    EncRcTask *rc_task = &enc->rc_task;
    EncCpbStatus *cpb = &rc_task->cpb;
    EncFrmStatus *frm = &rc_task->frm;
    HalEncTask *hal_task = &task->info.enc;
    MPP_RET ret = MPP_OK;

    enc_dbg_func("enter\n");

    frm_cfg->force_pskip++;
    frm_cfg->force_flag |= ENC_FORCE_PSKIP;

    /* NOTE: in some condition the pskip should not happen */

    mpp_enc_refs_rollback(enc->refs);
    mpp_enc_refs_set_usr_cfg(enc->refs, frm_cfg);

    enc_dbg_detail("task %d enc proc dpb\n", frm->seq_idx);
    mpp_enc_refs_get_cpb(enc->refs, cpb);

    enc_dbg_frm_status("frm %d start ***********************************\n",
                       cpb->curr.seq_idx);
    ENC_RUN_FUNC2(enc_impl_proc_dpb, impl, hal_task, enc, ret);

    enc_dbg_detail("task %d enc sw enc start\n", frm->seq_idx);
    ENC_RUN_FUNC2(enc_impl_sw_enc, impl, hal_task, enc, ret);

    enc_dbg_detail("task %d rc frame end\n", frm->seq_idx);
    ENC_RUN_FUNC2(rc_frm_end, enc->rc_ctx, rc_task, enc, ret);
    enc->qp_out = rc_task->qp_out;
    frm_cfg->force_flag = 0;

TASK_DONE:
    enc_dbg_func("leave\n");
    return ret;
}

static void mpp_enc_terminate_task(MppEncImpl *enc, EncTask *task)
{
    enc->frame = NULL;

    if (enc->packet) {
        kmpp_packet_set_length(enc->packet, 0);
        kmpp_packet_ring_buf_put_used(enc->packet);
        kmpp_packet_put(enc->packet);
        enc->packet = NULL;
    }

    reset_enc_task(enc);
    task->status.val = 0;
}

MPP_RET mpp_enc_impl_alloc_task(MppEncImpl *enc)
{
    enc->enc_task = (void *)mpp_calloc(EncTask, 1);
    if (!enc->enc_task)
        return MPP_NOK;
    enc->init_time = kmpp_time_us();
    enc->time_base = kmpp_time_us();
    enc->time_end = kmpp_time_us();
    return MPP_OK;
}

MPP_RET mpp_enc_impl_free_task(MppEncImpl *enc)
{
    if (enc->enc_task) {
        mpp_free(enc->enc_task);
        enc->enc_task = NULL;
    }
    return MPP_OK;
}

MPP_RET mpp_enc_impl_get_roi_osd(MppEncImpl *enc, KmppFrame frame)
{
    MppEncCfgSet *cfg = enc->cfg;
    KmppShmPtr sptr;

    if (cfg->roi.change) {
        kmpp_memcpy(&enc->cur_roi, &cfg->roi, sizeof(enc->cur_roi));
        cfg->roi.change = 0;
    }

    if (cfg->jpeg_roi.change) {
        kmpp_memcpy(&enc->cur_jpeg_roi, &cfg->jpeg_roi, sizeof(enc->cur_jpeg_roi));
        cfg->jpeg_roi.change = 0;
    }

    if (cfg->osd.change) {
        kmpp_memcpy(&enc->cur_osd, &cfg->osd, sizeof(enc->cur_osd));
        cfg->osd.change = 0;
    }

    if (!frame)
        return MPP_OK;

    if (!kmpp_frame_get_meta(frame, &sptr)) {
        KmppMeta meta = kmpp_obj_from_shmptr(&sptr);

        if (enc->cur_roi.change)
            kmpp_meta_set_ptr(meta, KEY_ROI_DATA, &enc->cur_roi);
        if (enc->cur_jpeg_roi.change)
            kmpp_meta_set_ptr(meta, KEY_JPEG_ROI_DATA, &enc->cur_jpeg_roi);
        if (enc->cur_osd.change)
            kmpp_meta_set_ptr(meta, KEY_OSD_DATA3, &enc->cur_osd);
    }

    return MPP_OK;
}

static MPP_RET mpp_enc_check_frm_valid(MppEncImpl *enc)
{
    MppEncCfgSet *cfg = enc->cfg;
    RK_U32 hor_stride = 0, ver_stride = 0;
    RK_U32 width = 0, height = 0;
    MppEncPrepCfg *prep = &cfg->prep;

    if (!enc->frame)
        return MPP_ERR_NULL_PTR;

    if (cfg->split.split_mode)
        return MPP_OK;

    kmpp_frame_get_hor_stride(enc->frame, &hor_stride);
    kmpp_frame_get_ver_stride(enc->frame, &ver_stride);
    kmpp_frame_get_width(enc->frame, &width);
    kmpp_frame_get_height(enc->frame, &height);
    if (prep->rotation == MPP_ENC_ROT_90 || prep->rotation == MPP_ENC_ROT_270)
        MPP_SWAP(RK_U32, width, height);
    if (width < prep->width || height < prep->height) {
        kmpp_logi_f("chan %d frame info no equal set drop: input frame [%d, %d, %d, %d], enc config [%d, %d, %d, %d]",
                    enc->chan_id, width, height, hor_stride, ver_stride, prep->width, prep->height,
                    prep->hor_stride, prep->ver_stride);
        return MPP_NOK;
    }

    return  MPP_OK;
}

MPP_RET mpp_enc_impl_force_pskip(MppEnc ctx, KmppFrame frame, KmppPacket *packet)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;
    MPP_RET ret = MPP_OK;
    EncTask *task = (EncTask *)enc->enc_task;
    EncRcTask *rc_task = &enc->rc_task;
    EncCpbStatus *cpb = &rc_task->cpb;
    EncFrmStatus *frm = &rc_task->frm;
    MppEncRefFrmUsrCfg *frm_cfg = &enc->frm_cfg;
    EncTaskStatus *status = &task->status;
    HalEncTask *hal_task = &task->info.enc;
    EncImpl impl = enc->impl;
    RK_U32 eos = 0;

    if (task->seq_idx == 0) {
        kmpp_loge_f("force pskip should not happen at first frame\n");
        return MPP_NOK;
    }

    enc->frame = frame;
    enc->packet = NULL;
    reset_hal_enc_task(hal_task);
    reset_enc_rc_task(rc_task);
    frm->seq_idx = task->seq_idx++;

    hal_task->rc_task = rc_task;
    hal_task->frm_cfg = frm_cfg;

    rc_task->frame = enc->frame;
    enc_dbg_detail("task seq idx %d start\n", frm->seq_idx);
    ENC_RUN_FUNC2(rc_frm_check_drop, enc->rc_ctx, rc_task, enc, ret);
    status->rc_check_frm_drop = 1;
    enc_dbg_detail("task %d drop %d\n", frm->seq_idx, frm->drop);

    frm_cfg->force_pskip++;
    frm_cfg->force_flag |= ENC_FORCE_PSKIP;
    mpp_enc_refs_set_usr_cfg(enc->refs, frm_cfg);

    hal_task->valid = 1;
    kmpp_assert(hal_task->valid);
    ret = mpp_enc_alloc_output(enc);
    if (ret)
        goto TASK_DONE;
    hal_task->frame = enc->frame;
    hal_task->input = enc->frm_buf;
    hal_task->packet = enc->packet;
    hal_task->output = enc->pkt_buf;

    status->pkt_buf_rdy = 1;

    hal_task->output = enc->pkt_buf;
    kmpp_assert(enc->packet);

    enc_dbg_detail("check_hal_task_pkt_len\n");

    enc_dbg_detail("task %d enc start\n", frm->seq_idx);
    ENC_RUN_FUNC2(enc_impl_start, enc->impl, hal_task, enc, ret);

    mpp_enc_refs_stash(enc->refs);

    enc_dbg_detail("task %d enc proc dpb\n", frm->seq_idx);
    mpp_enc_refs_get_cpb(enc->refs, cpb);
    ENC_RUN_FUNC2(enc_impl_proc_dpb, impl, hal_task, enc, ret);

    enc_dbg_detail("task %d enc sw enc start\n", frm->seq_idx);
    ENC_RUN_FUNC2(enc_impl_sw_enc, impl, hal_task, enc, ret);

    enc_dbg_detail("task %d rc frame end\n", frm->seq_idx);
    ENC_RUN_FUNC2(rc_frm_end, enc->rc_ctx, rc_task, enc, ret);
    enc->qp_out = rc_task->qp_out;
    frm_cfg->force_flag = 0;

TASK_DONE:
    if (ret) {
        mpp_enc_terminate_task(enc, task);
        return ret;
    }
    enc->frame_count++;
    /* setup output packet and meta data */
    kmpp_packet_set_length(enc->packet, hal_task->length);
    kmpp_frame_get_eos(enc->frame, &eos);
    if (enc->frame && eos)
        kmpp_packet_set_flag(enc->packet, KMPP_PACKET_FLAG_EOS);
    kmpp_packet_set_temporal_id(enc->packet, frm->temporal_id);
    if (kmpp_packet_ring_buf_put_used(enc->packet))
        kmpp_loge_f("kmpp_packet_ring_buf_put_used fail\n");
    *packet = enc->packet;
    enc->packet = NULL;

    reset_enc_task(enc);
    mpp_enc_rc_info_backup(enc);
    task->status.val = 0;

    return ret;
}

MPP_RET mpp_enc_impl_reg_cfg(MppEnc ctx, KmppFrame frame)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;
    MPP_RET ret = MPP_OK;
    EncTask *task = (EncTask *)enc->enc_task;
    EncRcTask *rc_task = &enc->rc_task;
    EncFrmStatus *frm = &rc_task->frm;
    MppEncRefFrmUsrCfg *frm_cfg = &enc->frm_cfg;
    MppEncHeaderStatus *hdr_status = &enc->hdr_status;
    EncTaskStatus *status = &task->status;
    HalEncTask *hal_task = &task->info.enc;
    RK_U32 idr_request = 0;
    KmppShmPtr sptr;
    KmppVencNtfy ntfy;

    /* online will no support reenc */
    if (status->rc_reenc) {
        mpp_enc_reenc_simple(enc, task);
        return MPP_OK;
    }

    enc->frame = frame;
    enc->packet = NULL;

    if (mpp_enc_check_frm_valid(enc) != MPP_OK) {
        ret = MPP_NOK;
        goto TASK_DONE;
    }

    if (!kmpp_frame_get_meta(frame, &sptr)) {
        KmppMeta meta = kmpp_obj_from_shmptr(&sptr);

        kmpp_meta_get_s32(meta, KEY_INPUT_IDR_REQ, &idr_request);
    }
    if (idr_request) {
        enc->frm_cfg.force_flag |= ENC_FORCE_IDR;
        enc->hdr_status.val = 0;
    }
    reset_hal_enc_task(hal_task);
    reset_enc_rc_task(rc_task);
    frm->seq_idx = task->seq_idx++;

    hal_task->rc_task = rc_task;
    hal_task->frm_cfg = frm_cfg;

    rc_task->frame = enc->frame;
    enc_dbg_detail("task seq idx %d start\n", frm->seq_idx);
    ENC_RUN_FUNC2(rc_frm_check_drop, enc->rc_ctx, rc_task, enc, ret);
    status->rc_check_frm_drop = 1;
    enc_dbg_detail("task %d drop %d\n", frm->seq_idx, frm->drop);

    /* when the frame should be dropped just return empty packet */
    if (frm->drop) {
        hal_task->valid = 0;
        hal_task->length = 0;
        ret = MPP_NOK;
        goto TASK_DONE;
    }

    if (frm_cfg->force_flag)
        mpp_enc_refs_set_usr_cfg(enc->refs, frm_cfg);

    hal_task->valid = 1;
    kmpp_assert(hal_task->valid);
    ret = mpp_enc_alloc_output(enc);
    if (ret)
        goto TASK_DONE;
    hal_task->frame = enc->frame;
    hal_task->input = enc->frm_buf;
    hal_task->packet = enc->packet;
    hal_task->output = enc->pkt_buf;
    hal_task->online = enc->online;

    status->pkt_buf_rdy = 1;

    hal_task->output = enc->pkt_buf;
    hal_task->chan_id = enc->chan_id;
    kmpp_assert(enc->packet);

    mpp_enc_impl_get_roi_osd(enc, enc->frame);

    /* generate header before hardware stream */
    if (!hdr_status->ready) {
        enc_impl_gen_hdr(enc->impl, enc->hdr_pkt);
        kmpp_packet_get_length(enc->hdr_pkt, &enc->hdr_len);
        hdr_status->ready = 1;

        enc_dbg_detail("task %d update header length %d\n",
                       frm->seq_idx, enc->hdr_len);

        kmpp_packet_append(enc->packet, enc->hdr_pkt);
        hal_task->header_length = enc->hdr_len;
        hal_task->length += enc->hdr_len;
        hdr_status->added_by_change = 1;

        enc_dbg_detail("added_by_change\n");
    }
    enc_dbg_detail("check_hal_task_pkt_len\n");

    check_hal_task_pkt_len(hal_task, "gen_hdr and adding");

    enc_dbg_detail("task %d enc start\n", frm->seq_idx);
    ENC_RUN_FUNC2(enc_impl_start, enc->impl, hal_task, enc, ret);

    /* call external hook for external real-time data setup */
    ntfy = mpp_enc_get_notify(enc);
    if (ntfy) {
        KmppVencNtfyImpl* ntfy_impl = kmpp_obj_to_entry(ntfy);

        ntfy_impl->cmd = KMPP_NOTIFY_VENC_TASK_PREPARE;
        ntfy_impl->chan_id = enc->chan_id;
        ntfy_impl->frame = enc->frame;
        kmpp_venc_notify(ntfy);
    }

    mpp_enc_refs_stash(enc->refs);
    ENC_RUN_FUNC2(mpp_enc_normal_cfg, enc, task, enc, ret);
    frm_cfg->force_flag = 0;

TASK_DONE:
    if (ret)
        mpp_enc_terminate_task(enc, task);

    return ret;
}

MPP_RET mpp_enc_impl_hw_start(MppEnc ctx, MppEnc jpeg_ctx)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;
    EncTask *task = (EncTask *)enc->enc_task;
    EncRcTask *rc_task = &enc->rc_task;
    EncFrmStatus *frm = &rc_task->frm;
    MppEncHal hal = enc->enc_hal;
    HalEncTask *hal_task = &task->info.enc;
    MPP_RET ret = MPP_OK;
    HalEncTask *jpeg_hal_task = NULL;

    if (jpeg_ctx) {
        MppEncImpl *jpeg_enc = (MppEncImpl *)jpeg_ctx;
        EncTask *jpeg_task = (EncTask *)jpeg_enc->enc_task;

        jpeg_hal_task = &jpeg_task->info.enc;
    }

    if (enc->online) {
        ret = mpp_enc_check_frm_valid(enc);
        if (ret)
            goto TASK_DONE;
    }

    enc_dbg_detail("task %d hal start\n", frm->seq_idx);
    ENC_RUN_FUNC3(mpp_enc_hal_start, hal, hal_task, jpeg_hal_task, enc, ret);

TASK_DONE:
    if (ret) {
        mpp_enc_terminate_task(enc, task);
        if (jpeg_ctx) {
            MppEncImpl *jpeg_enc = (MppEncImpl *)jpeg_ctx;
            EncTask *jpeg_task = (EncTask *)jpeg_enc->enc_task;

            mpp_enc_terminate_task(jpeg_enc, jpeg_task);
        }
        enc->cfg_fail_cnt++;
    }

    return ret;
}
static MPP_RET mpp_enc_comb_end_jpeg(MppEnc ctx, KmppPacket *packet)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;
    MPP_RET ret = MPP_OK;
    EncTask *task = (EncTask *)enc->enc_task;
    EncRcTask *rc_task = &enc->rc_task;
    HalEncTask *hal_task = &task->info.enc;
    EncFrmStatus *frm = &rc_task->frm;
    MppEncHal hal = enc->enc_hal;
    RK_U64 dts = 0, pts = 0;
    RK_U32 eos = 0;
    KmppVencNtfy ntfy = mpp_enc_get_notify(ctx);
    KmppVencNtfyImpl* ntfy_impl = ntfy ? kmpp_obj_to_entry(ntfy) : NULL;

    ret = mpp_enc_hal_ret_task(hal, hal_task, NULL);
    if (ret)
        goto TASK_DONE;
    ENC_RUN_FUNC2(rc_hal_end, enc->rc_ctx, rc_task, enc, ret);
    enc_dbg_detail("task %d hal wait\n", frm->seq_idx);
    ENC_RUN_FUNC2(rc_hal_end, enc->rc_ctx, rc_task, enc, ret);
    enc_dbg_detail("task %d rc enc->frame end\n", frm->seq_idx);
    ENC_RUN_FUNC2(rc_frm_end, enc->rc_ctx, rc_task, enc, ret);
    enc->qp_out = rc_task->qp_out;
    enc->time_end = kmpp_time_us();
    enc->frame_count++;

    kmpp_frame_get_dts(hal_task->frame, &dts);
    kmpp_frame_get_pts(hal_task->frame, &pts);

    if (enc->dev && enc->time_base && enc->time_end &&
        ((enc->time_end - enc->time_base) >= (RK_S64)(1000 * 1000)))
        update_hal_info_fps(enc);

    frm->reencode = 0;
    frm->reencode_times = 0;
TASK_DONE:

    /* setup output packet and meta data */
    if (ret) {
        if (ntfy_impl) {
            ntfy_impl->chan_id = enc->chan_id;
            ntfy_impl->frame = enc->frame;
            ntfy_impl->cmd = KMPP_NOTIFY_VENC_TASK_DROP;
            ntfy_impl->drop_type = KMPP_VENC_DROP_ENC_FAILED;
            kmpp_venc_notify(ntfy);
        }

        if (ret == MPP_ERR_INT_BS_OVFL && enc->ring_pool)
            ring_buf_update_min_size(enc->ring_pool, enc->pkt_buf->size);
        enc->frame_force_drop++;
        kmpp_packet_set_length(enc->packet, 0);
        kmpp_packet_ring_buf_put_used(enc->packet);
        kmpp_packet_put(enc->packet);
        enc->packet = NULL;
    } else {
        kmpp_packet_set_length(enc->packet, hal_task->length);
        if (frm->is_intra)
            kmpp_packet_set_flag(enc->packet, KMPP_PACKET_FLAG_INTRA); //set as key frame
        kmpp_frame_get_eos(enc->frame, &eos);
        if (eos)
            kmpp_packet_set_flag(enc->packet, KMPP_PACKET_FLAG_EOS);
        kmpp_packet_set_temporal_id(enc->packet, frm->temporal_id);
        if (kmpp_packet_ring_buf_put_used(enc->packet))
            kmpp_loge_f("kmpp_packet_ring_buf_put_used fail\n");

        if (ntfy_impl) {
            ntfy_impl->chan_id = enc->chan_id;
            ntfy_impl->frame = enc->frame;
            ntfy_impl->cmd = KMPP_NOTIFY_VENC_TASK_DONE;
            ntfy_impl->is_intra = 1;
            kmpp_venc_notify(ntfy);
        }
    }
    enc->frame = NULL;
    *packet = enc->packet;
    enc->packet = NULL;

    /*
     * First return output packet.
     * Then enqueue task back to input port.
     * Final user will release the mpp_frame they had input.
     */
    enc_dbg_detail("task %d enqueue packet pts %lld\n", frm->seq_idx,
                   enc->task_pts);
    reset_enc_task(enc);
    task->status.val = 0;

    return ret;
}

/* leafkvm: copy the frame's rc feedback (after rc_frm_end) into the packet */
static void mpp_enc_set_pkt_stats(MppEncImpl *enc, KmppPacket packet)
{
    KmppPacketImpl *p = (KmppPacketImpl *)kmpp_obj_to_entry(packet);
    EncRcTaskInfo *info = &enc->rc_task.info;

    if (!p)
        return;

    p->enc_valid = 1;
    p->enc_bit_target = info->bit_target;
    p->enc_bit_real = info->bit_real;
    p->enc_qp_avg = info->quality_real;
    p->enc_qp_target = info->quality_target;
    p->enc_qp_min = info->quality_min;
    p->enc_qp_max = info->quality_max;
    p->enc_qp_out = enc->rc_task.qp_out;
    p->enc_madi = info->madi;
    p->enc_madp = info->madp;
    p->enc_iblk4_prop = info->iblk4_prop;
    p->enc_motion_lvl = info->motion_level;
    p->enc_cplx_lvl = info->complex_level;
    p->enc_luma_ave = info->dsp_luma_ave;
    p->enc_rc_reinit = enc->rc_init_cnt ? enc->rc_init_cnt - 1 : 0;
}

MPP_RET mpp_enc_impl_int(MppEnc ctx, MppEnc jpeg_ctx, KmppPacket *packet,
                         KmppPacket *jpeg_packet)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;
    MppEncCfgSet *cfg = enc->cfg;
    MppEncHal hal = enc->enc_hal;
    EncTask *task = (EncTask *)enc->enc_task;
    EncRcTask *rc_task = &enc->rc_task;
    HalEncTask *hal_task = &task->info.enc;
    EncFrmStatus *frm = &rc_task->frm;
    EncTaskStatus *status = &task->status;
    EncTask *jpeg_task = NULL;
    MPP_RET ret = MPP_OK;
    RK_U8 force_idr = 1;
    RK_U32 eos = 0;

    if (jpeg_ctx) {
        MppEncImpl *jpeg_enc = (MppEncImpl *)jpeg_ctx;

        jpeg_task = (EncTask *)jpeg_enc->enc_task;
    }
    enc_dbg_detail("task %d hal wait\n", frm->seq_idx);
    ret = mpp_enc_hal_wait(hal, hal_task);
    if (ret)
        goto TASK_DONE;

    ret = mpp_enc_end(enc, task, jpeg_task);
    if (ret)
        goto TASK_DONE;

    while (frm->reencode && frm->reencode_times < cfg->rc.max_reenc_times) {
        /* only support I frame drop when online case */
        if (enc->online && (!frm->is_idr || !frm->drop))
            break;

        /* only support I frame reenc/drop when ref/recn shared*/
        if (enc->ref_buf_shared && !frm->is_idr)
            break;

        hal_task->length -= hal_task->hw_length;
        kmpp_packet_set_length(enc->packet, hal_task->length);
        hal_task->hw_length = 0;
        status->rc_reenc = 1;
        enc_dbg_detail("task %d reenc %d times %d\n", frm->seq_idx,
                       frm->reencode, frm->reencode_times);

        if (frm->drop) {
            if (!enc->ref_buf_shared && !frm->is_idr) {
                mpp_enc_reenc_drop(enc, task);
                force_idr = 0;
            }
            status->rc_reenc = 0;
            ret = MPP_NOK;
            goto TASK_DONE;
        }

        if (frm->force_pskip && !frm->is_idr && !frm->is_lt_ref) {
            mpp_enc_reenc_force_pskip(enc, task);
            status->rc_reenc = 0;
        }

        if (status->rc_reenc) {
            if (enc->ref_buf_shared && !frm->is_idr) {
                status->rc_reenc = 0;
                break;
            }
            if (jpeg_ctx)
                mpp_enc_comb_end_jpeg(jpeg_ctx, jpeg_packet);
            return MPP_OK;
        }
    }

    enc_dbg_detail("task %d rc enc->frame end\n", frm->seq_idx);
    ENC_RUN_FUNC2(rc_frm_end, enc->rc_ctx, rc_task, enc, ret);
    enc->qp_out = rc_task->qp_out;
    enc->time_end = kmpp_time_us();
    enc->frame_count++;

    if (enc->dev && enc->time_base && enc->time_end &&
        ((enc->time_end - enc->time_base) >= (RK_S64)(1000 * 1000)))
        update_hal_info_fps(enc);

    frm->reencode = 0;
    frm->reencode_times = 0;

TASK_DONE:

    if (ret) {
        if (ret == MPP_ERR_INT_BS_OVFL && enc->ring_pool)
            ring_buf_update_min_size(enc->ring_pool, enc->pkt_buf->size);
        enc->frame_force_drop++;
        if (force_idr) {
            enc->frm_cfg.force_flag |= ENC_FORCE_IDR;
            enc->hdr_status.val = 0;
        }
        kmpp_packet_set_length(enc->packet, 0);
        kmpp_packet_ring_buf_put_used(enc->packet);
        kmpp_packet_put(enc->packet);
        enc->packet = NULL;
    } else {
        RK_U32 flag;

        kmpp_packet_get_flag(enc->packet, &flag);
        /* setup output packet and meta data */
        if (!(flag & KMPP_PACKET_FLAG_PARTITION))
            kmpp_packet_set_length(enc->packet, hal_task->length);
        if (frm->is_intra)
            kmpp_packet_set_flag(enc->packet, KMPP_PACKET_FLAG_INTRA); //set as key frame
        kmpp_frame_get_eos(enc->frame, &eos);
        if (eos)
            kmpp_packet_set_flag(enc->packet, KMPP_PACKET_FLAG_EOS);
        kmpp_packet_set_temporal_id(enc->packet, frm->temporal_id);
        mpp_enc_set_pkt_stats(enc, enc->packet);
        if (kmpp_packet_ring_buf_put_used(enc->packet))
            kmpp_loge_f("kmpp_packet_ring_buf_put_used fail\n");
    }
    *packet = enc->packet;
    enc->packet = NULL;
    /*
     * First return output packet.
     * Then enqueue task back to input port.
     * Final user will release the mpp_frame they had input.
     */
    enc_dbg_detail("task %d enqueue packet pts %lld\n", frm->seq_idx,
                   enc->task_pts);

    {
        RK_U32 is_intra = (cfg->base.coding == MPP_VIDEO_CodingMJPEG || frm->is_intra);
        RK_U64 dts = 0, pts = 0;
        KmppVencNtfy ntfy = mpp_enc_get_notify(ctx);
        KmppVencNtfyImpl* ntfy_impl = ntfy ? kmpp_obj_to_entry(ntfy) : NULL;

        kmpp_frame_get_dts(hal_task->frame, &dts);
        kmpp_frame_get_pts(hal_task->frame, &pts);

        if (ntfy) {
            ntfy_impl->chan_id = enc->chan_id;
            ntfy_impl->frame = enc->frame;
            if (ret == MPP_ERR_INT_SOURCE_MIS) {
                ntfy_impl->cmd = KMPP_NOTIFY_VENC_SOURCE_ID_MISMATCH;
                kmpp_venc_notify(ntfy);
            }

            if (ret) {
                ntfy_impl->cmd = KMPP_NOTIFY_VENC_TASK_DROP;
                ntfy_impl->drop_type = KMPP_VENC_DROP_ENC_FAILED;
            } else {
                ntfy_impl->cmd = KMPP_NOTIFY_VENC_TASK_DONE;
                ntfy_impl->is_intra = is_intra;
            }
            kmpp_venc_notify(ntfy);
        }
    }
    enc->frame = NULL;
    mpp_enc_rc_info_backup(enc);
    reset_enc_task(enc);
    task->status.val = 0;
    if (jpeg_ctx)
        mpp_enc_comb_end_jpeg(jpeg_ctx, jpeg_packet);

    return ret;
}

void mpp_enc_impl_slice_info(MppEnc ctx, void *param, KmppPacket *out_packet)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;
    EncTask *task = (EncTask *)enc->enc_task;
    HalEncTask *hal_task = &task->info.enc;
    MppEncSliceInfo slice_info = *(MppEncSliceInfo *)param;
    KmppPacket packet = hal_task->packet;
    KmppPacket new_packet = NULL;
    EncRcTask *rc_task = &enc->rc_task;
    EncFrmStatus *frm = &rc_task->frm;
    RK_S32 packet_length;
    RK_S32 slice_length;
    RK_S64 pts;
    RK_S64 dts;

    kmpp_packet_get_length(packet, &packet_length);
    enc_dbg_detail("slice_info len %d %s, packet %p len %d\n",
                   slice_info.length, slice_info.last ? "last" : "continue",
                   packet, packet_length);

    /* first slice include header packet */
    slice_length = slice_info.length + packet_length;
    kmpp_packet_set_length(packet, slice_length);
    kmpp_packet_set_flag(packet, KMPP_PACKET_FLAG_PARTITION);
    if (slice_info.last) {
        kmpp_packet_set_flag(packet, KMPP_PACKET_FLAG_EOI);
        enc->packet = packet;
    } else {
        if (frm->is_intra)
            kmpp_packet_set_flag(packet, KMPP_PACKET_FLAG_INTRA);
        kmpp_packet_set_temporal_id(packet, frm->temporal_id);
        if (kmpp_packet_ring_buf_put_used(packet))
            kmpp_loge_f("kmpp_packet_ring_buf_put_used fail.\n");
        *out_packet = packet;
        mpp_enc_alloc_packet_with_ring_buf(&new_packet, enc->ring_pool, 1, 1);
        kmpp_assert(new_packet);
        kmpp_packet_get_pts(packet, &pts);
        kmpp_packet_get_dts(packet, &dts);
        kmpp_packet_set_pts(new_packet, pts);
        kmpp_packet_set_dts(new_packet, dts);
        hal_task->packet = new_packet;
        enc->packet = hal_task->packet;
    }

    return;
}

void mpp_enc_impl_pkt_full_inc(MppEnc ctx)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;
    enc->pkt_full_cnt++;
}

extern int mpp_show_session_info(void *seq, RK_U32 chan_id);
void mpp_enc_impl_poc_debug_info(KmppEnvNode node, MppEnc ctx, RK_U32 chl_id)
{
    MppEncImpl *enc = (MppEncImpl *)ctx;
    MppEncCfgSet *cfg = enc->cfg;
    MppEncHwCfg *hw = &cfg->hw;
    MppEncFineTuneCfg *tune = &cfg->tune;
    EncTask *task = (EncTask *)enc->enc_task;
    RK_S32 source_frate, target_frame_rate;

    kmpp_env_log(node,
                 "\n--------venc chn attr 1----------------------------------------------------------------------------------------------------------\n");
    kmpp_env_log(node, "%8s|%8s|%8s|%6s|%9s|%10s|%10s|%6s|%10s|%11s|%8s|%10s|%12s\n",
                 "ID", "Width", "Height", "Type", "ByFrame",
                 "Sequence", "GopMode", "Prio", "MaxWidth", "MaxHeight",
                 "Online", "RefShare", "ColorRange");

    kmpp_env_log(node, "%8d|%8u|%8u|%6s|%9s|%10u|%10s|%6d|%10d|%11d|%8d|%10d|%12s\n",
                 chl_id, cfg->prep.width, cfg->prep.height, strof_coding_type(cfg->base.coding), "y",
                 task->seq_idx, strof_gop_mode((MppEncRcGopMode)enc->gop_mode), 0,
                 cfg->prep.max_width, cfg->prep.max_height,
                 enc->online, enc->ref_buf_shared, strof_color_range(cfg->prep.range));

    source_frate = cfg->rc.fps_in_num / cfg->rc.fps_in_denom;
    target_frame_rate = cfg->rc.fps_out_num / cfg->rc.fps_out_denom;
    kmpp_env_log(node,
                 "\n--------venc chn attr 2----------------------------------------------------------------------------------------------------------\n");
    kmpp_env_log(node, "%8s|%8s|%8s|%8s|%12s|%12s|%12s|%12s|%10s\n",
                 "ID", "VeStr", "SrcFr", "TarFr", "Timeref",
                 "PixFmt", "RealFps*10", "rotation", "mirror");
    kmpp_env_log(node, "%8d|%8s|%8d|%8d|%12x|%12s|%12u|%12s|%10s\n",
                 chl_id, "y", source_frate, target_frame_rate, (RK_U32)enc->init_time,
                 strof_pixel_fmt(cfg->prep.format), mpp_enc_get_real_fps(enc),
                 strof_rotation(cfg->prep.rotation), strof_bool(cfg->prep.mirroring));

    if (enc->ring_pool) {
        kmpp_env_log(node,
                     "\n--------ring buf status----------------------------------------------------------------------------------------------------------\n");

        kmpp_env_log(node, "%8s|%8s|%8s|%8s|%10s|%10s|%10s|%10s|%12s\n",
                     "ID", "w_pos", "r_pos",
                     "usd_len", "total_len", "min_size",
                     "l_w_pos", "l_r_pos", "max_use_len");
        kmpp_env_log(node, "%8d|%8d|%8d|%8d|%10d|%10d|%10d|%10d|%12d\n",
                     chl_id, enc->ring_pool->w_pos, enc->ring_pool->r_pos,
                     enc->ring_pool->use_len, enc->ring_pool->len, enc->ring_pool->min_buf_size,
                     enc->ring_pool->l_w_pos, enc->ring_pool->l_r_pos,
                     enc->ring_pool->max_use_len);
    }

    kmpp_env_log(node,
                 "\n--------hw status----------------------------------------------------------------------------------------------------------------\n");
    kmpp_env_log(node, "%8s|%8s|%12s|%14s|%14s|%14s|%16s|%14s\n", "ID",
                 "hw_run", "enc_status", "pkt_fail_cnt",
                 "ring_fail_cnt", "cfg_fail_cnt", "start_fail_cnt", "pkt_full_cnt");
    kmpp_env_log(node, "%8d|%8d|%12d|%14u|%14u|%14u|%16u|%14u\n", chl_id,
                 kmpp_atomic_read(enc->hw_run), enc->enc_status, enc->pkt_fail_cnt,
                 enc->ringbuf_fail_cnt, enc->cfg_fail_cnt, enc->start_fail_cnt, enc->pkt_full_cnt);

    if (cfg->base.coding == MPP_VIDEO_CodingAVC || cfg->base.coding == MPP_VIDEO_CodingHEVC) {
        kmpp_env_log(node,
                     "\n--------hw param-----------------------------------------------------------------------------------------------------------------\n");
        kmpp_env_log(node, "%8s|%14s|%12s|%12s|%12s|%12s|%12s|%12s|\n", "ID",
                     "qp_dlt_row_i", "qp_dlt_row", "qbias_en", "qbias_i", "qbias_p", "flt_str_i", "flt_str_p");
        kmpp_env_log(node, "%8u|%14d|%12d|%12d|%12d|%12d|%12d|%12d\n", chl_id,
                     hw->qp_delta_row_i, hw->qp_delta_row, hw->qbias_en, hw->qbias_i, hw->qbias_p,
                     hw->flt_str_i, hw->flt_str_p);
        kmpp_env_log(node, "%8s|%24s\n", "ID", "thrd_i[0] ~ thrd_i[15]");
        kmpp_env_log(node, "%8u|%4u|%4u|%4u|%4u|%4u|%4u|%4u|%4u|%4u|%4u|%4u|%4u|%4u|%4u|%4u|%4u\n",
                     chl_id, hw->aq_thrd_i[0], hw->aq_thrd_i[1], hw->aq_thrd_i[2], hw->aq_thrd_i[3],
                     hw->aq_thrd_i[4], hw->aq_thrd_i[5], hw->aq_thrd_i[6], hw->aq_thrd_i[7],
                     hw->aq_thrd_i[8], hw->aq_thrd_i[9], hw->aq_thrd_i[10], hw->aq_thrd_i[11],
                     hw->aq_thrd_i[12], hw->aq_thrd_i[13], hw->aq_thrd_i[14], hw->aq_thrd_i[15]);
        kmpp_env_log(node, "%8s|%24s\n", "ID", "thrd_p[0] ~ thrd_p[15]");
        kmpp_env_log(node, "%8u|%4u|%4u|%4u|%4u|%4u|%4u|%4u|%4u|%4u|%4u|%4u|%4u|%4u|%4u|%4u|%4u\n",
                     chl_id, hw->aq_thrd_p[0], hw->aq_thrd_p[1], hw->aq_thrd_p[2], hw->aq_thrd_p[3],
                     hw->aq_thrd_p[4], hw->aq_thrd_p[5], hw->aq_thrd_p[6], hw->aq_thrd_p[7],
                     hw->aq_thrd_p[8], hw->aq_thrd_p[9], hw->aq_thrd_p[10], hw->aq_thrd_p[11],
                     hw->aq_thrd_p[12], hw->aq_thrd_p[13], hw->aq_thrd_p[14], hw->aq_thrd_p[15]);
        kmpp_env_log(node, "%8s|%24s\n", "ID", "step_i[0] ~ step_i[15]");
        kmpp_env_log(node, "%8u|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d\n",
                     chl_id, hw->aq_step_i[0], hw->aq_step_i[1], hw->aq_step_i[2], hw->aq_step_i[3],
                     hw->aq_step_i[4], hw->aq_step_i[5], hw->aq_step_i[6], hw->aq_step_i[7],
                     hw->aq_step_i[8], hw->aq_step_i[9], hw->aq_step_i[10], hw->aq_step_i[11],
                     hw->aq_step_i[12], hw->aq_step_i[13], hw->aq_step_i[14], hw->aq_step_i[15]);
        kmpp_env_log(node, "%8s|%24s\n", "ID", "step_p[0] ~ step_p[15]");
        kmpp_env_log(node, "%8u|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d\n",
                     chl_id, hw->aq_step_p[0], hw->aq_step_p[1], hw->aq_step_p[2], hw->aq_step_p[3],
                     hw->aq_step_p[4], hw->aq_step_p[5], hw->aq_step_p[6], hw->aq_step_p[7],
                     hw->aq_step_p[8], hw->aq_step_p[9], hw->aq_step_p[10], hw->aq_step_p[11],
                     hw->aq_step_p[12], hw->aq_step_p[13], hw->aq_step_p[14], hw->aq_step_p[15]);

#if defined(RKVEPU500_SUPPORT) || defined(RKVEPU511_SUPPORT)
        kmpp_env_log(node, "%8s|%24s\n", "ID", "qbias_arr[0] ~ qbias_arr[17]");
        kmpp_env_log(node, "%8u|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d\n",
                     chl_id, hw->qbias_arr[0], hw->qbias_arr[1], hw->qbias_arr[2], hw->qbias_arr[3],
                     hw->qbias_arr[4], hw->qbias_arr[5], hw->qbias_arr[6], hw->qbias_arr[7],
                     hw->qbias_arr[8], hw->qbias_arr[9], hw->qbias_arr[10], hw->qbias_arr[11],
                     hw->qbias_arr[12], hw->qbias_arr[13], hw->qbias_arr[14], hw->qbias_arr[15],
                     hw->qbias_arr[16], hw->qbias_arr[17]);
        kmpp_env_log(node, "%8s|%24s\n", "ID", "aq_rnge_arr[0] ~ aq_rnge_arr[9]");
        kmpp_env_log(node, "%8u|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d|%4d\n",
                     chl_id, hw->aq_rnge_arr[0], hw->aq_rnge_arr[1], hw->aq_rnge_arr[2], hw->aq_rnge_arr[3],
                     hw->aq_rnge_arr[4], hw->aq_rnge_arr[5], hw->aq_rnge_arr[6], hw->aq_rnge_arr[7],
                     hw->aq_rnge_arr[8], hw->aq_rnge_arr[9]);
#endif

        kmpp_env_log(node,
                     "\n--------fine tuning param--------------------------------------------------------------------------------------------------------\n");
        kmpp_env_log(node, "%8s|%12s|%12s|%12s|%12s|%10s|%10s|%12s|%12s|%9s\n", "ID",
                     "scene_mode", "md_swth_en", "deblur_str", "atr_str_i", "atr_str_p",
                     "atl_str", "lambda_idx_p", "lambda_idx_i", "atf_str");
        kmpp_env_log(node, "%8u|%12d|%12d|%12d|%12d|%10d|%10d|%12d|%12d|%9d\n", chl_id,
                     tune->scene_mode, tune->motion_static_switch_enable, tune->deblur_str,
                     tune->atr_str_i, tune->atr_str_p, tune->atl_str, tune->lambda_idx_p,
                     tune->lambda_idx_i, tune->atf_str);

#if defined(RKVEPU500_SUPPORT) || defined(RKVEPU511_SUPPORT)
        kmpp_env_log(node, "%8s|%14s|%12s|%12s|%12s|%12s|%10s|%10s|%10s|%10s\n", "ID",
                     "static_frm_num", "madp16_th", "skip16_wgt", "skip32_wgt", "lgt_chg_lvl",
                     "deblur_en", "qpmap_en", "smt1_en", "smt3_en");
        kmpp_env_log(node, "%8u|%14d|%12d|%12d|%12d|%12d|%10d|%10d|%10d|%10d\n", chl_id,
                     tune->static_frm_num, tune->madp16_th, tune->skip16_wgt, tune->skip32_wgt,
                     tune->lgt_chg_lvl, tune->deblur_en, tune->qpmap_en, cfg->base.smt1_en, cfg->base.smt3_en);
#endif

#if defined(RKVEPU511_SUPPORT)
        kmpp_env_log(node, "%8s|%14s|%14s|%14s|%14s|%12s|%12s|%12s|%12s\n", "ID",
                     "bmap_qpmin_i", "bmap_qpmax_i", "bmap_qpmin_p", "bmap_qpmax_p", "min_bg_fqp",
                     "max_bg_fqp", "min_fg_fqp", "max_fg_fqp");
        kmpp_env_log(node, "%8u|%14d|%14d|%14d|%14d|%12d|%12d|%12d|%12d\n", chl_id,
                     tune->bmap_qpmin_i, tune->bmap_qpmax_i, tune->bmap_qpmin_p, tune->bmap_qpmax_p,
                     tune->min_bg_fqp, tune->max_bg_fqp, tune->min_fg_fqp, tune->max_fg_fqp);
        kmpp_env_log(node, "%8s|%14s|%14s|%14s|%14s|%10s|%10s\n", "ID",
                     "bg_delta_qp_i", "bg_delta_qp_p", "fg_delta_qp_i", "fg_delta_qp_p", "fg_area", "se_mode");
        kmpp_env_log(node, "%8u|%14d|%14d|%14d|%14d|%10d|%10d\n", chl_id,tune->bg_delta_qp_i,
                     tune->bg_delta_qp_p, tune->fg_delta_qp_i, tune->fg_delta_qp_p, tune->fg_area, tune->se_mode);
#endif
    }

    if (cfg->roi.number > 0) {
        int i;
        kmpp_env_log(node,
                     "\n--------venc roi attr ----------------------------------------------------------------------------\n");
        kmpp_env_log(node, "%8s|%8s|%8s|%8s|%8s|%8s|%8s|%8s|%8s\n", "ID", "roi",
                     "x", "y", "w", "h",
                     "quality", "intra", "abs_qp");
        for (i = 0; i < cfg->roi.number; i++) {
            kmpp_env_log(node, "%8d|%8d|%8d|%8d|%8d|%8d|%8d|%8d|%8d\n", chl_id, i,
                         cfg->roi.regions[i].x, cfg->roi.regions[i].y, cfg->roi.regions[i].w, cfg->roi.regions[i].h,
                         cfg->roi.regions[i].quality, cfg->roi.regions[i].intra, cfg->roi.regions[i].abs_qp_en);
        }
    }

    mpp_show_session_info(kmpp_env_to_seq(node), chl_id);
    enc_impl_proc_debug(node, enc->impl, chl_id);
    rc_proc_show(node, enc->rc_ctx, chl_id);
}

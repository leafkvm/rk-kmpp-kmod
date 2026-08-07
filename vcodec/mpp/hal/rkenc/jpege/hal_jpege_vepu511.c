/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG  "hal_jpege_v511"

#define pr_fmt(fmt) MODULE_TAG ":" fmt

#include "kmpp_string.h"

#include "mpp_mem.h"
#include "mpp_maths.h"
#include "kmpp_frame.h"
#include "hal_jpege_debug.h"
#include "jpege_syntax.h"
#include "hal_bufs.h"
#include "rkv_enc_def.h"
#include "vepu541_common.h"
#include "vepu511_common.h"
#include "hal_jpege_vepu511.h"
#include "hal_jpege_vepu511_reg.h"
#include "hal_jpege_hdr.h"
#include "kmpp_obj.h"
#include "kmpp_buffer.h"
#include "kmpp_packet.h"
#include "rk-dvbm.h"
#include "kmpp_meta.h"

typedef struct JpegeV511HalContext_t {
    MppEncHalApi api;
    MppDev dev;
    void *regs;
    void *reg_out;

    void *dump_files;

    RK_S32 frame_type;
    RK_S32 last_frame_type;

    /* @frame_cnt starts from ZERO */
    RK_U32 frame_cnt;
    Vepu511OsdCfg osd_cfg;
    void *roi_data;
    MppEncCfgSet *cfg;

    RK_U32 frame_size;
    RK_S32 max_buf_cnt;
    RK_S32 hdr_status;
    void *input_fmt;
    RK_U8 *src_buf;
    RK_U8 *dst_buf;
    RK_S32 buf_size;
    RK_U32 frame_num;
    RK_S32 fbc_header_len;
    RK_U32 title_num;

    JpegeBits bits;
    JpegeSyntax syntax;
    RK_S32  online;
    RK_U32  session_run;
    HalJpegeRc hal_rc;
    Vepu511JpegCfg jpeg_cfg;

    MppBuffer h264e_out;
} JpegeV511HalContext;

MPP_RET hal_jpege_v511_init(void *hal, MppEncHalCfg *cfg)
{
    MPP_RET ret = MPP_OK;
    JpegeV511HalContext *ctx = (JpegeV511HalContext *) hal;
    JpegV511RegSet *regs = NULL;

    hal_jpege_enter();

    ctx->reg_out = mpp_calloc(JpegV511Status, 1);
    ctx->regs = mpp_calloc(JpegV511RegSet, 1);
    ctx->input_fmt = mpp_calloc(VepuFmtCfg, 1);
    ctx->cfg = cfg->cfg;

    ctx->frame_cnt = 0;
    ctx->online = cfg->online;
    regs = (JpegV511RegSet *)ctx->regs;
    ctx->dev = cfg->dev;
    ctx->osd_cfg.reg_base = (void *)&regs->reg_osd;
    ctx->osd_cfg.dev = ctx->dev;
    ctx->osd_cfg.osd_data3 = NULL;

    jpege_bits_init(&ctx->bits);
    kmpp_assert(ctx->bits);

    jpege_rc_init(&ctx->hal_rc, ctx->cfg);

    hal_jpege_leave();

    return ret;
}

MPP_RET hal_jpege_v511_deinit(void *hal)
{
    JpegeV511HalContext *ctx = (JpegeV511HalContext *) hal;

    hal_jpege_enter();

    if (ctx->h264e_out) {
        mpp_buffer_put(ctx->h264e_out);
        ctx->h264e_out = NULL;
    }

    if (ctx->dev) {
        mpp_dev_deinit(ctx->dev);
        ctx->dev = NULL;
    }

    jpege_bits_deinit(ctx->bits);

    MPP_FREE(ctx->regs);
    MPP_FREE(ctx->reg_out);
    MPP_FREE(ctx->input_fmt);

    hal_jpege_leave();

    return MPP_OK;
}

static MPP_RET hal_jpege_v511_prepare(void *hal)
{
    JpegeV511HalContext *ctx = (JpegeV511HalContext *) hal;
    VepuFmtCfg *fmt = (VepuFmtCfg *) ctx->input_fmt;

    hal_jpege_dbg_func("enter %p\n", hal);
    vepu5xx_set_fmt(fmt, ctx->cfg->prep.format);
    hal_jpege_dbg_func("leave %p\n", hal);

    return MPP_OK;
}

static void hal_jpege_vepu511_set_qtable(JpegV511RegSet *regs, const RK_U8 **qtable)
{
    RK_U16 *tbl = &regs->reg_table.qua_tab0[0];
    RK_U32 i, j;

    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++)
            tbl[i * 8 + j] = 0x8000 / qtable[0][j * 8 + i];
    }
    tbl += 64;
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++)
            tbl[i * 8 + j] = 0x8000 / qtable[1][j * 8 + i];
    }
    tbl += 64;
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++)
            tbl[i * 8 + j] = 0x8000 / qtable[1][j * 8 + i];
    }
}

MPP_RET hal_jpege_vepu511_set_osd(JpegeV511HalContext *ctx)
{
    Vepu511JpegCfg *jpeg_cfg = &ctx->jpeg_cfg;
    Vepu511OsdCfg *osd_cfg = &ctx->osd_cfg;
    Vepu511Osd *regs = (Vepu511Osd*) (osd_cfg->reg_base);
    MppEncOSDData3 *osd = osd_cfg->osd_data3;
    MppEncOSDRegion3 *region = osd->region;
    MppEncOSDRegion3 *tmp = region;
    HalEncTask *task = (HalEncTask *) jpeg_cfg->enc_task;
    JpegeSyntax *syn = (JpegeSyntax *) task->syntax.data;
    MppFrame frame = task->frame;
    RK_U32 width = syn->width;
    RK_U32 height = syn->height;
    RK_U32 slice_en;
    RK_U32 num;
    RK_U32 i = 0;
    RK_U32 cur_lt_x, cur_lt_y;
    RK_U32 cur_rb_x, cur_rb_y;
    RK_U32 frame_height;

    if (!osd || osd->num_region == 0)
        return MPP_OK;

    if (osd->num_region > 8) {
        kmpp_loge_f("do NOT support more than 8 regions invalid num %d\n",
                    osd->num_region);
        kmpp_assert(osd->num_region <= 8);
        return MPP_NOK;
    }

    kmpp_frame_get_height(frame, &frame_height);
    slice_en = (frame_height < height) && syn->restart_ri;
    if (slice_en) {
        RK_U32 mcu_w = MPP_ALIGN(width, 16) / 16;
        RK_U32 slice_height = syn->restart_ri / mcu_w * 16;
        RK_U32 cur_slice_index = jpeg_cfg->rst_marker;

        cur_lt_x = 0;
        cur_lt_y = cur_slice_index * slice_height;
        cur_rb_x = MPP_ALIGN(width, 16);
        cur_rb_y = (cur_slice_index + 1) * slice_height;
    }

    num = osd->num_region;
    for (i = 0; i < num; i++, tmp++) {
        KmppBuffer buffer = NULL;
        Vepu511OsdRegion *reg = (Vepu511OsdRegion *)&regs->osd_regions[i];
        VepuFmtCfg fmt_cfg;
        MppFrameFormat fmt = tmp->fmt;
        RK_U32 lt_x = tmp->lt_x, lt_y = tmp->lt_y;
        RK_U32 rb_x = tmp->rb_x, rb_y = tmp->rb_y;
        RK_U32 buf_size;
        MPP_RET ret;

        if (slice_en) {
            if (tmp->lt_x < cur_lt_x || tmp->lt_y < cur_lt_y ||
                tmp->rb_x > cur_rb_x || tmp->rb_y > cur_rb_y)
                continue;

            lt_x -= cur_lt_x;
            lt_y -= cur_lt_y;
            rb_x -= cur_lt_x;
            rb_y -= cur_lt_y;
        }

        vepu5xx_set_fmt(&fmt_cfg, fmt);
        reg->cfg0.osd_en = tmp->enable;
        reg->cfg0.osd_range_trns_en = tmp->range_trns_en;
        reg->cfg0.osd_range_trns_sel = tmp->range_trns_sel;
        reg->cfg0.osd_fmt = fmt_cfg.format;
        reg->cfg0.osd_rbuv_swap = tmp->rbuv_swap;
        reg->cfg1.osd_lt_xcrd = lt_x;
        reg->cfg1.osd_lt_ycrd = lt_y;
        reg->cfg2.osd_rb_xcrd = rb_x;
        reg->cfg2.osd_rb_ycrd = rb_y;
        reg->cfg1.osd_endn = tmp->osd_endn;
        reg->cfg5.osd_stride = tmp->stride;
        reg->cfg5.osd_ch_ds_mode = tmp->ch_ds_mode;

        reg->cfg0.osd_alpha_swap = tmp->alpha_cfg.alpha_swap;
        reg->cfg0.osd_fg_alpha = tmp->alpha_cfg.fg_alpha;
        reg->cfg0.osd_fg_alpha_sel = tmp->alpha_cfg.fg_alpha_sel;

        reg->cfg0.osd_qp_adj_en = tmp->qp_cfg.qp_adj_en;
        reg->cfg8.osd_qp_adj_sel = tmp->qp_cfg.qp_adj_sel;
        reg->cfg8.osd_qp = tmp->qp_cfg.qp;
        reg->cfg8.osd_qp_max = tmp->qp_cfg.qp_max;
        reg->cfg8.osd_qp_min = tmp->qp_cfg.qp_min;
        reg->cfg8.osd_qp_prj = tmp->qp_cfg.qp_prj;

        buffer = kmpp_obj_from_shmptr(&tmp->osd_buf);
        if (buffer) {
            buf_size = kmpp_buffer_get_size(buffer);
            ret = vepu5xx_check_osd_buffer_size(lt_x, lt_y, rb_x, rb_y, tmp->stride,
                                                fmt_cfg.format, buf_size);
            if (ret) {
                reg->cfg0.osd_en = 0;
                continue;
            }
            kmpp_buffer_get_iova_by_device(buffer, (RK_U64 *)&reg->osd_st_addr,
                                           mpp_get_dev(jpeg_cfg->dev));
        }

        kmpp_memcpy(reg->lut, tmp->lut, sizeof(tmp->lut));
    }

    regs->osd_whi_cfg0.osd_csc_yr = 77;
    regs->osd_whi_cfg0.osd_csc_yg = 150;
    regs->osd_whi_cfg0.osd_csc_yb = 29;

    regs->osd_whi_cfg1.osd_csc_ur = -43;
    regs->osd_whi_cfg1.osd_csc_ug = -85;
    regs->osd_whi_cfg1.osd_csc_ub = 128;

    regs->osd_whi_cfg2.osd_csc_vr = 128;
    regs->osd_whi_cfg2.osd_csc_vg = -107;
    regs->osd_whi_cfg2.osd_csc_vb = -21;

    regs->osd_whi_cfg3.osd_csc_ofst_y = 0;
    regs->osd_whi_cfg3.osd_csc_ofst_u = 128;
    regs->osd_whi_cfg3.osd_csc_ofst_v = 128;

    osd_cfg->osd_data3 = NULL;

    return MPP_OK;
}

static void hal_jpege_vepu511_set_roi(JpegeV511HalContext *ctx)
{
    MppJpegROICfg *roi_cfg = (MppJpegROICfg *)ctx->roi_data;
    JpegV511RegSet *regs = ctx->regs;
    Vepu511JpegRoi *reg_regions = &regs->reg_frm.jpeg_frame.roi_cfg[0];
    MppJpegROIRegion *region;
    RK_U32 frame_width = ctx->cfg->prep.width;
    RK_U32 frame_height = ctx->cfg->prep.height;
    RK_S32 i;

    if (roi_cfg == NULL)
        return;

    if (roi_cfg->non_roi_en) {
        if (roi_cfg->non_roi_level <= MPP_MAX_JPEG_ROI_LEVEL) {
            reg_regions->roi_cfg1.frm_rdoq_en = 1;
            reg_regions->roi_cfg1.frm_rdoq_level = roi_cfg->non_roi_level;
        } else {
            kmpp_loge_f("none roi level[%d] is invalid\n", roi_cfg->non_roi_level);
        }
    }

    for (i = 0; i < MPP_MAX_JPEG_ROI_NUM; i++) {
        region = &roi_cfg->regions[i];
        if (!region->roi_en)
            continue;

        if (region->w == 0 || region->h == 0 ||
            region->x + region->w > frame_width ||
            region->y + region->h > frame_height) {
            kmpp_loge_f("region[%d]: x[%d] y[%d] w[%d] h[%d] is invalid, frame width[%d] height[%d]\n",
                        i, region->x, region->y, region->w,
                        region->h, frame_width, frame_height);
            continue;
        }

        if (region->level > MPP_MAX_JPEG_ROI_LEVEL) {
            kmpp_loge_f("region[%d]: roi level[%d] is invalid\n", i, region->level);
            continue;
        }

        reg_regions->roi_cfg0.roi0_rdoq_en = 1;
        reg_regions->roi_cfg0.roi0_rdoq_start_x = MPP_ALIGN(region->x, 16) >> 3;
        reg_regions->roi_cfg0.roi0_rdoq_start_y = MPP_ALIGN(region->y, 16) >> 3;
        reg_regions->roi_cfg0.roi0_rdoq_level = region->level;
        reg_regions->roi_cfg1.roi0_rdoq_width_m1 = (MPP_ALIGN(region->w, 16) >> 3) - 1;
        reg_regions->roi_cfg1.roi0_rdoq_height_m1 = (MPP_ALIGN(region->h, 16) >> 3) - 1;
        reg_regions++;
    }
}

static void vepu511_jpeg_set_dvbm(JpegeV511HalContext *ctx, RK_U32 width)
{
    JpegV511RegSet *regs = ctx->regs;

    if (ctx->online == MPP_ENC_ONLINE_MODE_SW) {
        regs->reg_ctl.vs_ldly.dvbm_ack_soft = 1;
        regs->reg_ctl.vs_ldly.dvbm_ack_sel  = 1;
        regs->reg_ctl.vs_ldly.dvbm_inf_sel  = 1;
        regs->reg_ctl.dvbm_cfg.src_badr_sel = 1;
    }

    regs->reg_ctl.dvbm_cfg.dvbm_en = 1;
    regs->reg_ctl.dvbm_cfg.src_badr_sel = 0;
    regs->reg_ctl.dvbm_cfg.dvbm_vpu_fskp = 0;
    regs->reg_ctl.dvbm_cfg.src_oflw_drop = 1;
    regs->reg_ctl.dvbm_cfg.vepu_expt_type = 0;
    regs->reg_ctl.dvbm_cfg.vinf_dly_cycle = 2;
    regs->reg_ctl.dvbm_cfg.ybuf_full_mgn = MPP_ALIGN(width * 8, SZ_4K) / SZ_4K;
    regs->reg_ctl.dvbm_cfg.ybuf_oflw_mgn = 0;

    regs->reg_frm.enc_id.frame_id = 0;
    regs->reg_frm.enc_id.frm_id_match = 0;
    regs->reg_frm.enc_id.source_id = 0;
    regs->reg_frm.enc_id.src_id_match = 0;
    regs->reg_frm.enc_id.ch_id = 1;
    regs->reg_frm.enc_id.vinf_req_en = 1;
    regs->reg_frm.enc_id.vrsp_rtn_en = 1;

    regs->reg_ctl.int_en.dvbm_err_en = 1;
}

/* hack for rv1126b jpeg wrap enc with dvbm */
static void hal_jpege_v511_set_h264e_regs(void *hal, HalEncTask *task)
{
    JpegeV511HalContext *ctx = (JpegeV511HalContext *) hal;
    JpegV511RegSet *regs = ctx->regs;
    JpegeSyntax *syn = (JpegeSyntax *)task->syntax.data;
    RK_U32 width = syn->width;
    RK_U32 height = syn->height;
    RK_U32 offset_x, offset_y;

    {
        RK_U32 tmp = MPP_MAX(width >> 2, 16), tmp1;
        RK_U32 v_num = (MPP_ALIGN(tmp, 64) / 64) * (MPP_ALIGN(height, 16) / 16);
        RK_U32 j_num = (MPP_ALIGN(width, 64) / 64) * (MPP_ALIGN(height, 16) / 16);

        regs->reg_frm.jpeg_frame.enc_rsl.pp0_jnum_m1 = 3;
        tmp1 = (16 * v_num) % j_num;
        regs->reg_frm.jpeg_frame.enc_rsl.pp0_vnum_m1 = 4 * v_num / j_num - 1 + tmp1 ? 1 : 0;
        regs->reg_frm.enc_rsl.pic_wd8_m1 = MPP_ALIGN(tmp, 16) / 8 - 1;
    }
    // regs->reg_frm.enc_rsl.pic_wd8_m1 = MPP_ALIGN(width, 16) / 8 - 1;
    regs->reg_frm.src_fill.pic_wfill = 0;
    regs->reg_frm.enc_rsl.pic_hd8_m1 = MPP_ALIGN(height, 16) / 8 - 1;
    regs->reg_frm.src_fill.pic_hfill = MPP_ALIGN(height, 16) - height;

    regs->reg_frm.src_fmt.src_cfmt = VEPU541_FMT_YUV420SP;
    regs->reg_frm.src_fmt.out_fmt  = 1;

    regs->reg_frm.src_strd0.src_strd0  = width;
    regs->reg_frm.src_strd1.src_strd1  = width;

    kmpp_frame_get_offset_y(task->frame, &offset_y);
    kmpp_frame_get_offset_x(task->frame, &offset_x);
    regs->reg_frm.pic_ofst.pic_ofst_y = offset_y;
    regs->reg_frm.pic_ofst.pic_ofst_x = offset_x;

    /* set codec regs */
    {
        regs->reg_frm.enc_pic.enc_stnd       = 0;
        regs->reg_frm.enc_pic.cur_frm_ref    = 0;
        regs->reg_frm.enc_pic.bs_scp         = 1;

        regs->reg_frm.synt_nal.nal_ref_idc    = 1;//slice->nal_reference_idc;
        regs->reg_frm.synt_nal.nal_unit_type  = 5;//slice->nalu_type;

        regs->reg_frm.synt_sps.max_fnum       = 12;//sps->log2_max_frame_num_minus4;
        regs->reg_frm.synt_sps.drct_8x8       = 1;//sps->direct8x8_inference;
        regs->reg_frm.synt_sps.mpoc_lm4       = 12;//sps->log2_max_poc_lsb_minus4;
        regs->reg_frm.synt_pps.etpy_mode      = 1;//pps->entropy_coding_mode;
        regs->reg_frm.synt_pps.trns_8x8       = 1;//pps->transform_8x8_mode;
        regs->reg_frm.synt_pps.pic_init_qp    = 26;//pps->pic_init_qp;
        regs->reg_frm.synt_pps.cb_ofst        = -6;//pps->chroma_qp_index_offset;
        regs->reg_frm.synt_pps.cr_ofst        = -6;//pps->second_chroma_qp_index_offset;
        regs->reg_frm.synt_pps.dbf_cp_flg     = 1;//pps->deblocking_filter_control;
        regs->reg_frm.synt_sli0.sli_type      = 2;//(slice->slice_type == H264_I_SLICE) ? (2) : (0);
    }

    /* rdo cfg */
    {
        static RK_U32 h264e_lambda_default[60] = {
            0x00000005, 0x00000006, 0x00000007, 0x00000009,
            0x0000000b, 0x0000000e, 0x00000012, 0x00000016,
            0x0000001c, 0x00000024, 0x0000002d, 0x00000039,
            0x00000048, 0x0000005b, 0x00000073, 0x00000091,
            0x000000b6, 0x000000e6, 0x00000122, 0x0000016d,
            0x000001cc, 0x00000244, 0x000002db, 0x00000399,
            0x00000489, 0x000005b6, 0x00000733, 0x00000912,
            0x00000b6d, 0x00000e66, 0x00001224, 0x000016db,
            0x00001ccc, 0x00002449, 0x00002db7, 0x00003999,
            0x00004892, 0x00005b6f, 0x00007333, 0x00009124,
            0x0000b6de, 0x0000e666, 0x00012249, 0x00016dbc,
            0x0001cccc, 0x00024492, 0x0002db79, 0x00039999,
            0x00048924, 0x0005b6f2, 0x00073333, 0x00091249,
            0x000b6de5, 0x000e6666, 0x00122492, 0x0016dbcb,
            0x001ccccc, 0x00244924, 0x002db796, 0x00399998,
        };
        kmpp_memcpy(regs->reg_param.rdo_wgta_qp_grpa_0_51, &h264e_lambda_default[0], 52*4);

        regs->reg_frm.rdo_mark_mode.p16_interp_num     = 2;
        regs->reg_frm.rdo_mark_mode.p16t8_rdo_num      = 1;
        regs->reg_frm.rdo_mark_mode.p16t4_rmd_num      = 1;
        regs->reg_frm.rdo_mark_mode.p8_interp_num      = 2;
        regs->reg_frm.rdo_mark_mode.p8t8_rdo_num       = 1;
        regs->reg_frm.rdo_mark_mode.p8t4_rmd_num       = 1;
        regs->reg_frm.rdo_mark_mode.iframe_i4_rdo_num  = 1;
        regs->reg_frm.rdo_mark_mode.iframe_i16_rdo_num = 2;
        regs->reg_frm.rdo_mark_mode.i8_rdo_num         = 2;
    }
    /* rc */
    {
        regs->reg_frm.enc_pic.pic_qp    = 51;//qp_target;
        regs->reg_frm.rc_qp.rc_max_qp   = 51;
        regs->reg_frm.rc_qp.rc_min_qp   = 51;
        regs->reg_frm.enc_pic.pic_qp    = 51;
        regs->reg_frm.rc_cfg.rc_ctu_num = MPP_ALIGN(width, 16) / 16;

    }

    /* me */
    {
        Vepu511FrameCfg *reg_frm = &regs->reg_frm;
        Vepu511Param *reg_param = &regs->reg_param;

        reg_frm->me_rnge.cime_srch_dwnh   = 4;
        reg_frm->me_rnge.cime_srch_uph    = 4;
        reg_frm->me_rnge.cime_srch_rgtw   = 4;
        reg_frm->me_rnge.cime_srch_lftw   = 4;
        reg_frm->me_cfg.rme_srch_h        = 0;
        reg_frm->me_cfg.rme_srch_v        = 0;
        reg_frm->me_cfg.srgn_max_num      = 54;
        reg_frm->me_cfg.cime_dist_thre    = 1024;
        reg_frm->me_cfg.rme_dis           = 0;
        reg_frm->me_cfg.fme_dis           = 0;
        reg_frm->me_rnge.dlt_frm_num      = 0x0;
        reg_frm->me_cach.cime_zero_thre   = 64;

        /* CIME: 0x1760 - 0x176C */
        reg_param->me_sqi_cfg.cime_pmv_num      = 1;
        reg_param->me_sqi_cfg.cime_fuse         = 0;
        reg_param->me_sqi_cfg.move_lambda       = 0;
        reg_param->me_sqi_cfg.rime_lvl_mrg      = 1;
        reg_param->me_sqi_cfg.rime_prelvl_en    = 3;
        reg_param->me_sqi_cfg.rime_prersu_en    = 0;
        reg_param->me_sqi_cfg.fme_lvl_mrg       = 0;
        reg_param->cime_mvd_th.cime_mvd_th0     = 16;
        reg_param->cime_mvd_th.cime_mvd_th1     = 48;
        reg_param->cime_mvd_th.cime_mvd_th2     = 80;
        reg_param->cime_madp_th.cime_madp_th    = 16;
        reg_param->cime_madp_th.ratio_consi_cfg = 13;
        reg_param->cime_madp_th.ratio_bmv_dist  = 9;
        reg_param->cime_multi.cime_multi0       = 8;
        reg_param->cime_multi.cime_multi1       = 12;
        reg_param->cime_multi.cime_multi2       = 16;
        reg_param->cime_multi.cime_multi3       = 20;

        /* RFME: 0x1770 - 0x1778 */
        reg_param->rime_mvd_th.rime_mvd_th0   = 1;
        reg_param->rime_mvd_th.rime_mvd_th1   = 2;
        reg_param->rime_mvd_th.fme_madp_th    = 0;
        reg_param->rime_madp_th.rime_madp_th0 = 8;
        reg_param->rime_madp_th.rime_madp_th1 = 16;
        reg_param->rime_multi.rime_multi0     = 4;
        reg_param->rime_multi.rime_multi1     = 8;
        reg_param->rime_multi.rime_multi2     = 12;
    }

    /* io buf */
    {
        if (!ctx->h264e_out) {
            mpp_buffer_get(NULL, &ctx->h264e_out, SZ_8K);
        }

        if (ctx->h264e_out) {
            RK_U32 size = mpp_buffer_get_size(ctx->h264e_out);
            RK_U32 out_addr = mpp_buffer_get_iova(ctx->h264e_out, ctx->dev);

            regs->reg_frm.bsbb_addr = out_addr;
            regs->reg_frm.bsbs_addr = out_addr;
            regs->reg_frm.bsbr_addr = out_addr;
            regs->reg_frm.bsbt_addr = out_addr + size;
        }
    }
}

MPP_RET hal_jpege_v511_gen_regs(void *hal, HalEncTask *task)
{
    JpegeV511HalContext *ctx = (JpegeV511HalContext *) hal;
    JpegV511RegSet *regs = ctx->regs;
    Vepu511ControlCfg *reg_ctl = &regs->reg_ctl;
    Vepu511JpegFrame *jpeg_frm = &regs->reg_frm.jpeg_frame;
    JpegeBits bits = ctx->bits;
    RK_S32 length;
    RK_U8 *buf = mpp_buffer_get_ptr(task->output->buf) + task->output->start_offset;
    size_t size = task->output->size;
    JpegeSyntax *syntax = &ctx->syntax;
    VepuFmtCfg *fmt = (VepuFmtCfg *)ctx->input_fmt;
    Vepu511JpegCfg *cfg = &ctx->jpeg_cfg;
    RK_S32 bitpos;

    hal_jpege_enter();
    kmpp_packet_get_length(task->packet, &length);
    cfg->enc_task = task;
    cfg->jpeg_reg_base = jpeg_frm;
    cfg->dev = ctx->dev;
    cfg->input_fmt = ctx->input_fmt;
    cfg->online = ctx->online;
    kmpp_memset(regs, 0, sizeof(JpegV511RegSet));

    if (syntax->q_mode == JPEG_QFACTOR) {
        syntax->q_factor = 100 - task->rc_task->info.quality_target;
        jpege_rc_update(&ctx->hal_rc, syntax);
    }
    /* write header to output buffer */
    jpege_bits_setup(bits, buf, (RK_U32) size);
    /* seek length bytes data */
    jpege_seek_bits(bits, length << 3);
    /* NOTE: write header will update qtable */
    write_jpeg_header(bits, syntax, &ctx->hal_rc);

    bitpos = jpege_bits_get_bitpos(bits);
    task->length = length;
    /* software JPEG header length of the Hal layer is counted to task->hw_length */
    length = (bitpos + 7) >> 3;
    task->hw_length = length - task->length;
    kmpp_packet_set_length(task->packet, length);

    if (task->output->buf) {
        task->output->use_len = length;
        ring_buf_flush(task->output, 0);
    }

    reg_ctl->enc_clr.safe_clr = 0x0;
    reg_ctl->enc_clr.force_clr = 0x0;
    reg_ctl->enc_clr.dvbm_clr = 0;
    reg_ctl->enc_strt.vepu_cmd = 1;

    reg_ctl->int_en.enc_done_en          = 1;
    reg_ctl->int_en.lkt_node_done_en     = 1;
    reg_ctl->int_en.sclr_done_en         = 1;
    reg_ctl->int_en.vslc_done_en         = 1;
    reg_ctl->int_en.vbsf_oflw_en         = 1;
    reg_ctl->int_en.vbuf_lens_en         = 1;
    reg_ctl->int_en.enc_err_en           = 1;
    reg_ctl->int_en.vsrc_err_en          = 1;
    reg_ctl->int_en.wdg_en               = 1;
    reg_ctl->int_en.lkt_err_int_en       = 1;
    reg_ctl->int_en.lkt_err_stop_en      = 1;
    reg_ctl->int_en.lkt_force_stop_en    = 1;
    reg_ctl->int_en.jslc_done_en         = 1;
    reg_ctl->int_en.jbsf_oflw_en         = 1;
    reg_ctl->int_en.jbuf_lens_en         = 1;
    reg_ctl->int_en.dvbm_err_en          = 0;

    reg_ctl->dtrns_map.jpeg_bus_edin = 0x7;
    reg_ctl->dtrns_map.src_bus_edin = 0x0;
    reg_ctl->dtrns_map.meiw_bus_edin = 0x0;
    reg_ctl->dtrns_map.bsw_bus_edin = 0x0;
    reg_ctl->dtrns_map.lktr_bus_edin = 0x0;
    reg_ctl->dtrns_map.roir_bus_edin = 0x0;
    reg_ctl->dtrns_map.lktw_bus_edin = 0x0;
    reg_ctl->dtrns_map.rec_nfbc_bus_edin = 0x0;
    reg_ctl->dtrns_cfg.jsrc_bus_edin = fmt->src_endian;
    regs->reg_frm.enc_pic.enc_stnd = 2;

    reg_ctl->dtrns_cfg.axi_brsp_cke = 0x0;
    reg_ctl->enc_wdg.vs_load_thd = 0x1fffff;

    if (ctx->online) {
        vepu511_jpeg_set_dvbm(ctx, syntax->width);
        /*
         * hack for rv1126b hw bug:
         * In the case of jpeg wrap encoding only, there is a hw bug,
         * the jpeg encoding speed is not controlled by the isp's line counter.
         * This issue does not occur when jpeg and video combo encoding.
         * But need some speciel configure.
         */
        hal_jpege_v511_set_h264e_regs(ctx, task);
    }
    hal_jpege_vepu511_set_osd(ctx);
    hal_jpege_vepu511_set_roi(ctx);
    vepu511_set_jpeg_reg(cfg);
    hal_jpege_vepu511_set_qtable(regs, ctx->hal_rc.qtables);
    task->jpeg_osd_reg = &regs->reg_osd;
    task->jpeg_tlb_reg = &regs->reg_table;
    ctx->frame_num++;
    hal_jpege_leave();

    return MPP_OK;
}

MPP_RET hal_jpege_v511_start(void *hal, HalEncTask *enc_task)
{
    MPP_RET ret = MPP_OK;
    JpegeV511HalContext *ctx = (JpegeV511HalContext *) hal;
    JpegV511RegSet *hw_regs = ctx->regs;
    JpegV511Status *reg_out = ctx->reg_out;
    MppDevRegWrCfg cfg;
    MppDevRegRdCfg cfg1;
    hal_jpege_enter();

    if (enc_task->flags.err) {
        kmpp_loge_f("enc_task->flags.err %08x, return e arly",
                    enc_task->flags.err);
        return MPP_NOK;
    }

    cfg.reg = &hw_regs->reg_ctl;
    cfg.size = sizeof(Vepu511ControlCfg);
    cfg.offset = VEPU511_CTL_OFFSET;

    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_REG_WR, &cfg);
    if (ret) {
        kmpp_loge_f("set register write failed %d\n", ret);
        return ret;
    }

    cfg.reg = &hw_regs->reg_frm;
    cfg.size = sizeof(Vepu511FrameCfg);
    cfg.offset = VEPU511_FRAME_OFFSET;

    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_REG_WR, &cfg);
    if (ret) {
        kmpp_loge_f("set register write failed %d\n", ret);
        return ret;
    }

    cfg.reg = &hw_regs->reg_param;
    cfg.size = sizeof(Vepu511Param);
    cfg.offset = VEPU511_PARAM_OFFSET;

    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_REG_WR, &cfg);
    if (ret) {
        kmpp_loge_f("set register write failed %d\n", ret);
        return ret;
    }

    cfg.reg = &hw_regs->reg_table;
    cfg.size = sizeof(Vepu511JpegTable);
    cfg.offset = VEPU511_JPEGE_TABLE_OFFSET;

    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_REG_WR, &cfg);
    if (ret) {
        kmpp_loge_f("set register write failed %d\n", ret);
        return ret;
    }

    cfg.reg = &hw_regs->reg_osd;
    cfg.size = sizeof(Vepu511Osd);
    cfg.offset = VEPU511_JPEGE_OSD_OFFSET;

    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_REG_WR, &cfg);
    if (ret) {
        kmpp_loge_f("set register write failed %d\n", ret);
        return ret;
    }

    cfg1.reg = &reg_out->hw_status;
    cfg1.size = sizeof(RK_U32);
    cfg1.offset = VEPU511_JPEGE_INT_OFFSET;

    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_REG_RD, &cfg1);
    if (ret) {
        kmpp_loge_f("set register read failed %d\n", ret);
        return ret;
    }

    cfg1.reg = &reg_out->st;
    cfg1.size = sizeof(JpegV511Status) - 4;
    cfg1.offset = VEPU511_JPEGE_STATUS_OFFSET;

    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_REG_RD, &cfg1);
    if (ret) {
        kmpp_loge_f("set register read failed %d\n", ret);
        return ret;
    }

    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_CMD_SEND, NULL);
    if (ret)
        kmpp_loge_f("send cmd failed %d\n", ret);
    ctx->session_run = 1;
    hal_jpege_leave();

    return ret;
}

//#define DUMP_DATA
static MPP_RET hal_jpege_vepu511_status_check(void *hal)
{
    JpegeV511HalContext *ctx = (JpegeV511HalContext *) hal;
    JpegV511Status *elem = (JpegV511Status *) ctx->reg_out;
    RK_U32 hw_status = elem->hw_status;

    if (hw_status & RKV_ENC_INT_LINKTABLE_FINISH)
        hal_jpege_dbg_detail("RKV_ENC_INT_LINKTABLE_FINISH");

    if (hw_status & RKV_ENC_INT_ONE_FRAME_FINISH)
        hal_jpege_dbg_detail("RKV_ENC_INT_ONE_FRAME_FINISH");

    if (hw_status & RKV_ENC_INT_ONE_SLICE_FINISH)
        hal_jpege_dbg_detail("RKV_ENC_INT_ONE_SLICE_FINISH");

    if (hw_status & RKV_ENC_INT_SAFE_CLEAR_FINISH)
        hal_jpege_dbg_detail("RKV_ENC_INT_SAFE_CLEAR_FINISH");

    if (hw_status & RKV_ENC_INT_BIT_STREAM_OVERFLOW) {
        hal_jpege_dbg_warning("RKV_ENC_INT_BIT_STREAM_OVERFLOW");
        return MPP_ERR_INT_BS_OVFL;
    }

    if (hw_status & RKV_ENC_INT_BUS_WRITE_FULL)
        hal_jpege_dbg_warning("RKV_ENC_INT_BUS_WRITE_FULL");

    if (hw_status & RKV_ENC_INT_BUS_WRITE_ERROR)
        hal_jpege_dbg_warning("RKV_ENC_INT_BUS_WRITE_ERROR");

    if (hw_status & RKV_ENC_INT_BUS_READ_ERROR)
        hal_jpege_dbg_warning("RKV_ENC_INT_BUS_READ_ERROR");

    if (hw_status & RKV_ENC_INT_TIMEOUT_ERROR)
        hal_jpege_dbg_warning("RKV_ENC_INT_TIMEOUT_ERROR");

    if (hw_status & RKV_ENC_INT_JPEG_OVERFLOW) {
        hal_jpege_dbg_warning("JPEG BIT_STREAM_OVERFLOW");
        return MPP_ERR_INT_BS_OVFL;
    }

    return MPP_OK;
}

MPP_RET hal_jpege_v511_wait(void *hal, HalEncTask *task)
{
    MPP_RET ret = MPP_OK;
    JpegeV511HalContext *ctx = (JpegeV511HalContext *) hal;
    HalEncTask *enc_task = task;

    hal_jpege_enter();

    if (enc_task->flags.err) {
        kmpp_loge_f("enc_task->flags.err %08x, return early",
                    enc_task->flags.err);
        return MPP_NOK;
    }

    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_CMD_POLL, NULL);

    hal_jpege_leave();

    return ret;
}

MPP_RET hal_jpege_v511_get_task(void *hal, HalEncTask *task)
{
    JpegeV511HalContext *ctx = (JpegeV511HalContext *) hal;
    JpegeSyntax *syntax = (JpegeSyntax *) task->syntax.data;
    KmppShmPtr sptr;

    hal_jpege_enter();

    kmpp_memcpy(&ctx->syntax, syntax, sizeof(ctx->syntax));
    ctx->last_frame_type = ctx->frame_type;
    ctx->online = task->online;

    if (!kmpp_frame_get_meta(task->frame, &sptr)) {
        KmppMeta meta = kmpp_obj_from_shmptr(&sptr);

        kmpp_meta_get_ptr_d(meta, KEY_JPEG_ROI_DATA, (void**)&ctx->roi_data, NULL);
        kmpp_meta_get_ptr_d(meta, KEY_OSD_DATA3, (void**)&ctx->osd_cfg.osd_data3, NULL);
        /* Set the osd, because rockit needs to release osd buffer. */
        if (ctx->osd_cfg.osd_data3)
            kmpp_meta_set_ptr(meta, KEY_OSD_DATA3, ctx->osd_cfg.osd_data3);
    }

    if (ctx->cfg->jpeg.update) {
        jpege_rc_update(&ctx->hal_rc, syntax);
        ctx->cfg->jpeg.update = 0;
    }

    task->rc_task->frm.is_intra = 1;

    hal_jpege_leave();

    return MPP_OK;
}

MPP_RET hal_jpege_v511_ret_task(void *hal, HalEncTask *task)
{
    EncRcTaskInfo *rc_info = &task->rc_task->info;
    JpegeV511HalContext *ctx = (JpegeV511HalContext *) hal;
    JpegV511Status *elem = (JpegV511Status *)ctx->reg_out;
    MPP_RET ret = MPP_OK;

    hal_jpege_enter();

    ret = hal_jpege_vepu511_status_check(hal);
    if (ret)
        return ret;
    task->hw_length += elem->st.jpeg_head_bits_l32;

    ctx->hal_rc.q_factor_prev = 100 - task->rc_task->info.quality_target;
    task->length += task->hw_length;

    // setup bit length for rate control
    rc_info->bit_real = task->hw_length * 8;
    rc_info->quality_real = rc_info->quality_target;

    if (task->jpeg_overflow)
        return MPP_ERR_INT_BS_OVFL;

    if (!task->hw_length)
        return MPP_NOK;

    ctx->session_run = 0;
    hal_jpege_leave();

    return ret;
}

const MppEncHalApi hal_jpege_vepu511 = {
    .name       = "hal_jpege_v511",
    .coding     = MPP_VIDEO_CodingMJPEG,
    .type       = VPU_CLIENT_RKVENC,
    .ctx_size   = sizeof(JpegeV511HalContext),
    .flag       = 0,
    .init       = hal_jpege_v511_init,
    .deinit     = hal_jpege_v511_deinit,
    .prepare    = hal_jpege_v511_prepare,
    .get_task   = hal_jpege_v511_get_task,
    .gen_regs   = hal_jpege_v511_gen_regs,
    .start      = hal_jpege_v511_start,
    .wait       = hal_jpege_v511_wait,
    .part_start = NULL,
    .part_wait  = NULL,
    .ret_task   = hal_jpege_v511_ret_task,
    .init_cfg   = NULL,
};

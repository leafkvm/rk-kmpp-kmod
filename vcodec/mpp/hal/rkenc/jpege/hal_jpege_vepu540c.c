/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG  "hal_jpege_v540c"

#define pr_fmt(fmt) MODULE_TAG ":" fmt

#include "kmpp_string.h"

#include "mpp_mem.h"
#include "mpp_maths.h"
#include "kmpp_frame.h"
#include "hal_jpege_debug.h"
#include "jpege_syntax.h"
#include "hal_bufs.h"
#include "rkv_enc_def.h"
#include "vepu540c_common.h"
#include "hal_jpege_vepu540c.h"
#include "hal_jpege_vepu540c_reg.h"
#include "hal_jpege_hdr.h"
#include "kmpp_obj.h"
#include "kmpp_buffer.h"
#include "kmpp_packet.h"
#include "rk-dvbm.h"

typedef struct jpegeV540cHalContext_t {
    MppEncHalApi api;
    MppDev dev;
    void *regs;
    void *reg_out;

    void *dump_files;

    RK_S32 frame_type;
    RK_S32 last_frame_type;

    /* @frame_cnt starts from ZERO */
    RK_U32 frame_cnt;
    Vepu540cOsdCfg osd_cfg;
    void *roi_data;
    MppEncCfgSet *cfg;

    RK_U32 enc_mode;
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
    Vepu540cJpegCfg jpeg_cfg;
} jpegeV540cHalContext;

MPP_RET hal_jpege_v540c_init(void *hal, MppEncHalCfg *cfg)
{
    MPP_RET ret = MPP_OK;
    jpegeV540cHalContext *ctx = (jpegeV540cHalContext *) hal;
    JpegV540cRegSet *regs = NULL;

    // mpp_env_get_u32("hal_jpege_debug", &hal_jpege_debug, 0);
    hal_jpege_enter();

    ctx->reg_out = mpp_calloc(JpegV540cStatus, 1);
    ctx->regs = mpp_calloc(JpegV540cRegSet, 1);
    ctx->input_fmt = mpp_calloc(VepuFmtCfg, 1);
    ctx->cfg = cfg->cfg;

    ctx->frame_cnt = 0;
    ctx->enc_mode = 1;
    ctx->online = cfg->online;
    regs = (JpegV540cRegSet *) ctx->regs;
    ctx->dev = cfg->dev;
    ctx->osd_cfg.reg_base = (void *)&regs->reg_osd_cfg.osd_jpeg_cfg;
    ctx->osd_cfg.dev = ctx->dev;
    ctx->osd_cfg.osd_data3 = NULL;
    jpege_bits_init(&ctx->bits);
    kmpp_assert(ctx->bits);

    jpege_rc_init(&ctx->hal_rc, ctx->cfg);

    hal_jpege_leave();

    return ret;
}

MPP_RET hal_jpege_v540c_deinit(void *hal)
{
    jpegeV540cHalContext *ctx = (jpegeV540cHalContext *) hal;

    hal_jpege_enter();

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

static MPP_RET hal_jpege_vepu540c_prepare(void *hal)
{
    jpegeV540cHalContext *ctx = (jpegeV540cHalContext *) hal;
    VepuFmtCfg *fmt = (VepuFmtCfg *) ctx->input_fmt;

    hal_jpege_dbg_func("enter %p\n", hal);
    vepu5xx_set_fmt(fmt, ctx->cfg->prep.format);
    hal_jpege_dbg_func("leave %p\n", hal);

    return MPP_OK;
}
#ifdef HW_DVBM
static void vepu540c_jpeg_set_dvbm(JpegV540cRegSet *regs)
{
    RK_U32 soft_resync = 1;
    RK_U32 frame_match = 0;

    // mpp_env_get_u32("soft_resync", &soft_resync, 1);
    // mpp_env_get_u32("frame_match", &frame_match, 0);
    // mpp_env_get_u32("dvbm_en", &dvbm_en, 1);

    regs->reg_ctl.reg0024_dvbm_cfg.dvbm_en = 1;
    regs->reg_ctl.reg0024_dvbm_cfg.src_badr_sel = 0;
    regs->reg_ctl.reg0024_dvbm_cfg.vinf_frm_match = frame_match;
    regs->reg_ctl.reg0024_dvbm_cfg.vrsp_half_cycle = 15;

    regs->reg_ctl.reg0006_vs_ldly.dvbm_ack_sel = soft_resync;
    regs->reg_ctl.reg0006_vs_ldly.dvbm_ack_soft = 1;
    regs->reg_ctl.reg0006_vs_ldly.dvbm_inf_sel = 0;

    regs->reg_base.reg0194_dvbm_id.ch_id = 1;
    regs->reg_base.reg0194_dvbm_id.frame_id = 0;
    regs->reg_base.reg0194_dvbm_id.vrsp_rtn_en = 1;
}
#else
static void vepu540c_jpeg_set_dvbm(JpegV540cRegSet *regs)
{
    regs->reg_ctl.reg0024_dvbm_cfg.dvbm_en = 1;
    regs->reg_ctl.reg0024_dvbm_cfg.src_badr_sel = 1;
    regs->reg_ctl.reg0024_dvbm_cfg.vinf_frm_match = 0;
    regs->reg_ctl.reg0024_dvbm_cfg.vrsp_half_cycle = 8;

    regs->reg_ctl.reg0006_vs_ldly.dvbm_ack_sel = 1;
    regs->reg_ctl.reg0006_vs_ldly.dvbm_ack_soft = 1;
    regs->reg_ctl.reg0006_vs_ldly.dvbm_inf_sel = 1;

    regs->reg_base.reg0194_dvbm_id.ch_id = 1;
    regs->reg_base.reg0194_dvbm_id.frame_id = 0;
    regs->reg_base.reg0194_dvbm_id.vrsp_rtn_en = 1;
}
#endif

static void hal_jpege_vepu540c_set_qtable(JpegV540cRegSet *regs, const RK_U8 **qtable)
{
    RK_U16 *tbl = &regs->jpeg_table.qua_tab0[0];
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

MPP_RET hal_jpege_vepu540c_set_osd(jpegeV540cHalContext *ctx)
{
    Vepu540cJpegCfg *jpeg_cfg = &ctx->jpeg_cfg;
    Vepu540cOsdCfg *osd_cfg = &ctx->osd_cfg;
    vepu540c_osd_reg *regs = (vepu540c_osd_reg *) (osd_cfg->reg_base);
    MppEncOSDData3 *osd = osd_cfg->osd_data3;
    MppEncOSDRegion3 *region = osd->region;
    MppEncOSDRegion3 *tmp = region;
    HalEncTask *task = (HalEncTask *) jpeg_cfg->enc_task;
    JpegeSyntax *syn = (JpegeSyntax *) task->syntax.data;
    KmppFrame frame = task->frame;
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
        KmppBuffer inv_buffer = NULL;
        KmppBuffer osd_buffer = NULL;
        vepu540c_osd_com *reg = (vepu540c_osd_com *) & regs->osd_cfg[i];
        VepuFmtCfg fmt_cfg;
        MppFrameFormat fmt = tmp->fmt;
        RK_U32 lt_x = tmp->lt_x, lt_y = tmp->lt_y;
        RK_U32 rb_x = tmp->rb_x, rb_y = tmp->rb_y;

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

        reg->cfg0.osd_yg_inv_en = tmp->inv_cfg.yg_inv_en;
        reg->cfg0.osd_uvrb_inv_en = tmp->inv_cfg.uvrb_inv_en;
        reg->cfg0.osd_alpha_inv_en = tmp->inv_cfg.alpha_inv_en;
        reg->cfg0.osd_inv_sel = tmp->inv_cfg.inv_sel;
        reg->cfg2.osd_uv_sw_inv_en = tmp->inv_cfg.uv_sw_inv_en;
        reg->cfg2.osd_inv_size = tmp->inv_cfg.inv_size;
        reg->cfg5.osd_inv_stride = tmp->inv_cfg.inv_stride;

        reg->cfg0.osd_alpha_swap = tmp->alpha_cfg.alpha_swap;
        reg->cfg0.osd_bg_alpha = tmp->alpha_cfg.bg_alpha;
        reg->cfg0.osd_fg_alpha = tmp->alpha_cfg.fg_alpha;
        reg->cfg0.osd_fg_alpha_sel = tmp->alpha_cfg.fg_alpha_sel;

        reg->cfg0.osd_qp_adj_en = tmp->qp_cfg.qp_adj_en;
        reg->cfg8.osd_qp_adj_sel = tmp->qp_cfg.qp_adj_sel;
        reg->cfg8.osd_qp = tmp->qp_cfg.qp;
        reg->cfg8.osd_qp_max = tmp->qp_cfg.qp_max;
        reg->cfg8.osd_qp_min = tmp->qp_cfg.qp_min;
        reg->cfg8.osd_qp_prj = tmp->qp_cfg.qp_prj;

        inv_buffer = kmpp_obj_from_shmptr(&tmp->inv_cfg.inv_buf);
        if (inv_buffer)
            kmpp_buffer_get_iova_by_device(inv_buffer,
                                           (RK_U64 *)&reg->osd_inv_st_addr, mpp_get_dev(jpeg_cfg->dev));

        osd_buffer = kmpp_obj_from_shmptr(&tmp->osd_buf);
        if (osd_buffer)
            kmpp_buffer_get_iova_by_device(osd_buffer,
                                           (RK_U64 *)&reg->osd_st_addr, mpp_get_dev(jpeg_cfg->dev));
        kmpp_memcpy(reg->lut, tmp->lut, sizeof(tmp->lut));
    }

    regs->whi_cfg0.osd_csc_yr = 77;
    regs->whi_cfg0.osd_csc_yg = 150;
    regs->whi_cfg0.osd_csc_yb = 29;

    regs->whi_cfg1.osd_csc_ur = -43;
    regs->whi_cfg1.osd_csc_ug = -85;
    regs->whi_cfg1.osd_csc_ub = 128;

    regs->whi_cfg2.osd_csc_vr = 128;
    regs->whi_cfg2.osd_csc_vg = -107;
    regs->whi_cfg2.osd_csc_vb = -21;

    regs->whi_cfg3.osd_csc_ofst_y = 0;
    regs->whi_cfg3.osd_csc_ofst_u = 128;
    regs->whi_cfg3.osd_csc_ofst_v = 128;

    regs->whi_cfg4.osd_inv_yg_max = 255;
    regs->whi_cfg4.osd_inv_yg_min = 0;
    regs->whi_cfg4.osd_inv_uvrb_max = 255;
    regs->whi_cfg4.osd_inv_uvrb_min = 0;
    regs->whi_cfg5.osd_inv_alpha_max = 255;
    regs->whi_cfg5.osd_inv_alpha_min = 0;

    return MPP_OK;
}

MPP_RET hal_jpege_v540c_gen_regs(void *hal, HalEncTask *task)
{
    jpegeV540cHalContext *ctx = (jpegeV540cHalContext *) hal;
    JpegV540cRegSet *regs = ctx->regs;
    jpeg_vepu540c_control_cfg *reg_ctl = &regs->reg_ctl;
    jpeg_vepu540c_base *reg_base = &regs->reg_base;
    JpegeBits bits = ctx->bits;
    RK_S32 length;
    RK_U8 *buf = mpp_buffer_get_ptr(task->output->buf) + task->output->start_offset;
    size_t size = task->output->size;
    JpegeSyntax *syntax = &ctx->syntax;
    Vepu540cJpegCfg *cfg = &ctx->jpeg_cfg;
    RK_S32 bitpos;

    hal_jpege_enter();
    kmpp_packet_get_length(task->packet, &length);
    cfg->enc_task = task;
    cfg->jpeg_reg_base = &reg_base->jpegReg;
    cfg->dev = ctx->dev;
    cfg->input_fmt = ctx->input_fmt;
    cfg->online = ctx->online;
    kmpp_memset(regs, 0, sizeof(JpegV540cRegSet));

    if (syntax->q_mode == JPEG_QFACTOR) {
        syntax->q_factor = 100 - task->rc_task->info.quality_target;
        jpege_rc_update(&ctx->hal_rc, syntax);
    }
    /* write header to output buffer */
    jpege_bits_setup(bits, buf, (RK_U32) size);
    /* seek length bytes data */
    jpege_seek_bits(bits, length << 3);
    /* NOTE: write header will update qtable */
    if (ctx->cfg->rc.rc_mode != MPP_ENC_RC_MODE_FIXQP) {
        hal_jpege_vepu_rc(ctx, task);
        qtable[0] = ctx->hal_rc.qtable_y;
        qtable[1] = ctx->hal_rc.qtable_u;
    } else {
        qtable[0] = NULL;
        qtable[1] = NULL;
    }
    write_jpeg_header(bits, syntax, qtable);

    bitpos = jpege_bits_get_bitpos(bits);
    task->length = (bitpos + 7) >> 3;
    kmpp_packet_set_length(task->packet, task->length);

    if (task->output->buf) {
        task->output->use_len = task->length;
        ring_buf_flush(task->output, 0);
    }

    reg_ctl->reg0004_enc_strt.lkt_num = 0;
    reg_ctl->reg0004_enc_strt.vepu_cmd = ctx->enc_mode;
    reg_ctl->reg0005_enc_clr.safe_clr = 0x0;
    reg_ctl->reg0005_enc_clr.force_clr = 0x0;

    reg_ctl->reg0008_int_en.enc_done_en = 1;
    reg_ctl->reg0008_int_en.lkt_node_done_en = 1;
    reg_ctl->reg0008_int_en.sclr_done_en = 1;
    reg_ctl->reg0008_int_en.slc_done_en = 1;
    reg_ctl->reg0008_int_en.bsf_oflw_en = 1;
    reg_ctl->reg0008_int_en.brsp_otsd_en = 1;
    reg_ctl->reg0008_int_en.wbus_err_en = 1;
    reg_ctl->reg0008_int_en.rbus_err_en = 1;
    reg_ctl->reg0008_int_en.wdg_en = 1;
    reg_ctl->reg0008_int_en.jbsf_oflw_en = 1;
    reg_ctl->reg0008_int_en.lkt_err_int_en = 0;

    reg_ctl->reg0012_dtrns_map.jpeg_bus_edin = 0x7;
    reg_ctl->reg0012_dtrns_map.src_bus_edin = 0x0;
    reg_ctl->reg0012_dtrns_map.meiw_bus_edin = 0x0;
    reg_ctl->reg0012_dtrns_map.bsw_bus_edin = 0x0;
    reg_ctl->reg0012_dtrns_map.lktr_bus_edin = 0x0;
    reg_ctl->reg0012_dtrns_map.roir_bus_edin = 0x0;
    reg_ctl->reg0012_dtrns_map.lktw_bus_edin = 0x0;
    reg_ctl->reg0012_dtrns_map.rec_nfbc_bus_edin = 0x0;
    reg_base->reg0192_enc_pic.enc_stnd = 2; // disable h264 or hevc

    reg_ctl->reg0013_dtrns_cfg.axi_brsp_cke = 0x0;
    reg_ctl->reg0014_enc_wdg.vs_load_thd = 0x1fffff;
    reg_ctl->reg0014_enc_wdg.rfp_load_thd = 0xff;

    if (ctx->online)
        vepu540c_jpeg_set_dvbm(regs);
    hal_jpege_vepu540c_set_osd(ctx);
    vepu540c_set_jpeg_reg(cfg);
    hal_jpege_vepu540c_set_qtable(regs, ctx->hal_rc.qtables);
    task->jpeg_osd_reg = &regs->reg_osd_cfg.osd_jpeg_cfg;
    task->jpeg_tlb_reg = &regs->jpeg_table;
    ctx->frame_num++;
    hal_jpege_leave();

    return MPP_OK;
}

MPP_RET hal_jpege_v540c_start(void *hal, HalEncTask *enc_task)
{
    MPP_RET ret = MPP_OK;
    jpegeV540cHalContext *ctx = (jpegeV540cHalContext *) hal;
    JpegV540cRegSet *hw_regs = ctx->regs;
    JpegV540cStatus *reg_out = ctx->reg_out;
    MppDevRegWrCfg cfg;
    MppDevRegRdCfg cfg1
    ;
    hal_jpege_enter();

    if (enc_task->flags.err) {
        kmpp_loge_f("enc_task->flags.err %08x, return e arly",
                    enc_task->flags.err);
        return MPP_NOK;
    }

    cfg.reg = (RK_U32 *) & hw_regs->reg_ctl;
    cfg.size = sizeof(jpeg_vepu540c_control_cfg);
    cfg.offset = VEPU540C_CTL_OFFSET;

    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_REG_WR, &cfg);
    if (ret) {
        kmpp_loge_f("set register write failed %d\n", ret);
        return ret;
    }

    cfg.reg = &hw_regs->jpeg_table;
    cfg.size = sizeof(vepu540c_jpeg_tab);
    cfg.offset = VEPU540C_JPEGTAB_OFFSET;

    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_REG_WR, &cfg);
    if (ret) {
        kmpp_loge_f("set register write failed %d\n", ret);
        return ret;
    }

    cfg.reg = &hw_regs->reg_base;
    cfg.size = sizeof(jpeg_vepu540c_base);
    cfg.offset = VEPU540C_BASE_OFFSET;

    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_REG_WR, &cfg);
    if (ret) {
        kmpp_loge_f("set register write failed %d\n", ret);
        return ret;
    }

    cfg.reg = &hw_regs->reg_osd_cfg;
    cfg.size = sizeof(vepu540c_osd_regs);
    cfg.offset = VEPU540C_OSD_OFFSET;

    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_REG_WR, &cfg);
    if (ret) {
        kmpp_loge_f("set register write failed %d\n", ret);
        return ret;
    }

    cfg1.reg = &reg_out->hw_status;
    cfg1.size = sizeof(RK_U32);
    cfg1.offset = VEPU540C_REG_BASE_HW_STATUS;

    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_REG_RD, &cfg1);
    if (ret) {
        kmpp_loge_f("set register read failed %d\n", ret);
        return ret;
    }

    cfg1.reg = &reg_out->st;
    cfg1.size = sizeof(JpegV540cStatus) - 4;
    cfg1.offset = VEPU540C_STATUS_OFFSET;

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
static MPP_RET hal_jpege_vepu540c_status_check(void *hal)
{
    jpegeV540cHalContext *ctx = (jpegeV540cHalContext *) hal;
    JpegV540cStatus *elem = (JpegV540cStatus *) ctx->reg_out;
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

MPP_RET hal_jpege_v540c_wait(void *hal, HalEncTask *task)
{
    MPP_RET ret = MPP_OK;
    jpegeV540cHalContext *ctx = (jpegeV540cHalContext *) hal;
    HalEncTask *enc_task = task;
    JpegV540cStatus *elem = (JpegV540cStatus *) ctx->reg_out;

    hal_jpege_enter();

    if (enc_task->flags.err) {
        kmpp_loge_f("enc_task->flags.err %08x, return early",
                    enc_task->flags.err);
        return MPP_NOK;
    }

    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_CMD_POLL, NULL);
    if (ret) {
        kmpp_loge_f("poll cmd failed %d\n", ret);
        ret = MPP_ERR_VPUHW;
    } else {
        ret = hal_jpege_vepu540c_status_check(hal);
        if (ret)
            return ret;
        task->hw_length += elem->st.jpeg_head_bits_l32;
    }

    hal_jpege_leave();

    return ret;
}

MPP_RET hal_jpege_v540c_get_task(void *hal, HalEncTask *task)
{
    jpegeV540cHalContext *ctx = (jpegeV540cHalContext *) hal;
    JpegeSyntax *syntax = (JpegeSyntax *) task->syntax.data;

    hal_jpege_enter();

    kmpp_memcpy(&ctx->syntax, syntax, sizeof(ctx->syntax));

    ctx->last_frame_type = ctx->frame_type;
    if (ctx->cfg->jpeg.update) {
        jpege_rc_update(&ctx->hal_rc, syntax);
        ctx->cfg->jpeg.update = 0;
    }

    kmpp_frame_get_osd(task->frame, (MppOsd*)&ctx->osd_cfg.osd_data3);

    hal_jpege_leave();

    return MPP_OK;
}

MPP_RET hal_jpege_v540c_ret_task(void *hal, HalEncTask *task)
{

    EncRcTaskInfo *rc_info = &task->rc_task->info;
    jpegeV540cHalContext *ctx = (jpegeV540cHalContext *) hal;
    (void)hal;

    hal_jpege_enter();
    ctx->hal_rc.q_factor_prev = 100 - task->rc_task->info.quality_target;
    task->length += task->hw_length;

    // setup bit length for rate control
    rc_info->bit_real = task->hw_length * 8;
    rc_info->quality_real = rc_info->quality_target;

    if (task->jpeg_overflow)
        return MPP_ERR_INT_BS_OVFL;

    ctx->session_run = 0;
    hal_jpege_leave();

    return MPP_OK;
}

const MppEncHalApi hal_jpege_vepu540c = {
    "hal_jpege_v540c",
    MPP_VIDEO_CodingMJPEG,
    VPU_CLIENT_RKVENC,
    sizeof(jpegeV540cHalContext),
    0,
    hal_jpege_v540c_init,
    hal_jpege_v540c_deinit,
    hal_jpege_vepu540c_prepare,
    hal_jpege_v540c_get_task,
    hal_jpege_v540c_gen_regs,
    hal_jpege_v540c_start,
    hal_jpege_v540c_wait,
    NULL,
    NULL,
    hal_jpege_v540c_ret_task,
    NULL,
    NULL,
    NULL,
};

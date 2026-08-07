/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG  "hal_jpege_v500"

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
#include "vepu500_common.h"
#include "hal_jpege_vepu500.h"
#include "hal_jpege_vepu500_reg.h"
#include "hal_jpege_hdr.h"
#include "kmpp_packet.h"
#include "rk-dvbm.h"
#include "kmpp_meta.h"
#include "kmpp_obj.h"
#include "kmpp_buffer.h"

typedef struct JpegeV500HalContext_t {
    MppEncHalApi api;
    MppDev dev;
    void *regs;
    void *reg_out;

    void *dump_files;

    RK_S32 frame_type;
    RK_S32 last_frame_type;

    /* @frame_cnt starts from ZERO */
    RK_U32 frame_cnt;
    Vepu500OsdCfg osd_cfg;
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
    Vepu500JpegCfg jpeg_cfg;
} JpegeV500HalContext;

MPP_RET hal_jpege_v500_init(void *hal, MppEncHalCfg *cfg)
{
    MPP_RET ret = MPP_OK;
    JpegeV500HalContext *ctx = (JpegeV500HalContext *) hal;
    JpegV500RegSet *regs = NULL;

    hal_jpege_enter();

    ctx->reg_out = mpp_calloc(JpegV500Status, 1);
    ctx->regs = mpp_calloc(JpegV500RegSet, 1);
    ctx->input_fmt = mpp_calloc(VepuFmtCfg, 1);
    ctx->cfg = cfg->cfg;

    ctx->frame_cnt = 0;
    ctx->online = cfg->online;
    regs = (JpegV500RegSet *)ctx->regs;
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

MPP_RET hal_jpege_v500_deinit(void *hal)
{
    JpegeV500HalContext *ctx = (JpegeV500HalContext *) hal;

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

static MPP_RET hal_jpege_v500_prepare(void *hal)
{
    JpegeV500HalContext *ctx = (JpegeV500HalContext *) hal;
    VepuFmtCfg *fmt = (VepuFmtCfg *) ctx->input_fmt;

    hal_jpege_dbg_func("enter %p\n", hal);
    vepu5xx_set_fmt(fmt, ctx->cfg->prep.format);
    hal_jpege_dbg_func("leave %p\n", hal);

    return MPP_OK;
}

static void hal_jpege_vepu500_set_qtable(JpegV500RegSet *regs, const RK_U8 **qtable)
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

MPP_RET hal_jpege_vepu500_set_osd(JpegeV500HalContext *ctx)
{
    Vepu500JpegCfg *jpeg_cfg = &ctx->jpeg_cfg;
    Vepu500OsdCfg *osd_cfg = &ctx->osd_cfg;
    Vepu500Osd *regs = (Vepu500Osd*) (osd_cfg->reg_base);
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
        KmppBuffer buffer = NULL;
        Vepu500OsdRegion *reg = (Vepu500OsdRegion *)&regs->osd_regions[i];
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
        if (buffer)
            kmpp_buffer_get_iova_by_device(buffer,
                                           (RK_U64 *)&reg->osd_st_addr, mpp_get_dev(jpeg_cfg->dev));
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

    return MPP_OK;
}

static void vepu500_jpeg_set_dvbm(JpegeV500HalContext *ctx, RK_U32 width)
{
    JpegV500RegSet *regs = ctx->regs;

    if (ctx->online == MPP_ENC_ONLINE_MODE_SW) {
        regs->reg_ctl.vs_ldly.dvbm_ack_soft = 1;
        regs->reg_ctl.vs_ldly.dvbm_ack_sel  = 1;
        regs->reg_ctl.vs_ldly.dvbm_inf_sel  = 1;
        regs->reg_ctl.dvbm_cfg.src_badr_sel = 1;
    }

    regs->reg_ctl.dvbm_cfg.dvbm_en = 1;
    /* 1: cur frame 0: next frame */
    regs->reg_ctl.dvbm_cfg.ptr_gbck = 0;
    regs->reg_ctl.dvbm_cfg.src_oflw_drop = 1;
    regs->reg_ctl.dvbm_cfg.vepu_expt_type = 0;
    regs->reg_ctl.dvbm_cfg.vinf_dly_cycle = 0;
    regs->reg_ctl.dvbm_cfg.ybuf_full_mgn = MPP_ALIGN(width * 8, SZ_4K) / SZ_4K;
    regs->reg_ctl.dvbm_cfg.ybuf_oflw_mgn = 0;

    regs->reg_frm.enc_id.frame_id = 0;
    regs->reg_frm.enc_id.frm_id_match = 0;
    regs->reg_frm.enc_id.source_id = 0;
    regs->reg_frm.enc_id.src_id_match = 0;
    regs->reg_frm.enc_id.ch_id = 1;
    regs->reg_frm.enc_id.vinf_req_en = 1;
    regs->reg_frm.enc_id.vrsp_rtn_en = 1;
}

MPP_RET hal_jpege_v500_gen_regs(void *hal, HalEncTask *task)
{
    JpegeV500HalContext *ctx = (JpegeV500HalContext *) hal;
    JpegV500RegSet *regs = ctx->regs;
    Vepu500ControlCfg *reg_ctl = &regs->reg_ctl;
    Vepu500JpegFrame *jpeg_frm = &regs->reg_frm.jpeg_frame;
    JpegeBits bits = ctx->bits;
    RK_S32 length;
    RK_U8 *buf = mpp_buffer_get_ptr(task->output->buf) + task->output->start_offset;
    size_t size = task->output->size;
    JpegeSyntax *syntax = &ctx->syntax;
    Vepu500JpegCfg *cfg = &ctx->jpeg_cfg;
    RK_S32 bitpos;

    hal_jpege_enter();
    kmpp_packet_get_length(task->packet, &length);
    cfg->enc_task = task;
    cfg->jpeg_reg_base = jpeg_frm;
    cfg->dev = ctx->dev;
    cfg->input_fmt = ctx->input_fmt;
    cfg->online = ctx->online;
    kmpp_memset(regs, 0, sizeof(JpegV500RegSet));

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
    task->length = (bitpos + 7) >> 3;
    kmpp_packet_set_length(task->packet, task->length);

    if (task->output->buf) {
        task->output->use_len = task->length;
        ring_buf_flush(task->output, 0);
    }

    reg_ctl->enc_clr.safe_clr = 0x0;
    reg_ctl->enc_clr.force_clr = 0x0;
    reg_ctl->enc_clr.dvbm_clr_disable = 1;

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
    regs->reg_frm.video_enc_pic.enc_stnd = 2;   // disable h264 or hevc

    reg_ctl->dtrns_cfg.axi_brsp_cke = 0x0;
    reg_ctl->enc_wdg.vs_load_thd = 0x1fffff;

    if (ctx->online)
        vepu500_jpeg_set_dvbm(ctx, syntax->width);
    hal_jpege_vepu500_set_osd(ctx);
    vepu500_set_jpeg_reg(cfg);
    hal_jpege_vepu500_set_qtable(regs, ctx->hal_rc.qtables);
    task->jpeg_osd_reg = &regs->reg_osd;
    task->jpeg_tlb_reg = &regs->reg_table;
    ctx->frame_num++;
    hal_jpege_leave();

    return MPP_OK;
}

MPP_RET hal_jpege_v500_start(void *hal, HalEncTask *enc_task)
{
    MPP_RET ret = MPP_OK;
    JpegeV500HalContext *ctx = (JpegeV500HalContext *) hal;
    JpegV500RegSet *hw_regs = ctx->regs;
    JpegV500Status *reg_out = ctx->reg_out;
    MppDevRegWrCfg cfg;
    MppDevRegRdCfg cfg1;
    hal_jpege_enter();

    if (enc_task->flags.err) {
        kmpp_loge_f("enc_task->flags.err %08x, return e arly",
                    enc_task->flags.err);
        return MPP_NOK;
    }

    cfg.reg = &hw_regs->reg_ctl;
    cfg.size = sizeof(Vepu500ControlCfg);
    cfg.offset = VEPU500_CTL_OFFSET;

    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_REG_WR, &cfg);
    if (ret) {
        kmpp_loge_f("set register write failed %d\n", ret);
        return ret;
    }

    cfg.reg = &hw_regs->reg_frm;
    cfg.size = sizeof(JpegVepu500Frame);
    cfg.offset = VEPU500_JPEGE_FRAME_OFFSET;

    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_REG_WR, &cfg);
    if (ret) {
        kmpp_loge_f("set register write failed %d\n", ret);
        return ret;
    }

    cfg.reg = &hw_regs->reg_table;
    cfg.size = sizeof(Vepu500JpegTable);
    cfg.offset = VEPU500_JPEGE_TABLE_OFFSET;

    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_REG_WR, &cfg);
    if (ret) {
        kmpp_loge_f("set register write failed %d\n", ret);
        return ret;
    }

    cfg.reg = &hw_regs->reg_osd;
    cfg.size = sizeof(Vepu500Osd);
    cfg.offset = VEPU500_JPEGE_OSD_OFFSET;

    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_REG_WR, &cfg);
    if (ret) {
        kmpp_loge_f("set register write failed %d\n", ret);
        return ret;
    }

    cfg1.reg = &reg_out->hw_status;
    cfg1.size = sizeof(RK_U32);
    cfg1.offset = VEPU500_JPEGE_INT_OFFSET;

    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_REG_RD, &cfg1);
    if (ret) {
        kmpp_loge_f("set register read failed %d\n", ret);
        return ret;
    }

    cfg1.reg = &reg_out->st;
    cfg1.size = sizeof(JpegV500Status) - 4;
    cfg1.offset = VEPU500_JPEGE_STATUS_OFFSET;

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
static MPP_RET hal_jpege_vepu500_status_check(void *hal)
{
    JpegeV500HalContext *ctx = (JpegeV500HalContext *) hal;
    JpegV500Status *elem = (JpegV500Status *) ctx->reg_out;
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

MPP_RET hal_jpege_v500_wait(void *hal, HalEncTask *task)
{
    MPP_RET ret = MPP_OK;
    JpegeV500HalContext *ctx = (JpegeV500HalContext *) hal;
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

MPP_RET hal_jpege_v500_get_task(void *hal, HalEncTask *task)
{
    JpegeV500HalContext *ctx = (JpegeV500HalContext *) hal;
    JpegeSyntax *syntax = (JpegeSyntax *) task->syntax.data;
    KmppShmPtr sptr;

    hal_jpege_enter();

    kmpp_memcpy(&ctx->syntax, syntax, sizeof(ctx->syntax));
    ctx->last_frame_type = ctx->frame_type;
    ctx->online = task->online;

    if (!kmpp_frame_get_meta(task->frame, &sptr)) {
        KmppMeta meta = kmpp_obj_from_shmptr(&sptr);

        kmpp_meta_get_ptr(meta, KEY_OSD_DATA3, (void**)&ctx->osd_cfg.osd_data3);
    }

    if (ctx->cfg->jpeg.update) {
        jpege_rc_update(&ctx->hal_rc, syntax);
        ctx->cfg->jpeg.update = 0;
    }

    hal_jpege_leave();

    return MPP_OK;
}

MPP_RET hal_jpege_v500_ret_task(void *hal, HalEncTask *task)
{
    EncRcTaskInfo *rc_info = &task->rc_task->info;
    JpegeV500HalContext *ctx = (JpegeV500HalContext *) hal;
    JpegV500Status *elem = (JpegV500Status *)ctx->reg_out;
    MPP_RET ret = MPP_OK;

    hal_jpege_enter();

    ret = hal_jpege_vepu500_status_check(hal);
    if (ret)
        return ret;
    task->hw_length += elem->st.jpeg_head_bits_l32;

    ctx->hal_rc.q_factor_prev = task->rc_task->info.quality_target;
    task->length += task->hw_length;

    // setup bit length for rate control
    rc_info->bit_real = task->hw_length * 8;
    rc_info->quality_real = rc_info->quality_target;

    if (task->jpeg_overflow)
        return MPP_ERR_INT_BS_OVFL;

    ctx->session_run = 0;
    hal_jpege_leave();

    return ret;
}

const MppEncHalApi hal_jpege_vepu500 = {
    .name       = "hal_jpege_v500",
    .coding     = MPP_VIDEO_CodingMJPEG,
    .type       = VPU_CLIENT_RKVENC,
    .ctx_size   = sizeof(JpegeV500HalContext),
    .flag       = 0,
    .init       = hal_jpege_v500_init,
    .deinit     = hal_jpege_v500_deinit,
    .prepare    = hal_jpege_v500_prepare,
    .get_task   = hal_jpege_v500_get_task,
    .gen_regs   = hal_jpege_v500_gen_regs,
    .start      = hal_jpege_v500_start,
    .wait       = hal_jpege_v500_wait,
    .part_start = NULL,
    .part_wait  = NULL,
    .ret_task   = hal_jpege_v500_ret_task,
    .init_cfg   = NULL,
};

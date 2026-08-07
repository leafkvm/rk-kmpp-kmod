/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG  "vepu500_common"

#include "kmpp_string.h"

#include "kmpp_log.h"
#include "mpp_mem.h"
#include "mpp_maths.h"
#include "jpege_syntax.h"
#include "vepu500_common.h"
#include "hal_enc_task.h"
#include "kmpp_obj.h"
#include "kmpp_buffer.h"
#include "kmpp_frame.h"
#include "kmpp_packet.h"
#include "mpp_buffer_impl.h"

MPP_RET vepu500_set_roi(Vepu500RoiCfg *roi_reg_base, MppEncROICfg * roi, RK_S32 w, RK_S32 h)
{
    MppEncROIRegion *region = roi->regions;
    Vepu500RoiCfg  *roi_cfg = (Vepu500RoiCfg *)roi_reg_base;
    Vepu500RoiRegion *reg_regions = &roi_cfg->regions[0];
    MPP_RET ret = MPP_NOK;
    RK_S32 i = 0;

    kmpp_memset(reg_regions, 0, sizeof(Vepu500RoiRegion) * 8);
    if (NULL == roi_cfg || NULL == roi) {
        kmpp_loge_f("invalid buf %p roi %p\n", roi_cfg, roi);
        goto DONE;
    }

    if (roi->number > VEPU500_MAX_ROI_NUM) {
        kmpp_loge_f("invalid region number %d\n", roi->number);
        goto DONE;
    }

    /* check region config */
    ret = MPP_OK;
    for (i = 0; i < (RK_S32) roi->number; i++, region++) {
        if (region->x + region->w > w || region->y + region->h > h)
            ret = MPP_NOK;

        if (region->intra > 1 || region->qp_area_idx >= VEPU500_MAX_ROI_NUM ||
            region->area_map_en > 1 || region->abs_qp_en > 1)
            ret = MPP_NOK;

        if ((region->abs_qp_en && region->quality > 51) ||
            (!region->abs_qp_en && (region->quality > 51 || region->quality < -51)))
            ret = MPP_NOK;

        if (ret) {
            kmpp_loge_f("region %d invalid param:\n", i);
            kmpp_loge_f("position [%d:%d:%d:%d] vs [%d:%d]\n",
                        region->x, region->y, region->w, region->h, w, h);
            kmpp_loge_f("force intra %d qp area index %d\n",
                        region->intra, region->qp_area_idx);
            kmpp_loge_f("abs qp mode %d value %d\n",
                        region->abs_qp_en, region->quality);
            goto DONE;
        }

        reg_regions->roi_pos_lt.roi_lt_x = MPP_ALIGN(region->x, 16) >> 4;
        reg_regions->roi_pos_lt.roi_lt_y = MPP_ALIGN(region->y, 16) >> 4;
        reg_regions->roi_pos_rb.roi_rb_x = MPP_ALIGN(region->x + region->w, 16) >> 4;
        reg_regions->roi_pos_rb.roi_rb_y = MPP_ALIGN(region->y + region->h, 16) >> 4;
        reg_regions->roi_base.roi_qp_value = region->quality;
        reg_regions->roi_base.roi_qp_adj_mode = region->abs_qp_en;
        reg_regions->roi_base.roi_en = 1;
        reg_regions->roi_base.roi_pri = 0x1f;
        if (region->intra) {
            reg_regions->roi_mdc.roi_mdc_intra16 = 1;
            reg_regions->roi_mdc.roi0_mdc_intra32_hevc = 1;
        }
        reg_regions++;
    }

DONE:
    return ret;
}

MPP_RET vepu500_set_osd(Vepu500OsdCfg * cfg)
{
    Vepu500Osd *regs = (Vepu500Osd *) (cfg->reg_base);
    MppEncOSDData3 *osd = cfg->osd_data3;
    MppEncOSDRegion3 *region = osd->region;
    MppEncOSDRegion3 *tmp = region;
    RK_U32 num;
    RK_U32 i = 0;

    if (!osd || osd->num_region == 0)
        return MPP_OK;

    if (osd->num_region > 8) {
        kmpp_loge_f("do NOT support more than 8 regions invalid num %d\n",
                    osd->num_region);
        kmpp_assert(osd->num_region <= 8);
        return MPP_NOK;
    }
    num = osd->num_region;
    for (i = 0; i < num; i++, tmp++) {
        KmppBuffer buffer = NULL;
        Vepu500OsdRegion *reg = (Vepu500OsdRegion *) & regs->osd_regions[i];
        VepuFmtCfg fmt_cfg;
        MppFrameFormat fmt = tmp->fmt;

        vepu5xx_set_fmt(&fmt_cfg, fmt);
        reg->cfg0.osd_en = tmp->enable;
        reg->cfg0.osd_range_trns_en = tmp->range_trns_en;
        reg->cfg0.osd_range_trns_sel = tmp->range_trns_sel;
        reg->cfg0.osd_fmt = fmt_cfg.format;
        reg->cfg0.osd_rbuv_swap = tmp->rbuv_swap;
        reg->cfg1.osd_lt_xcrd = tmp->lt_x;
        reg->cfg1.osd_lt_ycrd = tmp->lt_y;
        reg->cfg2.osd_rb_xcrd = tmp->rb_x;
        reg->cfg2.osd_rb_ycrd = tmp->rb_y;
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
                                           (RK_U64 *)&reg->osd_st_addr, mpp_get_dev(cfg->dev));
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

static MPP_RET vepu500_jpeg_set_uv_offset(Vepu500JpegFrame *regs, JpegeSyntax * syn,
                                          VepuFmt input_fmt, HalEncTask *task)
{
    RK_U32 hor_stride, ver_stride;
    RK_U32 frame_size;
    RK_U32 u_offset = 0, v_offset = 0;
    MPP_RET ret = MPP_OK;
    RK_U32 fmt;

    kmpp_frame_get_hor_stride(task->frame, &hor_stride);
    kmpp_frame_get_ver_stride(task->frame, &ver_stride);

    if (hor_stride == 0 || ver_stride == 0) {
        kmpp_frame_get_width(task->frame, &hor_stride);
        kmpp_frame_get_height(task->frame, &ver_stride);
    }
    frame_size = hor_stride * ver_stride;
    kmpp_frame_get_fmt(task->frame, &fmt);
    if (MPP_FRAME_FMT_IS_FBC(fmt)) {
        kmpp_frame_get_fbc_offset(task->frame, &u_offset);
        v_offset = 0;
        kmpp_logd_f("fbc case u_offset = %d", u_offset);
    } else {
        switch (input_fmt) {
        case VEPU541_FMT_YUV420P: {
            u_offset = frame_size;
            v_offset = frame_size * 5 / 4;
        }
        break;
        case VEPU541_FMT_YUV420SP:
        case VEPU541_FMT_YUV422SP: {
            u_offset = frame_size;
            v_offset = frame_size;
        }
        break;
        case VEPU541_FMT_YUV422P: {
            u_offset = frame_size;
            v_offset = frame_size * 3 / 2;
        }
        break;
        case VEPU541_FMT_YUYV422:
        case VEPU541_FMT_UYVY422: {
            u_offset = 0;
            v_offset = 0;
        }
        break;
        case VEPU541_FMT_BGR565:
        case VEPU541_FMT_BGR888:
        case VEPU541_FMT_BGRA8888: {
            u_offset = 0;
            v_offset = 0;
        }
        break;
        default: {
            kmpp_logd_f("unknown color space: %d\n", input_fmt);
            u_offset = frame_size;
            v_offset = frame_size * 5 / 4;
        }
        }
    }

    /* input cb addr */
    if (u_offset)
        regs->adr_src1 += u_offset;
    /* input cr addr */
    if (v_offset)
        regs->adr_src2 += v_offset;

    return ret;
}

MPP_RET vepu500_set_jpeg_reg(Vepu500JpegCfg * cfg)
{
    HalEncTask *task = (HalEncTask *) cfg->enc_task;
    JpegeSyntax *syn = (JpegeSyntax *) task->syntax.data;
    Vepu500JpegFrame *regs = (Vepu500JpegFrame *)cfg->jpeg_reg_base;
    VepuFmtCfg *fmt = (VepuFmtCfg *) cfg->input_fmt;
    RK_S32 stridey = 0;
    RK_S32 stridec = 0;
    KmppFrame frame = task->frame;
    RK_U32 width = syn->width;
    RK_U32 height = syn->height;
    RK_U32 slice_en = 0;
    RK_U32 pkt_len;
    RK_U32 frame_height = 0;
    RK_U32 offset_x, offset_y;

    kmpp_frame_get_height(frame, &frame_height);
    slice_en = (frame_height < height) && syn->restart_ri;

    if (slice_en) {
        kmpp_frame_get_width(frame, &width);
        kmpp_frame_get_height(frame, &height);
        /* if not first marker, do not write header */
        if (cfg->rst_marker) {
            task->length = 0;
            kmpp_packet_set_length(task->packet, 0);
        }
    }

    if (!cfg->online) {
        regs->adr_src0 = mpp_dev_get_iova_address(cfg->dev, task->input, 264);
        regs->adr_src1 = regs->adr_src0;
        regs->adr_src2 = regs->adr_src0;
        vepu500_jpeg_set_uv_offset(regs, syn, (VepuFmt)fmt->format, task);
    }

    kmpp_packet_get_length(task->packet, &pkt_len);
    if (!task->output->cir_flag) {
        if (task->output->buf) {
            RK_U32 out_addr = mpp_dev_get_iova_address(cfg->dev, task->output->buf, 257);

            regs->adr_bsbb = out_addr + task->output->start_offset;
        }

        regs->adr_bsbt = regs->adr_bsbb + task->output->size - 1;
        regs->adr_bsbr = regs->adr_bsbb;
        regs->adr_bsbs = regs->adr_bsbb + pkt_len;
    } else {
        RK_U32 size = mpp_buffer_get_size(task->output->buf);
        RK_U32 start_off = (task->output->start_offset + pkt_len) % size;

        regs->adr_bsbb = mpp_dev_get_iova_address(cfg->dev, task->output->buf, 257);
        regs->adr_bsbs = regs->adr_bsbb + start_off;
        regs->adr_bsbr = regs->adr_bsbb + task->output->r_pos;
        regs->adr_bsbt = regs->adr_bsbb + size;
    }

    regs->enc_rsl.pic_wd8_m1 = MPP_ALIGN(width, 16) / 8 - 1;
    regs->src_fill.pic_wfill = MPP_ALIGN(width, 16) - width;
    regs->enc_rsl.pic_hd8_m1 = MPP_ALIGN(height, 16) / 8 - 1;
    regs->src_fill.pic_hfill = MPP_ALIGN(height, 16) - height;

    regs->src_fmt.src_cfmt = fmt->format;
    regs->src_fmt.alpha_swap = fmt->alpha_swap;
    regs->src_fmt.rbuv_swap = fmt->rbuv_swap;
    regs->src_fmt.src_range_trns_en = 0;
    regs->src_fmt.src_range_trns_sel = 0;
    regs->src_fmt.chroma_ds_mode = 0;

    regs->src_proc.src_mirr = syn->mirroring > 0;
    regs->src_proc.src_rot = syn->rotation;

    if (syn->hor_stride)
        stridey = syn->hor_stride;

    else {
        if (regs->src_fmt.src_cfmt == VEPU541_FMT_BGRA8888)
            stridey = syn->width * 4;
        else if (regs->src_fmt.src_cfmt == VEPU541_FMT_BGR888)
            stridey = syn->width * 3;
        else if (regs->src_fmt.src_cfmt == VEPU541_FMT_BGR565 ||
                 regs->src_fmt.src_cfmt == VEPU541_FMT_YUYV422
                 || regs->src_fmt.src_cfmt ==
                 VEPU541_FMT_UYVY422)
            stridey = syn->width * 2;
    }

    stridec = (regs->src_fmt.src_cfmt == VEPU541_FMT_YUV422SP ||
               regs->src_fmt.src_cfmt == VEPU541_FMT_YUV420SP) ?
              stridey : stridey / 2;

    if (regs->src_fmt.src_cfmt < VEPU541_FMT_ARGB1555) {
        regs->src_udfy.csc_wgt_r2y = 77;
        regs->src_udfy.csc_wgt_g2y = 150;
        regs->src_udfy.csc_wgt_b2y = 29;

        regs->src_udfu.csc_wgt_r2u = -43;
        regs->src_udfu.csc_wgt_g2u = -85;
        regs->src_udfu.csc_wgt_b2u = 128;

        regs->src_udfv.csc_wgt_r2v = 128;
        regs->src_udfv.csc_wgt_g2v = -107;
        regs->src_udfv.csc_wgt_b2v = -21;

        regs->src_udfo.csc_ofst_y = 0;
        regs->src_udfo.csc_ofst_u = 128;
        regs->src_udfo.csc_ofst_v = 128;
    }
    regs->src_strd0.src_strd0 = stridey;
    regs->src_strd1.src_strd1 = stridec;
    kmpp_frame_get_offset_y(task->frame, &offset_y);
    kmpp_frame_get_offset_x(task->frame, &offset_x);
    regs->pic_ofst.pic_ofst_y = offset_y;
    regs->pic_ofst.pic_ofst_x = offset_x;

    regs->src_flt_cfg.pp_corner_filter_strength = 0;
    regs->src_flt_cfg.pp_edge_filter_strength = 0;
    regs->src_flt_cfg.pp_internal_filter_strength = 0;

    regs->y_cfg.bias_y = 0;
    regs->u_cfg.bias_u = 0;
    regs->v_cfg.bias_v = 0;

    regs->base_cfg.jpeg_ri = syn->restart_ri;
    regs->base_cfg.jpeg_out_mode = 0;
    regs->base_cfg.jpeg_start_rst_m = cfg->rst_marker & 0x7;
    if (slice_en) {
        RK_U32 eos = 0;

        kmpp_frame_get_eos(frame, &eos);
        if (eos) {
            regs->base_cfg.jpeg_pic_last_ecs = 1;
            cfg->rst_marker = 0;
        } else {
            regs->base_cfg.jpeg_pic_last_ecs = 0;
            cfg->rst_marker++;
        }
    } else
        regs->base_cfg.jpeg_pic_last_ecs = 1;

    regs->base_cfg.jpeg_stnd = 1;   //enable

    regs->uvc_cfg.uvc_partition0_len = 0;
    regs->uvc_cfg.uvc_partition_len = 0;
    regs->uvc_cfg.uvc_skip_len = 0;

    return MPP_OK;
}

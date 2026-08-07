/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */


#define MODULE_TAG  "vepu541_common"

#include "kmpp_string.h"

#include "kmpp_log.h"
#include "mpp_mem.h"
#include "mpp_maths.h"
#include "jpege_syntax.h"
// #include "vepu541_common.h"
#include "vepu5xx_common.h"
#include "vepu540c_common.h"
#include "hal_enc_task.h"
#include "kmpp_obj.h"
#include "kmpp_buffer.h"
#include "kmpp_frame.h"
#include "kmpp_packet.h"
#include "rk-dvbm.h"
#include "mpp_buffer_impl.h"

MPP_RET vepu540c_set_roi(void *roi_reg_base, MppEncROICfg * roi,
                         RK_S32 w, RK_S32 h)
{
    MppEncROIRegion *region = roi->regions;
    Vepu540cRoiCfg *roi_cfg = (Vepu540cRoiCfg *) roi_reg_base;
    Vepu540cRoiRegion *reg_regions = &roi_cfg->regions[0];
    MPP_RET ret = MPP_NOK;
    RK_S32 i = 0;
    kmpp_memset(reg_regions, 0, sizeof(Vepu540cRoiRegion) * 8);
    if (NULL == roi_cfg || NULL == roi) {
        kmpp_loge_f("invalid buf %p roi %p\n", roi_cfg, roi);
        goto DONE;
    }

    if (roi->number > VEPU540C_MAX_ROI_NUM) {
        kmpp_loge_f("invalid region number %d\n", roi->number);
        goto DONE;
    }

    /* check region config */
    ret = MPP_OK;
    for (i = 0; i < (RK_S32) roi->number; i++, region++) {
        if (region->x + region->w > w || region->y + region->h > h)
            ret = MPP_NOK;

        if (region->intra > 1
            || region->qp_area_idx >= VEPU540C_MAX_ROI_NUM
            || region->area_map_en > 1 || region->abs_qp_en > 1)
            ret = MPP_NOK;

        if ((region->abs_qp_en && region->quality > 51) ||
            (!region->abs_qp_en
             && (region->quality > 51 || region->quality < -51)))
            ret = MPP_NOK;

        if (ret) {
            kmpp_loge_f("region %d invalid param:\n", i);
            kmpp_loge_f("position [%d:%d:%d:%d] vs [%d:%d]\n",
                        region->x, region->y, region->w, region->h, w,
                        h);
            kmpp_loge_f("force intra %d qp area index %d\n",
                        region->intra, region->qp_area_idx);
            kmpp_loge_f("abs qp mode %d value %d\n",
                        region->abs_qp_en, region->quality);
            goto DONE;
        }
        reg_regions->roi_pos_lt.roi_lt_x =
            MPP_ALIGN(region->x, 16) >> 4;
        reg_regions->roi_pos_lt.roi_lt_y =
            MPP_ALIGN(region->y, 16) >> 4;
        reg_regions->roi_pos_rb.roi_rb_x =
            MPP_ALIGN(region->x + region->w, 16) >> 4;
        reg_regions->roi_pos_rb.roi_rb_y =
            MPP_ALIGN(region->y + region->h, 16) >> 4;
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

MPP_RET vepu540c_set_qpmap_smart(void *roi_reg_base, MppBuffer mv_info, MppBuffer qpmap,
                                 RK_U8 *mv_flag, RK_U32 qp_out, RK_S32 w, RK_S32 h, RK_S32 is_hevc,
                                 RK_S32 is_idr, RK_S32 reenc, RK_S32 deblur_str)
{
    Vepu540cRoiCfg *roi_cfg = (Vepu540cRoiCfg *) roi_reg_base;
    RK_S32 i, mb_w, mb_h;
    RK_U32 *mdr = NULL;
    RK_U32 qpmap_idx = 0;
    Vepu540cAvcQpmapCfg *qpmap_avc = NULL;
    Vepu540cHevcQpmapCfg *qpmap_hevc = NULL;
    RK_S32 cnt = 0;
    RK_U16 sad = 0;
    RK_U8 mv_final_flag;
    RK_U8 max_mv_final_flag;
    RK_S32 qp_delta_base;
    RK_S32 dqp;
    RK_S32 mv_blk_cnt = 0;
    RK_S32 coef_move;
    RK_S32 b = 0;
    RK_U8 *p = NULL;

    if (!mv_info || !qpmap || !mv_flag)
        return MPP_NOK;

    roi_cfg->bmap_cfg.bmap_en = is_idr ? 0 : 1;
    roi_cfg->bmap_cfg.bmap_pri = 17;
    roi_cfg->bmap_cfg.bmap_qpmin = 10;
    roi_cfg->bmap_cfg.bmap_qpmax = 51;
    roi_cfg->bmap_cfg.bmap_mdc_dpth = 0;

    p = mv_flag;
    if (!is_hevc) {
        mdr = (RK_U32 *)mpp_buffer_get_ptr(mv_info);
        qpmap_avc = (Vepu540cAvcQpmapCfg *)mpp_buffer_get_ptr(qpmap);
        mb_w = MPP_ALIGN(w, 64) / 16;
        mb_h = MPP_ALIGN(h, 16) / 16;
        if (is_idr) {
            kmpp_memset(mv_flag, 0, mb_w * mb_h);
            return MPP_NOK;
        }
        kmpp_memset(qpmap_avc, 0, sizeof(Vepu540cAvcQpmapCfg) * mb_w * mb_h);
        cnt = mb_h * mb_w;
        if (reenc) {
            for (i = 0; i < cnt; i++) {
                b = (!(p[i] & 0x3));
                b &= (p[i] & 0x8) || ((p[i] & 0x30) && (p[i] & 0xc));
                if (b)
                    mv_blk_cnt ++;
            }
        } else {
            for (i = 0; i < cnt; i++) {
                sad = mdr[i] & 0x0fff;
                p[i] = ((p[i] << 2) & 0x3f);
                p[i] |= (sad > 144 ? 2 : (sad > 72 ? 1 : 0));
                b = (!(p[i] & 0x3));
                b &= (p[i] & 0x8) || ((p[i] & 0x30) && (p[i] & 0xc));
                if (b)
                    mv_blk_cnt ++;
            }
        }

        mpp_buffer_flush_for_cpu(qpmap);
        if (qp_out < 36)
            qp_delta_base = 0;
        else if (qp_out < 42)
            qp_delta_base = 1;
        else if (qp_out < 46)
            qp_delta_base = 2;
        else
            qp_delta_base = 3;
        coef_move = mv_blk_cnt * 100;
        if (coef_move < 10 * cnt) {
            if (qp_out > 40)
                roi_cfg->bmap_cfg.bmap_qpmin = 27;
            else if (qp_out > 35)
                roi_cfg->bmap_cfg.bmap_qpmin = 26;
            else
                roi_cfg->bmap_cfg.bmap_qpmin = 25;

            dqp = qp_delta_base;
            if (coef_move < 2 * cnt)
                dqp += 2;
            else
                dqp += 1;
            dqp -= (deblur_str < 2);
            dqp = (dqp == 0) ? 1 : dqp;
            for (i = 0; i < cnt; i++) {
                b = (!(p[i] & 0x3));
                b &= (p[i] & 0x8) || ((p[i] & 0x30) && (p[i] & 0xc));
                if (b) {
                    qpmap_avc[i].qp_adju = 0x80 - dqp;
                    qpmap_avc[i].mdc_adju_intra = 3;
                }
            }
        }
        mpp_buffer_flush_for_device(qpmap);
    } else {
        mdr = (RK_U32 *)mpp_buffer_get_ptr(mv_info);
        qpmap_hevc = (Vepu540cHevcQpmapCfg *)mpp_buffer_get_ptr(qpmap);
        mb_w = MPP_ALIGN(w, 32) / 16;
        mb_h = MPP_ALIGN(h, 32) / 16;
        if (is_idr) {
            kmpp_memset(mv_flag, 0, mb_w * mb_h);
            return MPP_NOK;
        }
        kmpp_memset(qpmap_hevc, 0, sizeof(Vepu540cHevcQpmapCfg) * mb_w * mb_h / 4);
        cnt = mb_h * mb_w;
        if (reenc) {
            for (i = 0; i < cnt; i++) {
                b = (p[i] & 0x8) || (p[i] & 0x20);
                b |= ((p[i] & 0x30) && (p[i] & 0xc));
                b &= (!(p[i] & 0x3));
                if (b)
                    mv_blk_cnt ++;
            }
        } else {
            for (i = 0; i < cnt; i++) {
                sad = mdr[i] & 0x0fff;
                p[i] = ((p[i] << 2) & 0x3f);
                p[i] |= (sad > 144 ? 2 : (sad > 72 ? 1 : 0));
                b = (p[i] & 0x8) || (p[i] & 0x20);
                b |= ((p[i] & 0x30) && (p[i] & 0xc));
                b &= (!(p[i] & 0x3));
                if (b)
                    mv_blk_cnt ++;
            }
        }

        mpp_buffer_flush_for_cpu(qpmap);
        if (qp_out < 36)
            qp_delta_base = 0;
        else if (qp_out < 42)
            qp_delta_base = 1;
        else if (qp_out < 46)
            qp_delta_base = 2;
        else
            qp_delta_base = 3;
        coef_move = mv_blk_cnt * 100;
        max_mv_final_flag = 0;
        if (coef_move < 10 * cnt) {
            if (qp_out > 40)
                roi_cfg->bmap_cfg.bmap_qpmin = 27;
            else if (qp_out > 35)
                roi_cfg->bmap_cfg.bmap_qpmin = 25 + (deblur_str < 2);
            else
                roi_cfg->bmap_cfg.bmap_qpmin = 24 + (deblur_str < 2);

            dqp = qp_delta_base;
            if (coef_move < 1 * cnt)
                dqp += 5;
            else if (coef_move < 3 * cnt)
                dqp += 4;
            else if (coef_move < 5 * cnt)
                dqp += 3;
            else
                dqp += 2;
            dqp -= (deblur_str < 2);
            for (i = 0; i < cnt; i++) {
                mv_final_flag = 0;
                b = (p[i] & 0x8) || (p[i] & 0x20);
                b |= ((p[i] & 0x30) && (p[i] & 0xc));
                b &= (!(p[i] & 0x3));
                if (b)
                    mv_final_flag = 1;
                max_mv_final_flag = max(max_mv_final_flag, mv_final_flag);

                if ((i + 1) % 4)
                    continue;
                else {
                    if (max_mv_final_flag > 0)
                        qpmap_hevc[qpmap_idx].qp_adju = 0x80 - dqp;
                    qpmap_idx++;

                    max_mv_final_flag = 0;
                }
            }
        }
        mpp_buffer_flush_for_device(qpmap);
    }

    return MPP_OK;
}

MPP_RET vepu540c_set_qpmap_normal(void *roi_reg_base, MppBuffer mv_info, MppBuffer qpmap,
                                  RK_U8 *mv_flag, RK_U32 qp_out, RK_S32 w, RK_S32 h, RK_S32 is_hevc,
                                  RK_S32 is_idr, RK_S32 reenc, RK_S32 deblur_str)
{
    Vepu540cRoiCfg *roi_cfg = (Vepu540cRoiCfg *) roi_reg_base;
    RK_S32 i, j, k;
    RK_S32 mb_w, mb_h;
    RK_U32 *mdr = NULL;
    RK_U32 qpmap_idx = 0;
    Vepu540cAvcQpmapCfg *qpmap_avc = NULL;
    Vepu540cHevcQpmapCfg *qpmap_hevc = NULL;
    RK_S32 cnt = 0;
    RK_U8 mv_final_flag;
    RK_U8 max_mv_final_flag;
    RK_U16 sad = 0;
    RK_S32 qp_delta_base;
    RK_S32 dqp;
    RK_S32 mv_blk_cnt = 0;
    RK_S32 mv_blk_cnt1 = 0;
    RK_S32 coef_move;
    RK_S32 b = 0;
    RK_U8 *p = NULL;

    if (!mv_info || !qpmap || !mv_flag)
        return MPP_NOK;

    roi_cfg->bmap_cfg.bmap_en = is_idr ? 0 : 1;
    roi_cfg->bmap_cfg.bmap_pri = 17;
    roi_cfg->bmap_cfg.bmap_qpmin = 10;
    roi_cfg->bmap_cfg.bmap_qpmax = 51;
    roi_cfg->bmap_cfg.bmap_mdc_dpth = 0;

    p = mv_flag;
    if (!is_hevc) {
        mdr = (RK_U32 *)mpp_buffer_get_ptr(mv_info);
        qpmap_avc = (Vepu540cAvcQpmapCfg *)mpp_buffer_get_ptr(qpmap);
        mb_w = MPP_ALIGN(w, 64) / 16;
        mb_h = MPP_ALIGN(h, 16) / 16;
        if (is_idr) {
            kmpp_memset(mv_flag, 0, mb_w * mb_h);
            return MPP_NOK;
        }
        kmpp_memset(qpmap_avc, 0, sizeof(Vepu540cAvcQpmapCfg) * mb_w * mb_h);
        cnt = mb_h * mb_w;
        if (reenc) {
            for (i = 0; i < cnt; i++) {
                b = (!(p[i] & 0x3));
                b &= (p[i] & 0x8) || ((p[i] & 0x30) && (p[i] & 0xc));
                if (b)
                    mv_blk_cnt ++;
            }
        } else {
            for (i = 0; i < cnt; i++) {
                sad = mdr[i] & 0x0fff;
                p[i] = ((p[i] << 2) & 0x3f);
                p[i] |= (sad > 144 ? 2 : (sad > 72 ? 1 : 0));
                b = (!(p[i] & 0x3));
                b &= (p[i] & 0x8) || ((p[i] & 0x30) && (p[i] & 0xc));
                if (b)
                    mv_blk_cnt ++;
            }
        }


        if (qp_out < 36)
            qp_delta_base = 0;
        else if (qp_out < 42)
            qp_delta_base = 4;
        else if (qp_out < 46)
            qp_delta_base = 4;
        else
            qp_delta_base = 3;
        coef_move = mv_blk_cnt * 100;
        if (coef_move < 15 * cnt && coef_move > (cnt >> 5)) {
            if (qp_out > 40)
                roi_cfg->bmap_cfg.bmap_qpmin = 28;
            else if (qp_out > 35)
                roi_cfg->bmap_cfg.bmap_qpmin = 27;
            else
                roi_cfg->bmap_cfg.bmap_qpmin = 25;
        }
        mpp_buffer_flush_for_cpu(qpmap);
        if (coef_move < 15 * cnt && coef_move > 0) {
            dqp = qp_delta_base;
            if (coef_move < 1 * cnt)
                dqp += 7;
            else if (coef_move < 3 * cnt)
                dqp += 6;
            else if (coef_move < 7 * cnt)
                dqp += 5;
            else
                dqp += 4;

            if (deblur_str < 2)
                dqp -= 2;

            for (i = 0; i < cnt; i++) {
                b = (!(p[i] & 0x3));
                b &= (p[i] & 0x8) || ((p[i] & 0x30) && (p[i] & 0xc));
                if (b)
                    qpmap_avc[i].qp_adju = 0x80 - dqp;
            }
        }
        mpp_buffer_flush_for_device(qpmap);
    } else {
        mdr = (RK_U32 *)mpp_buffer_get_ptr(mv_info);
        qpmap_hevc = (Vepu540cHevcQpmapCfg *)mpp_buffer_get_ptr(qpmap);
        mb_w = MPP_ALIGN(w, 32) / 16;
        mb_h = MPP_ALIGN(h, 32) / 16;
        if (is_idr) {
            kmpp_memset(mv_flag, 0, mb_w * mb_h);
            return MPP_NOK;
        }
        kmpp_memset(qpmap_hevc, 0, sizeof(Vepu540cHevcQpmapCfg) * mb_w * mb_h / 4);
        cnt = mb_h * mb_w;
        if (reenc) {
            for (i = 0; i < cnt / 4; i++) {
                mv_blk_cnt1 = 0;
                for (k = 0; k < 4; k ++) {
                    j = i * 4 + k;
                    b = ((p[j] & 0xc) == 0xc);
                    b |= ((p[j] & 0x20) && (p[j] & 0xc));
                    b &= (!(p[j] & 0x3));
                    if (b)
                        mv_blk_cnt1 ++;
                }
                mv_blk_cnt += mv_blk_cnt1 > 0;
            }
        } else {
            for (i = 0; i < cnt / 4; i++) {
                mv_blk_cnt1 = 0;
                for (k = 0; k < 4; k ++) {
                    j = i * 4 + k;
                    sad = mdr[j] & 0x0fff;
                    p[j] = ((p[j] << 2) & 0x3f);
                    b = sad > 160 ? 3 : (sad > 120 ? 2 : (sad > 72 ? 1 : 0));
                    p[j] |= b;
                    b = ((p[j] & 0xc) == 0xc);
                    b |= ((p[j] & 0x20) && (p[j] & 0xc));
                    b &= (!(p[j] & 0x3));
                    if (b)
                        mv_blk_cnt1++;
                }
                mv_blk_cnt += mv_blk_cnt1 > 0;
            }
        }

        if (qp_out < 36)
            qp_delta_base = 0;
        else if (qp_out < 42)
            qp_delta_base = 3;
        else if (qp_out < 46)
            qp_delta_base = 3;
        else
            qp_delta_base = 2;
        coef_move = mv_blk_cnt * 4 * 100;
        max_mv_final_flag = 0;
        if (coef_move < 15 * cnt && coef_move > (cnt >> 5)) {
            if (qp_out > 40)
                roi_cfg->bmap_cfg.bmap_qpmin = 29;
            else if (qp_out > 35)
                roi_cfg->bmap_cfg.bmap_qpmin = 28;
            else
                roi_cfg->bmap_cfg.bmap_qpmin = deblur_str < 2 ? 27 : 26;
        }

        mpp_buffer_flush_for_cpu(qpmap);
        if (coef_move < 15 * cnt && coef_move > 0) {
            dqp = qp_delta_base;
            if (coef_move < 1 * cnt)
                dqp += 7;
            else if (coef_move < 3 * cnt)
                dqp += 6;
            else if (coef_move < 7 * cnt)
                dqp += 5;
            else
                dqp += 4;

            if (deblur_str < 2)
                dqp -= 2;

            for (i = 0; i < cnt; i++) {
                mv_final_flag = 0;
                b = ((p[i] & 0xc) == 0xc);
                b |= ((p[i] & 0x20) && (p[i] & 0xc));
                b &= (!(p[i] & 0x3));
                if (b)
                    mv_final_flag = 1;
                max_mv_final_flag = max(max_mv_final_flag, mv_final_flag);

                if ((i + 1) % 4)
                    continue;
                else {
                    if (max_mv_final_flag > 0)
                        qpmap_hevc[qpmap_idx].qp_adju = 0x80 - dqp;

                    qpmap_idx++;
                    max_mv_final_flag = 0;
                }
            }
        }
        mpp_buffer_flush_for_device(qpmap);
    }

    return MPP_OK;
}

MPP_RET vepu540c_set_osd(Vepu540cOsdCfg * cfg)
{
    vepu540c_osd_reg *regs = (vepu540c_osd_reg *) (cfg->reg_base);
    //  MppDev dev = cfg->dev;
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
        KmppBuffer inv_buffer = NULL;
        KmppBuffer osd_buffer = NULL;
        vepu540c_osd_com *reg = (vepu540c_osd_com *) & regs->osd_cfg[i];
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
                                           (RK_U64 *)&reg->osd_inv_st_addr, mpp_get_dev(cfg->dev));

        osd_buffer = kmpp_obj_from_shmptr(&tmp->osd_buf);
        if (osd_buffer)
            kmpp_buffer_get_iova_by_device(osd_buffer,
                                           (RK_U64 *)&reg->osd_st_addr, mpp_get_dev(cfg->dev));
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
static MPP_RET
vepu540c_jpeg_set_uv_offset(Vepu540cJpegReg * regs, JpegeSyntax * syn,
                            VepuFmt input_fmt, HalEncTask *task)
{
    RK_U32 hor_stride = 0;
    RK_U32 ver_stride = 0;
    RK_U32 frame_size = 0;
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
        regs->reg0265_adr_src1 += u_offset;
    /* input cr addr */
    if (v_offset)
        regs->reg0266_adr_src2 += v_offset;

    return ret;
}

MPP_RET vepu540c_set_jpeg_reg(Vepu540cJpegCfg * cfg)
{
    HalEncTask *task = (HalEncTask *) cfg->enc_task;
    JpegeSyntax *syn = (JpegeSyntax *) task->syntax.data;
    Vepu540cJpegReg *regs = (Vepu540cJpegReg *) cfg->jpeg_reg_base;
    VepuFmtCfg *fmt = (VepuFmtCfg *) cfg->input_fmt;
    RK_S32 stridey = 0;
    RK_S32 stridec = 0;
    KmppFrame frame = task->frame;
    RK_U32 width = syn->width;
    RK_U32 height = syn->height;
    RK_U32 slice_en = 0;
    RK_U32 frame_height = 0;
    RK_U32 offset_x, offset_y;
    RK_S32 pkt_len;

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
        regs->reg0264_adr_src0 = mpp_dev_get_iova_address(cfg->dev, task->input, 264);
        regs->reg0265_adr_src1 = regs->reg0264_adr_src0;
        regs->reg0266_adr_src2 = regs->reg0264_adr_src0;
        vepu540c_jpeg_set_uv_offset(regs, syn, (VepuFmt) fmt->format, task);
    }

    kmpp_packet_get_length(task->packet, &pkt_len);
    if (!task->output->cir_flag) {
        if (task->output->buf) {
            regs->reg0257_adr_bsbb = mpp_dev_get_iova_address(cfg->dev, task->output->buf,
                                                              257) + task->output->start_offset;
        }

        regs->reg0256_adr_bsbt = regs->reg0257_adr_bsbb + task->output->size - 1;
        regs->reg0258_adr_bsbr = regs->reg0257_adr_bsbb;
        regs->reg0259_adr_bsbs = regs->reg0257_adr_bsbb + pkt_len;
    } else {
        RK_U32 size = mpp_buffer_get_size(task->output->buf);
        regs->reg0257_adr_bsbb = mpp_dev_get_iova_address(cfg->dev, task->output->buf, 257);
        regs->reg0259_adr_bsbs = regs->reg0257_adr_bsbb + ((task->output->start_offset +
                                                            pkt_len) % size);
        regs->reg0258_adr_bsbr = regs->reg0257_adr_bsbb + task->output->r_pos;
        regs->reg0256_adr_bsbt = regs->reg0257_adr_bsbb + size;
    }

    regs->reg0272_enc_rsl.pic_wd8_m1 = MPP_ALIGN(width, 16) / 8 - 1;
    regs->reg0273_src_fill.pic_wfill = MPP_ALIGN(width, 16) - width;
    regs->reg0272_enc_rsl.pic_hd8_m1 = MPP_ALIGN(height, 16) / 8 - 1;
    regs->reg0273_src_fill.pic_hfill = MPP_ALIGN(height, 16) - height;

    regs->reg0274_src_fmt.src_cfmt = fmt->format;
    regs->reg0274_src_fmt.alpha_swap = fmt->alpha_swap;
    regs->reg0274_src_fmt.rbuv_swap = fmt->rbuv_swap;
    regs->reg0274_src_fmt.src_range_trns_en = 0;
    regs->reg0274_src_fmt.src_range_trns_sel = 0;
    regs->reg0274_src_fmt.chroma_ds_mode = 0;
    regs->reg0274_src_fmt.out_fmt = 1;

    regs->reg0279_src_proc.src_mirr = syn->mirroring > 0;
    regs->reg0279_src_proc.src_rot = syn->rotation;

    if (syn->hor_stride)
        stridey = syn->hor_stride;

    else {
        if (regs->reg0274_src_fmt.src_cfmt == VEPU541_FMT_BGRA8888)
            stridey = syn->width * 4;
        else if (regs->reg0274_src_fmt.src_cfmt == VEPU541_FMT_BGR888)
            stridey = syn->width * 3;
        else if (regs->reg0274_src_fmt.src_cfmt == VEPU541_FMT_BGR565 ||
                 regs->reg0274_src_fmt.src_cfmt == VEPU541_FMT_YUYV422
                 || regs->reg0274_src_fmt.src_cfmt ==
                 VEPU541_FMT_UYVY422)
            stridey = syn->width * 2;
    }

    stridec = (regs->reg0274_src_fmt.src_cfmt == VEPU541_FMT_YUV422SP ||
               regs->reg0274_src_fmt.src_cfmt == VEPU541_FMT_YUV420SP) ?
              stridey : stridey / 2;

    if (regs->reg0274_src_fmt.src_cfmt < VEPU541_FMT_ARGB1555) {
        regs->reg0275_src_udfy.csc_wgt_r2y = 77;
        regs->reg0275_src_udfy.csc_wgt_g2y = 150;
        regs->reg0275_src_udfy.csc_wgt_b2y = 29;

        regs->reg0276_src_udfu.csc_wgt_r2u = -43;
        regs->reg0276_src_udfu.csc_wgt_g2u = -85;
        regs->reg0276_src_udfu.csc_wgt_b2u = 128;

        regs->reg0277_src_udfv.csc_wgt_r2v = 128;
        regs->reg0277_src_udfv.csc_wgt_g2v = -107;
        regs->reg0277_src_udfv.csc_wgt_b2v = -21;

        regs->reg0278_src_udfo.csc_ofst_y = 0;
        regs->reg0278_src_udfo.csc_ofst_u = 128;
        regs->reg0278_src_udfo.csc_ofst_v = 128;
    }
    regs->reg0281_src_strd0.src_strd0 = stridey;
    regs->reg0282_src_strd1.src_strd1 = stridec;
    kmpp_frame_get_offset_y(task->frame, &offset_y);
    kmpp_frame_get_offset_x(task->frame, &offset_x);
    regs->reg0280_pic_ofst.pic_ofst_y = offset_y;
    regs->reg0280_pic_ofst.pic_ofst_x = offset_x;
    //to be done

    regs->reg0283_src_flt.pp_corner_filter_strength = 0;
    regs->reg0283_src_flt.pp_edge_filter_strength = 0;
    regs->reg0283_src_flt.pp_internal_filter_strength = 0;

    regs->reg0284_y_cfg.bias_y = 0;
    regs->reg0285_u_cfg.bias_u = 0;
    regs->reg0286_v_cfg.bias_v = 0;

    regs->reg0287_base_cfg.jpeg_ri = syn->restart_ri;
    regs->reg0287_base_cfg.jpeg_out_mode = 0;
    regs->reg0287_base_cfg.jpeg_start_rst_m = cfg->rst_marker & 0x7;
    if (slice_en) {
        RK_U32 eos = 0;

        kmpp_frame_get_eos(frame, &eos);
        if (eos) {
            regs->reg0287_base_cfg.jpeg_pic_last_ecs = 1;
            cfg->rst_marker = 0;
        } else {
            regs->reg0287_base_cfg.jpeg_pic_last_ecs = 0;
            cfg->rst_marker++;
        }
    } else
        regs->reg0287_base_cfg.jpeg_pic_last_ecs = 1;

    regs->reg0287_base_cfg.jpeg_slen_fifo = 0;
    regs->reg0287_base_cfg.jpeg_stnd = 1;   //enable

    regs->reg0288_uvc_cfg.uvc_partition0_len = 0;
    regs->reg0288_uvc_cfg.uvc_partition_len = 0;
    regs->reg0288_uvc_cfg.uvc_skip_len = 0;

    return MPP_OK;
}

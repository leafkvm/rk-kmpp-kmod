/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include "kmpp_log.h"
#include "kmpp_mem.h"
#include "kmpp_string.h"
#include "kmpp_env.h"

#include "rk_export_func.h"
#include "kmpp_vsp_impl.h"
#include "kmpp_vsp_objs_impl.h"
#include "kmpp_obj.h"
#include "kmpp_meta_impl.h"
#include "kmpp_frame_impl.h"
#include "kmpp_buffer.h"

#include "mpp_buffer.h"
#include "vepu500_pp.h"

#define MDW_STRIDE  (64) /* 64 bytes, 512(64 * 8) blk8x8 */

static struct vepu_pp_ctx_t g_pp_ctx;

static struct pp_buffer_t * pp_malloc_buffer(struct pp_chn_info_t *info, rk_u32 size)
{
    struct pp_buffer_t *pp_buf = NULL;
    rk_s32 ret;

    pp_buf = kmpp_calloc(sizeof(*pp_buf));
    if (!pp_buf) {
        kmpp_loge_f("failed\n");
        return NULL;
    }

    ret = sys_dmabuf_alloc_by_device(&pp_buf->buf, info->device, size, 0);
    if (ret) {
        kmpp_loge_f("sys_dmabuf_alloc ret %d\n", ret);
        return NULL;
    }

    pp_buf->iova = pp_buf->buf->daddr;

    return pp_buf;
}

static void pp_free_buffer(struct pp_chn_info_t *info, struct pp_buffer_t *pp_buf)
{
    if (!pp_buf)
        return;

    if (pp_buf->buf) {
        sys_dmabuf_free(pp_buf->buf);
        pp_buf->buf = NULL;
    }

    kmpp_free(pp_buf);
}

static void pp_release_buffer(struct pp_chn_info_t *info)
{
    pp_free_buffer(info, info->buf_rfpw);
    pp_free_buffer(info, info->buf_rfpr);
}

static rk_s32 pp_allocate_buffer(struct pp_chn_info_t *info)
{
    rk_s32 w = PP_ALIGN(info->max_width, 32);
    rk_s32 h = PP_ALIGN(info->max_height, 32);
    rk_s32 buf_len = 0;
    enum PP_RET ret = VEPU_PP_OK;

    /* alloc buffer for down scale rw */
    buf_len = (w * h) >> 4;
    info->buf_rfpw = pp_malloc_buffer(info, buf_len);
    info->buf_rfpr = pp_malloc_buffer(info, buf_len);
    if (!info->buf_rfpw || !info->buf_rfpr) {
        kmpp_loge_f("alloc buffer failed\n");
        ret = VEPU_PP_NOK;
        goto __return;
    }

__return:
    if (ret)
        pp_release_buffer(info);

    return ret;
}

static void pp_set_src_addr(struct pp_chn_info_t *info)
{
    KmppVspPpCfg *cfg = &info->cfg;
    KmppFrame frame = info->frame;
    struct pp_param_t *p = &info->param;
    rk_u32 adr_src0 = 0, adr_src1 = 0, adr_src2 = 0;
    rk_u32 width = cfg->width, height = cfg->height;
    rk_u32 stride_y = 0, stride_c = 0;
    MppFrameFormat fmt = MPP_FMT_YUV420P;
    rk_u64 iova = 0;

    kmpp_frame_get_fmt(frame, &fmt);

    kmpp_dmabuf_get_iova_by_device(info->in_buf, &iova, info->device);
    adr_src0 = iova;

    switch (fmt) {
    case MPP_FMT_YUV420P: {
        adr_src1 = adr_src0 + width * height;
        adr_src2 = adr_src1 + width * height / 4;
        stride_y = width;
        stride_c = width / 2;
    } break;
    case MPP_FMT_YUV420SP: {
        adr_src1 = adr_src0 + width * height;
        adr_src2 = adr_src1;
        stride_y = width;
        stride_c = width;
    } break;
    default: {
    }
    }

    p->adr_src0 = adr_src0;
    p->adr_src1 = adr_src1;
    p->adr_src2 = adr_src2;
    p->src_strd0.src_strd0 = stride_y;
    p->src_strd1.src_strd1 = stride_c;
}

static void pp_set_common_cfg(struct pp_chn_info_t *info, struct pp_com_cfg *cfg)
{
    struct pp_param_t *p = &info->param;
    rk_s32 frm_cnt = cfg->frm_cnt;
    rk_s32 gop = cfg->gop ? cfg->gop : 30;
    rk_s32 interval = cfg->md_interval ? cfg->md_interval : 1;
    rk_s32 md_od_switch = (info->frm_accum_interval == 0);

    info->frm_num = frm_cnt;
    p->int_en.enc_done_en = 1;
    p->int_msk = 0;
    p->enc_pic.enc_stnd = 2;
    p->enc_pic.cur_frm_ref = 1;//TODO:
    p->enc_rsl.pic_wd8_m1 = (PP_ALIGN(info->width, 8) >> 3) - 1;
    p->enc_rsl.pic_hd8_m1 = (PP_ALIGN(info->height, 8) >> 3) - 1;

    p->src_fmt.src_cfmt = cfg->fmt;

    p->src_fill.pic_wfill = 0;
    p->src_fill.pic_hfill = 0;

    p->pic_ofst.pic_ofst_x = 0;
    p->pic_ofst.pic_ofst_y = 0;

    p->vpp_base_cfg.cur_frm_en_md = info->md_en && md_od_switch && (frm_cnt > 0);
    /*
     * Fix bug:
     * Disable the ref_frm_en_md to prevent the hw stuck.
     * This will cause the flying catkin detection to fail.
     */
    p->vpp_base_cfg.ref_frm_en_md = 0;

    p->vpp_base_cfg.en_od = info->od_en && md_od_switch;
    p->vpp_base_cfg.background_en_od = (frm_cnt > 0);
    p->vpp_base_cfg.sad_comp_en_od = 1;
    p->vpp_base_cfg.vepu_pp_en = 1;

    /* cime cfg */
    p->me_rnge.cime_srch_dwnh = 4;
    p->me_rnge.cime_srch_uph  = 4;
    p->me_rnge.cime_srch_rgtw = 4;
    p->me_rnge.cime_srch_lftw = 4;
    p->me_rnge.dlt_frm_num    = 1;

    p->me_cfg.srgn_max_num   = 54;
    p->me_cfg.cime_dist_thre = 1024;
    p->me_cfg.rme_srch_h     = 0;
    p->me_cfg.rme_srch_v     = 0;

    p->me_cach.cime_zero_thre = 64;
    p->me_cach.fme_prefsu_en  = 0;
    p->me_cach.colmv_load     = 0;
    p->me_cach.colmv_stor     = 0;

    p->synt_sli0.sli_type = frm_cnt > 0 ? 1 : 2;

    p->jpeg_base_cfg.jpeg_stnd = 0;

    info->frm_accum_interval++;
    if (info->frm_accum_interval == interval)
        info->frm_accum_interval = 0;

    info->frm_accum_gop++;
    if (info->frm_accum_gop == gop)
        info->frm_accum_gop = 0;
}

static rk_s32 vepu_pp_flycatkin_filter_init_buf(struct pp_chn_info_t *info)
{
    rk_s32 buf_len = PP_ALIGN(info->max_height, 32) / 32 * MDW_STRIDE * 3;

    info->buf_rfmwr = kmpp_calloc(buf_len);
    if (!info->buf_rfmwr) {
        kmpp_loge_f("vepu pp create flycatkin buffer failed\n");
        return VEPU_PP_NOK;
    }

    info->buf_rfmwr0 = info->buf_rfmwr;
    info->buf_rfmwr1 = info->buf_rfmwr + buf_len / 3;
    info->buf_rfmwr2 = info->buf_rfmwr + buf_len / 3 * 2;
    info->mdw_len = buf_len / 3;

    return VEPU_PP_OK;
}

/* x,y: blk8x8 pos */
static rk_s32 vepu_pp_get_blk8_move_flag(rk_u8 *src, rk_s32 x, rk_s32 y)
{
    rk_u8 *mdw_ptr = src + MDW_STRIDE * y + x / 8;

    return (*mdw_ptr >> (x % 8)) & 1;
}

static rk_s32 vepu_pp_get_blk8_vld_flag(struct pp_chn_info_t *info, rk_s32 x, rk_s32 y)
{
    rk_s32 b8_col = PP_ALIGN(info->width, 32) / 32;
    rk_s32 b8_row = PP_ALIGN(info->height, 32) / 32;

    if (x < 0 || x >= b8_col || y < 0 || y >= b8_row)
        return 0;

    return 1;
}

static rk_s32 vepu_pp_get_blk8_move_cnt(struct pp_chn_info_t *info, rk_s32 x, rk_s32 y, rk_s32 *vld_cnt)
{
    rk_u8 *mdw_buf = kmpp_dmabuf_get_kptr(info->md_buf);
    rk_s32 mv_cnt = 0, vld_num = 0;

    if (vepu_pp_get_blk8_vld_flag(info, x - 1, y - 1)) {
        vld_num++;
        mv_cnt += vepu_pp_get_blk8_move_flag(mdw_buf, x - 1, y - 1);
    }

    if (vepu_pp_get_blk8_vld_flag(info, x, y - 1)) {
        vld_num++;
        mv_cnt += vepu_pp_get_blk8_move_flag(mdw_buf, x, y - 1);
    }

    if (vepu_pp_get_blk8_vld_flag(info, x + 1, y - 1)) {
        vld_num++;
        mv_cnt += vepu_pp_get_blk8_move_flag(mdw_buf, x + 1, y - 1);
    }

    if (vepu_pp_get_blk8_vld_flag(info, x - 1, y)) {
        vld_num++;
        mv_cnt += vepu_pp_get_blk8_move_flag(mdw_buf, x - 1, y);
    }

    if (vepu_pp_get_blk8_vld_flag(info, x + 1, y)) {
        vld_num++;
        mv_cnt += vepu_pp_get_blk8_move_flag(mdw_buf, x + 1, y);
    }

    if (vepu_pp_get_blk8_vld_flag(info, x - 1, y + 1)) {
        vld_num++;
        mv_cnt += vepu_pp_get_blk8_move_flag(mdw_buf, x - 1, y + 1);
    }

    if (vepu_pp_get_blk8_vld_flag(info, x, y + 1)) {
        vld_num++;
        mv_cnt += vepu_pp_get_blk8_move_flag(mdw_buf, x, y + 1);
    }

    if (vepu_pp_get_blk8_vld_flag(info, x + 1, y + 1)) {
        vld_num++;
        mv_cnt += vepu_pp_get_blk8_move_flag(mdw_buf, x + 1, y + 1);
    }

    *vld_cnt = vld_num;

    return mv_cnt;
}

/* src: downscaled 8x8 block
 * stride: height of blk8x8
 * x,y: blk8x8 pos
 * avg: average value of blk4x4
 */
static void vepu_pp_get_blk4_average(rk_u8 *src, rk_s32 stride, rk_s32 x, rk_s32 y, rk_u8 *avg)
{
    rk_u8 *src_b8 = src + stride * x * 8 + y * 64;
    rk_u8 *src_b4 = NULL;
    rk_u32 sum;
    rk_s32 idx;

    for (idx = 0; idx < 4; idx++) {
        src_b4 = src_b8 + (idx % 2) * 4 + (idx / 2) * 4 * 8;
        sum = src_b4[0 + 0 * 8] + src_b4[1 + 0 * 8] + src_b4[2 + 0 * 8] + src_b4[3 + 0 * 8] +
              src_b4[0 + 1 * 8] + src_b4[1 + 1 * 8] + src_b4[2 + 1 * 8] + src_b4[3 + 1 * 8] +
              src_b4[0 + 2 * 8] + src_b4[1 + 2 * 8] + src_b4[2 + 2 * 8] + src_b4[3 + 2 * 8] +
              src_b4[0 + 3 * 8] + src_b4[1 + 3 * 8] + src_b4[2 + 3 * 8] + src_b4[3 + 3 * 8];
        avg[idx] = (rk_u8)(sum >> 4);
    }
}

static rk_u8 vepu_pp_update_dust_flag(struct pp_chn_info_t *info, rk_s32 x, rk_s32 y)
{
    rk_u8 sum_luma_chg = 0;
    rk_u8 cur_ave[4];
    rk_u8 ref_ave[4];
    rk_u8 idx;
    rk_u8 *src, *ref;
    rk_u8 *pre_move;
    rk_s32 stride = PP_ALIGN(info->height, 32) / 4;

    if (info->frm_num % 2 == 0) {
        src = info->buf_rfpw->buf->kaddr;
        ref = info->buf_rfpr->buf->kaddr;
        pre_move = info->buf_rfmwr1;
    } else {
        src = info->buf_rfpr->buf->kaddr;
        ref = info->buf_rfpw->buf->kaddr;
        pre_move = info->buf_rfmwr0;
    }

    vepu_pp_get_blk4_average(src, stride, x, y, cur_ave);
    vepu_pp_get_blk4_average(ref, stride, x, y, ref_ave);

    kmpp_logi("frame %d pos(%d, %d) cur_ave %d %d %d %d ref_ave %d %d %d %d\n",
              info->frm_num, x * 32, y * 32, cur_ave[0], cur_ave[1], cur_ave[2], cur_ave[3],
              ref_ave[0], ref_ave[1], ref_ave[2], ref_ave[3]);

    if (vepu_pp_get_blk8_move_flag(pre_move, x, y)) {
        for (idx = 0; idx < 4; idx++) {
            if (ref_ave[idx] > cur_ave[idx] &&
                ref_ave[idx] < cur_ave[idx] + info->param.thd_md_vpp.thres_dust_chng_md)
                sum_luma_chg++;
        }
    } else {
        for (idx = 0; idx < 4; idx++) {
            if (cur_ave[idx] > ref_ave[idx] &&
                (cur_ave[idx] < ref_ave[idx] + info->param.thd_md_vpp.thres_dust_chng_md))
                sum_luma_chg++;
        }
    }

    kmpp_logi("frame %d pos(%d, %d) sum_luma_chg %d luma_chg %d\n",
              info->frm_num, x * 32, y * 32, sum_luma_chg, info->param.thd_md_vpp.thres_dust_chng_md);

    return (sum_luma_chg >= info->param.thd_md_vpp.thres_dust_blk_md);
}

static void vepu_pp_flycatkin_filter(struct pp_chn_info_t *info)
{
    if (!info->buf_rfmwr)
        vepu_pp_flycatkin_filter_init_buf(info);

    if (info->buf_rfmwr) {
        rk_s32 b8_col = PP_ALIGN(info->width, 32) / 32;
        rk_s32 b8_row = PP_ALIGN(info->height, 32) / 32;
        rk_s32 c, r, k;
        rk_s32 vld_cnt, dust_flg, move_sum;
        rk_u8 *mdw_buf = kmpp_dmabuf_get_kptr(info->md_buf);
        rk_u8 *mdw_ptr = NULL, *mdw_flt;

        if (info->frm_num % 2 == 0)
            sys_dmabuf_sync_for_cpu(info->buf_rfpw->buf, SYS_DMA_FROM_DEVICE);
        else
            sys_dmabuf_sync_for_cpu(info->buf_rfpr->buf, SYS_DMA_FROM_DEVICE);

        kmpp_dmabuf_flush_for_cpu(info->md_buf);
        kmpp_memcpy(info->buf_rfmwr2, mdw_buf, info->mdw_len);

        for (r = 0; r < b8_row; r++) {
            mdw_ptr = mdw_buf + MDW_STRIDE * r;
            mdw_flt = info->buf_rfmwr2 + MDW_STRIDE * r;

            for (c = 0; c < b8_col; c += 8) {
                for (k = 0; k < 8; k++) {
                    dust_flg = 0;

                    if (((c + k) * 32 < info->width) && (r * 32 < info->height) &&
                        (*mdw_ptr >> k) & 1) {
                        if (vepu_pp_get_blk8_move_flag(info->buf_rfmwr0, c + k, r) &&
                            vepu_pp_get_blk8_move_flag(info->buf_rfmwr1, c + k, r))
                            dust_flg = 0;
                        else {
                            move_sum = vepu_pp_get_blk8_move_cnt(info, c + k, r, &vld_cnt);

                            if (move_sum <= info->param.thd_md_vpp.thres_dust_move_md * vld_cnt / 8)
                                dust_flg = vepu_pp_update_dust_flag(info, c + k, r);
                        }

                        if (dust_flg) {
                            *mdw_flt = *mdw_flt & (~(1 << k));
                            kmpp_logi("frame %d pos(%d, %d) fly-catkin block\n",
                                      info->frm_num, (c + k) * 32, r * 32);
                        }
                    }
                }
                mdw_ptr++;
                mdw_flt++;
            }
        }

        kmpp_memcpy((info->frm_num % 2 == 0) ? info->buf_rfmwr0 : info->buf_rfmwr1,
                    mdw_buf, info->mdw_len);
        kmpp_memcpy(mdw_buf, info->buf_rfmwr2, info->mdw_len);
    }
}

static void vepu_pp_show_chn_attr(KmppEnvNode node)
{
    struct pp_chn_info_t *info = NULL;
    rk_s32 i;

    kmpp_env_log(node, "\n-----------------------------------ivs channel attr---------------------------------------------\n");
    kmpp_env_log(node, "%8s|%8s|%8s|%12s|%12s|%8s|%8s\n",
                 "ID", "width", "height", "max_width", "max_height", "md_en", "od_en");
    for (i = 0; i < MAX_CHN_NUM; i++) {
        info = &g_pp_ctx.chn_info[i];
        if (info->buf_rfpw == NULL || info->buf_rfpr == NULL)
            continue;

        kmpp_env_log(node, "%8d|%8d|%8d|%12d|%12d|%8d|%8d\n",
                     info->chn, info->width, info->height, info->max_width, info->max_height,
                     info->md_en, info->od_en);
    }
}

static void vepu_pp_show_com_cfg(KmppEnvNode node)
{
    struct pp_chn_info_t *info = NULL;
    struct pp_param_t *p = NULL;
    rk_s32 i;

    kmpp_env_log(node, "\n-----------------------------------ivs common cfg-----------------------------------------------\n");
    kmpp_env_log(node, "%8s|%8s|%15s|%15s\n",
                 "ID", "fmt", "frm_acc_intval", "frm_acc_gop");
    for (i = 0; i < MAX_CHN_NUM; i++) {
        info = &g_pp_ctx.chn_info[i];
        if (info->buf_rfpw == NULL || info->buf_rfpr == NULL)
            continue;
        p = &info->param;
        kmpp_env_log(node, "%8d|%8d|%15d|%15d\n",
                     info->chn, p->src_fmt.src_cfmt,
                     info->frm_accum_interval, info->frm_accum_gop);
    }
}

static void vepu_pp_show_md_cfg(KmppEnvNode node)
{
    struct pp_chn_info_t *info = NULL;
    struct pp_param_t *p = NULL;
    rk_s32 i;

    kmpp_env_log(node, "\n-----------------------------------ivs md cfg---------------------------------------------------\n");
    kmpp_env_log(node, "%8s|%10s|%10s|%10s|%10s|%10s|%15s|%15s|%15s\n",
                 "ID", "switch_sad", "thres_sad", "thres_move", "night_mode",
                 "filter_sw", "thres_dust_mv", "thres_dust_blk", "thres_dust_chng");
    for (i = 0; i < MAX_CHN_NUM; i++) {
        info = &g_pp_ctx.chn_info[i];
        if (info->buf_rfpw == NULL || info->buf_rfpr == NULL)
            continue;
        p = &info->param;
        kmpp_env_log(node, "%8d|%10d|%10d|%10d|%10d|%10d|%15d|%15d|%15d\n",
                     info->chn, p->vpp_base_cfg.switch_sad_md, p->thd_md_vpp.thres_sad_md,
                     p->thd_md_vpp.thres_move_md, p->vpp_base_cfg.night_mode_en_md,
                     info->flycatkin_en, p->thd_md_vpp.thres_dust_move_md,
                     p->thd_md_vpp.thres_dust_blk_md, p->thd_md_vpp.thres_dust_chng_md);
    }
}

static void vepu_pp_show_od_cfg(KmppEnvNode node)
{
    struct pp_chn_info_t *info = NULL;
    struct pp_param_t *p = NULL;
    rk_s32 i;

    kmpp_env_log(node, "\n-----------------------------------ivs od cfg---------------------------------------------------\n");
    kmpp_env_log(node, "%8s|%15s|%18s|%10s\n",
                 "ID", "thres_complex", "thres_complex_cnt", "thres_sad");
    for (i = 0; i < MAX_CHN_NUM; i++) {
        info = &g_pp_ctx.chn_info[i];
        if (info->buf_rfpw == NULL || info->buf_rfpr == NULL)
            continue;
        p = &info->param;
        kmpp_env_log(node, "%8d|%15d|%18d|%10d\n",
                     info->chn, p->thd_od_vpp.thres_complex_od,
                     p->thd_od_vpp.thres_complex_cnt_od, p->thd_od_vpp.thres_sad_od);
    }
}

void vepu_show_pp_info(KmppEnvNode node)
{
    vepu_pp_show_chn_attr(node);
    vepu_pp_show_com_cfg(node);
    vepu_pp_show_md_cfg(node);
    vepu_pp_show_od_cfg(node);
}

static void *vepu500_pp_chans[MAX_CHN_NUM] = {NULL};

void *vepu500_pp_id_to_ctx(rk_s32 id)
{
    return vepu500_pp_chans[id];
}

rk_s32 vepu500_pp_ctx_to_id(void *ctx)
{
    struct pp_chn_info_t *info = ctx;

    return info->chn;
}

rk_s32 vepu500_pp_init(void **ctx, void *cfg)
{
    struct pp_chn_info_t *info = NULL;
    struct pp_chn_attr *attr = (struct pp_chn_attr *)cfg;
    rk_s32 ret = rk_nok;
    rk_s32 i;

    for (i = 0; i < MAX_CHN_NUM; i++) {
        if (vepu500_pp_chans[i])
            continue;

        info = &g_pp_ctx.chn_info[i];
        kmpp_memset(info, 0, sizeof(*info));
        info->chn = i;
        info->used = 1;

        info->width = attr->width;
        info->height = attr->height;
        info->md_en = attr->md_en;
        info->od_en = attr->od_en;

        if (attr->max_width > 0 && attr->max_width <= 4096 &&
            attr->max_height > 0 && attr->max_height <= 4096) {
            info->max_width = attr->max_width;
            info->max_height = attr->max_height;
        } else {
            info->max_width = attr->width;
            info->max_height = attr->height;
        }

        {
            MppDevCfg dev_cfg = {0};

            dev_cfg.type = MPP_DEVICE_PP;
            dev_cfg.chan_id = i;

            ret = mpp_dev_init(&info->dev, &dev_cfg);
            if (ret) {
                kmpp_loge_f("mpp dev init failed\n");
                return VEPU_PP_NOK;
            }

            info->device = mpp_get_dev(info->dev);
        }

        ret = pp_allocate_buffer(info);
        *ctx = info;
        vepu500_pp_chans[i] = info;
        return ret;
    }

    kmpp_loge_f("vepu500 pp no free channel\n");

    return ret;
}

rk_s32 vepu500_pp_deinit(void *ctx)
{
    struct pp_chn_info_t *info = ctx;

    if (!info) {
        kmpp_loge_f("vepu500 pp deinit failed\n");
        return VEPU_PP_NOK;
    }

    vepu500_pp_chans[info->chn] = NULL;

    pp_release_buffer(info);

    mpp_dev_deinit(info->dev);

    kmpp_free(info->buf_rfmwr);

    return VEPU_PP_OK;
}

static void update_st_reg(struct pp_chn_info_t *info)
{
    struct pp_param_t *p = &info->param;

    info->frm_cnt = 0;
    p->int_en.enc_done_en = 1;
    p->int_msk = 0;
    p->enc_pic.enc_stnd = 2;
    p->enc_pic.cur_frm_ref = 1;//TODO:
    p->enc_rsl.pic_wd8_m1 = (PP_ALIGN(info->width, 8) >> 3) - 1;
    p->enc_rsl.pic_hd8_m1 = (PP_ALIGN(info->height, 8) >> 3) - 1;

    //p->src_fmt.src_cfmt = cfg->fmt;

    p->src_fill.pic_wfill = 0;
    p->src_fill.pic_hfill = 0;

    p->pic_ofst.pic_ofst_x = 0;
    p->pic_ofst.pic_ofst_y = 0;

    //p->vpp_base_cfg.cur_frm_en_md = info->md_en && (info->frm_cnt > 0);
    /*
     * Fix bug:
     * Disable the ref_frm_en_md to prevent the hw stuck.
     * This will cause the flying catkin detection to fail.
     */
    p->vpp_base_cfg.ref_frm_en_md = 0;

    p->vpp_base_cfg.en_od = info->od_en;
    //p->vpp_base_cfg.background_en_od = (info->frm_cnt > 0);
    p->vpp_base_cfg.sad_comp_en_od = 1;
    p->vpp_base_cfg.vepu_pp_en = 1;

    /* cime cfg */
    p->me_rnge.cime_srch_dwnh = 4;
    p->me_rnge.cime_srch_uph  = 4;
    p->me_rnge.cime_srch_rgtw = 4;
    p->me_rnge.cime_srch_lftw = 4;
    p->me_rnge.dlt_frm_num    = 1;

    p->me_cfg.srgn_max_num   = 54;
    p->me_cfg.cime_dist_thre = 1024;
    p->me_cfg.rme_srch_h     = 0;
    p->me_cfg.rme_srch_v     = 0;

    p->me_cach.cime_zero_thre = 64;
    p->me_cach.fme_prefsu_en  = 0;
    p->me_cach.colmv_load     = 0;
    p->me_cach.colmv_stor     = 0;

    //p->synt_sli0.sli_type = info->frm_cnt > 0 ? 1 : 2;

    p->jpeg_base_cfg.jpeg_stnd = 0;
}

static void update_rt_reg(struct pp_chn_info_t *info)
{
    struct pp_param_t *p = &info->param;
    KmppFrameImpl *frame = info->frame;
    MppFrameFormat fmt = MPP_FMT_YUV420SP;

    kmpp_logi_f("update dynamic registers\n");

    kmpp_frame_get_fmt(frame, &fmt);

    p->src_fmt.src_cfmt = fmt;

    p->vpp_base_cfg.cur_frm_en_md = info->md_en && (info->frm_cnt > 0);
    p->vpp_base_cfg.background_en_od = (info->frm_cnt > 0);
    p->synt_sli0.sli_type = info->frm_cnt > 0 ? 1 : 2;
}

rk_s32 vepu500_pp_ctrl(void *ctx, rk_s32 cmd, void *arg)
{
    struct pp_chn_info_t *info = (struct pp_chn_info_t *)ctx;
    struct pp_param_t *p = &info->param;

    switch (cmd) {
    case KMPP_VSP_CMD_GET_ST_CFG : {
    } break;
    case KMPP_VSP_CMD_SET_ST_CFG : {
    } break;
    case KMPP_VSP_CMD_GET_RT_CFG : {
        kmpp_memcpy(arg, &info->cfg, sizeof(info->cfg));
    } break;
    case KMPP_VSP_CMD_SET_RT_CFG : {
        KmppVspPpCfg *cfg = (KmppVspPpCfg *)arg;
        struct pp_com_cfg com;

        kmpp_memcpy(&info->cfg, cfg, sizeof(*cfg));

        info->width = cfg->width;
        info->height = cfg->height;
        info->md_en = cfg->md.enable;
        info->od_en = cfg->od.enable;
        info->frm_accum_gop = 0;
        info->frm_accum_interval = 0;

        kmpp_memset(&com, 0, sizeof(com));
        com.frm_cnt = info->frm_num++;
        com.fmt = MPP_FMT_YUV420SP;
        com.gop = 0;
        com.md_interval = 0;
        com.od_interval = 0;
        com.src_buf = NULL;
        com.stride0 = cfg->width;
        com.stride1 = cfg->width;

        kmpp_logi_f("width %d, height %d\n", cfg->width, cfg->height);
        pp_set_common_cfg(info, &com);

        p->vpp_base_cfg.switch_sad_md = cfg->md.switch_sad;
        p->vpp_base_cfg.night_mode_en_md = 0;
        p->vpp_base_cfg.flycatkin_flt_en_md = 0;

        p->thd_md_vpp.thres_sad_md = cfg->md.thres_sad;
        p->thd_md_vpp.thres_move_md = cfg->md.thres_move;
        p->thd_md_vpp.thres_dust_move_md = cfg->md.thres_dust_move;
        p->thd_md_vpp.thres_dust_blk_md = cfg->md.thres_dust_blk;
        p->thd_md_vpp.thres_dust_chng_md = cfg->md.thres_dust_chng;
        p->vpp_base_cfg.sto_stride_md = 4;

        info->flycatkin_en = cfg->md.filter_switch || cfg->md.night_mode;

        p->vpp_base_cfg.sto_stride_od = 4;
        p->thd_od_vpp.thres_complex_od = cfg->od.thres_complex;
        p->thd_od_vpp.thres_complex_cnt_od = cfg->od.thres_complex_cnt;
        p->thd_od_vpp.thres_sad_od = cfg->od.thres_sad;

        kmpp_logi_f("update static registers\n");
        update_st_reg(info);
    } break;
    default : {
        kmpp_loge_f("invalid cmd %d\n", cmd);
        return rk_nok;
    } break;
    }

    return rk_ok;
}

rk_s32 vepu500_pp_proc(void *ctx, KmppFrame in, KmppFrame *out)
{
    struct pp_chn_info_t *info = (struct pp_chn_info_t *)ctx;
    struct pp_param_t *p = &info->param;
    KmppFrameImpl *impl = kmpp_obj_to_entry(in);
    KmppMeta meta = impl->meta.kptr;
    KmppBuffer md = NULL;
    KmppBuffer od = NULL;
    KmppBuffer in_buf = NULL;

    info->frame = in;

    if (meta) {
        KmppMetaImpl *impl = kmpp_obj_to_entry(meta);

        /* NOTE: keep md and od meta key state */
        md = impl->vals.pp_md_buf.val_shm.kptr;
        od = impl->vals.pp_od_buf.val_shm.kptr;
    }

    update_rt_reg(info);

    /* setup input address registers */
    in_buf = kmpp_obj_from_shmptr(&impl->buffer);
    info->in_buf = kmpp_buffer_get_dmabuf(in_buf);

    pp_set_src_addr(info);

    {
        rk_u64 iova;

        if (info->md_en) {
            iova = 0;
            info->md_buf = kmpp_buffer_get_dmabuf(md);
            kmpp_dmabuf_get_iova_by_device(info->md_buf, &iova, info->device);
            p->adr_md_vpp = iova;
        }

        if (info->od_en) {
            iova = 0;
            info->od_buf = kmpp_buffer_get_dmabuf(od);
            kmpp_dmabuf_get_iova_by_device(info->od_buf, &iova, info->device);
            p->adr_od_vpp = iova;
        }
    }

    /* setup internal address registers */
    if (info->frm_cnt % 2 == 0) {
        p->dspw_addr = info->buf_rfpw->iova;
        p->dspr_addr = info->buf_rfpr->iova;
    } else {
        p->dspw_addr = info->buf_rfpr->iova;
        p->dspr_addr = info->buf_rfpw->iova;
    }

    /* send task to hardware */
    {
        MppDevRegWrCfg reg_wr;
        MppDevRegRdCfg reg_rd;

        reg_wr.reg = &info->param;
        reg_wr.size = sizeof(info->param);
        reg_wr.offset = 0;

        mpp_dev_ioctl(info->dev, MPP_DEV_REG_WR, &reg_wr);

        reg_rd.reg = &info->output;
        reg_rd.size = sizeof(info->output);
        reg_rd.offset = 0;

        mpp_dev_ioctl(info->dev, MPP_DEV_REG_RD, &reg_rd);
        mpp_dev_ioctl(info->dev, MPP_DEV_CMD_SEND, NULL);
        mpp_dev_ioctl(info->dev, MPP_DEV_CMD_POLL, NULL);

        if (info->md_en && info->flycatkin_en && info->md_buf)
            vepu_pp_flycatkin_filter(info);
    }

    if (p->adr_src0) {
        kmpp_dmabuf_put_iova_by_device(info->in_buf, p->adr_src0, info->device);
        p->adr_src0 = 0;
    }
    if (p->adr_md_vpp) {
        kmpp_dmabuf_put_iova_by_device(info->md_buf, p->adr_md_vpp, info->device);
        p->adr_md_vpp = 0;
    }
    if (p->adr_od_vpp) {
        kmpp_dmabuf_put_iova_by_device(info->od_buf, p->adr_od_vpp, info->device);
        p->adr_od_vpp = 0;
    }

    if (meta) {
        KmppMetaImpl *impl = kmpp_obj_to_entry(meta);
        KmppVspRtOut pp_out = impl->vals.pp_out.val_shm.kptr;

        if (pp_out) {
            KmppVspPpOut *out = kmpp_obj_to_entry(pp_out);

            out->od.flag = od && info->od_en;
            out->od.pix_sum = info->output.od_out_pix_sum;
        }
    }

    info->frm_cnt++;

    if (out)
        *out = in;

    return rk_ok;
}

KmppVspApi vepu500_pp_api = {
    .name       = "vepu500_pp",
    .ctx_size   = sizeof(struct pp_chn_info_t),
    .id_to_ctx  = vepu500_pp_id_to_ctx,
    .ctx_to_id  = vepu500_pp_ctx_to_id,
    .init       = vepu500_pp_init,
    .deinit     = vepu500_pp_deinit,
    .ctrl       = vepu500_pp_ctrl,
    .proc       = vepu500_pp_proc,
};

/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
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
#include "vepu511_pp.h"

#define MDW_STRIDE  (64) /* 64 bytes, 512(64 * 8) blk8x8 */

static struct vepu511_pp_ctx_t g_pp_ctx;

static struct pp_buffer_t * pp_malloc_buffer(vepu511_pp_chn_info *info, rk_u32 size)
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

static void pp_free_buffer(vepu511_pp_chn_info *info, struct pp_buffer_t *pp_buf)
{
    if (!pp_buf)
        return;

    if (pp_buf->buf) {
        sys_dmabuf_free(pp_buf->buf);
        pp_buf->buf = NULL;
    }

    kmpp_free(pp_buf);
}

static void pp_release_buffer(vepu511_pp_chn_info *info)
{
    pp_free_buffer(info, info->buf_rfpw);
    pp_free_buffer(info, info->buf_rfpr);
    pp_free_buffer(info, info->buf_rfmwr);
    pp_free_buffer(info, info->buf_rfmrd);
}

static rk_s32 pp_allocate_buffer(vepu511_pp_chn_info *info)
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
        kmpp_loge_f("alloc down scale rw buffer failed\n");
        ret = VEPU_PP_NOK;
        goto __return;
    }

    /* alloc buffer for md rw */
    buf_len = (64) * (h >> 5);
    info->buf_rfmwr = pp_malloc_buffer(info, buf_len);
    info->buf_rfmrd = pp_malloc_buffer(info, buf_len);
    if (!info->buf_rfmwr || !info->buf_rfmrd) {
        kmpp_loge_f("alloc md rw buffer failed\n");
        ret = VEPU_PP_NOK;
        goto __return;
    }

__return:
    if (ret)
        pp_release_buffer(info);
    return ret;
}

static void pp_set_src_addr(vepu511_pp_chn_info *info, vepu_pp_hw_fmt_cfg hw_fmt_cfg)
{
    KmppVspPpCfg *cfg = &info->cfg;
    vepu511_pp_param *p = &info->param;
    rk_u32 adr_src0 = 0, adr_src1 = 0, adr_src2 = 0;
    rk_u32 width = cfg->width, height = cfg->height;
    rk_u32 stride_y = 0, stride_c = 0;
    rk_u64 iova = 0;

    kmpp_dmabuf_get_iova_by_device(info->in_buf, &iova, info->device);
    adr_src0 = iova;

    switch (hw_fmt_cfg.format) {
    case VEPU_PP_HW_FMT_YUV420P: {
        adr_src1 = adr_src0 + width * height;
        adr_src2 = adr_src1 + width * height / 4;
        stride_y = width;
        stride_c = width / 2;
    } break;
    case VEPU_PP_HW_FMT_YUV420SP: {
        adr_src1 = adr_src0 + width * height;
        adr_src2 = adr_src1;
        stride_y = width;
        stride_c = width;
    } break;
    case VEPU_PP_HW_FMT_RGB888: {
        adr_src1 = adr_src0;
        adr_src2 = adr_src0;
        stride_y = width * 3;
        stride_c = stride_y / 2;
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

static void vepu_pp_show_chn_attr(KmppEnvNode node)
{
    vepu511_pp_chn_info *info = NULL;
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
    vepu511_pp_chn_info *info = NULL;
    vepu511_pp_param *p = NULL;
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
    vepu511_pp_chn_info *info = NULL;
    vepu511_pp_param *p = NULL;
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
    vepu511_pp_chn_info *info = NULL;
    vepu511_pp_param *p = NULL;
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

static void *vepu511_pp_chans[MAX_CHN_NUM] = {NULL};

void *vepu511_pp_id_to_ctx(rk_s32 id)
{
    return vepu511_pp_chans[id];
}

rk_s32 vepu511_pp_ctx_to_id(void *ctx)
{
    vepu511_pp_chn_info *info = ctx;

    return info->chn;
}

rk_s32 vepu511_pp_init(void **ctx, void *cfg)
{
    vepu511_pp_chn_info *info = NULL;
    struct pp_chn_attr *attr = (struct pp_chn_attr *)cfg;
    rk_s32 ret = rk_nok;
    rk_s32 i;

    for (i = 0; i < MAX_CHN_NUM; i++) {
        if (vepu511_pp_chans[i])
            continue;

        info = &g_pp_ctx.chn_info[i];
        kmpp_memset(info, 0, sizeof(*info));
        info->chn = i;
        info->used = 1;

        info->width = attr->width;
        info->height = attr->height;
        info->md_en = attr->md_en;
        info->od_en = attr->od_en;
        /* setup default value */
        info->frm_accum_gop = 0;
        info->frm_accum_interval = 0;
        info->gop = 0;
        info->md_interval = 0;
        info->od_interval = 0;

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
        vepu511_pp_chans[i] = info;
        return ret;
    }

    kmpp_loge_f("vepu511 pp no free channel\n");

    return ret;
}

rk_s32 vepu511_pp_deinit(void *ctx)
{
    vepu511_pp_chn_info *info = ctx;

    if (!info) {
        kmpp_loge_f("vepu511 pp deinit failed\n");
        return VEPU_PP_NOK;
    }

    vepu511_pp_chans[info->chn] = NULL;

    pp_release_buffer(info);

    mpp_dev_deinit(info->dev);

    return VEPU_PP_OK;
}

static void vepu511_pp_update_st_reg(vepu511_pp_chn_info *info)
{
    vepu511_pp_param *p = &info->param;
    KmppVspPpCfg *cfg = &info->cfg;

    p->int_en.enc_done_en = 1;
    p->int_msk = 0;
    /* enc_stnd  vepu_pp_en(0x530[31])
     * 2'b00            1              ->  h264 & vepu_pp(64x16)
     * 2'b01            1              ->  h265 & vepu_pp(32x32)
     * 2'b10            1              ->  vepu_pp(64x16)
     * 2'b11            1              ->  vepu_pp(32x32）
     */
    p->enc_pic.enc_stnd = 3;
    p->enc_pic.cur_frm_ref = 1;
    p->enc_rsl.pic_wd8_m1 = (PP_ALIGN(info->width, 8) >> 3) - 1;
    p->enc_rsl.pic_hd8_m1 = (PP_ALIGN(info->height, 8) >> 3) - 1;

    p->src_fill.pic_wfill = 0;
    p->src_fill.pic_hfill = 0;

    p->pic_ofst.pic_ofst_x = 0;
    p->pic_ofst.pic_ofst_y = 0;

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

    p->jpeg_base_cfg.jpeg_stnd = 0;

    if (info->md_en) {
        p->vpp_base_cfg.switch_sad_md = cfg->md.switch_sad;
        p->thd_md_vpp.thres_sad_md = cfg->md.thres_sad;
        p->thd_md_vpp.thres_move_md = cfg->md.thres_move;
        p->thd_md_vpp.thres_dust_move_md = cfg->md.thres_dust_move;
        p->thd_md_vpp.thres_dust_blk_md = cfg->md.thres_dust_blk;
        p->thd_md_vpp.thres_dust_chng_md = cfg->md.thres_dust_chng;
        p->vpp_base_cfg.sto_stride_md = 4;
        p->vpp_base_cfg.night_mode_en_md = cfg->md.night_mode;
        p->vpp_base_cfg.flycatkin_flt_en_md = cfg->md.filter_switch;
    }

    if (info->od_en) {
        p->vpp_base_cfg.sto_stride_od = 4;
        p->thd_od_vpp.thres_complex_od = cfg->od.thres_complex;
        p->thd_od_vpp.thres_complex_cnt_od = cfg->od.thres_complex_cnt;
        p->thd_od_vpp.thres_sad_od = cfg->od.thres_sad;
    }
}

static void vepu511_pp_update_rt_reg(vepu511_pp_chn_info *info, vepu_pp_hw_fmt_cfg hw_fmt_cfg)
{
    vepu511_pp_param *p = &info->param;
    rk_s32 gop = info->gop ? info->gop : 30;
    rk_s32 md_od_switch = (info->frm_accum_interval == 0);
    rk_s32 interval = info->md_interval ? info->md_interval : 1;

    p->src_fmt.src_cfmt = hw_fmt_cfg.format;
    p->src_fmt.alpha_swap = hw_fmt_cfg.alpha_swap;
    p->src_fmt.rbuv_swap = hw_fmt_cfg.rbuv_swap;
    p->vpp_base_cfg.en_od = info->od_en && md_od_switch;
    p->vpp_base_cfg.cur_frm_en_md = info->md_en && md_od_switch && (info->frm_cnt > 0);
    p->vpp_base_cfg.ref_frm_en_md = p->vpp_base_cfg.cur_frm_en_md && (info->frm_cnt > interval);
    p->vpp_base_cfg.background_en_od = (info->frm_cnt > 0);
    p->synt_sli0.sli_type = info->frm_cnt > 0 ? 1 : 2;

    if (p->src_fmt.src_cfmt < VEPU_PP_HW_FMT_RSV_3) {
        p->src_udfy.csc_wgt_r2y = 77;
        p->src_udfy.csc_wgt_g2y = 150;
        p->src_udfy.csc_wgt_b2y = 29;

        p->src_udfu.csc_wgt_r2u = -43;
        p->src_udfu.csc_wgt_g2u = -85;
        p->src_udfu.csc_wgt_b2u = 128;

        p->src_udfv.csc_wgt_r2v = 128;
        p->src_udfv.csc_wgt_g2v = -107;
        p->src_udfv.csc_wgt_b2v = -21;

        p->src_udfo.csc_ofst_y = 0;
        p->src_udfo.csc_ofst_u = 128;
        p->src_udfo.csc_ofst_v = 128;
    }

    info->frm_accum_interval++;
    if (info->frm_accum_interval == interval)
        info->frm_accum_interval = 0;

    info->frm_accum_gop++;
    if (info->frm_accum_gop == gop)
        info->frm_accum_gop = 0;
}

rk_s32 vepu511_pp_ctrl(void *ctx, rk_s32 cmd, void *arg)
{
    vepu511_pp_chn_info *info = (vepu511_pp_chn_info *)ctx;

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

        kmpp_memcpy(&info->cfg, cfg, sizeof(*cfg));

        if (info->width != cfg->width || info->height != cfg->height) {
            info->frm_accum_gop = 0;
            info->frm_accum_interval = 0;
            info->gop = 0;
            info->md_interval = 0;
            info->od_interval = 0;
        }

        info->width = cfg->width;
        info->height = cfg->height;
        info->md_en = cfg->md.enable;
        info->od_en = cfg->od.enable;

        kmpp_logi_f("width %d, height %d\n", cfg->width, cfg->height);
        vepu511_pp_update_st_reg(info);
    } break;
    default : {
        kmpp_loge_f("invalid cmd %d\n", cmd);
        return rk_nok;
    } break;
    }

    return rk_ok;
}

rk_s32 vepu511_pp_proc(void *ctx, KmppFrame in, KmppFrame *out)
{
    vepu511_pp_chn_info *info = (vepu511_pp_chn_info *)ctx;
    vepu511_pp_param *p = &info->param;
    KmppFrameImpl *impl = kmpp_obj_to_entry(in);
    KmppMeta meta = impl->meta.kptr;
    KmppBuffer md = NULL;
    KmppBuffer od = NULL;
    KmppBuffer in_buf = NULL;
    vepu_pp_hw_fmt_cfg hw_fmt_cfg;

    info->frame = in;

    if (meta) {
        KmppMetaImpl *impl = kmpp_obj_to_entry(meta);

        /* NOTE: keep md and od meta key state */
        md = impl->vals.pp_md_buf.val_shm.kptr;
        od = impl->vals.pp_od_buf.val_shm.kptr;
    }

    vepu_pp_transform_hw_fmt_cfg(&hw_fmt_cfg, impl->fmt);
    vepu511_pp_update_rt_reg(info, hw_fmt_cfg);

    /* setup input address registers */
    in_buf = kmpp_obj_from_shmptr(&impl->buffer);
    info->in_buf = kmpp_buffer_get_dmabuf(in_buf);
    pp_set_src_addr(info, hw_fmt_cfg);

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

    if (info->frm_accum_gop % 2) {
        p->adr_ref_mdw = info->buf_rfmwr->iova;
        p->adr_ref_mdr = info->buf_rfmrd->iova;
    } else {
        p->adr_ref_mdr = info->buf_rfmwr->iova;
        p->adr_ref_mdw = info->buf_rfmrd->iova;
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

KmppVspApi vepu511_pp_api = {
    .name       = "vepu511_pp",
    .ctx_size   = sizeof(vepu511_pp_chn_info),
    .id_to_ctx  = vepu511_pp_id_to_ctx,
    .ctx_to_id  = vepu511_pp_ctx_to_id,
    .init       = vepu511_pp_init,
    .deinit     = vepu511_pp_deinit,
    .ctrl       = vepu511_pp_ctrl,
    .proc       = vepu511_pp_proc,
};

/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */
#include "kmpp_mem.h"
#include "kmpp_env.h"

#include "rk_export_func.h"
#include "vepu_pp.h"

static struct vepu_pp_ctx_t g_pp_ctx;
static struct vcodec_mpibuf_fn *g_mpi_buf_fn = NULL;

void register_vmpibuf_func_to_pp(struct vcodec_mpibuf_fn *mpibuf_fn)
{
    g_mpi_buf_fn = mpibuf_fn;
}
EXPORT_SYMBOL(register_vmpibuf_func_to_pp);

void unregister_vmpibuf_func_pp(void)
{
    g_mpi_buf_fn = NULL;
}
EXPORT_SYMBOL(unregister_vmpibuf_func_pp);

static struct vcodec_mpibuf_fn * get_vmpibuf_func(void)
{
    if (IS_ERR_OR_NULL(g_mpi_buf_fn)) {
        pr_err("%s failed\n", __FUNCTION__);
        return ERR_PTR(-ENOMEM);
    } else
        return g_mpi_buf_fn;
}

static struct pp_buffer_t * pp_malloc_buffer(struct pp_chn_info_t *info, RK_U32 size)
{
    struct vcodec_mpibuf_fn *func = get_vmpibuf_func();
    struct pp_buffer_t *pp_buf = NULL;

    pp_buf = kmpp_calloc(sizeof(*pp_buf));
    if (IS_ERR_OR_NULL(pp_buf)) {
        pp_err("failed\n");
        return ERR_PTR(-ENOMEM);
    }

    if (func->buf_alloc) {
        pp_buf->buf = func->buf_alloc(size);
        if (pp_buf->buf) {
            if (func->buf_get_dmabuf) {
                pp_buf->buf_dma = func->buf_get_dmabuf(pp_buf->buf);
                if (func->buf_get_paddr)
                    pp_buf->iova = func->buf_get_paddr(pp_buf->buf);

                if (pp_buf->iova == -1)
                    pp_buf->iova = info->api->get_address(info->dev_srv, pp_buf->buf_dma, 0);
            }
        }
    }

    return pp_buf;
}

static void pp_free_buffer(struct pp_chn_info_t *info, struct pp_buffer_t *pp_buf)
{
    struct vcodec_mpibuf_fn *func = get_vmpibuf_func();

    if (pp_buf) {
        if (pp_buf->buf_dma)
            info->api->release_address(info->dev_srv, pp_buf->buf_dma);
        if (pp_buf->buf)
            func->buf_unref(pp_buf->buf);

        kmpp_free(pp_buf);
    }
}

static int pp_allocate_buffer(struct pp_chn_info_t *info)
{
    int w = ds_en ? PP_ALIGN(info->max_width, 32) : PP_ALIGN(info->max_width, 32) * 4;
    int h = ds_en ? PP_ALIGN(info->max_height, 32) : PP_ALIGN(info->max_height, 32) * 4;
    int pic_wd8_m1 = (w >> 3) - 1;
    int pic_hd8_m1 = (h >> 3) - 1;
    int buf_len = 0, wi, hi;
    int ds_en = 0;
    enum PP_RET ret = VEPU_PP_OK;

    if (ds_en) {
        wi = ((pic_wd8_m1 + 4) >> 2) * 2;
        hi = ((pic_hd8_m1 + 4) >> 2) * 2;
    } else {
        wi = ((pic_wd8_m1 + 16) >> 4) * 8;
        hi = ((pic_hd8_m1 + 16) >> 4) * 8;
    }
    buf_len = wi * hi * 16;

    info->buf_rfpw = pp_malloc_buffer(info, buf_len);
    info->buf_rfpr = info->buf_rfpw;
    if (IS_ERR_OR_NULL(info->buf_rfpw) ||
        IS_ERR_OR_NULL(info->buf_rfpr)) {
        pp_err("failed\n");
        ret = VEPU_PP_NOK;
    }

    {
        if (ds_en) {
            wi = (pic_wd8_m1 + 4) >> 2;
            hi = (pic_hd8_m1 + 4) >> 2;
        } else {
            wi = ((pic_wd8_m1 + 16) >> 4) * 4;
            hi = ((pic_hd8_m1 + 16) >> 4) * 4;
        }
        buf_len = ((wi * hi + 63) >> 6) * 16;

        info->buf_rfmwr = pp_malloc_buffer(info, buf_len);
        if (IS_ERR_OR_NULL(info->buf_rfmwr)) {
            pp_err("failed\n");
            ret = VEPU_PP_NOK;
        }
    }

    {
        if (ds_en) {
            wi = ((pic_wd8_m1 + 4) >> 2) * 2;
            hi = ((pic_hd8_m1 + 4) >> 2) * 2;
        } else {
            wi = ((pic_wd8_m1 + 16) >> 4) * 8;
            hi = ((pic_hd8_m1 + 16) >> 4) * 8;
        }
        buf_len = ((wi * hi + 127) >> 7) * 16;

        info->buf_rfswr = pp_malloc_buffer(info, buf_len);
        if (IS_ERR_OR_NULL(info->buf_rfswr)) {
            pp_err("failed\n");
            ret = VEPU_PP_NOK;
        }

        info->smear_stride = ((pic_wd8_m1 + 4) / 4 + 7) / 8 * 16;
    }

    return ret;
}

static void pp_release_buffer(struct pp_chn_info_t *info)
{
    pp_free_buffer(info, info->buf_rfpw);
    pp_free_buffer(info, info->buf_rfmwr);
    pp_free_buffer(info, info->buf_rfswr);
}

int vepu_pp_create_chn(int chn, struct pp_chn_attr *attr)
{
    struct pp_chn_info_t *info = NULL;
    KmppSessionCfg sess_cfg = {0};

    pr_info("%s %d\n", __FUNCTION__, chn);
    if (chn >= MAX_CHN_NUM) {
        pp_err("vepu pp create channel id %d error\n", chn);
        return VEPU_PP_NOK;
    }

    info = &g_pp_ctx.chn_info[chn];

    memset(info, 0, sizeof(*info));
    info->chn = chn;
    info->width = attr->width;
    info->height = attr->height;
    info->smear_en = attr->smear_en;
    info->weightp_en = attr->weightp_en;
    info->md_en = attr->md_en;
    info->od_en = attr->od_en;
    info->down_scale_en = attr->down_scale_en;

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
        MPP_RET ret;

        dev_cfg.type = MPP_DEVICE_RKVENC_PP;
        dev_cfg.chan_id = 0;

        ret = mpp_dev_init(&info->dev, &dev_cfg);
        if (ret) {
            kmpp_loge_f("mpp dev init failed\n");
            return VEPU_PP_NOK;
        }

        info->device = mpp_get_dev(info->dev);
    }

    return pp_allocate_buffer(info);
}
EXPORT_SYMBOL(vepu_pp_create_chn);

int vepu_pp_destroy_chn(int chn)
{
    struct pp_chn_info_t *info = NULL;

    pr_info("%s %d\n", __FUNCTION__, chn);

    if (chn >= MAX_CHN_NUM) {
        pp_err("vepu pp destroy channel id %d error\n", chn);
        return VEPU_PP_NOK;
    }

    info = &g_pp_ctx.chn_info[chn];

    pp_release_buffer(info);

    mpp_dev_deinit(info->dev);

    return VEPU_PP_OK;
}
EXPORT_SYMBOL(vepu_pp_destroy_chn);

static void pp_set_src_addr(struct pp_chn_info_t *info, struct pp_com_cfg *cfg)
{
    struct pp_param_t *p = &info->param;
    RK_U32 adr_src0 = 0, adr_src1 = 0, adr_src2 = 0;
    RK_U32 width = info->width, height = info->height;
    struct vcodec_mpibuf_fn *func = get_vmpibuf_func();

    if (func->buf_get_paddr)
        adr_src0 = func->buf_get_paddr(cfg->src_buf);

    switch (cfg->fmt) {
    case MPP_FMT_YUV420P: {
        adr_src1 = adr_src0 + width * height;
        adr_src2 = adr_src1 + width * height / 4;
        break;
    }
    case MPP_FMT_YUV420SP: {
        adr_src1 = adr_src0 + width * height;
        adr_src2 = adr_src1;
        break;
    }
    default: {
    }
    }

    p->adr_src0 = adr_src0;
    p->adr_src1 = adr_src1;
    p->adr_src2 = adr_src2;
}

static void pp_set_common_addr(struct pp_chn_info_t *info, struct pp_com_cfg *cfg)
{
    struct pp_param_t *p = &info->param;

    //TODO: md_interval > 1
    if (cfg->frm_cnt % 2 == 0) {
        p->adr_rfpw = info->buf_rfpw->iova;
        p->adr_rfpr0 = info->buf_rfpr->iova;
        p->adr_rfpr1 = p->adr_rfpr0;
    } else {
        p->adr_rfpw = info->buf_rfpr->iova;
        p->adr_rfpr0 = info->buf_rfpw->iova;
        p->adr_rfpr1 = p->adr_rfpr0;
    }
}

static void pp_set_channel_info(struct pp_chn_info_t *info, struct pp_chn_attr *attr)
{
    info->width = attr->width;
    info->height = attr->height;
    info->smear_en = attr->smear_en;
    info->weightp_en = attr->weightp_en;
    info->md_en = attr->md_en;
    info->od_en = attr->od_en;
    info->down_scale_en = attr->down_scale_en;
    info->frm_accum_gop = 0;
    info->frm_accum_interval = 0;
}

static void vepu_pp_set_param(struct pp_chn_info_t *info, enum pp_cmd cmd, void *param)
{
    struct pp_param_t *p = &info->param;

    switch (cmd) {
    case PP_CMD_SET_CHANNEL_INFO: {
        struct pp_chn_attr *cfg = (struct pp_chn_attr *)param;
        pp_set_channel_info(info, cfg);
        break;
    }
    case PP_CMD_SET_COMMON_CFG: {
        struct pp_com_cfg *cfg = (struct pp_com_cfg *)param;
        int frm_cnt = cfg->frm_cnt;
        int gop = cfg->gop ? cfg->gop : 30;
        int interval = cfg->md_interval ? cfg->md_interval : 1;
        int md_od_switch = (info->frm_accum_interval == 0);

        p->enc_pic_fmt.src_from_isp = !info->down_scale_en;
        p->enc_pic_fmt.ref_pic0_updt_en = (info->smear_en || info->weightp_en) &&
                                          (info->frm_accum_gop != (gop - 1));
        p->enc_pic_fmt.ref_pic1_updt_en = (info->md_en && md_od_switch) ||
                                          (info->od_en && md_od_switch);

        p->enc_pic_rsl.pic_wd8_m1 = (info->width >> 3) - 1;
        p->enc_pic_rsl.pic_hd8_m1 = (info->height >> 3) - 1;

        p->vsp_pic_con.src_cfmt = cfg->fmt;

        p->vsp_pic_fill.pic_wfill = 0;
        p->vsp_pic_fill.pic_hfill = 0;

        p->vsp_pic_ofst.pic_ofst_x = 0;
        p->vsp_pic_ofst.pic_ofst_y = 0;
        p->vsp_pic_strd0.src_strd0 = info->width;
        p->vsp_pic_strd1.src_strd1 = info->width >> 1; //TODO: different format

        p->smr_con_base.smear_cur_frm_en = info->smear_en && info->frm_accum_gop;
        p->smr_con_base.smear_ref_frm_en = p->smr_con_base.smear_cur_frm_en && (info->frm_accum_gop != 1);
        p->smr_sto_strd = ((((p->enc_pic_rsl.pic_wd8_m1 + 4) >> 2) + 7) >> 3) * 16;

        p->wp_con_comb0 = info->weightp_en && info->frm_accum_gop;

        p->md_con_base.cur_frm_en = info->md_en && md_od_switch && (frm_cnt > 0);
        p->md_con_base.ref_frm_en = p->md_con_base.cur_frm_en && (frm_cnt > interval);

        {
            //TODO:
            RK_U32 od_en = info->od_en && md_od_switch;
            RK_U32 od_background_en = (frm_cnt > 0);
            RK_U32 od_sad_comp_en = 1;
            p->od_con_base = od_en | (od_background_en << 1) | (od_sad_comp_en << 2);
        }

        pp_set_src_addr(info, cfg);
        pp_set_common_addr(info, cfg);

        info->frm_accum_interval++;
        if (info->frm_accum_interval == interval)
            info->frm_accum_interval = 0;

        info->frm_accum_gop++;
        if (info->frm_accum_gop == gop)
            info->frm_accum_gop = 0;
        break;
    }
    case PP_CMD_SET_MD_CFG: {
        struct pp_md_cfg *cfg = (struct pp_md_cfg *)param;
        struct vcodec_mpibuf_fn *func = get_vmpibuf_func();

        p->md_con_base.switch_sad = cfg->switch_sad;
        p->md_con_base.thres_sad = cfg->thres_sad;
        p->md_con_base.thres_move = cfg->thres_move;

        p->md_fly_chk.night_mode_en = cfg->night_mode;
        p->md_fly_chk.flycatkin_flt_en = cfg->filter_switch;
        p->md_fly_chk.thres_dust_move = cfg->thres_dust_move;
        p->md_fly_chk.thres_dust_blk = cfg->thres_dust_blk;
        p->md_fly_chk.thres_dust_chng = cfg->thres_dust_chng;
        p->md_sto_strd = 64; /* ?? */

        p->adr_rfmw = info->buf_rfmwr->iova;
        p->adr_rfmr = info->buf_rfmwr->iova;

        if (func->buf_get_paddr)
            p->adr_md_base = func->buf_get_paddr(cfg->mdw_buf);
        break;
    }
    case PP_CMD_SET_OD_CFG: {
        struct pp_od_cfg *cfg = (struct pp_od_cfg *)param;
        p->od_con_cmplx.od_thres_complex = cfg->thres_complex;
        p->od_con_cmplx.od_thres_area_complex = cfg->thres_area_complex;
        p->od_con_sad.od_thres_sad = 7;
        p->od_con_sad.od_thres_area_sad = 0; //cfg->thres_area_complex;
        break;
    }
    case PP_CMD_SET_SMEAR_CFG: {
        struct pp_smear_cfg *cfg = (struct pp_smear_cfg *)param;
        struct vcodec_mpibuf_fn *func = get_vmpibuf_func();

        if (func->buf_get_paddr)
            p->adr_smr_base = func->buf_get_paddr(cfg->smrw_buf);
        p->adr_rfsw = info->buf_rfswr->iova;
        p->adr_rfsr = info->buf_rfswr->iova;
        p->smr_resi_thd0 = 0x080b080b;
        p->smr_resi_thd1 = 0x06101414;
        p->smr_resi_thd2 = 0x09060606;
        p->smr_resi_thd3 = 0x00000706;
        p->smr_madp_thd0 = 0x00180000;
        p->smr_madp_thd1 = 0x00400030;
        p->smr_madp_thd2 = 0x00200010;
        p->smr_madp_thd3 = 0x00600030;
        p->smr_madp_thd4 = 0x00180030;
        p->smr_madp_thd5 = 0x00300060;
        p->smr_cnt_thd0 = 0x41415252;
        p->smr_cnt_thd1 = 0x00303030;
        p->smr_sto_strd = info->smear_stride;
        break;
    }
    case PP_CMD_SET_WEIGHTP_CFG: {
        p->wp_con_comb0 |= (0x03fd << 16);
        p->wp_con_comb1 = 0x00800080;
        p->wp_con_comb2 = 0x01000080;
        p->wp_con_comb3 = 0x01000100;
        p->wp_con_comb4 = 0x00600060;
        p->wp_con_comb5 = 0x00080060;
        p->wp_con_comb6 = 0x00080008;
        break;
    }
    default: {
    }
    }
}

int vepu_pp_control(int chn, enum pp_cmd cmd, void *param)
{
    struct pp_chn_info_t *info = NULL;

    if (chn >= MAX_CHN_NUM) {
        pp_err("vepu pp control channel id %d error\n", chn);
        return VEPU_PP_NOK;
    }

    info = &g_pp_ctx.chn_info[chn];

    if (cmd == PP_CMD_SET_COMMON_CFG || cmd == PP_CMD_SET_MD_CFG ||
        cmd == PP_CMD_SET_OD_CFG || cmd == PP_CMD_SET_SMEAR_CFG ||
        cmd == PP_CMD_SET_WEIGHTP_CFG || cmd == PP_CMD_SET_CHANNEL_INFO)
        vepu_pp_set_param(info, cmd, param);

    if (cmd == PP_CMD_RUN_SYNC) {
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

    if (cmd == PP_CMD_GET_OD_OUTPUT) {
        struct pp_od_out *out = (struct pp_od_out *)param;

        out->od_flg = info->output.od_out_flag;
        out->pix_sum = info->output.od_out_pix_sum;
        pr_debug("od_flg %d pix_sum 0x%08x\n", out->od_flg, out->pix_sum);
    }

    if (cmd == PP_CMD_GET_WEIGHTP_OUTPUT) {
        struct pp_weightp_out *out = (struct pp_weightp_out *)param;

        out->wp_out_par_y = info->output.wp_out_par_y;
        out->wp_out_par_u = info->output.wp_out_par_u;
        out->wp_out_par_v = info->output.wp_out_par_v;
        out->wp_out_pic_mean = info->output.wp_out_pic_mean;
        pr_debug("wp_par 0x%08x 0x%08x 0x%08x pic_mean 0x%08x\n",
                 out->wp_out_par_y, out->wp_out_par_u,
                 out->wp_out_par_v, out->wp_out_pic_mean);
    }

    pr_debug("vepu pp control cmd 0x%08x finished\n", cmd);

    return 0;
}
EXPORT_SYMBOL(vepu_pp_control);

static void vepu_pp_show_chn_attr(KmppEnvNode node)
{
    struct pp_chn_info_t *info = NULL;
    int i;

    seq_puts(seq, "\n-----------------------------------ivs channel attr---------------------------------------------\n");
    seq_printf(seq, "%8s|%8s|%8s|%10s|%10s|%12s|%12s|%8s|%8s|%15s\n",
               "ID", "width", "height", "smear_en", "weightp_en", "max_width", "max_height", "md_en", "od_en",
               "down_scale_en");
    for (i = 0; i < MAX_CHN_NUM; i++) {
        info = &g_pp_ctx.chn_info[i];
        if (info->buf_rfpw == NULL || info->buf_rfpr == NULL)
            continue;

        seq_printf(seq, "%8d|%8d|%8d|%10d|%10d|%12d|%12d|%8d|%8d|%15d\n",
                   info->chn, info->width, info->height, info->smear_en,
                   info->weightp_en, info->max_width, info->max_height,
                   info->md_en, info->od_en, info->down_scale_en);
    }
}

static void vepu_pp_show_com_cfg(KmppEnvNode node)
{
    struct pp_chn_info_t *info = NULL;
    struct pp_param_t *p = NULL;
    int i;

    seq_puts(seq, "\n-----------------------------------ivs common cfg-----------------------------------------------\n");
    seq_printf(seq, "%8s|%8s|%15s|%15s\n",
               "ID", "fmt", "frm_acc_intval", "frm_acc_gop");
    for (i = 0; i < MAX_CHN_NUM; i++) {
        info = &g_pp_ctx.chn_info[i];
        if (info->buf_rfpw == NULL || info->buf_rfpr == NULL)
            continue;
        p = &info->param;
        seq_printf(seq, "%8d|%8d|%15d|%15d\n",
                   info->chn, p->vsp_pic_con.src_cfmt,
                   info->frm_accum_interval, info->frm_accum_gop);
    }
}

static void vepu_pp_show_md_cfg(KmppEnvNode node)
{
    struct pp_chn_info_t *info = NULL;
    struct pp_param_t *p = NULL;
    int i;

    seq_puts(seq, "\n-----------------------------------ivs md cfg---------------------------------------------------\n");
    seq_printf(seq, "%8s|%10s|%10s|%10s|%10s|%10s|%15s|%15s|%15s\n",
               "ID", "switch_sad", "thres_sad", "thres_move", "night_mode",
               "filter_sw", "thres_dust_mv", "thres_dust_blk", "thres_dust_chng");
    for (i = 0; i < MAX_CHN_NUM; i++) {
        info = &g_pp_ctx.chn_info[i];
        if (info->buf_rfpw == NULL || info->buf_rfpr == NULL)
            continue;
        p = &info->param;
        seq_printf(seq, "%8d|%10d|%10d|%10d|%10d|%10d|%15d|%15d|%15d\n",
                   info->chn, p->md_con_base.switch_sad, p->md_con_base.thres_sad,
                   p->md_con_base.thres_move, p->md_fly_chk.night_mode_en,
                   p->md_fly_chk.flycatkin_flt_en, p->md_fly_chk.thres_dust_move,
                   p->md_fly_chk.thres_dust_blk, p->md_fly_chk.thres_dust_chng);
    }
}

static void vepu_pp_show_od_cfg(KmppEnvNode node)
{
    struct pp_chn_info_t *info = NULL;
    struct pp_param_t *p = NULL;
    int i;

    seq_puts(seq, "\n-----------------------------------ivs od cfg---------------------------------------------------\n");
    seq_printf(seq, "%8s|%15s|%18s|%10s\n",
               "ID", "thres_complex", "thres_area_complex", "thres_sad");
    for (i = 0; i < MAX_CHN_NUM; i++) {
        info = &g_pp_ctx.chn_info[i];
        if (info->buf_rfpw == NULL || info->buf_rfpr == NULL)
            continue;
        p = &info->param;
        seq_printf(seq, "%8d|%15d|%18d|%10d\n",
                   info->chn, p->od_con_cmplx.od_thres_complex,
                   p->od_con_cmplx.od_thres_area_complex, p->od_con_sad.od_thres_sad);
    }
}

void vepu_show_pp_info(KmppEnvNode node)
{
    vepu_pp_show_chn_attr(node);
    vepu_pp_show_com_cfg(node);
    vepu_pp_show_md_cfg(node);
    vepu_pp_show_od_cfg(node);
}

int vepu_pp_init(void)
{
    pr_info("vepu_pp init\n");
    return 0;
}

void vepu_pp_exit(void)
{
    pr_info("vepu_pp exit\n");
}

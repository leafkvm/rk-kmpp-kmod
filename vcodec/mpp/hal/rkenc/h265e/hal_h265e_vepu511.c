/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG  "hal_h265e_v511"

#include "kmpp_string.h"

#include "kmpp_string.h"
#include <mpp_maths.h>

#include "mpp_mem.h"
#include "mpp_rc_api.h"
#include "kmpp_frame.h"
#include "kmpp_buffer.h"
#include "kmpp_packet.h"

#include "hal_h265e_debug.h"
#include "h265e_syntax_new.h"
#include "hal_bufs.h"
#include "rkv_enc_def.h"
#include "vepu541_common.h"
#include "vepu511_common.h"
#include "hal_h265e_vepu511.h"
#include "hal_h265e_vepu511_reg.h"
#include "kmpp_meta.h"
#include "kmpp_obj.h"
#include "kmpp_sym.h"

#define hal_h265e_err(fmt, ...) \
    do {\
        kmpp_loge_f(fmt, ## __VA_ARGS__);\
    } while (0)

const static RefType ref_type_map[2][2] = {
    /* ref_lt = 0   ref_lt = 1 */
    /* cur_lt = 0 */{ST_REF_TO_ST, ST_REF_TO_LT},
    /* cur_lt = 1 */{LT_REF_TO_ST, LT_REF_TO_LT},
};

const static RK_U32 lambda_tbl_pre_intra[52] = {
    4206,   4945,   5814,   6835,   8035,   9446,   11105,  13056,
    15348,  18044,  21213,  24938,  29318,  34467,  40521,  47637,
    56003,  65839,  77402,  90996,  106977, 125765, 147852, 173819,
    204346, 240234, 983,    1206,   1479,   1813,   2223,   2727,
    3344,   4100,   5028,   6166,   7561,   9272,   11371,  13944,
    17099,  20969,  25714,  31533,  38669,  47420,  58150,  71310,
    87447, 107236, 131504, 161263,
};

const static RK_U32 lambda_tbl_pre_inter[52] = {
    760,    959,    1210,   1526,   1925,   2428,   3063,   3864,
    4874,   6147,   7754,   9781,   12337,  15562,  19629,  24760,
    31231,  39394,  49691,  62678,  79061,  99725,  125790, 158668,
    200140, 252451, 579,    730,    919,    1159,   1461,   1993,
    2898,   3652,   4601,   5411,   6818,   7362,   9276,   11688,
    14725,  18553,  25324,  31906,  40200,  50649,  68724,  74217,
    101300, 127630, 148435, 187017,
};

/* scale_iq */
static RK_U8 vepu511_h265_cqm_intra8[64] = {
    16, 16, 16, 16, 17, 18, 21, 24,
    16, 16, 16, 16, 17, 19, 22, 25,
    16, 16, 17, 18, 20, 22, 25, 29,
    16, 16, 18, 21, 24, 27, 31, 36,
    17, 17, 20, 24, 30, 35, 41, 47,
    18, 19, 22, 27, 35, 44, 54, 65,
    21, 22, 25, 31, 41, 54, 70, 88,
    24, 25, 29, 36, 47, 65, 88, 115
};

/* scale_iq */
static RK_U8 vepu511_h265_cqm_inter8[64] = {
    16, 16, 16, 16, 17, 18, 20, 24,
    16, 16, 16, 17, 18, 20, 24, 25,
    16, 16, 17, 18, 20, 24, 25, 28,
    16, 17, 18, 20, 24, 25, 28, 33,
    17, 18, 20, 24, 25, 28, 33, 41,
    18, 20, 24, 25, 28, 33, 41, 54,
    20, 24, 25, 28, 33, 41, 54, 71,
    24, 25, 28, 33, 41, 54, 71, 91
};

/* scale_q = (65536 + scale_iq / 2) / scale_iq */
static RK_U16 vepu511_h265_cqm_intra8_q[64] = {
    4096,  4096,  4096,  4096,  3855,  3641,  3121,  2731,
    4096,  4096,  4096,  4096,  3855,  3449,  2979,  2621,
    4096,  4096,  3855,  3641,  3277,  2979,  2621,  2260,
    4096,  4096,  3641,  3121,  2731,  2427,  2114,  1820,
    3855,  3855,  3277,  2731,  2185,  1872,  1598,  1394,
    3641,  3449,  2979,  2427,  1872,  1489,  1214,  1008,
    3121,  2979,  2621,  2114,  1598,  1214,   936,   745,
    2731,  2621,  2260,  1820,  1394,  1008,   745,   570
};

/* scale_q = (65536 + scale_iq / 2) / scale_iq */
static RK_U16 vepu511_h265_cqm_inter8_q[64] = {
    4096,  4096,  4096,  4096,  3855,  3641,  3277,  2731,
    4096,  4096,  4096,  3855,  3641,  3277,  2731,  2621,
    4096,  4096,  3855,  3641,  3277,  2731,  2621,  2341,
    4096,  3855,  3641,  3277,  2731,  2621,  2341,  1986,
    3855,  3641,  3277,  2731,  2621,  2341,  1986,  1598,
    3641,  3277,  2731,  2621,  2341,  1986,  1598,  1214,
    3277,  2731,  2621,  2341,  1986,  1598,  1214,   923,
    2731,  2621,  2341,  1986,  1598,  1214,   923,   720
};

typedef struct vepu511_h265_fbk_t {
    RK_U32 hw_status;       /* 0:corret, 1:error */
    RK_U32 frame_type;
    RK_U32 qp_sum;
    RK_U32 out_strm_size;
    RK_U32 out_hw_strm_size;
    RK_S64 sse_sum;
    RK_U32 st_lvl64_inter_num;
    RK_U32 st_lvl32_inter_num;
    RK_U32 st_lvl16_inter_num;
    RK_U32 st_lvl8_inter_num;
    RK_U32 st_lvl32_intra_num;
    RK_U32 st_lvl16_intra_num;
    RK_U32 st_lvl8_intra_num;
    RK_U32 st_lvl4_intra_num;
    RK_U32 st_cu_num_qp[52];
    RK_U32 st_madp;
    RK_U32 st_madi;
    RK_U32 st_mb_num;
    RK_U32 st_ctu_num;
    RK_U32 st_smear_cnt[5];
    RK_U32 acc_cover16_num;
    RK_U32 acc_bndry16_num;
    RK_U32 acc_zero_mv;
    RK_S8 tgt_sub_real_lvl[6];
} vepu511_h265_fbk;

typedef struct H265eV511HalContext_t {
    MppEncHalApi            api;
    MppDev                  dev;
    void                    *regs;

    vepu511_h265_fbk        feedback;
    vepu511_h265_fbk        last_frame_fb;
    void                    *dump_files;
    RK_U32                  frame_cnt_gen_ready;

    RK_S32                  frame_type;

    /* @frame_cnt starts from ZERO */
    RK_U32                  frame_cnt;
    void                    *roi_data;
    /* osd */
    Vepu511OsdCfg           osd_cfg;
    MppEncCfgSet            *cfg;

    /* two-pass deflicker */
    MppBuffer               buf_pass1;

    RK_U32                  frame_size;
    RK_S32                  max_buf_cnt;
    RK_S32                  hdr_status;
    void                    *input_fmt;
    RK_U8                   *src_buf;
    RK_U8                   *dst_buf;
    RK_S32                  buf_size;
    RK_U32                  frame_num;
    HalBufs                 dpb_bufs;
    RK_S32                  fbc_header_len;
    RK_U32                  title_num;
    RK_S32                  online;
    RK_S32                  smear_size;

    /* recn and ref buffer offset */
    RK_U32                  recn_ref_wrap;
    MppBuffer               recn_ref_buf;
    WrapBufInfo             wrap_infos;
    struct hal_shared_buf   *shared_buf;
    RK_U32                  recn_buf_clear;

    RK_S32                  smart_en;       /* deprecated */
    RK_U32                  is_gray;

    /* external line buffer over 4096 */
    RK_S32                  ext_line_buf_size;
    MppBuffer               ext_line_buf;
    RK_U32                  only_smartp;

    /* object dectection buffer */
    KmppBuffer              obj_buf;
    /* smart v3 only */
    RK_S32                  start_qp; /* frame start qp */
    RK_S32                  bg_delta;
    RK_S32                  fg_delta;
    RK_S32                  bmap_qp_min;
    RK_S32                  bmap_qp_max;

    /* qpmap buffer */
    MppBuffer               mv_info;
    MppBuffer               qpmap;
    RK_U8                   *mv_flag_info;  /* b32 */
    RK_U8                   *mv_flag;
    RK_S32                  b16_flag_size;

    KmppSyms                syms;
    KmppSym                 sym;
} H265eV511HalContext;

#define H265E_LAMBDA_TAB_SIZE  (52 * sizeof(RK_U32))
#define H265E_SMEAR_STR_NUM    (8)

static RK_U32 aq_thd_default[16] = {
    0,  0,  0,  0,  3,  3,  5,  5,
    8,  8,  15, 15, 20, 25, 25, 25
};

static RK_S32 aq_qp_delta_default[16] = {
    -8, -7, -6, -5, -4, -3, -2, -1,
    1,  2,  3,  4,  5,  6,  7,  8
};

static RK_U32 aq_thd_smt_I[16] = {
    1,  2,  3,   3,  3,  3,  5,  5,
    8,  8,  13,  15, 20, 25, 25, 25
};

static RK_S32 aq_qp_delta_smt_I[16] = {
    -8, -7, -6, -5, -4, -3, -2, -1,
    1,  2,  3,  5,  7,  8,  9,  9
};

static RK_U32 aq_thd_smt_P[16] = {
    0,  0,  0,   0,  3,  3,  5,  5,
    8,  8,  15, 15, 20, 25, 25, 25
};

static RK_S32 aq_qp_delta_smt_P[16] = {
    -8, -7, -6, -5, -4, -3, -2, -1,
    1,  2,  3,  4,  6,  7,  9,  9
};

static RK_S32 aq_rnge_default[10] = {
    5, 5, 10, 12, 12, 5, 5, 10, 12, 12
};

static RK_S32 aq_rnge_smt[10] = {
    8, 8, 12, 12, 12, 8, 8, 12, 12, 12
};

static RK_U32 rdo_lambda_table_I[60] = {
    0x00000012, 0x00000017,
    0x0000001d, 0x00000024, 0x0000002e, 0x0000003a,
    0x00000049, 0x0000005c, 0x00000074, 0x00000092,
    0x000000b8, 0x000000e8, 0x00000124, 0x00000170,
    0x000001cf, 0x00000248, 0x000002df, 0x0000039f,
    0x0000048f, 0x000005bf, 0x0000073d, 0x0000091f,
    0x00000b7e, 0x00000e7a, 0x0000123d, 0x000016fb,
    0x00001cf4, 0x0000247b, 0x00002df6, 0x000039e9,
    0x000048f6, 0x00005bed, 0x000073d1, 0x000091ec,
    0x0000b7d9, 0x0000e7a2, 0x000123d7, 0x00016fb2,
    0x0001cf44, 0x000247ae, 0x0002df64, 0x00039e89,
    0x00048f5c, 0x0005bec8, 0x00073d12, 0x00091eb8,
    0x000b7d90, 0x000e7a23, 0x00123d71, 0x0016fb20,
    0x001cf446, 0x00247ae1, 0x002df640, 0x0039e88c,
    0x0048f5c3, 0x005bec81, 0x0073d119, 0x0091eb85,
    0x00b7d902, 0x00e7a232
};

static RK_U32 rdo_lambda_table_P[60] = {
    0x0000002c, 0x00000038, 0x00000044, 0x00000058,
    0x00000070, 0x00000089, 0x000000b0, 0x000000e0,
    0x00000112, 0x00000160, 0x000001c0, 0x00000224,
    0x000002c0, 0x00000380, 0x00000448, 0x00000580,
    0x00000700, 0x00000890, 0x00000b00, 0x00000e00,
    0x00001120, 0x00001600, 0x00001c00, 0x00002240,
    0x00002c00, 0x00003800, 0x00004480, 0x00005800,
    0x00007000, 0x00008900, 0x0000b000, 0x0000e000,
    0x00011200, 0x00016000, 0x0001c000, 0x00022400,
    0x0002c000, 0x00038000, 0x00044800, 0x00058000,
    0x00070000, 0x00089000, 0x000b0000, 0x000e0000,
    0x00112000, 0x00160000, 0x001c0000, 0x00224000,
    0x002c0000, 0x00380000, 0x00448000, 0x00580000,
    0x00700000, 0x00890000, 0x00b00000, 0x00e00000,
    0x01120000, 0x01600000, 0x01c00000, 0x02240000,
};

static void get_wrap_buf(H265eV511HalContext *ctx, RK_S32 max_lt_cnt)
{
    MppEncPrepCfg *prep = &ctx->cfg->prep;
    RK_S32 alignment = 64;
    RK_S32 aligned_w = MPP_ALIGN(prep->max_width, alignment);
    RK_S32 aligned_h = MPP_ALIGN(prep->max_height, alignment);
    RK_U32 total_wrap_size;
    WrapInfo *body = &ctx->wrap_infos.body;
    WrapInfo *hdr = &ctx->wrap_infos.hdr;

    body->size = REF_BODY_SIZE(aligned_w, aligned_h);
    body->total_size = body->size + REF_WRAP_BODY_EXT_SIZE(aligned_w, aligned_h);
    hdr->size = REF_HEADER_SIZE(aligned_w, aligned_h);
    hdr->total_size = hdr->size + REF_WRAP_HEADER_EXT_SIZE(aligned_w, aligned_h);
    total_wrap_size = body->total_size + hdr->total_size;

    if (max_lt_cnt > 0) {
        WrapInfo *body_lt = &ctx->wrap_infos.body_lt;
        WrapInfo *hdr_lt = &ctx->wrap_infos.hdr_lt;

        body_lt->size = body->size;
        body_lt->total_size = body->total_size;

        hdr_lt->size = hdr->size;
        hdr_lt->total_size = hdr->total_size;
        if (ctx->only_smartp) {
            body_lt->total_size = body->size;
            hdr_lt->total_size =  hdr->size;
        }

        total_wrap_size += (body_lt->total_size + hdr_lt->total_size);
    }

    /*
     * bottom                      top
     * ┌──────────────────────────►
     * │
     * ├──────┬───────┬──────┬─────┐
     * │hdr_lt│  hdr  │bdy_lt│ bdy │
     * └──────┴───────┴──────┴─────┘
     */

    if (max_lt_cnt > 0) {
        WrapInfo *body_lt = &ctx->wrap_infos.body_lt;
        WrapInfo *hdr_lt = &ctx->wrap_infos.hdr_lt;

        hdr_lt->bottom = 0;
        hdr_lt->top = hdr_lt->bottom + hdr_lt->total_size;
        hdr_lt->cur_off = hdr_lt->bottom;

        hdr->bottom = hdr_lt->top;
        hdr->top = hdr->bottom + hdr->total_size;
        hdr->cur_off = hdr->bottom;

        body_lt->bottom = hdr->top;
        body_lt->top = body_lt->bottom + body_lt->total_size;
        body_lt->cur_off = body_lt->bottom;

        body->bottom = body_lt->top;
        body->top = body->bottom + body->total_size;
        body->cur_off = body->bottom;
    } else {
        hdr->bottom = 0;
        hdr->top = hdr->bottom + hdr->total_size;
        hdr->cur_off = hdr->bottom;

        body->bottom = hdr->top;
        body->top = body->bottom + body->total_size;
        body->cur_off = body->bottom;
    }

    if (!ctx->shared_buf->recn_ref_buf) {
        if (ctx->recn_ref_buf)
            mpp_buffer_put(ctx->recn_ref_buf);
        mpp_buffer_get(NULL, &ctx->recn_ref_buf, total_wrap_size);
    } else
        ctx->recn_ref_buf = ctx->shared_buf->recn_ref_buf;
}

static MPP_RET vepu511_h265_setup_hal_bufs(H265eV511HalContext *ctx)
{
    MPP_RET ret = MPP_OK;
    VepuFmtCfg *fmt = (VepuFmtCfg *)ctx->input_fmt;
    RK_U32 frame_size;
    VepuFmt input_fmt = VEPU541_FMT_YUV420P;
    RK_S32 ctu_w, ctu_h, ctu_size = 32;
    MppEncRefCfg ref_cfg = ctx->cfg->ref_cfg;
    MppEncPrepCfg *prep = &ctx->cfg->prep;
    RK_S32 old_max_cnt = ctx->max_buf_cnt;
    RK_S32 new_max_cnt = 2;
    RK_S32 aligned_w = MPP_ALIGN(prep->width,  ctu_size);
    RK_S32 max_lt_cnt;
    RK_S32 smear_size = 0;
    RK_S32 smear_r_size = 0;

    hal_h265e_enter();

    ctu_w = MPP_ALIGN(prep->width, ctu_size) / ctu_size;
    ctu_h = MPP_ALIGN(prep->height, ctu_size) / ctu_size;

    frame_size = MPP_ALIGN(prep->width, 16) * MPP_ALIGN(prep->height, 16);
    vepu5xx_set_fmt(fmt, ctx->cfg->prep.format);
    input_fmt = (VepuFmt)fmt->format;
    switch (input_fmt) {
    case VEPU540_FMT_YUV400:
        break;
    case VEPU541_FMT_YUV420P:
    case VEPU541_FMT_YUV420SP: {
        frame_size = frame_size * 3 / 2;
    } break;
    case VEPU541_FMT_YUV422P:
    case VEPU541_FMT_YUV422SP:
    case VEPU541_FMT_YUYV422:
    case VEPU541_FMT_UYVY422:
    case VEPU541_FMT_BGR565: {
        frame_size *= 2;
    } break;
    case VEPU540C_FMT_YUV444SP:
    case VEPU540C_FMT_YUV444P:
    case VEPU541_FMT_BGR888: {
        frame_size *= 3;
    } break;
    case VEPU541_FMT_BGRA8888: {
        frame_size *= 4;
    } break;
    default: {
        hal_h265e_err("invalid src color space: %d\n", input_fmt);
        return MPP_NOK;
    }
    }

    if (ref_cfg) {
        MppEncCpbInfo *info = mpp_enc_ref_cfg_get_cpb_info(ref_cfg);

        new_max_cnt = MPP_MAX(new_max_cnt, info->dpb_size + 1);
        max_lt_cnt = info->max_lt_cnt;
    }

    if (aligned_w > SZ_4K) {
        RK_S32 ext_line_buf_size = (ctu_w - 118) * 27 * 16;

        if (!ctx->shared_buf->ext_line_buf ) {
            if (ctx->ext_line_buf && (ext_line_buf_size > ctx->ext_line_buf_size)) {
                mpp_buffer_put(ctx->ext_line_buf);
                ctx->ext_line_buf = NULL;
            }

            if (NULL == ctx->ext_line_buf)
                mpp_buffer_get(NULL, &ctx->ext_line_buf, ext_line_buf_size);
        } else
            ctx->ext_line_buf = ctx->shared_buf->ext_line_buf;

        ctx->ext_line_buf_size = ext_line_buf_size;
    } else {
        if (ctx->ext_line_buf && !ctx->shared_buf->ext_line_buf) {
            mpp_buffer_put(ctx->ext_line_buf);
            ctx->ext_line_buf = NULL;
        }
        ctx->ext_line_buf_size = 0;
    }

    smear_size = MPP_ALIGN(prep->max_width, 256) / 256 * MPP_ALIGN(prep->max_height, 32) / 32 * 16;
    smear_r_size = MPP_ALIGN(prep->max_height, 256) / 256 * MPP_ALIGN(prep->max_width, 32) / 32 * 16;
    smear_size = MPP_MAX(smear_size, smear_r_size);

    if (frame_size != ctx->frame_size || new_max_cnt != old_max_cnt || smear_size != ctx->smear_size) {
        size_t size[HAL_BUF_TYPE_BUTT] = { 0 };

        if (!ctx->shared_buf->dpb_bufs) {
            if (ctx->dpb_bufs)
                hal_bufs_deinit(ctx->dpb_bufs);
            hal_bufs_init(&ctx->dpb_bufs);
        }
        new_max_cnt = MPP_MAX(new_max_cnt, old_max_cnt);

        hal_h265e_dbg_detail("frame size %d -> %d max count %d -> %d\n",
                             ctx->frame_size, frame_size, old_max_cnt, new_max_cnt);
        if (ctx->cfg->h265.tmvp_enable)
            size[CMV_TYPE] = ctu_w * ctu_h * 16;

        if (ctx->recn_ref_wrap) {
            get_wrap_buf(ctx, max_lt_cnt);
            size[RECREF_TYPE] = 0;
        } else {
            ctx->fbc_header_len = MPP_ALIGN(((ctu_w * (ctu_h + 1)) << 4), SZ_4K);
            size[RECREF_TYPE] = ctx->fbc_header_len + ((ctu_w * (ctu_h + 1)) << 10) * 3 / 2;
        }

        size[THUMB_TYPE] = (ctu_w * ctu_h) << 6;
        size[SMEAR_TYPE] = smear_size;
        if (ctx->shared_buf->dpb_bufs)
            ctx->dpb_bufs = ctx->shared_buf->dpb_bufs;
        else
            hal_bufs_setup(ctx->dpb_bufs, new_max_cnt, MPP_ARRAY_ELEMS(size), size);

        ctx->smear_size = smear_size;
        ctx->frame_size = frame_size;
        ctx->max_buf_cnt = new_max_cnt;
        ctx->recn_buf_clear = 1;
    }

    hal_h265e_leave();
    return ret;
}

static void vepu511_h265_global_cfg_set(H265eV511HalContext *ctx, H265eV511RegSet *regs)
{
    HevcVepu511Frame *reg_frm = &regs->reg_frm;
    HevcVepu511Param *reg_param = &regs->reg_param;
    RK_S32 lambda_idx = ctx->cfg->tune.lambda_idx_i;

    reg_frm->reg0248_sao_cfg.sao_lambda_multi = ctx->cfg->h265.sao_cfg.sao_bit_ratio;

    if (ctx->frame_type == INTRA_FRAME)
        kmpp_memcpy(&reg_param->pprd_lamb_satd_0_51[0],
                    lambda_tbl_pre_intra, sizeof(lambda_tbl_pre_intra));
    else
        kmpp_memcpy(&reg_param->pprd_lamb_satd_0_51[0],
                    lambda_tbl_pre_inter, sizeof(lambda_tbl_pre_inter));

    {
        RK_U32 *lambda_tbl;

        if (ctx->frame_type == INTRA_FRAME) {
            lambda_tbl = &rdo_lambda_table_I[lambda_idx];
        } else {
            lambda_idx = ctx->cfg->tune.lambda_idx_p;
            lambda_tbl = &rdo_lambda_table_P[lambda_idx];
        }

        kmpp_memcpy(&reg_param->rdo_wgta_qp_grpa_0_51[0], lambda_tbl, H265E_LAMBDA_TAB_SIZE);
    }

    /* 0x1064 */
    regs->reg_rc_roi.madi_st_thd.madi_th0 = 5;
    regs->reg_rc_roi.madi_st_thd.madi_th1 = 12;
    regs->reg_rc_roi.madi_st_thd.madi_th2 = 20;
    /* 0x1068 */
    regs->reg_rc_roi.madp_st_thd0.madp_th0 = 4 << 4;
    regs->reg_rc_roi.madp_st_thd0.madp_th1 = 9 << 4;
    /* 0x106C */
    regs->reg_rc_roi.madp_st_thd1.madp_th2 = 15 << 4;
    regs->reg_param.prmd_intra_lamb_ofst.lambda_luma_offset = 11;
    regs->reg_param.prmd_intra_lamb_ofst.lambda_chroma_offset = 11;
}

MPP_RET hal_h265e_v511_deinit(void *hal)
{
    H265eV511HalContext *ctx = (H265eV511HalContext *)hal;

    hal_h265e_enter();

    MPP_FREE(ctx->regs);
    MPP_FREE(ctx->input_fmt);

    if (!ctx->shared_buf->ext_line_buf && ctx->ext_line_buf) {
        mpp_buffer_put(ctx->ext_line_buf);
        ctx->ext_line_buf = NULL;
    }

    if (!ctx->shared_buf->dpb_bufs && ctx->dpb_bufs)
        hal_bufs_deinit(ctx->dpb_bufs);

    if (!ctx->shared_buf->recn_ref_buf && ctx->recn_ref_buf) {
        mpp_buffer_put(ctx->recn_ref_buf);
        ctx->recn_ref_buf = NULL;
    }

    if (ctx->buf_pass1) {
        mpp_buffer_put(ctx->buf_pass1);
        ctx->buf_pass1 = NULL;
    }

    if (ctx->mv_info)
        mpp_buffer_put(ctx->mv_info);
    if (ctx->qpmap)
        mpp_buffer_put(ctx->qpmap);
    if (ctx->mv_flag_info)
        mpp_free(ctx->mv_flag_info);
    if (ctx->mv_flag)
        mpp_free(ctx->mv_flag);

    if (ctx->dev) {
        mpp_dev_deinit(ctx->dev);
        ctx->dev = NULL;
    }

    if (ctx->sym) {
        kmpp_sym_put(ctx->sym);
        ctx->sym = NULL;
    }

    if (ctx->syms) {
        kmpp_syms_put(ctx->syms);
        ctx->syms = NULL;
    }

    hal_h265e_leave();

    return MPP_OK;
}

MPP_RET hal_h265e_v511_init(void *hal, MppEncHalCfg *cfg)
{
    MPP_RET ret = MPP_OK;
    H265eV511HalContext *ctx = (H265eV511HalContext *)hal;
    H265eV511RegSet *regs = NULL;

    hal_h265e_enter();

    ctx->regs           = mpp_calloc(H265eV511RegSet, 1);
    ctx->input_fmt      = mpp_calloc(VepuFmtCfg, 1);
    ctx->cfg            = cfg->cfg;
    ctx->online         = cfg->online;
    ctx->recn_ref_wrap  = cfg->ref_buf_shared;
    ctx->shared_buf     = cfg->shared_buf;
    ctx->only_smartp    = cfg->only_smartp;
    ctx->b16_flag_size  = 0;

    ctx->frame_cnt           = 0;
    ctx->frame_cnt_gen_ready = 0;
    ctx->frame_type          = INTRA_FRAME;

    regs = (H265eV511RegSet *)ctx->regs;
    ctx->osd_cfg.reg_base = &regs->reg_osd.osd;
    ctx->osd_cfg.dev = cfg->dev;
    ctx->dev = cfg->dev;

    hal_h265e_leave();
    return ret;
}

static MPP_RET hal_h265e_vepu511_prepare(void *hal)
{
    H265eV511HalContext *ctx = (H265eV511HalContext *)hal;
    MppEncPrepCfg *prep = &ctx->cfg->prep;
    MppEncBaseCfg *base = &ctx->cfg->base;
    MppEncHwCfg *hw = &ctx->cfg->hw;

    hal_h265e_dbg_func("enter %p\n", hal);

    if (prep->change_res)
        prep->change_res = 0;

    if (base->change_smt_mode) {
        if (base->smt1_en || base->smt3_en) {
            kmpp_memcpy(hw->aq_step_i, aq_qp_delta_smt_I, sizeof(hw->aq_step_i));
            kmpp_memcpy(hw->aq_step_p, aq_qp_delta_smt_P, sizeof(hw->aq_step_p));
            kmpp_memcpy(hw->aq_thrd_i, aq_thd_smt_I, sizeof(hw->aq_thrd_i));
            kmpp_memcpy(hw->aq_thrd_p, aq_thd_smt_P, sizeof(hw->aq_thrd_p));
            kmpp_memcpy(hw->aq_rnge_arr, aq_rnge_smt, sizeof(hw->aq_rnge_arr));
        } else {
            kmpp_memcpy(hw->aq_step_i, aq_qp_delta_default, sizeof(hw->aq_step_i));
            kmpp_memcpy(hw->aq_step_p, aq_qp_delta_default, sizeof(hw->aq_step_p));
            kmpp_memcpy(hw->aq_thrd_i, aq_thd_default, sizeof(hw->aq_thrd_i));
            kmpp_memcpy(hw->aq_thrd_p, aq_thd_default, sizeof(hw->aq_thrd_p));
            kmpp_memcpy(hw->aq_rnge_arr, aq_rnge_default, sizeof(hw->aq_rnge_arr));
        }
        base->change_smt_mode = 0;
    }

    hal_h265e_dbg_func("leave %p\n", hal);

    return MPP_OK;
}

static MPP_RET vepu511_h265_uv_address(HevcVepu511Frame *regs, H265eSyntax_new *syn,
                                       VepuFmt input_fmt, HalEncTask *task)
{
    RK_U32 hor_stride, ver_stride;
    RK_U32 frame_size = 0;
    RK_U32 u_offset = 0, v_offset = 0;
    MPP_RET ret = MPP_OK;
    RK_U32 fmt;

    kmpp_frame_get_hor_stride(task->frame, &hor_stride);
    kmpp_frame_get_ver_stride(task->frame, &ver_stride);
    kmpp_frame_get_fmt(task->frame, &fmt);

    if (!hor_stride)
        kmpp_frame_get_width(task->frame, &hor_stride);

    if (!ver_stride)
        kmpp_frame_get_height(task->frame, &ver_stride);

    frame_size = hor_stride * ver_stride;
    if (MPP_FRAME_FMT_IS_FBC(fmt)) {
        kmpp_frame_get_fbc_offset(task->frame, &u_offset);
        v_offset = u_offset;
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
        case VEPU540C_FMT_YUV444SP : {
            u_offset = frame_size;
            v_offset = frame_size;
        } break;
        case VEPU540C_FMT_YUV444P : {
            u_offset = frame_size;
            v_offset = frame_size * 2;
        } break;
        case VEPU541_FMT_BGR565:
        case VEPU541_FMT_BGR888:
        case VEPU541_FMT_BGRA8888: {
            u_offset = 0;
            v_offset = 0;
        }
        break;
        default: {
            hal_h265e_err("unknown color space: %d\n",
                          input_fmt);
            u_offset = frame_size;
            v_offset = frame_size * 5 / 4;
        }
        }
    }

    regs->reg0161_adr_src1 += u_offset;
    regs->reg0162_adr_src2 += v_offset;
    return ret;
}

static MPP_RET vepu511_h265_set_rc_regs(H265eV511HalContext *ctx, H265eV511RegSet *regs,
                                        HalEncTask *task)
{
    H265eSyntax_new *syn = (H265eSyntax_new *)task->syntax.data;
    EncRcTaskInfo *rc_cfg = &task->rc_task->info;
    HevcVepu511Frame *reg_frm = &regs->reg_frm;
    HevcVepu511RcRoi *reg_rc = &regs->reg_rc_roi;
    MppEncCfgSet *cfg = ctx->cfg;
    MppEncBaseCfg *base = &cfg->base;
    MppEncRcCfg *rc = &cfg->rc;
    MppEncHwCfg *hw = &cfg->hw;
    MppEncH265Cfg *h265 = &cfg->h265;
    RK_S32 mb_wd32 = (syn->pp.pic_width + 31) / 32;
    RK_S32 mb_h32 = (syn->pp.pic_height + 31) / 32;

    RK_U32 ctu_target_bits_mul_16 = (rc_cfg->bit_target << 4) / (mb_wd32 * mb_h32);
    RK_U32 ctu_target_bits;
    RK_S32 negative_bits_thd, positive_bits_thd;

    if (rc->rc_mode == MPP_ENC_RC_MODE_FIXQP) {
        reg_frm->reg0192_enc_pic.pic_qp   = rc_cfg->quality_target;
        reg_frm->reg0240_synt_sli1.sli_qp = rc_cfg->quality_target;

        reg_frm->reg213_rc_qp.rc_max_qp   = rc_cfg->quality_target;
        reg_frm->reg213_rc_qp.rc_min_qp   = rc_cfg->quality_target;
        reg_frm->reg212_rc_cfg.rc_ctu_num = 1;
    } else {
        if (ctu_target_bits_mul_16 >= 0x100000)
            ctu_target_bits_mul_16 = 0x50000;
        ctu_target_bits = (ctu_target_bits_mul_16 * mb_wd32) >> 4;
        negative_bits_thd = 0 - 5 * ctu_target_bits / 16;
        positive_bits_thd = 5 * ctu_target_bits / 16;

        reg_frm->reg0192_enc_pic.pic_qp   = rc_cfg->quality_target;
        reg_frm->reg0240_synt_sli1.sli_qp = rc_cfg->quality_target;
        reg_frm->reg212_rc_cfg.rc_en      = 1;
        reg_frm->reg212_rc_cfg.aq_en      = 1;
        reg_frm->reg212_rc_cfg.rc_ctu_num = mb_wd32;
        reg_frm->reg213_rc_qp.rc_max_qp   = rc_cfg->quality_max;
        reg_frm->reg213_rc_qp.rc_min_qp   = rc_cfg->quality_min;
        reg_frm->reg214_rc_tgt.ctu_ebit   = ctu_target_bits_mul_16;

        if ((base->smt1_en || base->smt3_en) &&
            (rc->rc_mode == MPP_ENC_RC_MODE_SMTRC || rc->rc_mode == MPP_ENC_RC_MODE_SE)) {
            reg_frm->reg213_rc_qp.rc_qp_range = 0;
        } else {
            reg_frm->reg213_rc_qp.rc_qp_range = (ctx->frame_type == INTRA_FRAME) ?
                                                hw->qp_delta_row_i : hw->qp_delta_row;
        }

        {
            /* fixed frame qp */
            RK_S32 fqp_min, fqp_max;

            if (ctx->frame_type == INTRA_FRAME) {
                fqp_min = rc->fqp_min_i;
                fqp_max = rc->fqp_max_i;
            } else {
                fqp_min = rc->fqp_min_p;
                fqp_max = rc->fqp_max_p;
            }

            if ((fqp_min == fqp_max) && (fqp_min >= 0) && (fqp_max <= 51)) {
                reg_frm->reg0192_enc_pic.pic_qp = fqp_min;
                reg_frm->reg0240_synt_sli1.sli_qp = fqp_min;
                reg_frm->reg213_rc_qp.rc_qp_range = 0;
            }
        }

        reg_rc->rc_dthd_0_8[0] = 2 * negative_bits_thd;
        reg_rc->rc_dthd_0_8[1] = negative_bits_thd;
        reg_rc->rc_dthd_0_8[2] = positive_bits_thd;
        reg_rc->rc_dthd_0_8[3] = 2 * positive_bits_thd;
        reg_rc->rc_dthd_0_8[4] = 0x7FFFFFFF;
        reg_rc->rc_dthd_0_8[5] = 0x7FFFFFFF;
        reg_rc->rc_dthd_0_8[6] = 0x7FFFFFFF;
        reg_rc->rc_dthd_0_8[7] = 0x7FFFFFFF;
        reg_rc->rc_dthd_0_8[8] = 0x7FFFFFFF;

        reg_rc->rc_adj0.qp_adj0    = -2;
        reg_rc->rc_adj0.qp_adj1    = -1;
        reg_rc->rc_adj0.qp_adj2    = 0;
        reg_rc->rc_adj0.qp_adj3    = 1;
        reg_rc->rc_adj0.qp_adj4    = 2;
        reg_rc->rc_adj1.qp_adj5    = 0;
        reg_rc->rc_adj1.qp_adj6    = 0;
        reg_rc->rc_adj1.qp_adj7    = 0;
        reg_rc->rc_adj1.qp_adj8    = 0;

        reg_rc->roi_qthd0.qpmin_area0 = h265->qpmin_map[0] > 0 ? h265->qpmin_map[0] : rc_cfg->quality_min;
        reg_rc->roi_qthd0.qpmax_area0 = h265->qpmax_map[0] > 0 ? h265->qpmax_map[0] : rc_cfg->quality_max;
        reg_rc->roi_qthd0.qpmin_area1 = h265->qpmin_map[1] > 0 ? h265->qpmin_map[1] : rc_cfg->quality_min;
        reg_rc->roi_qthd0.qpmax_area1 = h265->qpmax_map[1] > 0 ? h265->qpmax_map[1] : rc_cfg->quality_max;
        reg_rc->roi_qthd0.qpmin_area2 = h265->qpmin_map[2] > 0 ? h265->qpmin_map[2] : rc_cfg->quality_min;
        reg_rc->roi_qthd1.qpmax_area2 = h265->qpmax_map[2] > 0 ? h265->qpmax_map[2] : rc_cfg->quality_max;
        reg_rc->roi_qthd1.qpmin_area3 = h265->qpmin_map[3] > 0 ? h265->qpmin_map[3] : rc_cfg->quality_min;
        reg_rc->roi_qthd1.qpmax_area3 = h265->qpmax_map[3] > 0 ? h265->qpmax_map[3] : rc_cfg->quality_max;
        reg_rc->roi_qthd1.qpmin_area4 = h265->qpmin_map[4] > 0 ? h265->qpmin_map[4] : rc_cfg->quality_min;
        reg_rc->roi_qthd1.qpmax_area4 = h265->qpmax_map[4] > 0 ? h265->qpmax_map[4] : rc_cfg->quality_max;
        reg_rc->roi_qthd2.qpmin_area5 = h265->qpmin_map[5] > 0 ? h265->qpmin_map[5] : rc_cfg->quality_min;
        reg_rc->roi_qthd2.qpmax_area5 = h265->qpmax_map[5] > 0 ? h265->qpmax_map[5] : rc_cfg->quality_max;
        reg_rc->roi_qthd2.qpmin_area6 = h265->qpmin_map[6] > 0 ? h265->qpmin_map[6] : rc_cfg->quality_min;
        reg_rc->roi_qthd2.qpmax_area6 = h265->qpmax_map[6] > 0 ? h265->qpmax_map[6] : rc_cfg->quality_max;
        reg_rc->roi_qthd2.qpmin_area7 = h265->qpmin_map[7] > 0 ? h265->qpmin_map[7] : rc_cfg->quality_min;
        reg_rc->roi_qthd3.qpmax_area7 = h265->qpmax_map[7] > 0 ? h265->qpmax_map[7] : rc_cfg->quality_max;
    }

    return MPP_OK;
}

static MPP_RET vepu511_h265_set_rdo_regs(H265eV511HalContext *ctx, H265eV511RegSet *regs)
{
    static RK_U32 fmdc_adju_split32[4] = { 0, 0, 0, 1 };
    static RK_U32 fmdc_adju_intra32[4] = { 0, 0, 15, 0 };

    HevcVepu511RcRoi *reg_rc = &regs->reg_rc_roi;
    (void)ctx;

    reg_rc->cudecis_thd0.base_thre_rough_mad32_intra           = 9;
    reg_rc->cudecis_thd0.delta0_thre_rough_mad32_intra         = 10;
    reg_rc->cudecis_thd0.delta1_thre_rough_mad32_intra         = 55;
    reg_rc->cudecis_thd0.delta2_thre_rough_mad32_intra         = 55;
    reg_rc->cudecis_thd0.delta3_thre_rough_mad32_intra         = 66;
    reg_rc->cudecis_thd0.delta4_thre_rough_mad32_intra_low5    = 2;

    reg_rc->cudecis_thd1.delta4_thre_rough_mad32_intra_high2   = 2;
    reg_rc->cudecis_thd1.delta5_thre_rough_mad32_intra         = 74;
    reg_rc->cudecis_thd1.delta6_thre_rough_mad32_intra         = 106;
    reg_rc->cudecis_thd1.base_thre_fine_mad32_intra            = 8;
    reg_rc->cudecis_thd1.delta0_thre_fine_mad32_intra          = 0;
    reg_rc->cudecis_thd1.delta1_thre_fine_mad32_intra          = 13;
    reg_rc->cudecis_thd1.delta2_thre_fine_mad32_intra_low3     = 6;

    reg_rc->cudecis_thd2.delta2_thre_fine_mad32_intra_high2    = 1;
    reg_rc->cudecis_thd2.delta3_thre_fine_mad32_intra          = 17;
    reg_rc->cudecis_thd2.delta4_thre_fine_mad32_intra          = 23;
    reg_rc->cudecis_thd2.delta5_thre_fine_mad32_intra          = 50;
    reg_rc->cudecis_thd2.delta6_thre_fine_mad32_intra          = 54;
    reg_rc->cudecis_thd2.base_thre_str_edge_mad32_intra        = 6;
    reg_rc->cudecis_thd2.delta0_thre_str_edge_mad32_intra      = 0;
    reg_rc->cudecis_thd2.delta1_thre_str_edge_mad32_intra      = 0;

    reg_rc->cudecis_thd3.delta2_thre_str_edge_mad32_intra      = 3;
    reg_rc->cudecis_thd3.delta3_thre_str_edge_mad32_intra      = 8;
    reg_rc->cudecis_thd3.base_thre_str_edge_bgrad32_intra      = 25;
    reg_rc->cudecis_thd3.delta0_thre_str_edge_bgrad32_intra    = 0;
    reg_rc->cudecis_thd3.delta1_thre_str_edge_bgrad32_intra    = 0;
    reg_rc->cudecis_thd3.delta2_thre_str_edge_bgrad32_intra    = 7;
    reg_rc->cudecis_thd3.delta3_thre_str_edge_bgrad32_intra    = 19;
    reg_rc->cudecis_thd3.base_thre_mad16_intra                 = 6;
    reg_rc->cudecis_thd3.delta0_thre_mad16_intra               = 0;

    reg_rc->cudecis_thd4.delta1_thre_mad16_intra          = 3;
    reg_rc->cudecis_thd4.delta2_thre_mad16_intra          = 3;
    reg_rc->cudecis_thd4.delta3_thre_mad16_intra          = 24;
    reg_rc->cudecis_thd4.delta4_thre_mad16_intra          = 28;
    reg_rc->cudecis_thd4.delta5_thre_mad16_intra          = 40;
    reg_rc->cudecis_thd4.delta6_thre_mad16_intra          = 52;
    reg_rc->cudecis_thd4.delta0_thre_mad16_ratio_intra    = 7;

    reg_rc->cudecis_thd5.delta1_thre_mad16_ratio_intra           =  7;
    reg_rc->cudecis_thd5.delta2_thre_mad16_ratio_intra           =  2;
    reg_rc->cudecis_thd5.delta3_thre_mad16_ratio_intra           =  2;
    reg_rc->cudecis_thd5.delta4_thre_mad16_ratio_intra           =  0;
    reg_rc->cudecis_thd5.delta5_thre_mad16_ratio_intra           =  0;
    reg_rc->cudecis_thd5.delta6_thre_mad16_ratio_intra           =  0;
    reg_rc->cudecis_thd5.delta7_thre_mad16_ratio_intra           =  4;
    reg_rc->cudecis_thd5.delta0_thre_rough_bgrad32_intra         =  1;
    reg_rc->cudecis_thd5.delta1_thre_rough_bgrad32_intra         =  5;
    reg_rc->cudecis_thd5.delta2_thre_rough_bgrad32_intra_low4    =  8;

    reg_rc->cudecis_thd6.delta2_thre_rough_bgrad32_intra_high2    = 2;
    reg_rc->cudecis_thd6.delta3_thre_rough_bgrad32_intra          = 540;
    reg_rc->cudecis_thd6.delta4_thre_rough_bgrad32_intra          = 692;
    reg_rc->cudecis_thd6.delta5_thre_rough_bgrad32_intra_low10    = 866;

    reg_rc->cudecis_thd7.delta5_thre_rough_bgrad32_intra_high1   = 1;
    reg_rc->cudecis_thd7.delta6_thre_rough_bgrad32_intra         = 3286;
    reg_rc->cudecis_thd7.delta7_thre_rough_bgrad32_intra         = 6620;
    reg_rc->cudecis_thd7.delta0_thre_bgrad16_ratio_intra         = 8;
    reg_rc->cudecis_thd7.delta1_thre_bgrad16_ratio_intra_low2    = 3;

    reg_rc->cudecis_thd8.delta1_thre_bgrad16_ratio_intra_high2    = 2;
    reg_rc->cudecis_thd8.delta2_thre_bgrad16_ratio_intra          = 15;
    reg_rc->cudecis_thd8.delta3_thre_bgrad16_ratio_intra          = 15;
    reg_rc->cudecis_thd8.delta4_thre_bgrad16_ratio_intra          = 13;
    reg_rc->cudecis_thd8.delta5_thre_bgrad16_ratio_intra          = 13;
    reg_rc->cudecis_thd8.delta6_thre_bgrad16_ratio_intra          = 7;
    reg_rc->cudecis_thd8.delta7_thre_bgrad16_ratio_intra          = 15;
    reg_rc->cudecis_thd8.delta0_thre_fme_ratio_inter              = 4;
    reg_rc->cudecis_thd8.delta1_thre_fme_ratio_inter              = 4;

    reg_rc->cudecis_thd9.delta2_thre_fme_ratio_inter    = 3;
    reg_rc->cudecis_thd9.delta3_thre_fme_ratio_inter    = 2;
    reg_rc->cudecis_thd9.delta4_thre_fme_ratio_inter    = 0;
    reg_rc->cudecis_thd9.delta5_thre_fme_ratio_inter    = 0;
    reg_rc->cudecis_thd9.delta6_thre_fme_ratio_inter    = 0;
    reg_rc->cudecis_thd9.delta7_thre_fme_ratio_inter    = 0;
    reg_rc->cudecis_thd9.base_thre_fme32_inter          = 4;
    reg_rc->cudecis_thd9.delta0_thre_fme32_inter        = 2;
    reg_rc->cudecis_thd9.delta1_thre_fme32_inter        = 7;
    reg_rc->cudecis_thd9.delta2_thre_fme32_inter        = 12;

    reg_rc->cudecis_thd10.delta3_thre_fme32_inter    = 23;
    reg_rc->cudecis_thd10.delta4_thre_fme32_inter    = 41;
    reg_rc->cudecis_thd10.delta5_thre_fme32_inter    = 71;
    reg_rc->cudecis_thd10.delta6_thre_fme32_inter    = 123;
    reg_rc->cudecis_thd10.thre_cme32_inter           = 48;

    reg_rc->cudecis_thd11.delta0_thre_mad_fme_ratio_inter    = 0;
    reg_rc->cudecis_thd11.delta1_thre_mad_fme_ratio_inter    = 7;
    reg_rc->cudecis_thd11.delta2_thre_mad_fme_ratio_inter    = 7;
    reg_rc->cudecis_thd11.delta3_thre_mad_fme_ratio_inter    = 6;
    reg_rc->cudecis_thd11.delta4_thre_mad_fme_ratio_inter    = 5;
    reg_rc->cudecis_thd11.delta5_thre_mad_fme_ratio_inter    = 4;
    reg_rc->cudecis_thd11.delta6_thre_mad_fme_ratio_inter    = 4;
    reg_rc->cudecis_thd11.delta7_thre_mad_fme_ratio_inter    = 4;

    reg_rc->cudecis_thd12.delta0_thre_cme32_static_inter = 1;
    reg_rc->cudecis_thd12.delta1_thre_cme32_static_inter = 3;
    reg_rc->cudecis_thd12.delta2_thre_cme32_static_inter = 6;
    reg_rc->cudecis_thd12.delta3_thre_cme32_static_inter = 9;
    reg_rc->cudecis_thd12.delta4_thre_cme32_static_inter = 10;
    reg_rc->cudecis_thd12.delta5_thre_cme32_static_inter = 11;
    reg_rc->cudecis_thd12.delta6_thre_cme32_static_inter = 12;
    reg_rc->cudecis_thd12.delta7_thre_cme32_static_inter = 15;

    reg_rc->fmdc_adj1.fmdc_adju_split32 = 0;
    reg_rc->fmdc_adj1.fmdc_adju_intra32 = 0;

    if (ctx->frame_type == INTER_P_FRAME) {
        RK_U32 speed_idx = ctx->cfg->tune.speed;

        // 0 - 0.63ppc; 1 - 0.68ppc; 2 - 0.69ppc; 3 - 0.7ppc
        reg_rc->fmdc_adj1.fmdc_adju_split32 = fmdc_adju_split32[speed_idx];
        reg_rc->fmdc_adj1.fmdc_adju_intra32 = fmdc_adju_intra32[speed_idx];

        if (speed_idx >= 1) {
            reg_rc->cudecis_thd9.base_thre_fme32_inter    = 0;
            reg_rc->cudecis_thd9.delta0_thre_fme32_inter  = 0;
            reg_rc->cudecis_thd9.delta1_thre_fme32_inter  = 0;
            reg_rc->cudecis_thd9.delta2_thre_fme32_inter  = 0;
            reg_rc->cudecis_thd10.delta3_thre_fme32_inter = 0;
            reg_rc->cudecis_thd10.delta4_thre_fme32_inter = 0;
            reg_rc->cudecis_thd10.delta5_thre_fme32_inter = 0;
            reg_rc->cudecis_thd10.delta6_thre_fme32_inter = 0;
        }
    }

    return MPP_OK;
}

static void vepu511_h265_set_quant_regs(H265eV511HalContext *ctx)
{
    MppEncHwCfg *hw = &ctx->cfg->hw;
    MppEncBaseCfg *base = &ctx->cfg->base;
    H265eV511RegSet *regs = ctx->regs;
    HevcVepu511Param *s = &regs->reg_param;
    RK_U8 th0 = 3, th1 = 6, th2 = 13;
    RK_U16 bias_i0 = 171, bias_i1 = 171, bias_i2 = 171, bias_i3 = 171;
    RK_U16 bias_p0 = 85, bias_p1 = 85, bias_p2 = 85, bias_p3 = 85;
    RK_U32 frm_type = ctx->frame_type;

    if (!hw->qbias_en) {
        if (base->smt1_en || base->smt3_en) {
            bias_i0 = bias_i1 = bias_i3 = 144;
            bias_i2 = (frm_type == INTRA_FRAME) ? 144 : 171;
        } else {
            bias_i0 = bias_i1 = bias_i3 = 171;
            bias_i2 = (frm_type == INTRA_FRAME) ? 171 : 220;
        }

        /* used for venc_info log */
        hw->qbias_arr[IFRAME_THD0] = hw->qbias_arr[PFRAME_THD0] = th0;
        hw->qbias_arr[IFRAME_THD1] = hw->qbias_arr[PFRAME_THD1] = th1;
        hw->qbias_arr[IFRAME_THD2] = hw->qbias_arr[PFRAME_THD2] = th2;
        hw->qbias_arr[IFRAME_BIAS0] = hw->qbias_arr[PFRAME_IBLK_BIAS0] = bias_i0;
        hw->qbias_arr[IFRAME_BIAS1] = hw->qbias_arr[PFRAME_IBLK_BIAS1] = bias_i1;
        hw->qbias_arr[IFRAME_BIAS2] = hw->qbias_arr[PFRAME_IBLK_BIAS2] = bias_i2;
        hw->qbias_arr[IFRAME_BIAS3] = hw->qbias_arr[PFRAME_IBLK_BIAS3] = bias_i3;
        hw->qbias_arr[PFRAME_PBLK_BIAS0] = bias_p0;
        hw->qbias_arr[PFRAME_PBLK_BIAS1] = bias_p1;
        hw->qbias_arr[PFRAME_PBLK_BIAS2] = bias_p2;
        hw->qbias_arr[PFRAME_PBLK_BIAS3] = bias_p3;
    } else {
        if (frm_type == INTRA_FRAME) {
            th0 = hw->qbias_arr[IFRAME_THD0];
            th1 = hw->qbias_arr[IFRAME_THD1];
            th2 = hw->qbias_arr[IFRAME_THD2];
            bias_i0 = hw->qbias_arr[IFRAME_BIAS0];
            bias_i1 = hw->qbias_arr[IFRAME_BIAS1];
            bias_i2 = hw->qbias_arr[IFRAME_BIAS2];
            bias_i3 = hw->qbias_arr[IFRAME_BIAS3];
        } else {
            th0 = hw->qbias_arr[PFRAME_THD0];
            th1 = hw->qbias_arr[PFRAME_THD1];
            th2 = hw->qbias_arr[PFRAME_THD2];
            bias_i0 = hw->qbias_arr[PFRAME_IBLK_BIAS0];
            bias_i1 = hw->qbias_arr[PFRAME_IBLK_BIAS1];
            bias_i2 = hw->qbias_arr[PFRAME_IBLK_BIAS2];
            bias_i3 = hw->qbias_arr[PFRAME_IBLK_BIAS3];
            bias_p0 = hw->qbias_arr[PFRAME_PBLK_BIAS0];
            bias_p1 = hw->qbias_arr[PFRAME_PBLK_BIAS1];
            bias_p2 = hw->qbias_arr[PFRAME_PBLK_BIAS2];
            bias_p3 = hw->qbias_arr[PFRAME_PBLK_BIAS3];
        }
    }

    s->bias_madi_thd_comb.bias_madi_th0 = th0;
    s->bias_madi_thd_comb.bias_madi_th1 = th1;
    s->bias_madi_thd_comb.bias_madi_th2 = th2;
    s->qnt0_i_bias_comb.bias_i_val0 = bias_i0;
    s->qnt0_i_bias_comb.bias_i_val1 = bias_i1;
    s->qnt0_i_bias_comb.bias_i_val2 = bias_i2;
    s->qnt1_i_bias_comb.bias_i_val3 = bias_i3;
    s->qnt0_p_bias_comb.bias_p_val0 = bias_p0;
    s->qnt0_p_bias_comb.bias_p_val1 = bias_p1;
    s->qnt0_p_bias_comb.bias_p_val2 = bias_p2;
    s->qnt1_p_bias_comb.bias_p_val3 = bias_p3;
}

static void vepu511_h265_set_sao_regs(H265eV511HalContext *ctx)
{
    static const RK_U32 blur_low_madi_thd[2][4]  = {{5, 5, 5, 5},
        {4, 4, 4, 4}
    };
    static const RK_U32 blur_high_madi_thd[2][4] = {{27, 27, 27, 27},
        {36, 32, 32, 32}
    };
    static const RK_U32 blur_low_cnt_thd[2][4]   = {{15, 2, 3, 2},
        {4, 4, 3, 3}
    };
    static const RK_U32 blur_high_cnt_thd[2][4]  = {{15, 2, 3, 2},
        {4, 4, 3, 3}
    };
    static const RK_U32 blur_sum_cnt_thd[2][4]   = {{15, 10, 10, 8},
        {14, 12, 12, 8}
    };
    static const RK_U32 blur_motion_thd[2][4]    = {{32, 32, 32, 32},
        {32, 32, 32, 32}
    };

    H265eV511RegSet *regs = ctx->regs;
    HevcVepu511Sqi *sqi = &regs->reg_sqi;
    RK_U32 mode_idx = (ctx->cfg->tune.scene_mode == MPP_ENC_SCENE_MODE_IPC) ? 0 : 1;
    RK_U32 str = ctx->frame_type == INTRA_FRAME ?
                 ctx->cfg->tune.sao_str_i : ctx->cfg->tune.sao_str_p;
    RK_U32 str_idx = 0;

    /* Weight values are set to 4 to disable SAO subjective optimization.
     * They are not under the control of anti_blur_en.
     */
    sqi->subj_anti_blur_wgt3.merge_cost_dist_eo_wgt0 = 4;
    sqi->subj_anti_blur_wgt3.merge_cost_dist_bo_wgt0 = 4;
    sqi->subj_anti_blur_wgt4.merge_cost_dist_eo_wgt1 = 4;
    sqi->subj_anti_blur_wgt4.merge_cost_dist_bo_wgt1 = 4;
    sqi->subj_anti_blur_wgt4.merge_cost_bit_eo_wgt0 = 4;
    sqi->subj_anti_blur_wgt4.merge_cost_bit_bo_wgt0 = 4;

    if (str == 0)
        return;

    str_idx = str - 1;
    sqi->subj_anti_blur_wgt3.merge_cost_dist_eo_wgt0 = 2;
    sqi->subj_anti_blur_wgt3.merge_cost_dist_bo_wgt0 = 3;
    sqi->subj_anti_blur_wgt4.merge_cost_dist_eo_wgt1 = 8;
    sqi->subj_anti_blur_wgt4.merge_cost_dist_bo_wgt1 = 6;
    sqi->subj_anti_blur_wgt4.merge_cost_bit_eo_wgt0 = 8;
    sqi->subj_anti_blur_wgt4.merge_cost_bit_bo_wgt0 = 6;

    sqi->subj_anti_blur_wgt0.notmerge_ofst_dist_eo_wgt0 = 12;
    sqi->subj_anti_blur_wgt0.notmerge_ofst_dist_bo_wgt0 = 12;
    sqi->subj_anti_blur_wgt0.notmerge_ofst_dist_eo_wgt1 = 24;
    sqi->subj_anti_blur_wgt0.notmerge_ofst_dist_bo_wgt1 = 20;

    sqi->subj_anti_blur_wgt1.notmerge_ofst_lambda_eo_wgt0 = 20;
    sqi->subj_anti_blur_wgt1.notmerge_ofst_lambda_bo_wgt0 = 20;
    sqi->subj_anti_blur_wgt1.notmerge_compare_dist_eo_wgt0 = 12;
    sqi->subj_anti_blur_wgt1.notmerge_compare_dist_bo_wgt0 = 12;

    sqi->subj_anti_blur_wgt2.notmerge_compare_dist_eo_wgt1 = 24;
    sqi->subj_anti_blur_wgt2.notmerge_compare_dist_bo_wgt1 = 20;
    sqi->subj_anti_blur_wgt2.notmerge_compare_rate_eo_wgt0 = 20;
    sqi->subj_anti_blur_wgt2.notmerge_compare_rate_bo_wgt0 = 20;

    sqi->subj_anti_blur_wgt3.sao_mode_compare_dist_eo_wgt0 = 24;
    sqi->subj_anti_blur_wgt3.sao_mode_compare_dist_bo_wgt0 = 20;

    sqi->subj_anti_blur_thd.anti_blur_en = 1;
    sqi->subj_anti_blur_thd.scene_mode = mode_idx;
    sqi->subj_anti_blur_sao.sao_ofst_thd_eo_luma = 4;
    sqi->subj_anti_blur_sao.sao_ofst_thd_bo_luma = 4;
    sqi->subj_anti_blur_sao.sao_ofst_thd_eo_chroma = 4;
    sqi->subj_anti_blur_sao.sao_ofst_thd_bo_chroma = 4;

    sqi->subj_anti_blur_thd.blur_low_madi_thd  = blur_low_madi_thd[mode_idx][str_idx];
    sqi->subj_anti_blur_thd.blur_high_madi_thd = blur_high_madi_thd[mode_idx][str_idx];
    sqi->subj_anti_blur_thd.blur_low_cnt_thd   = blur_low_cnt_thd[mode_idx][str_idx];
    sqi->subj_anti_blur_thd.blur_hight_cnt_thd = blur_high_cnt_thd[mode_idx][str_idx];
    sqi->subj_anti_blur_thd.blur_sum_cnt_thd   = blur_sum_cnt_thd[mode_idx][str_idx];
    sqi->subj_anti_blur_sao.blur_motion_thd    = blur_motion_thd[mode_idx][str_idx];
}

static MPP_RET vepu511_h265_set_pp_regs(H265eV511HalContext *ctx, HalEncTask *task)
{
    H265eV511RegSet *regs = ctx->regs;
    Vepu511ControlCfg *reg_ctl = &regs->reg_ctl;
    HevcVepu511Frame *reg_frm = &regs->reg_frm;
    RK_S32 stridey = 0;
    RK_S32 stridec = 0;
    VepuFmtCfg *fmt = ctx->input_fmt;
    MppEncPrepCfg *prep_cfg = &ctx->cfg->prep;
    RK_U32 format;
    RK_U32 hor_stride;

    reg_ctl->dtrns_map.src_bus_edin = fmt->src_endian;
    reg_frm->reg0198_src_fmt.src_cfmt = fmt->format;
    reg_frm->reg0198_src_fmt.alpha_swap = fmt->alpha_swap;
    reg_frm->reg0198_src_fmt.rbuv_swap = fmt->rbuv_swap;

    reg_frm->reg0198_src_fmt.out_fmt = (prep_cfg->format == MPP_FMT_YUV400) ? 0 : 1;

    reg_frm->reg0203_src_proc.src_mirr = prep_cfg->mirroring > 0;
    reg_frm->reg0203_src_proc.src_rot = prep_cfg->rotation;

    kmpp_frame_get_hor_stride(task->frame, &hor_stride);
    if (!hor_stride)
        hor_stride = prep_cfg->hor_stride;
    kmpp_frame_get_fmt(task->frame, &format);
    if (MPP_FRAME_FMT_IS_FBC(format)) {
        reg_frm->reg0203_src_proc.rkfbcd_en = 1;
        stridey = MPP_ALIGN(hor_stride, 64) >> 2;
    } else if (hor_stride) {
        stridey = hor_stride;
    } else {
        if (reg_frm->reg0198_src_fmt.src_cfmt == VEPU541_FMT_BGRA8888 )
            stridey = prep_cfg->width * 4;
        else if (reg_frm->reg0198_src_fmt.src_cfmt == VEPU541_FMT_BGR888 )
            stridey = prep_cfg->width * 3;
        else if (reg_frm->reg0198_src_fmt.src_cfmt == VEPU541_FMT_BGR565 ||
                 reg_frm->reg0198_src_fmt.src_cfmt == VEPU541_FMT_YUYV422 ||
                 reg_frm->reg0198_src_fmt.src_cfmt == VEPU541_FMT_UYVY422)
            stridey = prep_cfg->width * 2;
    }

    switch (fmt->format) {
    case VEPU540C_FMT_YUV444SP : {
        stridec = stridey * 2;
    } break;
    case VEPU541_FMT_YUV422SP :
    case VEPU541_FMT_YUV420SP :
    case VEPU540C_FMT_YUV444P : {
        stridec = stridey;
    } break;
    default : {
        stridec = stridey / 2;
    } break;
    }

    if (reg_frm->reg0198_src_fmt.src_cfmt < VEPU541_FMT_ARGB1555) {
        reg_frm->reg0199_src_udfy.csc_wgt_r2y = 77;
        reg_frm->reg0199_src_udfy.csc_wgt_g2y = 150;
        reg_frm->reg0199_src_udfy.csc_wgt_b2y = 29;

        reg_frm->reg0200_src_udfu.csc_wgt_r2u = -43;
        reg_frm->reg0200_src_udfu.csc_wgt_g2u = -85;
        reg_frm->reg0200_src_udfu.csc_wgt_b2u = 128;

        reg_frm->reg0201_src_udfv.csc_wgt_r2v = 128;
        reg_frm->reg0201_src_udfv.csc_wgt_g2v = -107;
        reg_frm->reg0201_src_udfv.csc_wgt_b2v = -21;

        reg_frm->reg0202_src_udfo.csc_ofst_y = 0;
        reg_frm->reg0202_src_udfo.csc_ofst_u = 128;
        reg_frm->reg0202_src_udfo.csc_ofst_v = 128;
    }

    reg_frm->reg0205_src_strd0.src_strd0  = stridey;
    reg_frm->reg0206_src_strd1.src_strd1  = stridec;

    return MPP_OK;
}

static void vepu511_h265_set_vsp_filtering(H265eV511HalContext *ctx, HalEncTask *task)
{
    H265eV511RegSet *regs = ctx->regs;
    HevcVepu511Frame *s = &regs->reg_frm;
    MppEncCfgSet *cfg = ctx->cfg;
    RK_S32 deblur_en = cfg->tune.deblur_en;
    RK_S32 deblur_str = cfg->tune.deblur_str;
    MppEncHwCfg *hw = &cfg->hw;
    RK_S32 lgt_chg_lvl = ctx->cfg->tune.lgt_chg_lvl;
    RK_U8 bit_chg_lvl = ctx->last_frame_fb.tgt_sub_real_lvl[5]; /* [0, 2] */
    RK_U8 corner_str = 0, edge_str = 0, internal_str = 0; /* [0, 3] */

    if (task->rc_task->info.bit_constr_en) {
        corner_str = edge_str = internal_str = 3;
    } else if (task->rc_task->info.lgt_chg_enable) {
        if (lgt_chg_lvl == 6)
            corner_str = edge_str = internal_str = 2;
        else if (lgt_chg_lvl == 7)
            corner_str = edge_str = internal_str = 3;
    } else if (deblur_en && (deblur_str % 2 == 0) &&
               (hw->flt_str_i == 0) && (hw->flt_str_p == 0)) {
        if (bit_chg_lvl == 2 && ctx->frame_type != INTRA_FRAME)
            corner_str = edge_str = internal_str = 3;
        else if (bit_chg_lvl > 0)
            corner_str = edge_str = internal_str = 2;
    } else {
        if (ctx->frame_type == INTRA_FRAME)
            corner_str = edge_str = internal_str = hw->flt_str_i;
        else
            corner_str = edge_str = internal_str = hw->flt_str_p;
    }

    s->reg0207_src_flt_cfg.pp_corner_filter_strength = corner_str;
    s->reg0207_src_flt_cfg.pp_edge_filter_strength = edge_str;
    s->reg0207_src_flt_cfg.pp_internal_filter_strength = internal_str;
}

static void vepu511_h265_set_slice_regs(H265eV511HalContext *ctx, H265eSyntax_new *syn, HevcVepu511Frame *regs)
{
    regs->reg0237_synt_sps.smpl_adpt_ofst_e     = syn->pp.sample_adaptive_offset_enabled_flag;
    regs->reg0237_synt_sps.num_st_ref_pic       = syn->pp.num_short_term_ref_pic_sets;
    regs->reg0237_synt_sps.num_lt_ref_pic       = syn->pp.num_long_term_ref_pics_sps;
    regs->reg0237_synt_sps.lt_ref_pic_prsnt     = syn->pp.long_term_ref_pics_present_flag;
    regs->reg0237_synt_sps.tmpl_mvp_e           = syn->pp.sps_temporal_mvp_enabled_flag;
    regs->reg0237_synt_sps.log2_max_poc_lsb     = syn->pp.log2_max_pic_order_cnt_lsb_minus4;
    regs->reg0237_synt_sps.strg_intra_smth      = syn->pp.strong_intra_smoothing_enabled_flag;

    regs->reg0238_synt_pps.dpdnt_sli_seg_en     = syn->pp.dependent_slice_segments_enabled_flag;
    regs->reg0238_synt_pps.out_flg_prsnt_flg    = syn->pp.output_flag_present_flag;
    regs->reg0238_synt_pps.num_extr_sli_hdr     = syn->pp.num_extra_slice_header_bits;
    regs->reg0238_synt_pps.sgn_dat_hid_en       = syn->pp.sign_data_hiding_enabled_flag;
    regs->reg0238_synt_pps.cbc_init_prsnt_flg   = syn->pp.cabac_init_present_flag;
    regs->reg0238_synt_pps.pic_init_qp          = syn->pp.init_qp_minus26 + 26;
    regs->reg0238_synt_pps.cu_qp_dlt_en         = syn->pp.cu_qp_delta_enabled_flag;
    regs->reg0238_synt_pps.chrm_qp_ofst_prsn    = syn->pp.pps_slice_chroma_qp_offsets_present_flag;
    regs->reg0238_synt_pps.lp_fltr_acrs_sli     = syn->pp.pps_loop_filter_across_slices_enabled_flag;
    regs->reg0238_synt_pps.dblk_fltr_ovrd_en    = syn->pp.deblocking_filter_override_enabled_flag;
    regs->reg0238_synt_pps.lst_mdfy_prsnt_flg   = syn->pp.lists_modification_present_flag;
    regs->reg0238_synt_pps.sli_seg_hdr_extn     = syn->pp.slice_segment_header_extension_present_flag;
    regs->reg0238_synt_pps.cu_qp_dlt_depth      = syn->pp.diff_cu_qp_delta_depth;
    regs->reg0238_synt_pps.lp_fltr_acrs_til    = syn->pp.loop_filter_across_tiles_enabled_flag;
    regs->reg0238_synt_pps.csip_flag            = syn->pp.constrained_intra_pred_flag;

    regs->reg0239_synt_sli0.cbc_init_flg        = syn->sp.cbc_init_flg;
    regs->reg0239_synt_sli0.mvd_l1_zero_flg     = syn->sp.mvd_l1_zero_flg;
    regs->reg0239_synt_sli0.ref_pic_lst_mdf_l0  = syn->sp.ref_pic_lst_mdf_l0;

    regs->reg0239_synt_sli0.num_refidx_l1_act   = syn->sp.num_refidx_l1_act;
    regs->reg0239_synt_sli0.num_refidx_l0_act   = syn->sp.num_refidx_l0_act;

    regs->reg0239_synt_sli0.num_refidx_act_ovrd = syn->sp.num_refidx_act_ovrd;

    regs->reg0239_synt_sli0.sli_sao_chrm_flg    = syn->sp.sli_sao_chrm_flg;
    regs->reg0239_synt_sli0.sli_sao_luma_flg    = syn->sp.sli_sao_luma_flg;
    regs->reg0239_synt_sli0.sli_tmprl_mvp_e     = syn->sp.sli_tmprl_mvp_en;
    regs->reg0192_enc_pic.num_pic_tot_cur_hevc  = syn->sp.tot_poc_num;

    regs->reg0239_synt_sli0.pic_out_flg         = syn->sp.pic_out_flg;
    regs->reg0239_synt_sli0.sli_type            = syn->sp.slice_type;
    regs->reg0239_synt_sli0.sli_rsrv_flg        = syn->sp.slice_rsrv_flg;
    regs->reg0239_synt_sli0.dpdnt_sli_seg_flg   = syn->sp.dpdnt_sli_seg_flg;
    regs->reg0239_synt_sli0.sli_pps_id          = syn->sp.sli_pps_id;
    regs->reg0239_synt_sli0.no_out_pri_pic      = syn->sp.no_out_pri_pic;


    regs->reg0240_synt_sli1.sp_tc_ofst_div2       = syn->sp.sli_tc_ofst_div2;;
    regs->reg0240_synt_sli1.sp_beta_ofst_div2     = syn->sp.sli_beta_ofst_div2;
    regs->reg0240_synt_sli1.sli_lp_fltr_acrs_sli  = syn->sp.sli_lp_fltr_acrs_sli;
    regs->reg0240_synt_sli1.sp_dblk_fltr_dis      = syn->sp.sli_dblk_fltr_dis;
    regs->reg0240_synt_sli1.dblk_fltr_ovrd_flg    = syn->sp.dblk_fltr_ovrd_flg;
    regs->reg0240_synt_sli1.sli_cb_qp_ofst = syn->pp.pps_slice_chroma_qp_offsets_present_flag ?
                                             syn->sp.sli_cb_qp_ofst : syn->pp.pps_cb_qp_offset;

    if (ctx->cfg->tune.speed == 0) // 0.63ppc
        regs->reg0240_synt_sli1.max_mrg_cnd = 3;
    else
        regs->reg0240_synt_sli1.max_mrg_cnd = 2;

    regs->reg0240_synt_sli1.col_ref_idx           = syn->sp.col_ref_idx;
    regs->reg0240_synt_sli1.col_frm_l0_flg        = syn->sp.col_frm_l0_flg;
    regs->reg0241_synt_sli2.sli_poc_lsb           = syn->sp.sli_poc_lsb;
    regs->reg0241_synt_sli2.sli_hdr_ext_len       = syn->sp.sli_hdr_ext_len;
}

static void vepu511_h265_set_ref_regs(H265eSyntax_new *syn, HevcVepu511Frame *regs)
{
    regs->reg0242_synt_refm0.st_ref_pic_flg = syn->sp.st_ref_pic_flg;
    regs->reg0242_synt_refm0.poc_lsb_lt0 = syn->sp.poc_lsb_lt0;
    regs->reg0242_synt_refm0.num_lt_pic = syn->sp.num_lt_pic;

    regs->reg0243_synt_refm1.dlt_poc_msb_prsnt0 = syn->sp.dlt_poc_msb_prsnt0;
    regs->reg0243_synt_refm1.dlt_poc_msb_cycl0 = syn->sp.dlt_poc_msb_cycl0;
    regs->reg0243_synt_refm1.used_by_lt_flg0 = syn->sp.used_by_lt_flg0;
    regs->reg0243_synt_refm1.used_by_lt_flg1 = syn->sp.used_by_lt_flg1;
    regs->reg0243_synt_refm1.used_by_lt_flg2 = syn->sp.used_by_lt_flg2;
    regs->reg0243_synt_refm1.dlt_poc_msb_prsnt0 = syn->sp.dlt_poc_msb_prsnt0;
    regs->reg0243_synt_refm1.dlt_poc_msb_cycl0 = syn->sp.dlt_poc_msb_cycl0;
    regs->reg0243_synt_refm1.dlt_poc_msb_prsnt1 = syn->sp.dlt_poc_msb_prsnt1;
    regs->reg0243_synt_refm1.num_negative_pics = syn->sp.num_neg_pic;
    regs->reg0243_synt_refm1.num_pos_pic = syn->sp.num_pos_pic;

    regs->reg0243_synt_refm1.used_by_s0_flg = syn->sp.used_by_s0_flg;
    regs->reg0244_synt_refm2.dlt_poc_s0_m10 = syn->sp.dlt_poc_s0_m10;
    regs->reg0244_synt_refm2.dlt_poc_s0_m11 = syn->sp.dlt_poc_s0_m11;
    regs->reg0245_synt_refm3.dlt_poc_s0_m12 = syn->sp.dlt_poc_s0_m12;
    regs->reg0245_synt_refm3.dlt_poc_s0_m13 = syn->sp.dlt_poc_s0_m13;

    regs->reg0246_synt_long_refm0.poc_lsb_lt1 = syn->sp.poc_lsb_lt1;
    regs->reg0247_synt_long_refm1.dlt_poc_msb_cycl1 = syn->sp.dlt_poc_msb_cycl1;
    regs->reg0246_synt_long_refm0.poc_lsb_lt2 = syn->sp.poc_lsb_lt2;
    regs->reg0243_synt_refm1.dlt_poc_msb_prsnt2 = syn->sp.dlt_poc_msb_prsnt2;
    regs->reg0247_synt_long_refm1.dlt_poc_msb_cycl2 = syn->sp.dlt_poc_msb_cycl2;
    regs->reg0240_synt_sli1.lst_entry_l0 = syn->sp.lst_entry_l0;
    regs->reg0239_synt_sli0.ref_pic_lst_mdf_l0 = syn->sp.ref_pic_lst_mdf_l0;

    return;
}

static void vepu511_h265_set_me_regs(H265eV511HalContext *ctx, H265eSyntax_new *syn)
{
    H265eV511RegSet *regs = ctx->regs;
    HevcVepu511Param *s = &regs->reg_param;
    HevcVepu511Frame *reg_frm = &regs->reg_frm;
    MppEncBaseCfg *base = &ctx->cfg->base;

    reg_frm->reg0220_me_rnge.cime_srch_dwnh = 15;
    reg_frm->reg0220_me_rnge.cime_srch_uph  = 15;
    reg_frm->reg0220_me_rnge.cime_srch_rgtw = 12;
    reg_frm->reg0220_me_rnge.cime_srch_lftw = 12;
    reg_frm->reg0221_me_cfg.rme_srch_h      = 3;
    reg_frm->reg0221_me_cfg.rme_srch_v      = 3;
    reg_frm->reg0221_me_cfg.srgn_max_num      = 72;

    reg_frm->reg0221_me_cfg.cime_dist_thre    = 1024;
    reg_frm->reg0221_me_cfg.rme_dis           = 0;
    reg_frm->reg0221_me_cfg.fme_dis           = 0;
    reg_frm->reg0220_me_rnge.dlt_frm_num      = 0;

    if (syn->pp.sps_temporal_mvp_enabled_flag && (ctx->frame_type != INTRA_FRAME)) {
        if (ctx->last_frame_fb.frame_type == INTRA_FRAME)
            reg_frm->reg0222_me_cach.colmv_load = 0;
        else
            reg_frm->reg0222_me_cach.colmv_load = 1;

        reg_frm->reg0222_me_cach.colmv_stor = 1;
    }

    reg_frm->reg0222_me_cach.cime_zero_thre = 64;
    reg_frm->reg0222_me_cach.fme_prefsu_en = 0;

    /* CIME: 0x1760 - 0x176C */
    s->me_sqi_cfg.cime_pmv_num = 1;
    s->me_sqi_cfg.cime_fuse   = 0;
    s->me_sqi_cfg.move_lambda = 2;
    s->me_sqi_cfg.rime_lvl_mrg     = 0;
    s->me_sqi_cfg.rime_prelvl_en   = 3;
    s->me_sqi_cfg.rime_prersu_en   = 0;
    s->me_sqi_cfg.fme_lvl_mrg = 0;

    s->cime_mvd_th.cime_mvd_th0 = 8;
    s->cime_mvd_th.cime_mvd_th1 = 20;
    s->cime_mvd_th.cime_mvd_th2 = 32;
    s->cime_madp_th.cime_madp_th = 16;
    s->cime_madp_th.ratio_consi_cfg = 8;
    s->cime_madp_th.ratio_bmv_dist = 8;
    s->cime_multi.cime_multi0 = 8;
    s->cime_multi.cime_multi1 = 12;
    s->cime_multi.cime_multi2 = 16;
    s->cime_multi.cime_multi3 = 20;

    /* RFME: 0x1770 - 0x177C */
    s->rime_mvd_th.rime_mvd_th0  = 1;
    s->rime_mvd_th.rime_mvd_th1  = 2;
    s->rime_mvd_th.fme_madp_th   = 10;
    s->rime_madp_th.rime_madp_th0 = 8;
    s->rime_madp_th.rime_madp_th1 = 16;
    s->rime_multi.rime_multi0 = 4;
    s->rime_multi.rime_multi1 = 8;
    s->rime_multi.rime_multi2 = 12;
    s->cmv_st_th.cmv_th0 = 64;
    s->cmv_st_th.cmv_th1 = 96;
    s->cmv_st_th.cmv_th2 = 128;

    if (ctx->cfg->tune.scene_mode != MPP_ENC_SCENE_MODE_IPC) {
        s->me_sqi_cfg.move_lambda = 8;
        s->cime_madp_th.cime_madp_th = 0;
        s->rime_madp_th.rime_madp_th0 = 0;
        s->rime_madp_th.rime_madp_th1 = 0;
        s->cime_multi.cime_multi0 = 4;
        s->cime_multi.cime_multi1 = 4;
        s->cime_multi.cime_multi2 = 4;
        s->cime_multi.cime_multi3 = 4;
        s->rime_multi.rime_multi0 = 4;
        s->rime_multi.rime_multi1 = 4;
        s->rime_multi.rime_multi2 = 4;
    } else if (base->smt1_en || base->smt3_en) {
        s->cime_multi.cime_multi0 = 4;
        s->cime_multi.cime_multi1 = 6;
        s->cime_multi.cime_multi2 = 8;
        s->cime_multi.cime_multi3 = 12;
        s->rime_multi.rime_multi0 = 4;
        s->rime_multi.rime_multi1 = 4;
        s->rime_multi.rime_multi2 = 4;
    }
}

static void setup_recn_refr_wrap(H265eV511HalContext *ctx, HevcVepu511Frame *regs, HalEncTask *task)
{
    MppDev dev = ctx->dev;
    RK_U32 recn_ref_wrap = ctx->recn_ref_wrap;
    H265eSyntax_new *syn = (H265eSyntax_new *)task->syntax.data;
    RK_U32 cur_is_non_ref = syn->sp.non_reference_flag;
    RK_U32 cur_is_lt = syn->sp.recon_pic.is_lt;
    RK_U32 refr_is_lt = syn->sp.ref_pic.is_lt;
    WrapInfo *bdy_lt = &ctx->wrap_infos.body_lt;
    WrapInfo *hdr_lt = &ctx->wrap_infos.hdr_lt;
    WrapInfo *bdy = &ctx->wrap_infos.body;
    WrapInfo *hdr = &ctx->wrap_infos.hdr;
    RK_U32 ref_iova;
    RK_U32 rfp_h_bot = 0;
    RK_U32 rfp_b_bot = 0;
    RK_U32 rfp_h_top = 0;
    RK_U32 rfp_b_top = 0;
    RK_U32 rfpw_h_off = 0;
    RK_U32 rfpw_b_off = 0;
    RK_U32 rfpr_h_off = 0;
    RK_U32 rfpr_b_off = 0;

    ref_iova = mpp_buffer_get_iova(ctx->recn_ref_buf, dev);

    if (ctx->frame_type == INTRA_FRAME &&
        syn->sp.recon_pic.slot_idx == syn->sp.ref_pic.slot_idx) {

        hal_h265e_dbg_wrap("cur is idr  lt %d\n", cur_is_lt);
        if (cur_is_lt) {
            rfpr_h_off = hdr_lt->cur_off;
            rfpr_b_off = bdy_lt->cur_off;
            rfpw_h_off = hdr_lt->cur_off;
            rfpw_b_off = bdy_lt->cur_off;

            rfp_h_bot = hdr->bottom < hdr_lt->bottom ? hdr->bottom : hdr_lt->bottom;
            rfp_h_top = hdr->top > hdr_lt->top ? hdr->top : hdr_lt->top;
            rfp_b_bot = bdy->bottom < bdy_lt->bottom ? bdy->bottom : bdy_lt->bottom;
            rfp_b_top = bdy->top > bdy_lt->top ? bdy->top : bdy_lt->top;
        } else {
            rfpr_h_off = hdr->cur_off;
            rfpr_b_off = bdy->cur_off;
            rfpw_h_off = hdr->cur_off;
            rfpw_b_off = bdy->cur_off;

            rfp_h_bot = hdr->bottom;
            rfp_h_top = hdr->top;
            rfp_b_bot = bdy->bottom;
            rfp_b_top = bdy->top;
        }
    } else {
        RefType type = ref_type_map[cur_is_lt][refr_is_lt];

        hal_h265e_dbg_wrap("ref type %d\n", type);
        switch (type) {
        case ST_REF_TO_ST: {
            /* refr */
            rfpr_h_off = hdr->pre_off;
            rfpr_b_off = bdy->pre_off;
            /* recn */
            rfpw_h_off = hdr->cur_off;
            rfpw_b_off = bdy->cur_off;

            rfp_h_bot = hdr->bottom;
            rfp_h_top = hdr->top;
            rfp_b_bot = bdy->bottom;
            rfp_b_top = bdy->top;
        } break;
        case ST_REF_TO_LT: {
            /* refr */
            rfpr_h_off = hdr_lt->cur_off;
            rfpr_b_off = bdy_lt->cur_off;
            /* recn */
            hdr->cur_off = hdr->bottom;
            bdy->cur_off = bdy->bottom;
            rfpw_h_off = hdr->cur_off;
            rfpw_b_off = bdy->cur_off;

            rfp_h_bot = hdr->bottom < hdr_lt->bottom ? hdr->bottom : hdr_lt->bottom;
            rfp_h_top = hdr->top > hdr_lt->top ? hdr->top : hdr_lt->top;
            rfp_b_bot = bdy->bottom < bdy_lt->bottom ? bdy->bottom : bdy_lt->bottom;
            rfp_b_top = bdy->top > bdy_lt->top ? bdy->top : bdy_lt->top;
        } break;
        case LT_REF_TO_ST: {
            /* not support this case */
            kmpp_loge_f("WARNING: not support lt ref to st when buf is wrap");
        } break;
        case LT_REF_TO_LT: {
            if (ctx->only_smartp) {
                WrapInfo tmp;
                /* the case is hard to implement */
                rfpr_h_off = hdr_lt->cur_off;
                rfpr_b_off = bdy_lt->cur_off;

                hdr->cur_off = hdr->bottom;
                bdy->cur_off = bdy->bottom;
                rfpw_h_off = hdr->cur_off;
                rfpw_b_off = bdy->cur_off;

                rfp_h_bot = hdr->bottom < hdr_lt->bottom ? hdr->bottom : hdr_lt->bottom;
                rfp_h_top = hdr->top > hdr_lt->top ? hdr->top : hdr_lt->top;
                rfp_b_bot = bdy->bottom < bdy_lt->bottom ? bdy->bottom : bdy_lt->bottom;
                rfp_b_top = bdy->top > bdy_lt->top ? bdy->top : bdy_lt->top;

                /* swap */
                kmpp_memcpy(&tmp, hdr, sizeof(WrapInfo));
                kmpp_memcpy(hdr, hdr_lt, sizeof(WrapInfo));
                kmpp_memcpy(hdr_lt, &tmp, sizeof(WrapInfo));

                kmpp_memcpy(&tmp, bdy, sizeof(WrapInfo));
                kmpp_memcpy(bdy, bdy_lt, sizeof(WrapInfo));
                kmpp_memcpy(bdy_lt, &tmp, sizeof(WrapInfo));
            } else
                kmpp_loge_f("WARNING: not support lt ref to lt when buf is wrap");
        } break;
        default: {
        } break;
        }
    }

    regs->reg0163_rfpw_h_addr = ref_iova + rfpw_h_off;
    regs->reg0164_rfpw_b_addr = ref_iova + rfpw_b_off;

    regs->reg0165_rfpr_h_addr = ref_iova + rfpr_h_off;
    regs->reg0166_rfpr_b_addr = ref_iova + rfpr_b_off;

    regs->reg0180_adr_rfpt_h = ref_iova + rfp_h_top;
    regs->reg0181_adr_rfpb_h = ref_iova + rfp_h_bot;
    regs->reg0182_adr_rfpt_b = ref_iova + rfp_b_top;
    regs->reg0183_adr_rfpb_b = ref_iova + rfp_b_bot;

    regs->reg0223_me_ref_comb.rfpw_mode = 1;
    regs->reg0223_me_ref_comb.rfpr_mode = 1;

    if (recn_ref_wrap) {
        RK_U32 cur_hdr_off;
        RK_U32 cur_bdy_off;

        hal_h265e_dbg_wrap("cur_is_ref %d\n", !cur_is_non_ref);
        hal_h265e_dbg_wrap("hdr[size %d top %d bot %d cur %d pre %d]\n",
                           hdr->size, hdr->top, hdr->bottom, hdr->cur_off, hdr->pre_off);
        hal_h265e_dbg_wrap("bdy [size %d top %d bot %d cur %d pre %d]\n",
                           bdy->size, bdy->top, bdy->bottom, bdy->cur_off, bdy->pre_off);
        if (!cur_is_non_ref) {
            hdr->pre_off = hdr->cur_off;
            cur_hdr_off = hdr->pre_off + hdr->size;
            cur_hdr_off = cur_hdr_off >= hdr->top ?
                          (cur_hdr_off - hdr->top + hdr->bottom) : cur_hdr_off;
            hdr->cur_off = cur_hdr_off;

            bdy->pre_off = bdy->cur_off;
            cur_bdy_off = bdy->pre_off + bdy->size;
            cur_bdy_off = cur_bdy_off >= bdy->top ?
                          (cur_bdy_off - bdy->top + bdy->bottom) : cur_bdy_off;
            bdy->cur_off = cur_bdy_off;
        }
    }
}

void vepu511_h265_set_hw_address(H265eV511HalContext *ctx, HevcVepu511Frame *regs, HalEncTask *task)
{
    HalEncTask *enc_task = task;
    HalBuf *recon_buf, *ref_buf;
    H265eSyntax_new *syn = (H265eSyntax_new *)enc_task->syntax.data;
    VepuFmtCfg *fmt = (VepuFmtCfg *) ctx->input_fmt;
    RK_U32 len;
    RK_U32 offset_x, offset_y;

    hal_h265e_enter();

    kmpp_packet_get_length(task->packet, &len);
    if (!ctx->online) {
        RK_U64 iova = 0;

        kmpp_buffer_get_iova_by_device(task->input, &iova, mpp_get_dev(ctx->dev));
        regs->reg0160_adr_src0 = iova;
        regs->reg0161_adr_src1 = regs->reg0160_adr_src0;
        regs->reg0162_adr_src2 = regs->reg0160_adr_src0;

        vepu511_h265_uv_address(regs, syn, fmt->format, task);
    }

    recon_buf = hal_bufs_get_buf(ctx->dpb_bufs, syn->sp.recon_pic.slot_idx);
    ref_buf = hal_bufs_get_buf(ctx->dpb_bufs, syn->sp.ref_pic.slot_idx);

    if (ctx->recn_ref_wrap)
        setup_recn_refr_wrap(ctx, regs, task);
    else {
        regs->reg0163_rfpw_h_addr = mpp_buffer_get_iova(recon_buf->buf[RECREF_TYPE], ctx->dev);
        regs->reg0164_rfpw_b_addr = regs->reg0163_rfpw_h_addr + ctx->fbc_header_len;
        regs->reg0165_rfpr_h_addr = mpp_buffer_get_iova(ref_buf->buf[RECREF_TYPE], ctx->dev);
        regs->reg0166_rfpr_b_addr = regs->reg0165_rfpr_h_addr + ctx->fbc_header_len;
        regs->reg0180_adr_rfpt_h = 0xffffffff;
        regs->reg0181_adr_rfpb_h = 0;
        regs->reg0182_adr_rfpt_b = 0xffffffff;
        regs->reg0183_adr_rfpb_b = 0;
    }

    if (recon_buf->buf[SMEAR_TYPE])
        regs->reg0185_adr_smr_wr = mpp_buffer_get_iova(recon_buf->buf[SMEAR_TYPE], ctx->dev);
    else
        regs->reg0185_adr_smr_wr = 0;
    if (ref_buf->buf[SMEAR_TYPE])
        regs->reg0184_adr_smr_rd = mpp_buffer_get_iova(ref_buf->buf[SMEAR_TYPE], ctx->dev);
    else
        regs->reg0184_adr_smr_rd = 0;

    if (ctx->cfg->h265.tmvp_enable) {
        regs->reg0167_cmvw_addr =
            mpp_buffer_get_iova(recon_buf->buf[CMV_TYPE], ctx->dev);
        regs->reg0168_cmvr_addr =
            mpp_buffer_get_iova(ref_buf->buf[CMV_TYPE], ctx->dev);
    }
    regs->reg0169_dspw_addr = mpp_buffer_get_iova(recon_buf->buf[THUMB_TYPE], ctx->dev);
    regs->reg0170_dspr_addr = mpp_buffer_get_iova(ref_buf->buf[THUMB_TYPE], ctx->dev);

    if (enc_task->output->cir_flag) {
        RK_U32 size = mpp_buffer_get_size(enc_task->output->buf);

        regs->reg0173_bsbb_addr = mpp_buffer_get_iova(enc_task->output->buf, ctx->dev);
        regs->reg0174_bsbs_addr = regs->reg0173_bsbb_addr + ((enc_task->output->start_offset + len) % size);
        regs->reg0175_bsbr_addr = regs->reg0173_bsbb_addr + enc_task->output->r_pos;
        regs->reg0172_bsbt_addr = regs->reg0173_bsbb_addr + size;
    } else {
        RK_U32 out_addr = mpp_buffer_get_iova(enc_task->output->buf, ctx->dev);

        regs->reg0172_bsbt_addr = out_addr + enc_task->output->size - 1;
        regs->reg0173_bsbb_addr = out_addr + enc_task->output->start_offset;
        regs->reg0175_bsbr_addr = out_addr;
        regs->reg0174_bsbs_addr = out_addr + len;
    }

    kmpp_frame_get_offset_x(task->frame, &offset_x);
    kmpp_frame_get_offset_y(task->frame, &offset_y);
    regs->reg0204_pic_ofst.pic_ofst_x = offset_x;
    regs->reg0204_pic_ofst.pic_ofst_y = offset_y;

    if (len && task->output->buf) {
        task->output->use_len = len;
        ring_buf_flush(task->output, 0);
    }
}

static void vepu511_h265_set_ext_line_buf(H265eV511HalContext *ctx, H265eV511RegSet *regs)
{
    if (ctx->ext_line_buf) {
        RK_U32 addr = mpp_buffer_get_iova(ctx->ext_line_buf, ctx->dev);

        regs->reg_frm.reg0179_adr_ebufb = addr;
        regs->reg_frm.reg0178_adr_ebuft = addr + ctx->ext_line_buf_size;
    } else {
        regs->reg_frm.reg0179_adr_ebufb = 0;
        regs->reg_frm.reg0178_adr_ebuft = 0;
    }
}

static MPP_RET vepu511_h265_set_dvbm(H265eV511HalContext *ctx, HalEncTask *task)
{
    H265eV511RegSet *regs = ctx->regs;
    RK_U32 width = ctx->cfg->prep.width;

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

    regs->reg_frm.reg0194_enc_id.frame_id = 0;
    regs->reg_frm.reg0194_enc_id.frm_id_match = 0;
    regs->reg_frm.reg0194_enc_id.source_id = 0;
    regs->reg_frm.reg0194_enc_id.src_id_match = 0;
    regs->reg_frm.reg0194_enc_id.ch_id = 1;
    regs->reg_frm.reg0194_enc_id.vinf_req_en = 1;
    regs->reg_frm.reg0194_enc_id.vrsp_rtn_en = 1;

    regs->reg_ctl.int_en.dvbm_err_en = 1;

    return MPP_OK;
}

static MPP_RET vepu511_h265_set_normal(H265eV511HalContext *ctx)
{
    H265eV511RegSet *regs = ctx->regs;
    Vepu511ControlCfg *reg_ctl = &regs->reg_ctl;

    reg_ctl->enc_strt.vepu_cmd     = 1;
    reg_ctl->enc_clr.safe_clr      = 0;
    reg_ctl->enc_clr.force_clr     = 0;
    reg_ctl->enc_clr.dvbm_clr      = 0;

    reg_ctl->int_en.enc_done_en        = 1;
    reg_ctl->int_en.lkt_node_done_en   = 1;
    reg_ctl->int_en.sclr_done_en       = 1;
    reg_ctl->int_en.vslc_done_en       = 1;
    reg_ctl->int_en.vbsf_oflw_en       = 1;
    reg_ctl->int_en.vbuf_lens_en       = 1;
    reg_ctl->int_en.enc_err_en         = 1;
    reg_ctl->int_en.vsrc_err_en        = 1;
    reg_ctl->int_en.wdg_en             = 1;
    reg_ctl->int_en.lkt_err_int_en     = 1;
    reg_ctl->int_en.lkt_err_stop_en    = 1;
    reg_ctl->int_en.lkt_force_stop_en  = 1;
    reg_ctl->int_en.jslc_done_en       = 1;
    reg_ctl->int_en.jbsf_oflw_en       = 1;
    reg_ctl->int_en.jbuf_lens_en       = 1;
    reg_ctl->int_en.dvbm_err_en        = 0;

    reg_ctl->dtrns_map.jpeg_bus_edin    = 0x7;
    reg_ctl->int_clr.enc_done_clr = 1;

    reg_ctl->dtrns_map.src_bus_edin     = 0x0;
    reg_ctl->dtrns_map.meiw_bus_edin    = 0x0;
    reg_ctl->dtrns_map.bsw_bus_edin     = 0x7;
    reg_ctl->dtrns_map.lktr_bus_edin    = 0x0;
    reg_ctl->dtrns_map.roir_bus_edin    = 0x0;
    reg_ctl->dtrns_map.lktw_bus_edin    = 0x0;
    reg_ctl->dtrns_map.rec_nfbc_bus_edin   = 0x0;

    reg_ctl->dtrns_cfg.axi_brsp_cke     = 0x0;
    reg_ctl->enc_wdg.vs_load_thd        = 0x1fffff;
    reg_ctl->opt_strg.cke                = 1;
    reg_ctl->opt_strg.sram_ckg_en        = 1;
    reg_ctl->opt_strg.rfpr_err_e         = 1;
    reg_ctl->opt_strg.resetn_hw_en       = 1;

    /* enable rdo clk gating */
    {
        RK_U32 *rdo_ckg = (RK_U32*)&regs->reg_ctl.rdo_ckg_hevc;

        *rdo_ckg = 0xffffffff;
    }

    return MPP_OK;
}

static void vepu511_h265_set_split(H265eV511RegSet *regs, MppEncCfgSet *enc_cfg)
{
    MppEncSliceSplit *cfg = &enc_cfg->split;

    hal_h265e_dbg_func("enter\n");

    switch (cfg->split_mode) {
    case MPP_ENC_SPLIT_NONE : {
        regs->reg_frm.reg0216_sli_splt.sli_splt = 0;
        regs->reg_frm.reg0216_sli_splt.sli_splt_mode = 0;
        regs->reg_frm.reg0216_sli_splt.sli_splt_cpst = 0;
        regs->reg_frm.reg0216_sli_splt.sli_max_num_m1 = 0;
        regs->reg_frm.reg0216_sli_splt.sli_flsh = 0;
        regs->reg_frm.reg0218_sli_cnum.sli_splt_cnum_m1 = 0;

        regs->reg_frm.reg0217_sli_byte.sli_splt_byte = 0;
        regs->reg_frm.reg0192_enc_pic.slen_fifo = 0;
    } break;
    case MPP_ENC_SPLIT_BY_BYTE : {
        regs->reg_frm.reg0216_sli_splt.sli_splt = 1;
        regs->reg_frm.reg0216_sli_splt.sli_splt_mode = 0;
        regs->reg_frm.reg0216_sli_splt.sli_splt_cpst = 0;
        regs->reg_frm.reg0216_sli_splt.sli_max_num_m1 = 500;
        regs->reg_frm.reg0216_sli_splt.sli_flsh = 1;
        regs->reg_frm.reg0218_sli_cnum.sli_splt_cnum_m1 = 0;

        regs->reg_frm.reg0217_sli_byte.sli_splt_byte = cfg->split_arg;
        regs->reg_frm.reg0192_enc_pic.slen_fifo = cfg->split_out ? 1 : 0;
        regs->reg_ctl.int_en.vslc_done_en = cfg->split_out ? 1 : 0;
    } break;
    case MPP_ENC_SPLIT_BY_CTU : {
        regs->reg_frm.reg0216_sli_splt.sli_splt = 1;
        regs->reg_frm.reg0216_sli_splt.sli_splt_mode = 1;
        regs->reg_frm.reg0216_sli_splt.sli_splt_cpst = 0;
        regs->reg_frm.reg0216_sli_splt.sli_max_num_m1 = 500;
        regs->reg_frm.reg0216_sli_splt.sli_flsh = 1;
        regs->reg_frm.reg0218_sli_cnum.sli_splt_cnum_m1 = cfg->split_arg - 1;

        regs->reg_frm.reg0217_sli_byte.sli_splt_byte = 0;
        regs->reg_frm.reg0192_enc_pic.slen_fifo = cfg->split_out ? 1 : 0;
        regs->reg_ctl.int_en.vslc_done_en = cfg->split_out ? 1 : 0;
    } break;
    default : {
        kmpp_logi_f("invalide slice split mode %d\n", cfg->split_mode);
    } break;
    }

    hal_h265e_dbg_func("leave\n");
}

static MPP_RET vepu511_h265_set_prep(void *hal, HalEncTask *task)
{
    static RK_U32 cu_inter_e[4] = { (3 << 6) | (3 << 3) | (3 << 0), (1 << 6) | (3 << 3) | (3 << 0),
                                    (1 << 6) | (3 << 3) | (3 << 0), (0 << 6) | (3 << 3) | (3 << 0)
                                  };
    static RK_U32 intra_pu4_mode_num[4]  = { 2, 1, 1, 1 };
    static RK_U32 intra_pu8_mode_num[4]  = { 2, 1, 1, 1 };
    static RK_U32 intra_pu16_mode_num[4] = { 2, 1, 1, 1 };
    static RK_U32 intra_pu32_mode_num[4] = { 2, 1, 1, 1 };

    H265eV511HalContext *ctx = (H265eV511HalContext *)hal;
    H265eV511RegSet *regs = ctx->regs;
    HevcVepu511Frame *reg_frm = &regs->reg_frm;
    HevcVepu511RcRoi *reg_klut = &regs->reg_rc_roi;
    H265eSyntax_new *syn = (H265eSyntax_new *)task->syntax.data;
    RK_U32 pic_width_align8 = MPP_ALIGN(syn->pp.pic_width, 8);
    RK_U32 pic_height_align8 = MPP_ALIGN(syn->pp.pic_height, 8);
    RK_U32 pic_wd32 = MPP_ALIGN(syn->pp.pic_width, 32) >> 5;
    RK_U32 pic_h32 = MPP_ALIGN(syn->pp.pic_height, 32) >> 5;

    reg_frm->reg0196_enc_rsl.pic_wd8_m1 = (pic_width_align8 >> 3) - 1;
    reg_frm->reg0196_enc_rsl.pic_hd8_m1 = (pic_height_align8 >> 3) - 1;
    reg_frm->reg0197_src_fill.pic_wfill = pic_width_align8 - syn->pp.pic_width;
    reg_frm->reg0197_src_fill.pic_hfill = pic_height_align8 - syn->pp.pic_height;

    /* H.265 mode */
    reg_frm->reg0192_enc_pic.enc_stnd      = 1;
    /* current frame will be refered */
    reg_frm->reg0192_enc_pic.cur_frm_ref   = !syn->sp.non_reference_flag;

    reg_frm->reg0192_enc_pic.bs_scp        = 1;
    reg_frm->reg0192_enc_pic.log2_ctu_num_hevc  = mpp_ceil_log2(pic_wd32 * pic_h32);

    reg_klut->klut_ofst.chrm_klut_ofst = ((ctx->frame_type == INTRA_FRAME) ||
                                          (ctx->cfg->tune.scene_mode != MPP_ENC_SCENE_MODE_IPC)) ? 6 : 9;

    reg_frm->reg0192_enc_pic.rfpr_compress_mode = 0;
    reg_frm->reg0192_enc_pic.rec_fbc_dis = 0;

    reg_frm->reg0232_rdo_cfg.chrm_spcl  = 0;

    /*
    * H265 Max Inter/Intra cu prediction Mode.
    * More prediction modes lead to better compression performance but increase computational cycles.
    *
    * Default speed preset configuration to 0.67 PPC, ~40 FPS for 4K resolution at 500MHz:
    * - Set Inter prediction 32/16/8 CUs at 1/3/2 and Intra 32/16/8/4 CUs at 1,
    *   Maximize the number of modes while ensuring the prediction hierarchy remains unchanged.
    * - Set cime_fuse = 1, disable dual-window search for higher real-time performance.
    * - Set fme_lvl_mrg = 1, enable FME's depth1 and depth2 joint search,
    *   improves real-time performance but will reduce the compression ratio.
    * - Set cime_srch_lftw/rgtw/uph/dwnh = 12/12/15/15, expand CIME search range degraded real-time performance.
    * - Set rime_prelvl_en = 0, disable RIME pre-level to improve real-time performance.
    * - Set fmdc_adju_split32 = 0, enable CU32 block prediction.
    *   Setting fmdc_adju_split32 = 1 restricts prediction to CU16/8 only, improving real-time performance.
    */
    reg_frm->reg0232_rdo_cfg.cu_inter_e = (3 << 6) | (3 << 3) | (3 << 0);
    reg_frm->reg0233_rdo_intra_mode.intra_pu4_mode_num  = 2;
    reg_frm->reg0233_rdo_intra_mode.intra_pu8_mode_num  = 2;
    reg_frm->reg0233_rdo_intra_mode.intra_pu16_mode_num = 2;
    reg_frm->reg0233_rdo_intra_mode.intra_pu32_mode_num = 2;

    if (ctx->frame_type == INTER_P_FRAME) {
        RK_U32 speed_idx = ctx->cfg->tune.speed;

        // 0 - 0.63ppc; 1 - 0.68ppc; 2 - 0.69ppc; 3 - 0.7ppc
        reg_frm->reg0232_rdo_cfg.cu_inter_e = cu_inter_e[speed_idx];
        reg_frm->reg0233_rdo_intra_mode.intra_pu4_mode_num  = intra_pu4_mode_num[speed_idx];
        reg_frm->reg0233_rdo_intra_mode.intra_pu8_mode_num  = intra_pu8_mode_num[speed_idx];
        reg_frm->reg0233_rdo_intra_mode.intra_pu16_mode_num = intra_pu16_mode_num[speed_idx];
        reg_frm->reg0233_rdo_intra_mode.intra_pu32_mode_num = intra_pu32_mode_num[speed_idx];
    }

    if (syn->pp.num_long_term_ref_pics_sps) {
        reg_frm->reg0232_rdo_cfg.ltm_col = 0;
        reg_frm->reg0232_rdo_cfg.ltm_idx0l0 = 1;
    } else {
        reg_frm->reg0232_rdo_cfg.ltm_col = 0;
        reg_frm->reg0232_rdo_cfg.ltm_idx0l0 = 0;
    }

    reg_frm->reg0232_rdo_cfg.ccwa_e = 1;
    reg_frm->reg0232_rdo_cfg.scl_lst_sel = syn->pp.scaling_list_mode;

    {
        RK_U32 i_nal_type = 0;

        if (ctx->frame_type == INTRA_FRAME)
            i_nal_type = NAL_IDR_W_RADL;
        else if (ctx->frame_type == INTER_P_FRAME)
            i_nal_type = NAL_TRAIL_R;
        else
            i_nal_type = NAL_TRAIL_R;

        reg_frm->reg0236_synt_nal.nal_unit_type = i_nal_type;
    }

    return MPP_OK;
}

static MPP_RET vepu511_h265e_save_pass1_patch(H265eV511RegSet *regs, H265eV511HalContext *ctx)
{
    RK_S32 width_align = MPP_ALIGN(ctx->cfg->prep.width, 16);
    RK_S32 height_align = MPP_ALIGN(ctx->cfg->prep.height, 16);

    if (NULL == ctx->buf_pass1) {
        mpp_buffer_get(NULL, &ctx->buf_pass1, width_align * height_align * 3 / 2);
        if (!ctx->buf_pass1) {
            kmpp_loge_f("buf_pass1 malloc fail, debreath invaild");
            return MPP_NOK;
        }
    }

    regs->reg_frm.reg0192_enc_pic.cur_frm_ref = 1;
    regs->reg_frm.reg0163_rfpw_h_addr = mpp_buffer_get_iova(ctx->buf_pass1, ctx->dev);
    regs->reg_frm.reg0164_rfpw_b_addr = regs->reg_frm.reg0163_rfpw_h_addr
                                        + (width_align * height_align);
    regs->reg_frm.reg0192_enc_pic.rec_fbc_dis = 1;
    regs->reg_frm.reg0223_me_ref_comb.rfpw_mode = 0;

    /* NOTE: disable split to avoid lowdelay slice output */
    regs->reg_frm.reg0216_sli_splt.sli_splt = 0;
    regs->reg_frm.reg0192_enc_pic.slen_fifo = 0;

    return MPP_OK;
}

static MPP_RET vepu511_h265e_use_pass1_patch(H265eV511RegSet *regs, H265eV511HalContext *ctx,
                                             H265eSyntax_new *syn)
{
    Vepu511ControlCfg *reg_ctl = &regs->reg_ctl;
    VepuFmtCfg *fmt = (VepuFmtCfg *)ctx->input_fmt;
    RK_S32 width_align = MPP_ALIGN(ctx->cfg->prep.width, 16);
    RK_S32 height_align = MPP_ALIGN(ctx->cfg->prep.height, 16);
    RK_S32 stridey = width_align;

    regs->reg_frm.reg0192_enc_pic.rfpr_compress_mode = 1;
    reg_ctl->dtrns_map.src_bus_edin = fmt->src_endian;
    regs->reg_frm.reg0198_src_fmt.src_cfmt = VEPU541_FMT_YUV420SP;
    regs->reg_frm.reg0198_src_fmt.out_fmt = 1;
    regs->reg_frm.reg0198_src_fmt.alpha_swap = 0;
    regs->reg_frm.reg0198_src_fmt.rbuv_swap  = 0;

    regs->reg_frm.reg0203_src_proc.rkfbcd_en  = 0;
    regs->reg_frm.reg0205_src_strd0.src_strd0 = stridey;
    regs->reg_frm.reg0206_src_strd1.src_strd1 = stridey;

    regs->reg_frm.reg0203_src_proc.src_mirr   = 0;
    regs->reg_frm.reg0203_src_proc.src_rot    = 0;

    regs->reg_frm.reg0204_pic_ofst.pic_ofst_y = 0;
    regs->reg_frm.reg0204_pic_ofst.pic_ofst_x = 0;

    regs->reg_frm.reg0160_adr_src0 = mpp_buffer_get_iova(ctx->buf_pass1, ctx->dev);
    regs->reg_frm.reg0161_adr_src1 = regs->reg_frm.reg0160_adr_src0
                                     + (width_align * height_align);
    regs->reg_frm.reg0162_adr_src2 = 0;

    return MPP_OK;
}

static void vepu511_h265_set_atf_regs(H265eV511HalContext *ctx)
{
    static RK_U8 b32_skip_wgt0[3] =  { 24, 14, 12 };
    static RK_U8 b32_skip_wgt1[3] =  { 20, 12, 10 };
    static RK_U8 b32_skip_wgt2[3] =  { 20, 12, 10 };
    static RK_U8 b16_skip_wgt0[3] =  { 24, 14, 12 };
    static RK_U8 b16_skip_wgt1[3] =  { 20, 10, 8 };
    static RK_U8 b16_skip_wgt2[3] =  { 20, 10, 8 };
    static RK_U8 b32_inter_wgt0[3] = { 24, 26, 28 };
    static RK_U8 b32_inter_wgt1[3] = { 20, 22, 26 };
    static RK_U8 b32_inter_wgt2[3] = { 20, 22, 26 };
    static RK_U8 b16_inter_wgt0[3] = { 32, 40, 44 };
    static RK_U8 b16_inter_wgt1[3] = { 28, 40, 44 };
    static RK_U8 b16_inter_wgt2[3] = { 28, 40, 44 };
    static RK_U8 b32_intra_wgt0[3] = { 32, 36, 42 };
    static RK_U8 b32_intra_wgt1[3] = { 32, 36, 42 };
    static RK_U8 b32_intra_wgt2[3] = { 32, 36, 42 };
    static RK_U8 b16_intra_wgt0[3] = { 40, 44, 48 };
    static RK_U8 b16_intra_wgt1[3] = { 40, 44, 48 };
    static RK_U8 b16_intra_wgt2[3] = { 40, 44, 48 };

    H265eV511RegSet *regs = ctx->regs;
    HevcVepu511Sqi *reg = &regs->reg_sqi;
    RK_U32 str = ctx->cfg->tune.atf_str;
    RdoSkipPar *p_rdo_skip = NULL;
    RdoNoSkipPar *p_rdo_noskip = NULL;
    RK_U32 str_idx = 0;

    reg->rdo_b32_skip.atf_thd0.flckr_en = !!str;
    reg->rdo_b32_skip.atf_thd0.flckr_frame_qp_en = !!str;
    reg->rdo_b32_skip.atf_thd0.flckr_lgt_chng_en = !!str;

    if (str == 0)
        return;

    /* 0 - disable; 1 - weak; 2 - medium; 3 - strong */
    str_idx = str - 1;
    reg->dark_brgt_opt_cfg.thre_max_luma_dark = 20;
    reg->dark_brgt_opt_cfg.thre_min_luma_bright = 235;
    reg->dark_brgt_opt_cfg.thre_ratio_dark_bright = 48;

    p_rdo_skip = &reg->rdo_b32_skip;
    p_rdo_skip->atf_thd0.madp_thd0 = 1;
    p_rdo_skip->atf_thd0.madp_thd1 = 20;
    p_rdo_skip->atf_thd1.madp_thd2 = 40;
    p_rdo_skip->atf_thd1.madp_thd3 = 60;
    p_rdo_skip->atf_wgt0.wgt0 = b32_skip_wgt0[str_idx];
    p_rdo_skip->atf_wgt0.wgt1 = b32_skip_wgt1[str_idx];
    p_rdo_skip->atf_wgt0.wgt2 = b32_skip_wgt2[str_idx];
    p_rdo_skip->atf_wgt0.wgt3 = 0;

    p_rdo_noskip = &reg->rdo_b32_inter;
    p_rdo_noskip->atf_thd0.madp_thd0 = 8;
    p_rdo_noskip->atf_thd0.madp_thd1 = 16;
    p_rdo_noskip->atf_thd1.madp_thd2 = 32;
    p_rdo_noskip->atf_wgt.wgt0 = b32_inter_wgt0[str_idx];
    p_rdo_noskip->atf_wgt.wgt1 = b32_inter_wgt1[str_idx];
    p_rdo_noskip->atf_wgt.wgt2 = b32_inter_wgt2[str_idx];

    p_rdo_noskip = &reg->rdo_b32_intra;
    p_rdo_noskip->atf_thd0.madp_thd0 = 400;
    p_rdo_noskip->atf_thd0.madp_thd1 = 400;
    p_rdo_noskip->atf_thd1.madp_thd2 = 400;
    p_rdo_noskip->atf_wgt.wgt0 = b32_intra_wgt0[str_idx];
    p_rdo_noskip->atf_wgt.wgt1 = b32_intra_wgt1[str_idx];
    p_rdo_noskip->atf_wgt.wgt2 = b32_intra_wgt2[str_idx];

    p_rdo_skip = &reg->rdo_b16_skip;
    p_rdo_skip->atf_thd0.madp_thd0 = 1;
    p_rdo_skip->atf_thd0.madp_thd1 = 20;
    p_rdo_skip->atf_thd1.madp_thd2 = 40;
    p_rdo_skip->atf_thd1.madp_thd3 = 60;
    p_rdo_skip->atf_wgt0.wgt0 = b16_skip_wgt0[str_idx];
    p_rdo_skip->atf_wgt0.wgt1 = b16_skip_wgt1[str_idx];
    p_rdo_skip->atf_wgt0.wgt2 = b16_skip_wgt2[str_idx];
    p_rdo_skip->atf_wgt0.wgt3 = 2;

    p_rdo_noskip = &reg->rdo_b16_inter;
    p_rdo_noskip->atf_thd0.madp_thd0 = 8;
    p_rdo_noskip->atf_thd0.madp_thd1 = 16;
    p_rdo_noskip->atf_thd1.madp_thd2 = 32;
    p_rdo_noskip->atf_wgt.wgt0 = b16_inter_wgt0[str_idx];
    p_rdo_noskip->atf_wgt.wgt1 = b16_inter_wgt1[str_idx];
    p_rdo_noskip->atf_wgt.wgt2 = b16_inter_wgt2[str_idx];

    p_rdo_noskip = &reg->rdo_b16_intra;
    p_rdo_noskip->atf_thd0.madp_thd0 = 150;
    p_rdo_noskip->atf_thd0.madp_thd1 = 150;
    p_rdo_noskip->atf_thd1.madp_thd2 = 150;
    p_rdo_noskip->atf_wgt.wgt0 = b16_intra_wgt0[str_idx];
    p_rdo_noskip->atf_wgt.wgt1 = b16_intra_wgt1[str_idx];
    p_rdo_noskip->atf_wgt.wgt2 = b16_intra_wgt2[str_idx];
    p_rdo_noskip->atf_wgt.wgt3 = 25;
}

/* Note: Anti-stripe is also named as anti-line in RV1106/RV1103B */
static void vepu511_h265_set_anti_stripe_regs(H265eV511HalContext *ctx)
{
    static RK_U32 pre_i32_cst_thd[3][16] = {
        { 4, 14, 4, 3, 3, 5, 6, 4, 9, 4, 6, 4, 6, 4, 5, 28 },
        { 4, 14, 4, 3, 3, 5, 6, 4, 9, 4, 6, 4, 6, 4, 5, 28 },
        { 6, 17, 6, 4, 4, 7, 8, 6, 12, 6, 8, 6, 8, 6, 5, 28 }
    };

    static RK_U32 pre_i32_cst_wgt[3][15] = {
        { 19, 17, 18, 17, 13, 8, 14, 10, 13, 8, 14, 10, 17, 17, 17 },
        { 20, 18, 19, 18, 12, 6, 13, 9, 12, 6, 13, 9, 18, 17, 17 },
        { 22, 19, 20, 19, 11, 5, 12, 8, 11, 5, 12, 8, 19, 18, 18 }
    };

    static RK_U32 pre_i16_cst_thd[3][15] = {
        { 4, 14, 4, 4, 3, 5, 6, 4, 9, 4, 6, 4, 6, 4, 5 },
        { 4, 14, 4, 4, 3, 5, 6, 4, 9, 4, 6, 4, 6, 4, 5 },
        { 6, 18, 6, 4, 4, 7, 8, 6, 12, 6, 8, 6, 8, 6, 5 }
    };

    static RK_U32 pre_i16_cst_wgt[3][15] = {
        { 19, 17, 18, 17, 13, 8, 14, 10, 13, 8, 14, 10, 17, 17, 17 },
        { 20, 18, 19, 18, 12, 6, 13, 9, 12, 6, 13, 9, 18, 17, 17 },
        { 22, 19, 18, 19, 11, 5, 12, 8, 11, 5, 12, 8, 19, 18, 18 }
    };

    H265eV511RegSet *regs = ctx->regs;
    HevcVepu511Sqi *s = &regs->reg_sqi;
    RK_U32 str = ctx->cfg->tune.atl_str;
    RK_U32 str_idx = 0;
    RK_U32 idx = 0;

    s->pre_i32_cst_wgt3.anti_strp_e = !!ctx->cfg->tune.atl_str;

    if (str == 0)
        return;

    s->pre_i32_cst_wgt3.i32_lambda_mv_bit = 4;
    s->pre_i32_cst_wgt3.i16_lambda_mv_bit = 3;
    s->pre_i16_cst_wgt3.i8_lambda_mv_bit = 3;
    s->pre_i16_cst_wgt3.i4_lambda_mv_bit = 3;

    /* 0 - disable; 1 - weak; 2 - medium; 3 - strong */
    str_idx = str - 1;
    for (idx = 0; idx < MPP_ARRAY_ELEMS(s->pre_i32_cst_thd); idx++)
        s->pre_i32_cst_thd[idx] = pre_i32_cst_thd[str_idx][idx];
    s->pre_i32_cst_thd3.mode_th     = pre_i32_cst_thd[str_idx][14];
    s->pre_i32_cst_thd3.qp_thd  = pre_i32_cst_thd[str_idx][15];

    for (idx = 0; idx < MPP_ARRAY_ELEMS(s->pre_i32_cst_wgt); idx++)
        s->pre_i32_cst_wgt[idx] = pre_i32_cst_wgt[str_idx][idx];

    for (idx = 0; idx < MPP_ARRAY_ELEMS(s->pre_i16_cst_thd); idx++)
        s->pre_i16_cst_thd[idx] = pre_i16_cst_thd[str_idx][idx];
    s->pre_i16_cst_thd3.mode_th     = pre_i16_cst_thd[str_idx][14];

    for (idx = 0; idx < MPP_ARRAY_ELEMS(s->pre_i16_cst_wgt); idx++)
        s->pre_i16_cst_wgt[idx] = pre_i16_cst_wgt[str_idx][idx];
}

static void vepu511_h265_set_atr_regs(H265eV511HalContext *ctx)
{
    H265eV511RegSet *regs = ctx->regs;
    HevcVepu511Sqi *s = &regs->reg_sqi;
    RK_U32 str = ctx->frame_type == INTRA_FRAME ?
                 ctx->cfg->tune.atr_str_i : ctx->cfg->tune.atr_str_p;

    /* 0 - disable; 1 - weak; 2 - medium; 3 - strong */
    if (str == 0) {
        s->block_opt_cfg.block_en = 0; /* block_en and cmplx_en are not used so far(20240708) */
        s->cmplx_opt_cfg.cmplx_en = 0;
        s->line_opt_cfg.line_en = 0;
    } else {
        s->block_opt_cfg.block_en = 0;
        s->cmplx_opt_cfg.cmplx_en = 0;
        s->line_opt_cfg.line_en = 1;
    }

    if (str == 3) {
        s->block_opt_cfg.block_thre_cst_best_mad      = 1000;
        s->block_opt_cfg.block_thre_cst_best_grdn_blk = 39;
        s->block_opt_cfg.thre_num_grdnt_point_cmplx   = 3;
        s->block_opt_cfg.block_delta_qp_flag          = 3;

        s->cmplx_opt_cfg.cmplx_thre_cst_best_mad_dep0 = 4000;
        s->cmplx_opt_cfg.cmplx_thre_cst_best_mad_dep1 = 2000;

        s->cmplx_bst_mad_thd.cmplx_thre_cst_best_mad_dep2       = 200;
        s->cmplx_bst_mad_thd.cmplx_thre_cst_best_grdn_blk_dep0  = 977;

        s->cmplx_bst_grdn_thd.cmplx_thre_cst_best_grdn_blk_dep1 = 0;
        s->cmplx_bst_grdn_thd.cmplx_thre_cst_best_grdn_blk_dep2 = 488;

        s->line_opt_cfg.line_thre_min_cst_best_grdn_blk_dep0 = 4;
        s->line_opt_cfg.line_thre_min_cst_best_grdn_blk_dep1 = 255;
        s->line_opt_cfg.line_thre_min_cst_best_grdn_blk_dep2 = 255;
        s->line_opt_cfg.line_thre_ratio_best_grdn_blk_dep0   = 7;
        s->line_opt_cfg.line_thre_ratio_best_grdn_blk_dep1   = 7;

        s->line_cst_bst_grdn.line_thre_max_cst_best_grdn_blk_dep0 = 1;
        s->line_cst_bst_grdn.line_thre_max_cst_best_grdn_blk_dep1 = 60;
        s->line_cst_bst_grdn.line_thre_max_cst_best_grdn_blk_dep2 = 60;

        s->subj_opt_dqp0.line_thre_qp   = 25;
        s->subj_opt_dqp0.block_strength = 4;
        s->subj_opt_dqp0.block_thre_qp  = 30;
        s->subj_opt_dqp0.cmplx_strength = 4;
        s->subj_opt_dqp0.cmplx_thre_qp  = 34;
        s->subj_opt_dqp0.cmplx_thre_max_grdn_blk = 32;

        s->subj_opt_rdo_split.line_rdo_split_rcoef_d0 = 11;
        s->subj_opt_rdo_split.line_rdo_split_rcoef_d1 = 12;
    } else if (str == 2) {
        s->block_opt_cfg.block_thre_cst_best_mad      = 1000;
        s->block_opt_cfg.block_thre_cst_best_grdn_blk = 39;
        s->block_opt_cfg.thre_num_grdnt_point_cmplx   = 3;
        s->block_opt_cfg.block_delta_qp_flag          = 3;

        s->cmplx_opt_cfg.cmplx_thre_cst_best_mad_dep0 = 4000;
        s->cmplx_opt_cfg.cmplx_thre_cst_best_mad_dep1 = 2000;

        s->cmplx_bst_mad_thd.cmplx_thre_cst_best_mad_dep2      = 200;
        s->cmplx_bst_mad_thd.cmplx_thre_cst_best_grdn_blk_dep0 = 977;

        s->cmplx_bst_grdn_thd.cmplx_thre_cst_best_grdn_blk_dep1 = 0;
        s->cmplx_bst_grdn_thd.cmplx_thre_cst_best_grdn_blk_dep2 = 488;

        s->line_opt_cfg.line_thre_min_cst_best_grdn_blk_dep0 = 4;
        s->line_opt_cfg.line_thre_min_cst_best_grdn_blk_dep1 = 30;
        s->line_opt_cfg.line_thre_min_cst_best_grdn_blk_dep2 = 30;
        s->line_opt_cfg.line_thre_ratio_best_grdn_blk_dep0   = 7;
        s->line_opt_cfg.line_thre_ratio_best_grdn_blk_dep1   = 6;

        s->line_cst_bst_grdn.line_thre_max_cst_best_grdn_blk_dep0 = 1;
        s->line_cst_bst_grdn.line_thre_max_cst_best_grdn_blk_dep1 = 50;
        s->line_cst_bst_grdn.line_thre_max_cst_best_grdn_blk_dep2 = 50;

        s->subj_opt_dqp0.line_thre_qp            = 30;
        s->subj_opt_dqp0.block_strength          = 4;
        s->subj_opt_dqp0.block_thre_qp           = 30;
        s->subj_opt_dqp0.cmplx_strength          = 4;
        s->subj_opt_dqp0.cmplx_thre_qp           = 34;
        s->subj_opt_dqp0.cmplx_thre_max_grdn_blk = 32;

        s->subj_opt_rdo_split.line_rdo_split_rcoef_d0 = 11;
        s->subj_opt_rdo_split.line_rdo_split_rcoef_d1 = 13;
    } else {
        s->block_opt_cfg.block_thre_cst_best_mad      = 1000;
        s->block_opt_cfg.block_thre_cst_best_grdn_blk = 39;
        s->block_opt_cfg.thre_num_grdnt_point_cmplx   = 3;
        s->block_opt_cfg.block_delta_qp_flag          = 3;

        s->cmplx_opt_cfg.cmplx_thre_cst_best_mad_dep0 = 6000;
        s->cmplx_opt_cfg.cmplx_thre_cst_best_mad_dep1 = 2000;

        s->cmplx_bst_mad_thd.cmplx_thre_cst_best_mad_dep2       = 300;
        s->cmplx_bst_mad_thd.cmplx_thre_cst_best_grdn_blk_dep0  = 1280;

        s->cmplx_bst_grdn_thd.cmplx_thre_cst_best_grdn_blk_dep1 = 0;
        s->cmplx_bst_grdn_thd.cmplx_thre_cst_best_grdn_blk_dep2 = 512;

        s->line_opt_cfg.line_thre_min_cst_best_grdn_blk_dep0 = 3;
        s->line_opt_cfg.line_thre_min_cst_best_grdn_blk_dep1 = 20;
        s->line_opt_cfg.line_thre_min_cst_best_grdn_blk_dep2 = 20;
        s->line_opt_cfg.line_thre_ratio_best_grdn_blk_dep0   = 7;
        s->line_opt_cfg.line_thre_ratio_best_grdn_blk_dep1   = 8;

        s->line_cst_bst_grdn.line_thre_max_cst_best_grdn_blk_dep0 = 1;
        s->line_cst_bst_grdn.line_thre_max_cst_best_grdn_blk_dep1 = 60;
        s->line_cst_bst_grdn.line_thre_max_cst_best_grdn_blk_dep2 = 60;

        s->subj_opt_dqp0.line_thre_qp            = 30;
        s->subj_opt_dqp0.block_strength          = 4;
        s->subj_opt_dqp0.block_thre_qp           = 30;
        s->subj_opt_dqp0.cmplx_strength          = 4;
        s->subj_opt_dqp0.cmplx_thre_qp           = 34;
        s->subj_opt_dqp0.cmplx_thre_max_grdn_blk = 32;

        s->subj_opt_rdo_split.line_rdo_split_rcoef_d0 = 11;
        s->subj_opt_rdo_split.line_rdo_split_rcoef_d1 = 13;
    }
}

static void vepu511_h265_set_smear_regs(H265eV511HalContext *ctx)
{
    H265eV511RegSet *regs = ctx->regs;
    HevcVepu511Sqi *s = &regs->reg_sqi;
    RK_S32 frm_num = ctx->frame_num;
    RK_S32 gop = (ctx->cfg->rc.gop > 0) ? ctx->cfg->rc.gop : 1; //TODO: gop = 0
    RK_U32 cover_num = ctx->last_frame_fb.acc_cover16_num;
    RK_U32 bndry_num = ctx->last_frame_fb.acc_bndry16_num;
    RK_U32 st_ctu_num = ctx->last_frame_fb.st_ctu_num;
    RK_S32 deblur_en = ctx->cfg->tune.deblur_en;
    RK_S32 str = ctx->cfg->tune.deblur_str;
    RK_S16 flag_cover = 0;
    RK_S16 flag_bndry = 0;

    static RK_U8 qp_strength[H265E_SMEAR_STR_NUM] = { 5, 5, 5, 5, 5, 5, 5, 5 };
    static RK_U8 smear_strength[H265E_SMEAR_STR_NUM] = { 3, 3, 3, 3, 3, 3, 3, 3 };
    static RK_U8 bndry_intra_r_dep0[H265E_SMEAR_STR_NUM] = { 24, 24, 24, 24, 24, 24, 24, 24 };
    static RK_U8 bndry_intra_r_dep1[H265E_SMEAR_STR_NUM] = { 24, 24, 24, 24, 24, 24, 24, 24 };

    static RK_U8 flag_cover_thd0[H265E_SMEAR_STR_NUM] = { 12, 13, 13, 17, 12, 13, 13, 17 };
    static RK_U8 flag_cover_thd1[H265E_SMEAR_STR_NUM] = { 61, 70, 70, 90, 61, 70, 70, 90 };
    static RK_U8 flag_bndry_thd0[H265E_SMEAR_STR_NUM] = { 12, 12, 12, 12, 12, 12, 12, 12 };
    static RK_U8 flag_bndry_thd1[H265E_SMEAR_STR_NUM] = { 73, 73, 73, 73, 73, 73, 73, 73 };

    static RK_U8 smear_cfc_en[H265E_SMEAR_STR_NUM] = {0, 0, 1, 1, 0, 0, 1, 1};
    static RK_U8 thre_dsp_mov[H265E_SMEAR_STR_NUM] = {15, 15, 46, 46, 15, 15, 46, 46};
    static RK_U8 thre_madp_mov_dep0[H265E_SMEAR_STR_NUM] = {16, 16, 48, 48, 16, 16, 48, 48};
    static RK_U8 thre_madp_mov_dep1[H265E_SMEAR_STR_NUM] = {18, 18, 50, 50, 18, 18, 50, 50};
    static RK_U8 thre_madp_mov_dep2[H265E_SMEAR_STR_NUM] = {20, 20, 52, 52, 20, 20, 52, 52};
    static RK_U8 thre_madp_stc_cover0[H265E_SMEAR_STR_NUM] = { 20, 22, 22, 22, 20, 22, 22, 30 };
    static RK_U8 thre_madp_stc_cover1[H265E_SMEAR_STR_NUM] = { 20, 22, 22, 22, 20, 22, 22, 30 };

    static RK_S8 flag_cover_wgt[3] = { 1, 0, -3 };
    static RK_S8 flag_bndry_wgt[3] = { 0, 0, 0 };
    static RK_S8 flag_bndry_intra_wgt0[3] = { -12, 0, 12 };
    static RK_S8 flag_bndry_intra_wgt1[3] = { -12, 0, 12 };

    flag_cover = (cover_num * 1000 < flag_cover_thd0[str] * st_ctu_num) ? 0 :
                 (cover_num * 1000 < flag_cover_thd1[str] * st_ctu_num) ? 1 : 2;

    flag_bndry = (bndry_num * 1000 < flag_bndry_thd0[str] * st_ctu_num) ? 0 :
                 (bndry_num * 1000 < flag_bndry_thd1[str] * st_ctu_num) ? 1 : 2;

    /* anti smear */
    s->smear_opt_cfg0.anti_smear_en = deblur_en;
    s->smear_opt_cfg0.smear_strength = (smear_strength[str] > 2) ?
                                       (smear_strength[str] + flag_bndry_wgt[flag_bndry]) : smear_strength[str];

    s->smear_opt_cfg0.thre_mv_inconfor_cime       = 8;
    s->smear_opt_cfg0.thre_mv_confor_cime         = 0;
    s->smear_opt_cfg0.thre_mv_inconfor_cime_gmv   = 8;
    s->smear_opt_cfg0.thre_mv_confor_cime_gmv     = 0;
    s->smear_opt_cfg0.thre_num_mv_confor_cime     = 3;
    s->smear_opt_cfg0.thre_num_mv_confor_cime_gmv = 2;
    s->smear_opt_cfg0.frm_static                  = 1;
    s->smear_opt_cfg0.smear_cfc_en                = smear_cfc_en[str];

    s->smear_opt_cfg0.smear_load_en = ((frm_num % gop == 0) ||
                                       (s->smear_opt_cfg0.frm_static == 0) || (frm_num % gop == 1)) ? 0 : 1;
    s->smear_opt_cfg0.smear_stor_en = ((frm_num % gop == 0) ||
                                       (s->smear_opt_cfg0.frm_static == 0) || (frm_num % gop == gop - 1)) ? 0 : 1;

    s->smear_opt_cfg1.dist0_frm_avg               = 0;
    s->smear_opt_cfg1.thre_dsp_static             = 10;
    s->smear_opt_cfg1.thre_dsp_mov                = thre_dsp_mov[str];
    s->smear_opt_cfg1.thre_dist_mv_confor_cime    = 32;

    s->smear_madp_thd.thre_madp_stc_dep0          = 10;
    s->smear_madp_thd.thre_madp_stc_dep1          = 8;
    s->smear_madp_thd.thre_madp_stc_dep2          = 8;
    s->smear_madp_thd.thre_madp_mov_dep0          = thre_madp_mov_dep0[str];
    s->smear_madp_thd.thre_madp_mov_dep1          = thre_madp_mov_dep1[str];
    s->smear_madp_thd.thre_madp_mov_dep2          = thre_madp_mov_dep2[str];

    s->smear_stat_thd.thre_num_pt_stc_dep0        = 47;
    s->smear_stat_thd.thre_num_pt_stc_dep1        = 11;
    s->smear_stat_thd.thre_num_pt_stc_dep2        = 3;
    s->smear_stat_thd.thre_num_pt_mov_dep0        = 47;
    s->smear_stat_thd.thre_num_pt_mov_dep1        = 11;
    s->smear_stat_thd.thre_num_pt_mov_dep2        = 3;

    s->smear_bmv_dist_thd0.confor_cime_gmv0      = 21;
    s->smear_bmv_dist_thd0.confor_cime_gmv1      = 16;
    s->smear_bmv_dist_thd0.inconfor_cime_gmv0    = 48;
    s->smear_bmv_dist_thd0.inconfor_cime_gmv1    = 34;

    s->smear_bmv_dist_thd1.inconfor_cime_gmv2    = 32;
    s->smear_bmv_dist_thd1.inconfor_cime_gmv3    = 29;
    s->smear_bmv_dist_thd1.inconfor_cime_gmv4    = 27;

    s->smear_min_bndry_gmv.thre_min_num_confor_csu0_bndry_cime_gmv      = 0;
    s->smear_min_bndry_gmv.thre_max_num_confor_csu0_bndry_cime_gmv      = 3;
    s->smear_min_bndry_gmv.thre_min_num_inconfor_csu0_bndry_cime_gmv    = 0;
    s->smear_min_bndry_gmv.thre_max_num_inconfor_csu0_bndry_cime_gmv    = 3;
    s->smear_min_bndry_gmv.thre_split_dep0                              = 2;
    s->smear_min_bndry_gmv.thre_zero_srgn                               = 8;
    s->smear_min_bndry_gmv.madi_thre_dep0                               = 22;
    s->smear_min_bndry_gmv.madi_thre_dep1                               = 18;

    s->smear_madp_cov_thd.thre_madp_stc_cover0    = thre_madp_stc_cover0[str];
    s->smear_madp_cov_thd.thre_madp_stc_cover1    = thre_madp_stc_cover1[str];
    s->smear_madp_cov_thd.thre_madp_mov_cover0    = 12;
    s->smear_madp_cov_thd.thre_madp_mov_cover1    = 12;
    s->smear_madp_cov_thd.smear_qp_strength       = qp_strength[str] +
                                                    flag_cover_wgt[flag_cover];
    s->smear_madp_cov_thd.smear_thre_qp           = 25;

    s->subj_opt_dqp1.bndry_rdo_mode_intra_jcoef_d0   = bndry_intra_r_dep0[str] +
                                                       flag_bndry_intra_wgt0[flag_bndry];
    s->subj_opt_dqp1.bndry_rdo_mode_intra_jcoef_d1   = bndry_intra_r_dep1[str] +
                                                       flag_bndry_intra_wgt1[flag_bndry];

    s->smear_opt_rmd_intra.cover_rmd_mode_intra_jcoef_d0 = 16;
    s->smear_opt_rmd_intra.cover_rmd_mode_intra_jcoef_d1 = 16;
    s->smear_opt_rmd_intra.cover_rdo_mode_intra_jcoef_d0 = 16;
    s->smear_opt_rmd_intra.cover_rdo_mode_intra_jcoef_d1 = 16;

    s->smear_opt_cfg_coef.cfc_rmd_mode_intra_jcoef_d0 = 20;
    s->smear_opt_cfg_coef.cfc_rmd_mode_intra_jcoef_d1 = 20;
    s->smear_opt_cfg_coef.cfc_rdo_mode_intra_jcoef_d0 = 20;
    s->smear_opt_cfg_coef.cfc_rdo_mode_intra_jcoef_d1 = 20;
}

static void vepu511_h265_set_scaling_list(H265eV511HalContext *ctx)
{
    H265eV511RegSet *regs = ctx->regs;
    Vepu511SclCfg *s = &regs->reg_scl_jpgtbl.scl;
    Vepu511SclCfgExt *s_ext = &regs->reg_scl_jpgtbl.scl_ext;
    RK_U8 *p = (RK_U8 *)&s->tu8_intra_y[0];
    RK_U32 scl_lst_sel = regs->reg_frm.reg0232_rdo_cfg.scl_lst_sel;
    RK_U16 *q = (RK_U16 *)&s_ext->tu8_intra_y[0];
    RK_U8 idx;

    hal_h265e_enter();

    if (scl_lst_sel == 1) {
        for (idx = 0; idx < 64; idx++) {
            /* TU8 intra Y/U/V */
            p[idx + 64 * 0] = vepu511_h265_cqm_intra8[63 - idx];
            p[idx + 64 * 1] = vepu511_h265_cqm_intra8[63 - idx];
            p[idx + 64 * 2] = vepu511_h265_cqm_intra8[63 - idx];

            q[idx + 64 * 0] = vepu511_h265_cqm_intra8_q[63 - idx];
            q[idx + 64 * 1] = vepu511_h265_cqm_intra8_q[63 - idx];
            q[idx + 64 * 2] = vepu511_h265_cqm_intra8_q[63 - idx];

            /* TU8 inter Y/U/V */
            p[idx + 64 * 3] = vepu511_h265_cqm_inter8[63 - idx];
            p[idx + 64 * 4] = vepu511_h265_cqm_inter8[63 - idx];
            p[idx + 64 * 5] = vepu511_h265_cqm_inter8[63 - idx];

            q[idx + 64 * 3] = vepu511_h265_cqm_inter8_q[63 - idx];
            q[idx + 64 * 4] = vepu511_h265_cqm_inter8_q[63 - idx];
            q[idx + 64 * 5] = vepu511_h265_cqm_inter8_q[63 - idx];

            /* TU16 intra Y/U/V AC */
            p[idx + 64 * 6] = vepu511_h265_cqm_intra8[63 - idx];
            p[idx + 64 * 7] = vepu511_h265_cqm_intra8[63 - idx];
            p[idx + 64 * 8] = vepu511_h265_cqm_intra8[63 - idx];

            q[idx + 64 * 6] = vepu511_h265_cqm_intra8_q[63 - idx];
            q[idx + 64 * 7] = vepu511_h265_cqm_intra8_q[63 - idx];
            q[idx + 64 * 8] = vepu511_h265_cqm_intra8_q[63 - idx];

            /* TU16 inter Y/U/V AC */
            p[idx + 64 *  9] = vepu511_h265_cqm_inter8[63 - idx];
            p[idx + 64 * 10] = vepu511_h265_cqm_inter8[63 - idx];
            p[idx + 64 * 11] = vepu511_h265_cqm_inter8[63 - idx];

            q[idx + 64 *  9] = vepu511_h265_cqm_inter8_q[63 - idx];
            q[idx + 64 * 10] = vepu511_h265_cqm_inter8_q[63 - idx];
            q[idx + 64 * 11] = vepu511_h265_cqm_inter8_q[63 - idx];

            /* TU32 intra/inter Y AC */
            p[idx + 64 * 12] = vepu511_h265_cqm_intra8[63 - idx];
            p[idx + 64 * 13] = vepu511_h265_cqm_inter8[63 - idx];

            q[idx + 64 * 12] = vepu511_h265_cqm_intra8_q[63 - idx];
            q[idx + 64 * 13] = vepu511_h265_cqm_inter8_q[63 - idx];
        }

        s->tu_dc0.tu16_intra_y_dc = 16;
        s->tu_dc0.tu16_intra_u_dc = 16;
        s->tu_dc0.tu16_intra_v_dc = 16;
        s->tu_dc0.tu16_inter_y_dc = 16;
        s->tu_dc1.tu16_inter_u_dc = 16;
        s->tu_dc1.tu16_inter_v_dc = 16;
        s->tu_dc1.tu32_intra_y_dc = 16;
        s->tu_dc1.tu32_inter_y_dc = 16;

        s_ext->tu_dc0.tu16_intra_y_dc = 4096;
        s_ext->tu_dc0.tu16_intra_u_dc = 4096;
        s_ext->tu_dc1.tu16_intra_v_dc = 4096;
        s_ext->tu_dc1.tu16_inter_y_dc = 4096;
        s_ext->tu_dc2.tu16_inter_u_dc = 4096;
        s_ext->tu_dc2.tu16_inter_v_dc = 4096;
        s_ext->tu_dc3.tu32_intra_y_dc = 4096;
        s_ext->tu_dc3.tu32_inter_y_dc = 4096;
    } else if (scl_lst_sel == 2) {
        //TODO: Update scaling list for (scaling_list_mode == 2)
        kmpp_logi_f("scaling_list_mode 2 is not supported yet\n");
    }

    hal_h265e_leave();
}

static void vepu511_h265_set_aq(H265eV511HalContext *ctx)
{
    MppEncHwCfg *hw = &ctx->cfg->hw;
    H265eV511RegSet *regs = ctx->regs;
    HevcVepu511RcRoi *rc_regs =  &regs->reg_rc_roi;
    RK_U8* thd = (RK_U8*)&rc_regs->aq_tthd0;
    RK_S32 *aq_step, *aq_thd, *aq_rnge;
    RK_U32 i;

    if (ctx->frame_type == INTRA_FRAME) {
        aq_thd = &hw->aq_thrd_i[0];
        aq_step = &hw->aq_step_i[0];
        aq_rnge = &hw->aq_rnge_arr[0];
    } else {
        aq_thd = &hw->aq_thrd_p[0];
        aq_step = &hw->aq_step_p[0];
        aq_rnge = &hw->aq_rnge_arr[5];
    }

    rc_regs->aq_stp0.aq_stp_s0 = aq_step[0] & 0x1f;
    rc_regs->aq_stp0.aq_stp_0t1 = aq_step[1] & 0x1f;
    rc_regs->aq_stp0.aq_stp_1t2 = aq_step[2] & 0x1f;
    rc_regs->aq_stp0.aq_stp_2t3 = aq_step[3] & 0x1f;
    rc_regs->aq_stp0.aq_stp_3t4 = aq_step[4] & 0x1f;
    rc_regs->aq_stp0.aq_stp_4t5 = aq_step[5] & 0x1f;
    rc_regs->aq_stp1.aq_stp_5t6 = aq_step[6] & 0x1f;
    rc_regs->aq_stp1.aq_stp_6t7 = aq_step[7] & 0x1f;
    rc_regs->aq_stp1.aq_stp_7t8 = 0;
    rc_regs->aq_stp1.aq_stp_8t9 = aq_step[8] & 0x1f;
    rc_regs->aq_stp1.aq_stp_9t10 = aq_step[9] & 0x1f;
    rc_regs->aq_stp1.aq_stp_10t11 = aq_step[10] & 0x1f;
    rc_regs->aq_stp2.aq_stp_11t12 = aq_step[11] & 0x1f;
    rc_regs->aq_stp2.aq_stp_12t13 = aq_step[12] & 0x1f;
    rc_regs->aq_stp2.aq_stp_13t14 = aq_step[13] & 0x1f;
    rc_regs->aq_stp2.aq_stp_14t15 = aq_step[14] & 0x1f;
    rc_regs->aq_stp2.aq_stp_b15 = aq_step[15];

    for (i = 0; i < 16; i++)
        thd[i] = aq_thd[i];

    rc_regs->aq_clip.aq16_rnge = aq_rnge[0];
    rc_regs->aq_clip.aq32_rnge = aq_rnge[1];
    rc_regs->aq_clip.aq8_rnge = aq_rnge[2];
    rc_regs->aq_clip.aq16_dif0 = aq_rnge[3];
    rc_regs->aq_clip.aq16_dif1 = aq_rnge[4];

    rc_regs->aq_clip.aq_cme_en = 1;
    rc_regs->aq_clip.aq_subj_cme_en = 0;
    rc_regs->aq_clip.aq_rme_en = 1;
    rc_regs->aq_clip.aq_subj_rme_en = 1;
}

static void vepu511_h265_set_subj_opt_regs(H265eV511HalContext *ctx)
{
    H265eV511RegSet *regs = ctx->regs;
    HevcVepu511Sqi *s = &regs->reg_sqi;

    s->subj_opt_cfg.subj_opt_en = 1;
    s->subj_opt_cfg.subj_opt_strength = 3;
    s->subj_opt_cfg.aq_subj_en = 1;
    s->subj_opt_cfg.aq_subj_strength = 4;
    s->subj_opt_cfg.thre_sum_grdn_point = 1800;
    s->subj_opt_dpth_thd.common_thre_num_grdn_point_dep0   = 64;
    s->subj_opt_dpth_thd.common_thre_num_grdn_point_dep1   = 32;
    s->subj_opt_dpth_thd.common_thre_num_grdn_point_dep2   = 16;

    // Set the RDO split cost coefficient.
    // Priority: smear takes precedence over anti-ring.
    if (s->smear_opt_cfg0.anti_smear_en == 1) {
        s->subj_opt_rdo_split.choose_cu16_split_jcoef = 8;
    } else if (s->line_opt_cfg.line_en == 1) {
        s->subj_opt_rdo_split.choose_cu16_split_jcoef = 8;
    }
}

static MPP_RET setup_vepu511_intra_refresh(H265eV511HalContext *ctx, H265eV511RegSet *regs, RK_U32 refresh_idx)
{
    MPP_RET ret = MPP_OK;
    RK_U32 w = ctx->cfg->prep.width;
    RK_U32 h = ctx->cfg->prep.height;
    HevcVepu511Frame *reg_frm = &regs->reg_frm;
    Vepu511RoiRegion *reg_regions = &regs->reg_rc_roi.roi_cfg.regions[0];
    RK_U32 refresh_num = ctx->cfg->rc.refresh_num;
    RK_U32 roi_x = 0;
    RK_U32 roi_y = 0;
    RK_U32 roi_w = 0;
    RK_U32 roi_h = 0;
    RK_U32 ctu_size = 32;

    if (!ctx->cfg->rc.refresh_en)
        return MPP_ERR_VALUE;

    kmpp_memset(reg_regions, 0, sizeof(Vepu511RoiRegion));

    if (ctx->cfg->rc.refresh_mode == MPP_ENC_RC_INTRA_REFRESH_ROW) {
        roi_x = 0;
        roi_w = w;
        if (refresh_idx > 0) {
            roi_y = refresh_idx * ctu_size * refresh_num;// - ctu_size;
            roi_h = ctu_size * refresh_num;// + ctu_size;
        } else {
            roi_y = refresh_idx * ctu_size * refresh_num;
            roi_h = ctu_size * refresh_num;
        }
        reg_frm->reg0220_me_rnge.cime_srch_dwnh = 4;
    } else if (ctx->cfg->rc.refresh_mode == MPP_ENC_RC_INTRA_REFRESH_COL) {
        roi_y = 0;
        roi_h = h;
        if (refresh_idx > 0) {
            roi_x = refresh_idx * ctu_size * refresh_num - ctu_size;
            roi_w = ctu_size * refresh_num + ctu_size;
        } else {
            roi_x = refresh_idx * ctu_size * refresh_num;
            roi_w = ctu_size * refresh_num;
        }
        reg_frm->reg0220_me_rnge.cime_srch_rgtw = 4;
    }

    reg_regions->roi_pos_lt.roi_lt_x = MPP_ALIGN(roi_x, 16) >> 4;
    reg_regions->roi_pos_lt.roi_lt_y = MPP_ALIGN(roi_y, 16) >> 4;
    reg_regions->roi_pos_rb.roi_rb_x = MPP_ALIGN(roi_x + roi_w, 16) >> 4;
    reg_regions->roi_pos_rb.roi_rb_y = (MPP_ALIGN(roi_y + roi_h, 16) >> 4) - 1;
    reg_regions->roi_base.roi_en = 1;
    reg_regions->roi_base.roi_pri = 0x1f;
    reg_regions->roi_mdc0.h265.roi_mdc_intra16 = 1;
    reg_regions->roi_mdc1.roi_mdc_intra32 = 1;

    return ret;
}

typedef struct SymCtx_t {
    void        *hal;
    HalEncTask  *task;
} SymCtx;

MPP_RET hal_h265e_v511_gen_regs(void *hal, HalEncTask *task)
{
    H265eV511HalContext *ctx = (H265eV511HalContext *)hal;
    HalEncTask *enc_task = task;
    H265eSyntax_new *syn = (H265eSyntax_new *)enc_task->syntax.data;
    H265eV511RegSet *regs = ctx->regs;
    HevcVepu511Frame *reg_frm = &regs->reg_frm;
    EncFrmStatus *frm = &task->rc_task->frm;

    hal_h265e_enter();

    hal_h265e_dbg_simple("frame %d | type %d | start gen regs",
                         ctx->frame_cnt, ctx->frame_type);

    kmpp_memset(regs, 0, sizeof(H265eV511RegSet));

    if (ctx->online)
        vepu511_h265_set_dvbm(ctx, task);

    vepu511_h265_set_normal(ctx);
    vepu511_h265_set_prep(ctx, task);
    vepu511_h265_set_me_regs(ctx, syn);
    vepu511_h265_set_split(regs, ctx->cfg);
    vepu511_h265_set_hw_address(ctx, reg_frm, task);
    vepu511_h265_set_pp_regs(ctx, task);
    vepu511_h265_set_vsp_filtering(ctx, task);
    vepu511_h265_set_rc_regs(ctx, regs, task);
    vepu511_h265_set_rdo_regs(ctx, regs);
    vepu511_h265_set_quant_regs(ctx);
    vepu511_h265_set_sao_regs(ctx);
    vepu511_h265_set_slice_regs(ctx, syn, reg_frm);
    vepu511_h265_set_ref_regs(syn, reg_frm);
    vepu511_h265_set_ext_line_buf(ctx, ctx->regs);
    vepu511_h265_set_atf_regs(ctx);
    vepu511_h265_set_anti_stripe_regs(ctx);
    vepu511_h265_set_atr_regs(ctx);
    vepu511_h265_set_smear_regs(ctx);
    vepu511_h265_set_scaling_list(ctx);
    vepu511_h265_set_aq(ctx);

    if (ctx->sym) {
        SymCtx sym_ctx = { ctx, task };

        kmpp_sym_run(ctx->sym, &sym_ctx, NULL);
    }

    vepu511_h265_set_subj_opt_regs(ctx);

    if (ctx->osd_cfg.osd_data3) {
        vepu511_set_osd(&ctx->osd_cfg);
        ctx->osd_cfg.osd_data3 = NULL;
    }

    if (ctx->roi_data) {
        vepu511_set_roi(&regs->reg_rc_roi.roi_cfg, ctx->roi_data,
                        ctx->cfg->prep.width, ctx->cfg->prep.height);
        ctx->roi_data = NULL;
    }
    /*paramet cfg*/
    vepu511_h265_global_cfg_set(ctx, regs);

    if (frm->is_i_refresh)
        setup_vepu511_intra_refresh(ctx, regs, frm->seq_idx % ctx->cfg->rc.gop);

    /* two pass register patch */
    if (frm->save_pass1)
        vepu511_h265e_save_pass1_patch(regs, ctx);
    if (frm->use_pass1)
        vepu511_h265e_use_pass1_patch(regs, ctx, syn);

    ctx->frame_num++;

    hal_h265e_leave();
    return MPP_OK;
}

MPP_RET hal_h265e_v511_start(void *hal, HalEncTask *enc_task)
{
    MPP_RET ret = MPP_OK;
    H265eV511HalContext *ctx = (H265eV511HalContext *)hal;
    H265eV511RegSet *hw_regs = ctx->regs;
    MppDevRegWrCfg cfg;

    hal_h265e_enter();

    if (enc_task->flags.err) {
        hal_h265e_err("enc_task->flags.err %08x, return early",
                      enc_task->flags.err);
        return MPP_NOK;
    }

#define HAL_H265E_CFG_WR_REG(reg_set, size_set, off_set)                      \
    do {                                                                      \
        cfg.reg = (RK_U32*)&reg_set;                                      \
        cfg.size = size_set;                                              \
        cfg.offset = off_set;                                             \
        ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_REG_WR, &cfg);              \
        if (ret) {                                                        \
            kmpp_loge_f("set %s reg write failed %d\n", #reg_set, ret); \
            return ret;                                               \
        }                                                                 \
    } while(0);

#define HAL_H265E_CFG_RD_REG(reg_set, size_set, off_set)                        \
    do {                                                                        \
        cfg.reg = (RK_U32*)&reg_set;                                            \
        cfg.size = size_set;                                                    \
        cfg.offset = off_set;                                                   \
        ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_REG_RD, &cfg);                    \
        if (ret) {                                                              \
            kmpp_loge_f("set %s reg read failed %d\n", #reg_set, ret);        \
            return ret;                                                     \
        }                                                                       \
    } while(0);

    HAL_H265E_CFG_WR_REG(hw_regs->reg_ctl, sizeof(hw_regs->reg_ctl), VEPU511_CTL_OFFSET);
    HAL_H265E_CFG_WR_REG(hw_regs->reg_frm, sizeof(hw_regs->reg_frm), VEPU511_FRAME_OFFSET);
    HAL_H265E_CFG_WR_REG(hw_regs->reg_rc_roi, sizeof(hw_regs->reg_rc_roi), VEPU511_RC_ROI_OFFSET);
    HAL_H265E_CFG_WR_REG(hw_regs->reg_param, sizeof(hw_regs->reg_param), VEPU511_PARAM_OFFSET);
    HAL_H265E_CFG_WR_REG(hw_regs->reg_sqi, sizeof(hw_regs->reg_sqi), VEPU511_SQI_OFFSET);
    HAL_H265E_CFG_WR_REG(hw_regs->reg_scl_jpgtbl, sizeof(hw_regs->reg_scl_jpgtbl),
                         VEPU511_SCL_JPGTBL_OFFSET);
    HAL_H265E_CFG_WR_REG(hw_regs->reg_osd, sizeof(hw_regs->reg_osd), VEPU511_OSD_OFFSET);

    /* config read regs */
    HAL_H265E_CFG_RD_REG(hw_regs->reg_ctl.int_sta, sizeof(RK_U32), VEPU511_REG_BASE_HW_STATUS);
    HAL_H265E_CFG_RD_REG(hw_regs->reg_st, sizeof(hw_regs->reg_st), VEPU511_STATUS_OFFSET);

    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_CMD_SEND, NULL);
    if (ret)
        kmpp_loge_f("send cmd failed %d\n", ret);
    hal_h265e_leave();
    return ret;
}

static MPP_RET vepu511_h265_set_feedback(H265eV511HalContext *ctx, HalEncTask *enc_task)
{
    EncRcTaskInfo *hal_rc_ret = (EncRcTaskInfo *)&enc_task->rc_task->info;
    vepu511_h265_fbk *fb = &ctx->feedback;
    MppEncCfgSet    *cfg = ctx->cfg;
    RK_S32 mb64_num = ((cfg->prep.width + 63) / 64) * ((cfg->prep.height + 63) / 64);
    RK_S32 mb8_num = (mb64_num << 6);
    RK_S32 mb4_num = (mb8_num << 2);
    H265eV511RegSet *regs = (H265eV511RegSet*)ctx->regs;
    Vepu511Status *status = &regs->reg_st;

    hal_h265e_enter();

    fb->qp_sum += status->qp_sum;

    fb->out_strm_size += status->bs_lgth_l32;

    fb->sse_sum += (RK_S64)(status->sse_h32 << 16) +
                   ((status->st_sse_bsl.sse_l16 >> 16) & 0xffff) ;

    if (regs->reg_ctl.int_sta.vbsf_oflw_sta) {
        hal_h265e_dbg_warning("bit stream overflow");
        return MPP_ERR_INT_BS_OVFL;
    }

    if (regs->reg_ctl.int_sta.enc_err_sta) {
        hal_h265e_dbg_warning("enc err\n");
        return MPP_NOK;
    }

    if (regs->reg_ctl.int_sta.vsrc_err_sta) {
        hal_h265e_dbg_warning("wrap frame error\n");
        return MPP_NOK;
    }

    if (regs->reg_ctl.int_sta.wdg_sta) {
        hal_h265e_dbg_warning("wdg timeout");
        return MPP_NOK;
    }

    fb->st_mb_num += status->st_bnum_b16.num_b16;
    fb->st_lvl64_inter_num += status->st_pnum_p64.pnum_p64;
    fb->st_lvl32_inter_num += status->st_pnum_p32.pnum_p32;
    fb->st_lvl32_intra_num += status->st_pnum_i32.pnum_i32;
    fb->st_lvl16_inter_num += status->st_pnum_p16.pnum_p16;
    fb->st_lvl16_intra_num += status->st_pnum_i16.pnum_i16;
    fb->st_lvl8_inter_num  += status->st_pnum_p8.pnum_p8;
    fb->st_lvl8_intra_num  += status->st_pnum_i8.pnum_i8;
    fb->st_lvl4_intra_num  += status->st_pnum_i4.pnum_i4;
    kmpp_memcpy(&fb->st_cu_num_qp[0], &status->st_b8_qp, 52 * sizeof(RK_U32));

    fb->acc_cover16_num = status->st_cover_sum.acc_cover16_num;
    fb->acc_bndry16_num = status->st_bndry16_sum.acc_bndry16_num;
    fb->acc_zero_mv = status->acc_zero_mv;
    fb->st_ctu_num = status->st_bnum_b16.num_b16;

    if (mb4_num > 0)
        hal_rc_ret->iblk4_prop =  ((((fb->st_lvl4_intra_num + fb->st_lvl8_intra_num) << 2) +
                                    (fb->st_lvl16_intra_num << 4) +
                                    (fb->st_lvl32_intra_num << 6)) << 8) / mb4_num;

    if (mb64_num > 0) {
        hal_rc_ret->quality_real = fb->qp_sum / mb8_num;
    }

    hal_rc_ret->dsp_luma_ave = status->dsp_y_sum / mb4_num;
    hal_h265e_dbg_regs("frame %d dsp_luma_ave %d qp_real %d\n",
                       ctx->frame_num - 1, hal_rc_ret->dsp_luma_ave, hal_rc_ret->quality_real);

    hal_h265e_leave();

    return MPP_OK;
}

static void vepu511_h265e_update_tune_stat(H265eV511HalContext *ctx, HalEncTask *task)
{
    H265eV511RegSet *regs = ctx->regs;
    HevcVepu511RcRoi *s = &regs->reg_rc_roi;
    vepu511_h265_fbk *fb = &ctx->feedback;
    MppEncCfgSet *cfg = ctx->cfg;
    MppEncBaseCfg *base = &cfg->base;
    Vepu511Status *st = &regs->reg_st;
    EncRcTaskInfo *info = (EncRcTaskInfo *)&task->rc_task->info;
    RK_U32 b16_num = MPP_ALIGN(cfg->prep.width, 16) * MPP_ALIGN(cfg->prep.height, 16) / 256;
    RK_U32 madi_cnt = 0, madp_cnt = 0;

    RK_U32 madi_th_cnt0 = st->st_madi_lt_num0.madi_th_lt_cnt0 +
                          st->st_madi_rt_num0.madi_th_rt_cnt0 +
                          st->st_madi_lb_num0.madi_th_lb_cnt0 +
                          st->st_madi_rb_num0.madi_th_rb_cnt0;
    RK_U32 madi_th_cnt1 = st->st_madi_lt_num0.madi_th_lt_cnt1 +
                          st->st_madi_rt_num0.madi_th_rt_cnt1 +
                          st->st_madi_lb_num0.madi_th_lb_cnt1 +
                          st->st_madi_rb_num0.madi_th_rb_cnt1;
    RK_U32 madi_th_cnt2 = st->st_madi_lt_num1.madi_th_lt_cnt2 +
                          st->st_madi_rt_num1.madi_th_rt_cnt2 +
                          st->st_madi_lb_num1.madi_th_lb_cnt2 +
                          st->st_madi_rb_num1.madi_th_rb_cnt2;
    RK_U32 madi_th_cnt3 = st->st_madi_lt_num1.madi_th_lt_cnt3 +
                          st->st_madi_rt_num1.madi_th_rt_cnt3 +
                          st->st_madi_lb_num1.madi_th_lb_cnt3 +
                          st->st_madi_rb_num1.madi_th_rb_cnt3;
    RK_U32 madp_th_cnt0 = st->st_madp_lt_num0.madp_th_lt_cnt0 +
                          st->st_madp_rt_num0.madp_th_rt_cnt0 +
                          st->st_madp_lb_num0.madp_th_lb_cnt0 +
                          st->st_madp_rb_num0.madp_th_rb_cnt0;
    RK_U32 madp_th_cnt1 = st->st_madp_lt_num0.madp_th_lt_cnt1 +
                          st->st_madp_rt_num0.madp_th_rt_cnt1 +
                          st->st_madp_lb_num0.madp_th_lb_cnt1 +
                          st->st_madp_rb_num0.madp_th_rb_cnt1;
    RK_U32 madp_th_cnt2 = st->st_madp_lt_num1.madp_th_lt_cnt2 +
                          st->st_madp_rt_num1.madp_th_rt_cnt2 +
                          st->st_madp_lb_num1.madp_th_lb_cnt2 +
                          st->st_madp_rb_num1.madp_th_rb_cnt2;
    RK_U32 madp_th_cnt3 = st->st_madp_lt_num1.madp_th_lt_cnt3 +
                          st->st_madp_rt_num1.madp_th_rt_cnt3 +
                          st->st_madp_lb_num1.madp_th_lb_cnt3 +
                          st->st_madp_rb_num1.madp_th_rb_cnt3;

    madi_cnt = (6 * madi_th_cnt3 + 5 * madi_th_cnt2 + 4 * madi_th_cnt1) >> 2;
    info->complex_level = (madi_cnt * 100 > 30 * b16_num) ? 2 :
                          (madi_cnt * 100 > 13 * b16_num) ? 1 : 0;

    {
        RK_U32 md_cnt = 0, motion_level = 0;

        if (base->smt1_en || base->smt3_en)
            md_cnt = (12 * madp_th_cnt3 + 11 * madp_th_cnt2 + 8 * madp_th_cnt1) >> 2;
        else
            md_cnt = (24 * madp_th_cnt3 + 22 * madp_th_cnt2 + 17 * madp_th_cnt1) >> 2;

        if (md_cnt * 100 > 15 * b16_num)
            motion_level = 200;
        else if (md_cnt * 100 > 5 * b16_num)
            motion_level = 100;
        else if (md_cnt * 100 > (b16_num >> 2))
            motion_level = 1;
        else
            motion_level = 0;
        info->motion_level = motion_level;
    }
    hal_h265e_dbg_output("frame %d complex_level %d motion_level %d\n",
                         ctx->frame_num - 1, info->complex_level, info->motion_level);

    fb->st_madi = madi_th_cnt0 * s->madi_st_thd.madi_th0 +
                  madi_th_cnt1 * (s->madi_st_thd.madi_th0 + s->madi_st_thd.madi_th1) / 2 +
                  madi_th_cnt2 * (s->madi_st_thd.madi_th1 + s->madi_st_thd.madi_th2) / 2 +
                  madi_th_cnt3 * s->madi_st_thd.madi_th2;

    madi_cnt = madi_th_cnt0 + madi_th_cnt1 + madi_th_cnt2 + madi_th_cnt3;
    if (madi_cnt)
        fb->st_madi = fb->st_madi / madi_cnt;

    fb->st_madp = madp_th_cnt0 * s->madp_st_thd0.madp_th0 +
                  madp_th_cnt1 * (s->madp_st_thd0.madp_th0 + s->madp_st_thd0.madp_th1) / 2 +
                  madp_th_cnt2 * (s->madp_st_thd0.madp_th1 + s->madp_st_thd1.madp_th2) / 2 +
                  madp_th_cnt3 * s->madp_st_thd1.madp_th2;

    madp_cnt = madp_th_cnt0 + madp_th_cnt1 + madp_th_cnt2 + madp_th_cnt3;
    if (madp_cnt)
        fb->st_madp = fb->st_madp / madp_cnt;

    fb->st_mb_num += st->st_bnum_b16.num_b16;
    fb->frame_type = task->rc_task->frm.is_intra ? INTRA_FRAME : INTER_P_FRAME;
    info->bit_real = fb->out_strm_size * 8;
    info->madi = fb->st_madi;
    info->madp = fb->st_madp;

    hal_h265e_dbg_output("frame %d bit_real %d quality_real %d madi %d madp %d\n",
                         ctx->frame_num - 1, info->bit_real, info->quality_real, info->madi, info->madp);
}

//#define DUMP_DATA
MPP_RET hal_h265e_v511_wait(void *hal, HalEncTask *task)
{
    MPP_RET ret = MPP_OK;
    H265eV511HalContext *ctx = (H265eV511HalContext *)hal;
    HalEncTask *enc_task = task;

    hal_h265e_enter();

    if (enc_task->flags.err) {
        hal_h265e_err("enc_task->flags.err %08x, return early",
                      enc_task->flags.err);
        return MPP_NOK;
    }

    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_CMD_POLL, NULL);

#ifdef DUMP_DATA
    static FILE *fp_fbd = NULL;
    static FILE *fp_fbh = NULL;
    static FILE *fp_dws = NULL;
    HalBuf *recon_buf;
    static RK_U32 frm_num = 0;
    H265eSyntax_new *syn = (H265eSyntax_new *)enc_task->syntax.data;
    recon_buf = hal_bufs_get_buf(ctx->dpb_bufs, syn->sp.recon_pic.slot_idx);
    char file_name[20] = "";
    size_t rec_size = mpp_buffer_get_size(recon_buf->buf[0]);
    size_t dws_size = mpp_buffer_get_size(recon_buf->buf[1]);

    void *ptr = mpp_buffer_get_ptr(recon_buf->buf[0]);
    void *dws_ptr = mpp_buffer_get_ptr(recon_buf->buf[1]);

    sprintf(&file_name[0], "fbd%d.bin", frm_num);
    if (fp_fbd != NULL) {
        fclose(fp_fbd);
        fp_fbd = NULL;
    } else
        fp_fbd = fopen(file_name, "wb+");
    if (fp_fbd) {
        fwrite(ptr + ctx->fbc_header_len, 1, rec_size - ctx->fbc_header_len, fp_fbd);
        fflush(fp_fbd);
    }

    sprintf(&file_name[0], "fbh%d.bin", frm_num);

    if (fp_fbh != NULL) {
        fclose(fp_fbh);
        fp_fbh = NULL;
    } else
        fp_fbh = fopen(file_name, "wb+");

    if (fp_fbh) {
        fwrite(ptr, 1, ctx->fbc_header_len, fp_fbh);
        fflush(fp_fbh);
    }


    sprintf(&file_name[0], "dws%d.bin", frm_num);

    if (fp_dws != NULL) {
        fclose(fp_dws);
        fp_dws = NULL;
    } else
        fp_dws = fopen(file_name, "wb+");

    if (fp_dws) {
        fwrite(dws_ptr, 1, dws_size, fp_dws);
        fflush(fp_dws);
    }
    frm_num++;
#endif

    hal_h265e_leave();
    return ret;
}

MPP_RET hal_h265e_v511_get_task(void *hal, HalEncTask *task)
{
    H265eV511HalContext *ctx = (H265eV511HalContext *)hal;
    EncFrmStatus  *frm_status = &task->rc_task->frm;
    KmppShmPtr sptr;

    hal_h265e_enter();

    ctx->online = task->online;

    if (!kmpp_frame_get_meta(task->frame, &sptr)) {
        KmppMeta meta = kmpp_obj_from_shmptr(&sptr);

        kmpp_meta_get_ptr_d(meta, KEY_ROI_DATA, (void**)&ctx->roi_data, NULL);
        kmpp_meta_get_ptr_d(meta, KEY_OSD_DATA3, (void**)&ctx->osd_cfg.osd_data3, NULL);
        /* Set the osd, because rockit needs to release osd buffer. */
        if (ctx->osd_cfg.osd_data3)
            kmpp_meta_set_ptr(meta, KEY_OSD_DATA3, ctx->osd_cfg.osd_data3);
    }

    ctx->frame_type = frm_status->is_intra ? INTRA_FRAME : INTER_P_FRAME;

    if (!task->rc_task->frm.reencode) {
        if (vepu511_h265_setup_hal_bufs(ctx)) {
            hal_h265e_err("vepu541_h265_allocate_buffers failed, free buffers and return\n");
            task->flags.err |= HAL_ENC_TASK_ERR_ALLOC;
            return MPP_ERR_MALLOC;
        }
        ctx->last_frame_fb = ctx->feedback;
    }

    kmpp_memset(&ctx->feedback, 0, sizeof(vepu511_h265_fbk));

    if (!ctx->syms)
        kmpp_syms_get(&ctx->syms, "smt_rc");

    if (ctx->syms && !ctx->sym)
        kmpp_sym_get(&ctx->sym, ctx->syms, "setup_vepu511_h265e_qpmap");

    hal_h265e_leave();
    return MPP_OK;
}

static void vepu511_h265e_update_bitrate_info(H265eV511HalContext *ctx, HalEncTask *task)
{
    vepu511_h265_fbk *fb = &ctx->feedback;
    EncRcTaskInfo *rc_info = &task->rc_task->info;
    RK_S32 bit_tgt = rc_info->bit_target;
    RK_S32 bit_real = rc_info->bit_real;
    RK_S32 real_lvl = 0, i = 0;

    kmpp_memcpy(fb->tgt_sub_real_lvl, ctx->last_frame_fb.tgt_sub_real_lvl, 6 * sizeof(RK_S8));
    for (i = 3; i >= 0; i--)
        fb->tgt_sub_real_lvl[i + 1] = fb->tgt_sub_real_lvl[i];

    if (bit_tgt > bit_real) {
        fb->tgt_sub_real_lvl[0] = (bit_tgt > bit_real * 6 / 4) ? 3 :
                                  (bit_tgt > bit_real * 5 / 4) ? 2 :
                                  (bit_tgt > bit_real * 9 / 8) ? 1 : 0;
    } else {
        fb->tgt_sub_real_lvl[0] = (bit_real > bit_tgt * 2) ? -5 :
                                  (bit_real > bit_tgt * 7 / 4) ? -4 :
                                  (bit_real > bit_tgt * 6 / 4) ? -3 :
                                  (bit_real > bit_tgt * 5 / 4) ? -2 : -1;
    }

    for (i = 0; i < 5; i ++)
        real_lvl += fb->tgt_sub_real_lvl[i];

    if (task->rc_task->frm.is_intra)
        fb->tgt_sub_real_lvl[5] = 0;

    if (real_lvl < -9)
        fb->tgt_sub_real_lvl[5] = 2;
    else if (real_lvl < -2 && fb->tgt_sub_real_lvl[5] < 2)
        fb->tgt_sub_real_lvl[5] = 1;
}

MPP_RET hal_h265e_v511_ret_task(void *hal, HalEncTask *task)
{
    H265eV511HalContext *ctx = (H265eV511HalContext *)hal;
    HalEncTask *enc_task = task;
    vepu511_h265_fbk *fb = &ctx->feedback;
    MPP_RET ret = MPP_OK;

    hal_h265e_enter();

    ret = vepu511_h265_set_feedback(ctx, enc_task);
    if (ret)
        return ret;

    enc_task->hw_length = fb->out_strm_size;
    enc_task->length += fb->out_strm_size;

    vepu511_h265e_update_tune_stat(ctx, task);
    vepu511_h265e_update_bitrate_info(ctx, task);

    hal_h265e_dbg_detail("output stream size %d\n", fb->out_strm_size);

    hal_h265e_leave();
    return MPP_OK;
}

static MPP_RET hal_h265e_v511_comb_start(void *hal, HalEncTask *enc_task,
                                         HalEncTask *jpeg_enc_task)
{
    H265eV511HalContext *ctx = (H265eV511HalContext *) hal;
    H265eV511RegSet *hw_regs = ctx->regs;
    Vepu511JpegCfg jpeg_cfg;

    hal_h265e_enter();
    hw_regs->reg_ctl.dtrns_map.jpeg_bus_edin = 7;
    jpeg_cfg.dev = ctx->dev;
    jpeg_cfg.jpeg_reg_base = &hw_regs->reg_frm.jpeg_frame;
    jpeg_cfg.reg_tab = &hw_regs->reg_scl_jpgtbl.jpg_tbl;
    jpeg_cfg.enc_task = jpeg_enc_task;
    jpeg_cfg.input_fmt = ctx->input_fmt;
    jpeg_cfg.online = ctx->online;
    vepu511_set_jpeg_reg(&jpeg_cfg);

    if (jpeg_enc_task->jpeg_tlb_reg)
        kmpp_memcpy(&hw_regs->reg_scl_jpgtbl.jpg_tbl,
                    jpeg_enc_task->jpeg_tlb_reg, sizeof(Vepu511JpegTable));
    if (jpeg_enc_task->jpeg_osd_reg)
        kmpp_memcpy(&hw_regs->reg_osd.jpeg_osd, jpeg_enc_task->jpeg_osd_reg, sizeof(Vepu511Osd));

    hal_h265e_leave();

    return hal_h265e_v511_start(hal, enc_task);
}


static MPP_RET hal_h265e_v511_ret_comb_task(void *hal, HalEncTask *task, HalEncTask *jpeg_enc_task)
{
    H265eV511HalContext *ctx = (H265eV511HalContext *) hal;
    HalEncTask *enc_task = task;
    H265eV511RegSet *regs = (H265eV511RegSet*)ctx->regs;
    vepu511_h265_fbk *fb = &ctx->feedback;
    EncRcTaskInfo *hal_rc_ret = (EncRcTaskInfo *) &jpeg_enc_task->rc_task->info;
    MPP_RET ret = MPP_OK;

    hal_h265e_enter();
    ret = vepu511_h265_set_feedback(ctx, enc_task);
    if (ret)
        return ret;
    enc_task->hw_length = fb->out_strm_size;
    enc_task->length += fb->out_strm_size;

    if (regs->reg_ctl.int_sta.jbsf_oflw_sta)
        jpeg_enc_task->jpeg_overflow = 1;

    jpeg_enc_task->hw_length += regs->reg_st.jpeg_head_bits_l32;
    hal_rc_ret->bit_real += jpeg_enc_task->hw_length * 8;

    hal_h265e_dbg_detail("output stream size %d\n", fb->out_strm_size);
    hal_h265e_leave();

    return ret;
}

static MPP_RET hal_h265e_v511_init_cfg(void *hal, MppEncCfgSet *cfg, const rk_u8 *preset)
{
    MppEncHwCfg *hw = &cfg->hw;
    MppEncFineTuneCfg *tune = &cfg->tune;
    (void)hal;

    /* setup default hardware config */
    hw->qp_delta_row_i = 2;
    hw->qp_delta_row = 2;
    hw->qbias_i = 171;
    hw->qbias_p = 85;
    hw->qbias_en = 0;
    hw->flt_str_i = 0;
    hw->flt_str_p = 0;

    if (preset && kmpp_strstr(preset, "smart")) {
        kmpp_memcpy(hw->aq_step_i, aq_qp_delta_smt_I, sizeof(hw->aq_step_i));
        kmpp_memcpy(hw->aq_step_p, aq_qp_delta_smt_P, sizeof(hw->aq_step_p));
        kmpp_memcpy(hw->aq_thrd_i, aq_thd_smt_I, sizeof(hw->aq_thrd_i));
        kmpp_memcpy(hw->aq_thrd_p, aq_thd_smt_P, sizeof(hw->aq_thrd_p));
        kmpp_memcpy(hw->aq_rnge_arr, aq_rnge_smt, sizeof(hw->aq_rnge_arr));
    } else {
        kmpp_memcpy(hw->aq_step_i, aq_qp_delta_default, sizeof(hw->aq_step_i));
        kmpp_memcpy(hw->aq_step_p, aq_qp_delta_default, sizeof(hw->aq_step_p));
        kmpp_memcpy(hw->aq_thrd_i, aq_thd_default, sizeof(hw->aq_thrd_i));
        kmpp_memcpy(hw->aq_thrd_p, aq_thd_default, sizeof(hw->aq_thrd_p));
        kmpp_memcpy(hw->aq_rnge_arr, aq_rnge_default, sizeof(hw->aq_rnge_arr));
    }

    if (preset && kmpp_strstr(preset, "ipc")) {
        tune->deblur_str = 5; /* [0, 7] */
        tune->atr_str_i = 1; /* [0, 3] */
        tune->atr_str_p = 1; /* [0, 3] */
        tune->atl_str = 1; /* [0, 3] */
        tune->lambda_idx_i = 4; /* [0, 8] */
        tune->lambda_idx_p = 4; /* [0, 8] */
        tune->atf_str = 2; /* [0, 3] */
    } else {
        tune->deblur_str = 0;
        tune->atr_str_i = 0;
        tune->atr_str_p = 0;
        tune->atl_str = 0;
        tune->lambda_idx_i = 4;
        tune->lambda_idx_p = 4;
        tune->atf_str = 0;
    }
    tune->sao_str_i = 0; /* [0, 4] */
    tune->sao_str_p = 0; /* [0, 4] */

    return MPP_OK;
}

const MppEncHalApi hal_h265e_vepu511 = {
    .name       = "hal_h265e_v511",
    .coding     = MPP_VIDEO_CodingHEVC,
    .type       = VPU_CLIENT_RKVENC,
    .ctx_size   = sizeof(H265eV511HalContext),
    .flag       = 0,
    .init       = hal_h265e_v511_init,
    .deinit     = hal_h265e_v511_deinit,
    .prepare    = hal_h265e_vepu511_prepare,
    .get_task   = hal_h265e_v511_get_task,
    .gen_regs   = hal_h265e_v511_gen_regs,
    .start      = hal_h265e_v511_start,
    .wait       = hal_h265e_v511_wait,
    .part_start = NULL,
    .part_wait  = NULL,
    .ret_task   = hal_h265e_v511_ret_task,
    .comb_start = hal_h265e_v511_comb_start,
    .comb_ret_task = hal_h265e_v511_ret_comb_task,
    .init_cfg   = hal_h265e_v511_init_cfg,
};

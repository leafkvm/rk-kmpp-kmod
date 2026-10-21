/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "hal_h264e_vepu511"

#include "kmpp_string.h"

#include "mpp_mem.h"
#include "kmpp_frame.h"
#include "kmpp_packet.h"
#include "mpp_maths.h"

#include "h264e_sps.h"
#include "h264e_pps.h"
#include "h264e_dpb.h"
#include "h264e_slice.h"

#include "hal_h264e_debug.h"
#include "hal_bufs.h"
#include "mpp_enc_hal.h"
#include "rkv_enc_def.h"
#include "vepu541_common.h"
#include "vepu511_common.h"
#include "hal_h264e_vepu511_reg.h"
#include "hal_h264e_vepu511.h"
#include "kmpp_meta.h"
#include "kmpp_buffer.h"
#include "kmpp_obj.h"
#include "kmpp_sym.h"

#define DUMP_REG 0
#define MAX_TASK_CNT        1
#define VEPU511_MAX_ROI_NUM         8

/* Custom Quant Matrices: Joint Video Team */
static RK_U8 vepu511_h264_cqm_jvt8i[64] = {
    6, 10, 13, 16, 18, 23, 25, 27,
    10, 11, 16, 18, 23, 25, 27, 29,
    13, 16, 18, 23, 25, 27, 29, 31,
    16, 18, 23, 25, 27, 29, 31, 33,
    18, 23, 25, 27, 29, 31, 33, 36,
    23, 25, 27, 29, 31, 33, 36, 38,
    25, 27, 29, 31, 33, 36, 38, 40,
    27, 29, 31, 33, 36, 38, 40, 42
};

static RK_U8 vepu511_h264_cqm_jvt8p[64] = {
    9, 13, 15, 17, 19, 21, 22, 24,
    13, 13, 17, 19, 21, 22, 24, 25,
    15, 17, 19, 21, 22, 24, 25, 27,
    17, 19, 21, 22, 24, 25, 27, 28,
    19, 21, 22, 24, 25, 27, 28, 30,
    21, 22, 24, 25, 27, 28, 30, 32,
    22, 24, 25, 27, 28, 30, 32, 33,
    24, 25, 27, 28, 30, 32, 33, 35
};

/* scale_q = (65536  + scale_iq / 2) / scale_iq */
static RK_U16  vepu511_h264_cqm_jvt8i_q[64] = {
    10923,  6554,  5041,  4096,  3641,  2849,  2621,  2427,
    6554,  5958,  4096,  3641,  2849,  2621,  2427,  2260,
    5041,  4096,  3641,  2849,  2621,  2427,  2260,  2114,
    4096,  3641,  2849,  2621,  2427,  2260,  2114,  1986,
    3641,  2849,  2621,  2427,  2260,  2114,  1986,  1820,
    2849,  2621,  2427,  2260,  2114,  1986,  1820,  1725,
    2621,  2427,  2260,  2114,  1986,  1820,  1725,  1638,
    2427,  2260,  2114,  1986,  1820,  1725,  1638,  1560
};

/* scale_q = (65536 + scale_iq / 2) / scale_iq */
static RK_U16  vepu511_h264_cqm_jvt8p_q[64] = {
    7282,  5041,  4369,  3855,  3449,  3121,  2979,  2731,
    5041,  5041,  3855,  3449,  3121,  2979,  2731,  2621,
    4369,  3855,  3449,  3121,  2979,  2731,  2621,  2427,
    3855,  3449,  3121,  2979,  2731,  2621,  2427,  2341,
    3449,  3121,  2979,  2731,  2621,  2427,  2341,  2185,
    3121,  2979,  2731,  2621,  2427,  2341,  2185,  2048,
    2979,  2731,  2621,  2427,  2341,  2185,  2048,  1986,
    2731,  2621,  2427,  2341,  2185,  2048,  1986,  1872
};

const static RefType ref_type_map[2][2] = {
    /* ref_lt = 0   ref_lt = 1 */
    /* cur_lt = 0 */{ST_REF_TO_ST, ST_REF_TO_LT},
    /* cur_lt = 1 */{LT_REF_TO_ST, LT_REF_TO_LT},
};

typedef struct Vepu511RoiH264BsCfg_t {
    RK_U64 force_inter   : 42;
    RK_U64 mode_mask     : 9;
    RK_U64 reserved      : 10;
    RK_U64 force_intra   : 1;
    RK_U64 qp_adj_en     : 1;
    RK_U64 amv_en        : 1;
} Vepu511RoiH264BsCfg;

typedef struct vepu511_h264_fbk_t {
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
    RK_S8 tgt_sub_real_lvl[6];
} vepu511_h264_fbk;

typedef struct HalH264eVepu511Ctx_t {
    MppEncCfgSet            *cfg;

    MppDev                  dev;
    RK_S32                  frame_cnt;
    RK_U32                  task_cnt;

    /* buffers management */
    HalBufs                 hw_recn;
    RK_S32                  pixel_buf_fbc_hdr_size;
    RK_S32                  pixel_buf_fbc_bdy_size;
    RK_S32                  pixel_buf_fbc_bdy_offset;
    RK_S32                  pixel_buf_size;
    RK_S32                  thumb_buf_size;
    RK_S32                  max_buf_cnt;
    RK_U32          recn_buf_clear;

    /* external line buffer over 4K */
    MppBuffer               ext_line_buf;
    RK_S32                  ext_line_buf_size;

    /* syntax for input from enc_impl */
    RK_U32                  updated;
    H264eSps                *sps;
    H264ePps                *pps;
    H264eDpb                *dpb;
    H264eFrmInfo            *frms;

    /* syntax for output to enc_impl */
    EncRcTaskInfo           hal_rc_cfg;

    /* roi */
    void                    *roi_data;

    /* osd */
    Vepu511OsdCfg           osd_cfg;

    /* two-pass deflicker */
    MppBuffer               buf_pass1;

    /* register */
    HalVepu511RegSet        *regs_set;

    /* frame parallel info */
    RK_S32                  task_idx;
    RK_S32                  curr_idx;
    RK_S32                  prev_idx;
    H264ePrefixNal          *prefix;
    H264eSlice              *slice;

    RK_S32                  online;
    struct hal_shared_buf   *shared_buf;
    RK_S32                  smart_en;      /* deprecated */
    RK_U32                  is_gray;
    RK_U32                  only_smartp;

    /* feedback infos */
    vepu511_h264_fbk    feedback;
    vepu511_h264_fbk    last_frame_fb;

    /* recn and ref buffer offset */
    RK_U32                  recn_ref_wrap;
    MppBuffer               recn_ref_buf;
    WrapBufInfo     wrap_infos;

    RK_U32                  smear_size;

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
    RK_U8                   *mv_flag_info; /* b16 */
    RK_U8                   *mv_flag;
    RK_S32                  b16_flag_size;

    KmppSyms                syms;
    KmppSym                 sym;
} HalH264eVepu511Ctx;

#define H264E_LAMBDA_TAB_SIZE       (52 * sizeof(RK_U32))

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

static RK_U32 h264e_lambda_cvr[60] = {
    0x00000009, 0x0000000b, 0x0000000e, 0x00000011,
    0x00000016, 0x0000001b, 0x00000022, 0x0000002b,
    0x00000036, 0x00000045, 0x00000056, 0x0000006d,
    0x00000089, 0x000000ad, 0x000000da, 0x00000112,
    0x00000159, 0x000001b3, 0x00000224, 0x000002b3,
    0x00000366, 0x00000449, 0x00000566, 0x000006cd,
    0x00000891, 0x00000acb, 0x00000d9a, 0x000013c1,
    0x000018e4, 0x00001f5c, 0x00002783, 0x000031c8,
    0x00003eb8, 0x00004f06, 0x00006390, 0x00008e14,
    0x0000b302, 0x0000e18a, 0x00011c29, 0x00016605,
    0x0001c313, 0x00027ae1, 0x00031fe6, 0x0003efcf,
    0x0004f5c3, 0x0006e785, 0x0008b2ef, 0x000af5c3,
    0x000f1e7a, 0x00130c7f, 0x00180000, 0x001e3cf4,
    0x002618fe, 0x00300000, 0x003c79e8, 0x004c31fc,
    0x00600000, 0x0078f3d0, 0x009863f8, 0x0c000000,
};

static RK_S32 h264_aq_tthd_default[16] = {
    0,  0,  0,  0,  3,  3,  5,  5,
    8,  8,  15, 15, 20, 25, 25, 25
};

static RK_S32 h264_P_aq_step_default[16] = {
    -8, -7, -6, -5, -4, -3, -2, -1,
    1,  2,  3,  4,  5,  6,  7,  8
};

static RK_S32 h264_I_aq_step_default[16] = {
    -8, -7, -6, -5, -4, -3, -2, -1,
    1,  2,  3,  4,  5,  6,  7,  8
};

static RK_S32 h264_aq_tthd_smart[16] = {
    0,  0,  0,  0,  3,  3,  5,  5,
    8,  8,  15, 15, 20, 25, 28, 28
};

static RK_S32 h264_P_aq_step_smart[16] = {
    -8, -7, -6, -5, -4, -3, -2, -1,
    1,  2,  3,  4,  6,  8,  8,  10
};

static RK_S32 h264_I_aq_step_smart[16] = {
    -8, -7, -6, -5, -4, -3, -2, -1,
    1,  2,  3,  4,  6,  8,  8,  10
};

static MPP_RET hal_h264e_vepu511_deinit(void *hal)
{
    HalH264eVepu511Ctx *p = (HalH264eVepu511Ctx *)hal;

    hal_h264e_dbg_func("enter %p\n", p);

    if (p->dev) {
        mpp_dev_deinit(p->dev);
        p->dev = NULL;
    }

    if (!p->shared_buf->ext_line_buf && p->ext_line_buf) {
        mpp_buffer_put(p->ext_line_buf);
        p->ext_line_buf = NULL;
    }

    if (!p->shared_buf->dpb_bufs && p->hw_recn) {
        hal_bufs_deinit(p->hw_recn);
        p->hw_recn = NULL;
    }

    if (p->buf_pass1) {
        mpp_buffer_put(p->buf_pass1);
        p->buf_pass1 = NULL;
    }

    if (!p->shared_buf->recn_ref_buf && p->recn_ref_buf) {
        mpp_buffer_put(p->recn_ref_buf);
        p->recn_ref_buf = NULL;
    }

    if (p->mv_info)
        mpp_buffer_put(p->mv_info);
    if (p->qpmap)
        mpp_buffer_put(p->qpmap);
    if (p->mv_flag_info)
        mpp_free(p->mv_flag_info);
    if (p->mv_flag)
        mpp_free(p->mv_flag);

    MPP_FREE(p->regs_set);

    if (p->sym) {
        kmpp_sym_put(p->sym);
        p->sym = NULL;
    }

    if (p->syms) {
        kmpp_syms_put(p->syms);
        p->syms = NULL;
    }

    hal_h264e_dbg_func("leave %p\n", p);

    return MPP_OK;
}

static MPP_RET hal_h264e_vepu511_init(void *hal, MppEncHalCfg *cfg)
{
    HalH264eVepu511Ctx *p = (HalH264eVepu511Ctx *)hal;
    MPP_RET ret = MPP_OK;

    hal_h264e_dbg_func("enter %p\n", p);

    p->cfg = cfg->cfg;
    p->online = cfg->online;
    p->recn_ref_wrap = cfg->ref_buf_shared;
    p->shared_buf = cfg->shared_buf;
    p->only_smartp = cfg->only_smartp;
    p->b16_flag_size = 0;
    /* update output to MppEnc */
    p->dev = cfg->dev;
    p->task_cnt = MAX_TASK_CNT;
    kmpp_assert(p->task_cnt && p->task_cnt <= MAX_TASK_CNT);

    p->regs_set = mpp_malloc(HalVepu511RegSet, p->task_cnt);
    if (NULL == p->regs_set) {
        ret = MPP_ERR_MALLOC;
        kmpp_loge_f("init register buffer failed\n");
        goto DONE;
    }

    p->osd_cfg.reg_base = &p->regs_set->reg_osd.osd;
    p->osd_cfg.dev = p->dev;
DONE:
    if (ret)
        hal_h264e_vepu511_deinit(hal);

    hal_h264e_dbg_func("leave %p\n", p);
    return ret;
}

static void get_wrap_buf(HalH264eVepu511Ctx *ctx, RK_S32 max_lt_cnt)
{
    MppEncCfgSet *cfg = ctx->cfg;
    MppEncPrepCfg *prep = &cfg->prep;
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

/*
 * NOTE: recon / refer buffer is FBC data buffer.
 * And FBC data require extra 16 lines space for hardware io.
 */
static void setup_hal_bufs(HalH264eVepu511Ctx *ctx)
{
    MppEncCfgSet *cfg = ctx->cfg;
    MppEncPrepCfg *prep = &cfg->prep;
    RK_S32 alignment_w = 64;
    RK_S32 alignment_h = 16;
    RK_S32 aligned_w = MPP_ALIGN(prep->width,  alignment_w);
    RK_S32 aligned_h = MPP_ALIGN(prep->height, alignment_h);
    RK_S32 pixel_buf_fbc_hdr_size = MPP_ALIGN(aligned_w * (aligned_h + alignment_h) / 64, SZ_4K);
    RK_S32 pixel_buf_fbc_bdy_size = aligned_w * (aligned_h + alignment_h) * 3 / 2;
    RK_S32 pixel_buf_size = pixel_buf_fbc_hdr_size + pixel_buf_fbc_bdy_size;
    RK_S32 thumb_buf_size = MPP_ALIGN(aligned_w * aligned_h / 16, SZ_4K);
    RK_S32 old_max_cnt = ctx->max_buf_cnt;
    RK_S32 new_max_cnt = 2;
    MppEncRefCfg ref_cfg = cfg->ref_cfg;
    RK_S32 max_lt_cnt;
    RK_U32 smear_size = 0, smear_r_size = 0;

    if (ref_cfg) {
        MppEncCpbInfo *info = mpp_enc_ref_cfg_get_cpb_info(ref_cfg);

        if (new_max_cnt < MPP_MAX(new_max_cnt, info->dpb_size + 1))
            new_max_cnt = MPP_MAX(new_max_cnt, info->dpb_size + 1);

        max_lt_cnt = info->max_lt_cnt;
    }

    /* ext line buffer */
    if (aligned_w > SZ_4K) {
        RK_S32 ctu_w = aligned_w / 64;
        RK_S32 ext_line_buf_size = (ctu_w - 55) * 53 * 16;

        if (ctx->shared_buf->ext_line_buf) {
            ctx->ext_line_buf = ctx->shared_buf->ext_line_buf;
        } else {
            if (ctx->ext_line_buf && (ext_line_buf_size > ctx->ext_line_buf_size)) {
                mpp_buffer_put(ctx->ext_line_buf);
                ctx->ext_line_buf = NULL;
            }

            if (NULL == ctx->ext_line_buf)
                mpp_buffer_get(NULL, &ctx->ext_line_buf, ext_line_buf_size);
        }

        ctx->ext_line_buf_size = ext_line_buf_size;
    } else {
        if (ctx->ext_line_buf && !ctx->shared_buf->ext_line_buf) {
            mpp_buffer_put(ctx->ext_line_buf);
            ctx->ext_line_buf = NULL;
        }
        ctx->ext_line_buf_size = 0;
    }

    smear_size = MPP_ALIGN(aligned_w, 1024) / 1024 * MPP_ALIGN(aligned_h, 16) / 16 * 16;
    smear_r_size = MPP_ALIGN(aligned_h, 1024) / 1024 * MPP_ALIGN(aligned_w, 16) / 16 * 16;
    smear_size = MPP_MAX(smear_size, smear_r_size);

    if ((ctx->pixel_buf_fbc_hdr_size != pixel_buf_fbc_hdr_size) ||
        (ctx->pixel_buf_fbc_bdy_size != pixel_buf_fbc_bdy_size) ||
        (ctx->pixel_buf_size != pixel_buf_size) ||
        (ctx->thumb_buf_size != thumb_buf_size) ||
        (ctx->smear_size != smear_size) ||
        (new_max_cnt > old_max_cnt)) {
        size_t sizes[HAL_BUF_TYPE_BUTT] = {0};

        hal_h264e_dbg_detail("frame size %d -> %d max count %d -> %d\n",
                             ctx->pixel_buf_size, pixel_buf_size,
                             old_max_cnt, new_max_cnt);

        new_max_cnt = MPP_MAX(new_max_cnt, old_max_cnt);

        ctx->pixel_buf_fbc_hdr_size = pixel_buf_fbc_hdr_size;
        ctx->pixel_buf_fbc_bdy_size = pixel_buf_fbc_bdy_size;

        if (!ctx->shared_buf->dpb_bufs) {
            if (ctx->hw_recn)
                hal_bufs_deinit(ctx->hw_recn);
            hal_bufs_init(&ctx->hw_recn);
        }

        if (ctx->recn_ref_wrap) {
            sizes[RECREF_TYPE] = 0;
            get_wrap_buf(ctx, max_lt_cnt);
        } else {
            sizes[RECREF_TYPE] = pixel_buf_size;
            ctx->pixel_buf_fbc_bdy_offset = pixel_buf_fbc_hdr_size;
            ctx->pixel_buf_size = pixel_buf_size;
        }

        sizes[THUMB_TYPE] = thumb_buf_size;
        sizes[SMEAR_TYPE] = smear_size;
        if (ctx->shared_buf->dpb_bufs)
            ctx->hw_recn = ctx->shared_buf->dpb_bufs;
        else
            hal_bufs_setup(ctx->hw_recn, new_max_cnt, MPP_ARRAY_ELEMS(sizes), sizes);

        ctx->pixel_buf_size = pixel_buf_size;
        ctx->thumb_buf_size = thumb_buf_size;
        ctx->smear_size = smear_size;
        ctx->max_buf_cnt = new_max_cnt;
        ctx->recn_buf_clear = 1;
    }
}

static MPP_RET hal_h264e_vepu511_prepare(void *hal)
{
    HalH264eVepu511Ctx *ctx = (HalH264eVepu511Ctx *)hal;
    MppEncPrepCfg *prep = &ctx->cfg->prep;
    MppEncBaseCfg *base = &ctx->cfg->base;
    MppEncHwCfg *hw = &ctx->cfg->hw;

    hal_h264e_dbg_func("enter %p\n", hal);

    if (prep->change_res)
        prep->change_res = 0;

    if (base->change_smt_mode) {
        if (base->smt1_en || base->smt3_en) {
            kmpp_memcpy(hw->aq_thrd_i, h264_aq_tthd_smart, sizeof(hw->aq_thrd_i));
            kmpp_memcpy(hw->aq_thrd_p, h264_aq_tthd_smart, sizeof(hw->aq_thrd_p));
            kmpp_memcpy(hw->aq_step_i, h264_I_aq_step_smart, sizeof(hw->aq_step_i));
            kmpp_memcpy(hw->aq_step_p, h264_P_aq_step_smart, sizeof(hw->aq_step_p));
        } else  {
            kmpp_memcpy(hw->aq_thrd_i, h264_aq_tthd_default, sizeof(hw->aq_thrd_i));
            kmpp_memcpy(hw->aq_thrd_p, h264_aq_tthd_default, sizeof(hw->aq_thrd_p));
            kmpp_memcpy(hw->aq_step_i, h264_I_aq_step_default, sizeof(hw->aq_step_i));
            kmpp_memcpy(hw->aq_step_p, h264_P_aq_step_default, sizeof(hw->aq_step_p));
        }
        base->change_smt_mode = 0;
    }

    hal_h264e_dbg_func("leave %p\n", hal);

    return MPP_OK;
}

static RK_U32 update_vepu511_syntax(HalH264eVepu511Ctx *ctx, MppSyntax *syntax)
{
    H264eSyntaxDesc *desc = syntax->data;
    RK_S32 syn_num = syntax->number;
    RK_U32 updated = 0;
    RK_S32 i;

    for (i = 0; i < syn_num; i++, desc++) {
        switch (desc->type) {
        case H264E_SYN_CFG : {
            hal_h264e_dbg_detail("update cfg");
            ctx->cfg = desc->p;
        } break;
        case H264E_SYN_SPS : {
            hal_h264e_dbg_detail("update sps");
            ctx->sps = desc->p;
        } break;
        case H264E_SYN_PPS : {
            hal_h264e_dbg_detail("update pps");
            ctx->pps = desc->p;
        } break;
        case H264E_SYN_DPB : {
            hal_h264e_dbg_detail("update dpb");
            ctx->dpb = desc->p;
        } break;
        case H264E_SYN_SLICE : {
            hal_h264e_dbg_detail("update slice");
            ctx->slice = desc->p;
        } break;
        case H264E_SYN_FRAME : {
            hal_h264e_dbg_detail("update frames");
            ctx->frms = desc->p;
        } break;
        case H264E_SYN_PREFIX : {
            hal_h264e_dbg_detail("update prefix nal");
            ctx->prefix = desc->p;
        } break;
        default : {
            kmpp_logi_f("invalid syntax type %d\n", desc->type);
        } break;
        }

        updated |= SYN_TYPE_FLAG(desc->type);
    }

    return updated;
}

static MPP_RET hal_h264e_vepu511_get_task(void *hal, HalEncTask *task)
{
    HalH264eVepu511Ctx *ctx = (HalH264eVepu511Ctx *) hal;
    RK_U32 updated = update_vepu511_syntax(ctx, &task->syntax);
    EncFrmStatus *frm_status = &task->rc_task->frm;
    KmppShmPtr sptr;

    hal_h264e_dbg_func("enter %p\n", hal);

    ctx->online = task->online;

    if (!kmpp_frame_get_meta(task->frame, &sptr)) {
        KmppMeta meta = kmpp_obj_from_shmptr(&sptr);

        kmpp_meta_get_ptr_d(meta, KEY_ROI_DATA, (void**)&ctx->roi_data, NULL);
        kmpp_meta_get_ptr_d(meta, KEY_OSD_DATA3, (void**)&ctx->osd_cfg.osd_data3, NULL);
        /* Set the osd, because rockit needs to release osd buffer. */
        if (ctx->osd_cfg.osd_data3)
            kmpp_meta_set_ptr(meta, KEY_OSD_DATA3, ctx->osd_cfg.osd_data3);
    }

    if (!frm_status->reencode) {
        if (updated & SYN_TYPE_FLAG(H264E_SYN_CFG))
            setup_hal_bufs(ctx);
        ctx->last_frame_fb = ctx->feedback;
    }

    if (!ctx->syms)
        kmpp_syms_get(&ctx->syms, "smt_rc");

    if (ctx->syms && !ctx->sym)
        kmpp_sym_get(&ctx->sym, ctx->syms, "setup_vepu511_h264e_qpmap");

    hal_h264e_dbg_func("leave %p\n", hal);

    return MPP_OK;
}

static void setup_vepu511_normal(HalVepu511RegSet *regs)
{
    hal_h264e_dbg_func("enter\n");

    /* reg001 ENC_STRT */
    regs->reg_ctl.enc_strt.lkt_num           = 0;
    regs->reg_ctl.enc_strt.vepu_cmd          = 1;

    regs->reg_ctl.opt_strg.cke               = 1;
    regs->reg_ctl.opt_strg.sram_ckg_en       = 1;
    regs->reg_ctl.opt_strg.resetn_hw_en      = 1;
    regs->reg_ctl.opt_strg.rfpr_err_e        = 1;

    /* reg002 ENC_CLR */
    regs->reg_ctl.enc_clr.safe_clr           = 0;
    regs->reg_ctl.enc_clr.force_clr          = 0;
    regs->reg_ctl.enc_clr.dvbm_clr           = 0;

    /* reg003 LKT_ADDR */
    // regs->reg_ctl.lkt_addr           = 0;

    /* reg004 INT_EN */
    regs->reg_ctl.int_en.enc_done_en        = 1;
    regs->reg_ctl.int_en.lkt_node_done_en   = 1;
    regs->reg_ctl.int_en.sclr_done_en       = 1;
    regs->reg_ctl.int_en.vslc_done_en       = 1;
    regs->reg_ctl.int_en.vbsf_oflw_en       = 1;
    regs->reg_ctl.int_en.vbuf_lens_en       = 1;
    regs->reg_ctl.int_en.enc_err_en         = 1;

    regs->reg_ctl.int_en.wdg_en             = 1;
    regs->reg_ctl.int_en.vsrc_err_en        = 1;
    regs->reg_ctl.int_en.wdg_en             = 1;
    regs->reg_ctl.int_en.lkt_err_int_en     = 1;
    regs->reg_ctl.int_en.lkt_err_stop_en    = 1;
    regs->reg_ctl.int_en.lkt_force_stop_en  = 1;
    regs->reg_ctl.int_en.jslc_done_en       = 1;
    regs->reg_ctl.int_en.jbsf_oflw_en       = 1;
    regs->reg_ctl.int_en.jbuf_lens_en       = 1;
    regs->reg_ctl.int_en.dvbm_err_en        = 0;

    /* reg005 INT_MSK */
    regs->reg_ctl.int_msk.enc_done_msk        = 0;
    regs->reg_ctl.int_msk.lkt_node_done_msk   = 0;
    regs->reg_ctl.int_msk.sclr_done_msk       = 0;
    regs->reg_ctl.int_msk.vslc_done_msk       = 0;
    regs->reg_ctl.int_msk.vbsf_oflw_msk       = 0;
    regs->reg_ctl.int_msk.vbuf_lens_msk       = 0;
    regs->reg_ctl.int_msk.enc_err_msk         = 0;
    regs->reg_ctl.int_msk.vsrc_err_msk        = 0;
    regs->reg_ctl.int_msk.wdg_msk             = 0;
    regs->reg_ctl.int_msk.lkt_err_int_msk     = 0;
    regs->reg_ctl.int_msk.lkt_err_stop_msk    = 0;
    regs->reg_ctl.int_msk.lkt_force_stop_msk  = 0;
    regs->reg_ctl.int_msk.jslc_done_msk       = 0;
    regs->reg_ctl.int_msk.jbsf_oflw_msk       = 0;
    regs->reg_ctl.int_msk.jbuf_lens_msk       = 0;
    regs->reg_ctl.int_msk.dvbm_err_msk        = 0;

    /* reg006 INT_CLR is not set */
    /* reg007 INT_STA is read only */
    /* reg008 ~ reg0011 gap */
    regs->reg_ctl.enc_wdg.vs_load_thd       = 0;
    // regs->reg_ctl.enc_wdg.vs_load_thd       = 0xffffff;

    /* reg015 DTRNS_MAP */
    regs->reg_ctl.dtrns_map.jpeg_bus_edin      = 0;
    regs->reg_ctl.dtrns_map.src_bus_edin       = 0;
    regs->reg_ctl.dtrns_map.meiw_bus_edin      = 0;
    regs->reg_ctl.dtrns_map.bsw_bus_edin       = 7;
    // regs->reg_ctl.dtrns_map.lktr_bus_edin      = 0;
    // regs->reg_ctl.dtrns_map.roir_bus_edin      = 0;
    regs->reg_ctl.dtrns_map.lktw_bus_edin      = 0;
    regs->reg_ctl.dtrns_map.rec_nfbc_bus_edin  = 0;

    regs->reg_ctl.dtrns_cfg.axi_brsp_cke   = 0;
    /* enable rdo clk gating */
    {
        RK_U32 *rdo_ckg = (RK_U32*)&regs->reg_ctl.rdo_ckg_h264;

        *rdo_ckg = 0xffffffff;
    }
    hal_h264e_dbg_func("leave\n");
}

static MPP_RET setup_vepu511_prep(HalVepu511RegSet *regs, MppEncPrepCfg *prep,
                                  HalEncTask *task)
{
    VepuFmtCfg cfg;
    RK_U32 fmt = prep->format;
    MPP_RET ret = vepu5xx_set_fmt(&cfg, fmt);
    RK_U32 hw_fmt = cfg.format;
    RK_S32 y_stride;
    RK_S32 c_stride;
    RK_U32 offset_x, offset_y;
    RK_S32 hor_stride;

    hal_h264e_dbg_func("enter\n");

    /* do nothing when color format is not supported */
    if (ret)
        return ret;

    regs->reg_frm.enc_rsl.pic_wd8_m1 = MPP_ALIGN(prep->width, 16) / 8 - 1;
    regs->reg_frm.src_fill.pic_wfill = MPP_ALIGN(prep->width, 16) - prep->width;
    regs->reg_frm.enc_rsl.pic_hd8_m1 = MPP_ALIGN(prep->height, 16) / 8 - 1;
    regs->reg_frm.src_fill.pic_hfill = MPP_ALIGN(prep->height, 16) - prep->height;

    regs->reg_ctl.dtrns_map.src_bus_edin = cfg.src_endian;

    regs->reg_frm.src_fmt.src_cfmt   = hw_fmt;
    regs->reg_frm.src_fmt.alpha_swap = cfg.alpha_swap;
    regs->reg_frm.src_fmt.rbuv_swap  = cfg.rbuv_swap;
    // regs->reg_frm.src_fmt.src_range  = (prep->range == MPP_FRAME_RANGE_JPEG ? 1 : 0);
    regs->reg_frm.src_fmt.out_fmt = ((fmt & MPP_FRAME_FMT_MASK) == MPP_FMT_YUV400 ? 0 : 1);

    kmpp_frame_get_fmt(task->frame, &fmt);
    kmpp_frame_get_hor_stride(task->frame, &hor_stride);
    if (!hor_stride)
        hor_stride = prep->hor_stride;

    if (MPP_FRAME_FMT_IS_FBC(fmt)) {
        regs->reg_frm.src_proc.rkfbcd_en = 1;
        y_stride = MPP_ALIGN(hor_stride, 64) >> 2;
    } else if (hor_stride) {
        y_stride = hor_stride;
    } else {
        if (hw_fmt == VEPU541_FMT_BGRA8888 )
            y_stride = prep->width * 4;
        else if (hw_fmt == VEPU541_FMT_BGR888 )
            y_stride = prep->width * 3;
        else if (hw_fmt == VEPU541_FMT_BGR565 ||
                 hw_fmt == VEPU541_FMT_YUYV422 ||
                 hw_fmt == VEPU541_FMT_UYVY422)
            y_stride = prep->width * 2;
        else
            y_stride = prep->width;
    }

    switch (hw_fmt) {
    case VEPU540C_FMT_YUV444SP : {
        c_stride = y_stride * 2;
    } break;
    case VEPU541_FMT_YUV422SP :
    case VEPU541_FMT_YUV420SP :
    case VEPU540C_FMT_YUV444P : {
        c_stride = y_stride;
    } break;
    default : {
        c_stride = y_stride / 2;
    } break;
    }

    vepu511_get_csc(&cfg, prep->format, prep->color, prep->range);
    regs->reg_frm.src_udfy.csc_wgt_b2y = cfg.weight[0];
    regs->reg_frm.src_udfy.csc_wgt_g2y = cfg.weight[1];
    regs->reg_frm.src_udfy.csc_wgt_r2y = cfg.weight[2];
    regs->reg_frm.src_udfu.csc_wgt_b2u = cfg.weight[3];
    regs->reg_frm.src_udfu.csc_wgt_g2u = cfg.weight[4];
    regs->reg_frm.src_udfu.csc_wgt_r2u = cfg.weight[5];
    regs->reg_frm.src_udfv.csc_wgt_b2v = cfg.weight[6];
    regs->reg_frm.src_udfv.csc_wgt_g2v = cfg.weight[7];
    regs->reg_frm.src_udfv.csc_wgt_r2v = cfg.weight[8];
    regs->reg_frm.src_udfo.csc_ofst_y = cfg.offset[0];
    regs->reg_frm.src_udfo.csc_ofst_u = cfg.offset[1];
    regs->reg_frm.src_udfo.csc_ofst_v = cfg.offset[2];

    regs->reg_frm.src_strd0.src_strd0  = y_stride;
    regs->reg_frm.src_strd1.src_strd1  = c_stride;

    regs->reg_frm.src_proc.src_mirr   = prep->mirroring > 0;
    regs->reg_frm.src_proc.src_rot    = prep->rotation;
    regs->reg_frm.src_proc.tile4x4_en     = 0;

    regs->reg_frm.sli_cfg.mv_v_lmt_thd = 0;
    regs->reg_frm.sli_cfg.mv_v_lmt_en = 0;

    kmpp_frame_get_offset_y(task->frame, &offset_y);
    kmpp_frame_get_offset_x(task->frame, &offset_x);
    regs->reg_frm.pic_ofst.pic_ofst_y = offset_y;
    regs->reg_frm.pic_ofst.pic_ofst_x = offset_x;

    hal_h264e_dbg_func("leave\n");

    return ret;
}

static void setup_vepu511_vsp_filtering(HalH264eVepu511Ctx *ctx, HalEncTask *task)
{
    HalVepu511RegSet *regs = ctx->regs_set;
    Vepu511FrameCfg *s = &regs->reg_frm;
    MppEncCfgSet *cfg = ctx->cfg;
    MppEncHwCfg *hw = &cfg->hw;
    RK_U32 slice_type = ctx->slice->slice_type;
    RK_S32 deblur_en = ctx->cfg->tune.deblur_en;
    RK_S32 deblur_str = ctx->cfg->tune.deblur_str;
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
        if (bit_chg_lvl == 2 && slice_type != H264_I_SLICE)
            corner_str = edge_str = internal_str = 3;
        else if (bit_chg_lvl > 0)
            corner_str = edge_str = internal_str = 2;
    } else {
        if (slice_type == H264_I_SLICE)
            corner_str = edge_str = internal_str = hw->flt_str_i;
        else
            corner_str = edge_str = internal_str = hw->flt_str_p;
    }

    s->src_flt_cfg.pp_corner_filter_strength = corner_str;
    s->src_flt_cfg.pp_edge_filter_strength = edge_str;
    s->src_flt_cfg.pp_internal_filter_strength = internal_str;
}

static MPP_RET vepu511_h264e_save_pass1_patch(HalVepu511RegSet *regs, HalH264eVepu511Ctx *ctx)
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

    regs->reg_frm.enc_pic.cur_frm_ref = 1;
    regs->reg_frm.rfpw_h_addr = mpp_buffer_get_iova(ctx->buf_pass1, ctx->dev);
    regs->reg_frm.rfpw_b_addr = regs->reg_frm.rfpw_h_addr + (width_align * height_align);
    regs->reg_frm.enc_pic.rec_fbc_dis = 1;
    regs->reg_frm.me_ref_comb.rfpw_mode = 0;

    /* NOTE: disable split to avoid lowdelay slice output */
    regs->reg_frm.sli_splt.sli_splt = 0;
    regs->reg_frm.enc_pic.slen_fifo = 0;

    return MPP_OK;
}

static MPP_RET vepu511_h264e_use_pass1_patch(HalVepu511RegSet *regs, HalH264eVepu511Ctx *ctx)
{
    MppEncPrepCfg *prep = &ctx->cfg->prep;
    RK_S32 width_align = MPP_ALIGN(prep->width, 16);
    RK_S32 height_align = MPP_ALIGN(prep->height, 16);
    RK_S32 y_stride = width_align;

    hal_h264e_dbg_func("enter\n");

    regs->reg_frm.enc_pic.rfpr_compress_mode = 1;
    regs->reg_frm.src_fmt.src_cfmt   = VEPU541_FMT_YUV420SP;
    regs->reg_frm.src_fmt.alpha_swap = 0;
    regs->reg_frm.src_fmt.rbuv_swap  = 0;
    regs->reg_frm.src_fmt.out_fmt    = 1;

    regs->reg_frm.src_proc.rkfbcd_en   = 0;
    regs->reg_frm.src_strd0.src_strd0  = y_stride;
    regs->reg_frm.src_strd1.src_strd1  = y_stride;

    regs->reg_frm.src_proc.src_mirr   = 0;
    regs->reg_frm.src_proc.src_rot    = 0;

    regs->reg_frm.pic_ofst.pic_ofst_y = 0;
    regs->reg_frm.pic_ofst.pic_ofst_x = 0;

    regs->reg_frm.adr_src0 = mpp_buffer_get_iova(ctx->buf_pass1, ctx->dev);
    regs->reg_frm.adr_src1 = regs->reg_frm.adr_src0 + (width_align * height_align);
    regs->reg_frm.adr_src2 = 0;

    hal_h264e_dbg_func("leave\n");

    return MPP_OK;
}

static void setup_vepu511_codec(HalVepu511RegSet *regs, H264eSps *sps,
                                H264ePps *pps, H264eSlice *slice)
{
    hal_h264e_dbg_func("enter\n");

    regs->reg_frm.enc_pic.enc_stnd       = 0;
    regs->reg_frm.enc_pic.cur_frm_ref    = slice->nal_reference_idc > 0;
    regs->reg_frm.enc_pic.bs_scp         = 1;

    regs->reg_frm.synt_nal.nal_ref_idc    = slice->nal_reference_idc;
    regs->reg_frm.synt_nal.nal_unit_type  = slice->nalu_type;

    regs->reg_frm.synt_sps.max_fnum       = sps->log2_max_frame_num_minus4;
    regs->reg_frm.synt_sps.drct_8x8       = sps->direct8x8_inference;
    regs->reg_frm.synt_sps.mpoc_lm4       = sps->log2_max_poc_lsb_minus4;
    regs->reg_frm.synt_sps.poc_type       = sps->pic_order_cnt_type;
    regs->reg_frm.synt_pps.etpy_mode      = pps->entropy_coding_mode;
    regs->reg_frm.synt_pps.trns_8x8       = pps->transform_8x8_mode;
    regs->reg_frm.synt_pps.csip_flag      = pps->constrained_intra_pred;
    regs->reg_frm.synt_pps.num_ref0_idx   = pps->num_ref_idx_l0_default_active - 1;
    regs->reg_frm.synt_pps.num_ref1_idx   = pps->num_ref_idx_l1_default_active - 1;
    regs->reg_frm.synt_pps.pic_init_qp    = pps->pic_init_qp;
    regs->reg_frm.synt_pps.cb_ofst        = pps->chroma_qp_index_offset;
    regs->reg_frm.synt_pps.cr_ofst        = pps->second_chroma_qp_index_offset;
    regs->reg_frm.synt_pps.dbf_cp_flg     = pps->deblocking_filter_control;

    regs->reg_frm.synt_sli0.sli_type       = (slice->slice_type == H264_I_SLICE) ? (2) : (0);
    regs->reg_frm.synt_sli0.pps_id         = slice->pic_parameter_set_id;
    regs->reg_frm.synt_sli0.drct_smvp      = 0;
    regs->reg_frm.synt_sli0.num_ref_ovrd   = slice->num_ref_idx_override;
    regs->reg_frm.synt_sli0.cbc_init_idc   = slice->cabac_init_idc;
    regs->reg_frm.synt_sli0.frm_num        = slice->frame_num;

    regs->reg_frm.synt_sli1.idr_pid        = (slice->slice_type == H264_I_SLICE) ? slice->idr_pic_id :
                                             (RK_U32)(-1);
    regs->reg_frm.synt_sli1.poc_lsb        = slice->pic_order_cnt_lsb;

    regs->reg_frm.synt_sli2.dis_dblk_idc   = slice->disable_deblocking_filter_idc;
    regs->reg_frm.synt_sli2.sli_alph_ofst  = slice->slice_alpha_c0_offset_div2;

    h264e_reorder_rd_rewind(slice->reorder);
    {   /* reorder process */
        H264eRplmo rplmo;
        MPP_RET ret = h264e_reorder_rd_op(slice->reorder, &rplmo);

        if (MPP_OK == ret) {
            regs->reg_frm.synt_sli2.ref_list0_rodr = 1;
            regs->reg_frm.synt_sli2.rodr_pic_idx   = rplmo.modification_of_pic_nums_idc;

            switch (rplmo.modification_of_pic_nums_idc) {
            case 0 :
            case 1 : {
                regs->reg_frm.synt_sli2.rodr_pic_num   = rplmo.abs_diff_pic_num_minus1;
            } break;
            case 2 : {
                regs->reg_frm.synt_sli2.rodr_pic_num   = rplmo.long_term_pic_idx;
            } break;
            default : {
                kmpp_loge_f("invalid modification_of_pic_nums_idc %d\n",
                            rplmo.modification_of_pic_nums_idc);
            } break;
            }
        } else {
            // slice->ref_pic_list_modification_flag;
            regs->reg_frm.synt_sli2.ref_list0_rodr = 0;
            regs->reg_frm.synt_sli2.rodr_pic_idx   = 0;
            regs->reg_frm.synt_sli2.rodr_pic_num   = 0;
        }
    }

    /* clear all mmco arg first */
    regs->reg_frm.synt_refm0.nopp_flg               = 0;
    regs->reg_frm.synt_refm0.ltrf_flg               = 0;
    regs->reg_frm.synt_refm0.arpm_flg               = 0;
    regs->reg_frm.synt_refm0.mmco4_pre              = 0;
    regs->reg_frm.synt_refm0.mmco_type0             = 0;
    regs->reg_frm.synt_refm0.mmco_parm0             = 0;
    regs->reg_frm.synt_refm0.mmco_type1             = 0;
    regs->reg_frm.synt_refm1.mmco_parm1             = 0;
    regs->reg_frm.synt_refm0.mmco_type2             = 0;
    regs->reg_frm.synt_refm1.mmco_parm2             = 0;
    regs->reg_frm.synt_refm2.long_term_frame_idx0   = 0;
    regs->reg_frm.synt_refm2.long_term_frame_idx1   = 0;
    regs->reg_frm.synt_refm2.long_term_frame_idx2   = 0;

    h264e_marking_rd_rewind(slice->marking);

    /* only update used parameter */
    if (slice->slice_type == H264_I_SLICE) {
        regs->reg_frm.synt_refm0.nopp_flg       = slice->no_output_of_prior_pics;
        regs->reg_frm.synt_refm0.ltrf_flg       = slice->long_term_reference_flag;
    } else {
        if (!h264e_marking_is_empty(slice->marking)) {
            H264eMmco mmco;

            regs->reg_frm.synt_refm0.arpm_flg       = 1;

            /* max 3 mmco */
            do {
                RK_S32 type = 0;
                RK_S32 param_0 = 0;
                RK_S32 param_1 = 0;

                h264e_marking_rd_op(slice->marking, &mmco);
                type = mmco.mmco;
                switch (type) {
                case 1 : {
                    param_0 = mmco.difference_of_pic_nums_minus1;
                } break;
                case 2 : {
                    param_0 = mmco.long_term_pic_num;
                } break;
                case 3 : {
                    param_0 = mmco.difference_of_pic_nums_minus1;
                    param_1 = mmco.long_term_frame_idx;
                } break;
                case 4 : {
                    param_0 = mmco.max_long_term_frame_idx_plus1;
                } break;
                case 5 : {
                } break;
                case 6 : {
                    param_0 = mmco.long_term_frame_idx;
                } break;
                default : {
                    kmpp_loge_f("unsupported mmco 0 %d\n", type);
                    type = 0;
                } break;
                }

                regs->reg_frm.synt_refm0.mmco_type0 = type;
                regs->reg_frm.synt_refm0.mmco_parm0 = param_0;
                regs->reg_frm.synt_refm2.long_term_frame_idx0 = param_1;

                if (h264e_marking_is_empty(slice->marking))
                    break;

                h264e_marking_rd_op(slice->marking, &mmco);
                type = mmco.mmco;
                param_0 = 0;
                param_1 = 0;
                switch (type) {
                case 1 : {
                    param_0 = mmco.difference_of_pic_nums_minus1;
                } break;
                case 2 : {
                    param_0 = mmco.long_term_pic_num;
                } break;
                case 3 : {
                    param_0 = mmco.difference_of_pic_nums_minus1;
                    param_1 = mmco.long_term_frame_idx;
                } break;
                case 4 : {
                    param_0 = mmco.max_long_term_frame_idx_plus1;
                } break;
                case 5 : {
                } break;
                case 6 : {
                    param_0 = mmco.long_term_frame_idx;
                } break;
                default : {
                    kmpp_loge_f("unsupported mmco 0 %d\n", type);
                    type = 0;
                } break;
                }

                regs->reg_frm.synt_refm0.mmco_type1 = type;
                regs->reg_frm.synt_refm1.mmco_parm1 = param_0;
                regs->reg_frm.synt_refm2.long_term_frame_idx1 = param_1;

                if (h264e_marking_is_empty(slice->marking))
                    break;

                h264e_marking_rd_op(slice->marking, &mmco);
                type = mmco.mmco;
                param_0 = 0;
                param_1 = 0;
                switch (type) {
                case 1 : {
                    param_0 = mmco.difference_of_pic_nums_minus1;
                } break;
                case 2 : {
                    param_0 = mmco.long_term_pic_num;
                } break;
                case 3 : {
                    param_0 = mmco.difference_of_pic_nums_minus1;
                    param_1 = mmco.long_term_frame_idx;
                } break;
                case 4 : {
                    param_0 = mmco.max_long_term_frame_idx_plus1;
                } break;
                case 5 : {
                } break;
                case 6 : {
                    param_0 = mmco.long_term_frame_idx;
                } break;
                default : {
                    kmpp_loge_f("unsupported mmco 0 %d\n", type);
                    type = 0;
                } break;
                }

                regs->reg_frm.synt_refm0.mmco_type2 = type;
                regs->reg_frm.synt_refm1.mmco_parm2 = param_0;
                regs->reg_frm.synt_refm2.long_term_frame_idx2 = param_1;
            } while (0);
        }
    }

    hal_h264e_dbg_func("leave\n");
}

static void setup_vepu511_rdo_pred(HalH264eVepu511Ctx *ctx, H264eSps *sps,
                                   H264ePps *pps, H264eSlice *slice)
{
    HalVepu511RegSet *regs = ctx->regs_set;
    MppEncSceneMode sm = ctx->cfg->tune.scene_mode;
    RK_S32 lambda_idx = 0;

    hal_h264e_dbg_func("enter\n");

    /*
     * H264 Mode Mask of Mode Decision.
     * More prediction modes lead to better compression performance but increase computational cycles.
     *
     * Default speed preset configuration to 0.67 PPC, ~40 FPS for 4K resolution at 500MHz:
     * - Set i4/i16 partition RDO numbers to 1 for P-frames and all other CU RDO numbers to 2.
     * - Set cime_fuse = 0,  enable dual-window search for higher compression performance.
     * - Set fme_lvl_mrg = 1, enable FME's depth1 and depth2 joint search,
     *   improves real-time performance but will reduce the compression ratio.
     * - Set cime_srch_lftw/rgtw/uph/dwnh = 12/12/15/15, expand CIME search range degraded real-time performance.
     * - Set rime_prelvl_en = 0, disable RIME pre-level to improve real-time performance.
     */
    switch (ctx->cfg->tune.speed) {
    default :
    case 0 : {
        // FULL RDO MODE, 0.63ppc
        regs->reg_frm.rdo_mark_mode.p16_interp_num = 3;
        regs->reg_frm.rdo_mark_mode.p16t8_rdo_num = 3;
        regs->reg_frm.rdo_mark_mode.p16t4_rmd_num = 2;
        regs->reg_frm.rdo_mark_mode.p8_interp_num = 3;
        regs->reg_frm.rdo_mark_mode.p8t8_rdo_num = 2;
        regs->reg_frm.rdo_mark_mode.p8t4_rmd_num = 2;
        regs->reg_frm.rdo_mark_mode.i8_rdo_num = 2;
        if (slice->slice_type == H264_I_SLICE) {
            regs->reg_frm.rdo_mark_mode.iframe_i16_rdo_num = 2;
            regs->reg_frm.rdo_mark_mode.iframe_i4_rdo_num = 2;
        } else {
            regs->reg_frm.rdo_mark_mode.iframe_i16_rdo_num = 1;
            regs->reg_frm.rdo_mark_mode.iframe_i4_rdo_num = 1;
        }
    } break;
    case 1 : {
        // 0.69ppc
        regs->reg_frm.rdo_mark_mode.p16_interp_num = 2;
        regs->reg_frm.rdo_mark_mode.p16t8_rdo_num = 2;
        regs->reg_frm.rdo_mark_mode.p16t4_rmd_num = 2;
        regs->reg_frm.rdo_mark_mode.p8_interp_num = 2;
        regs->reg_frm.rdo_mark_mode.p8t8_rdo_num = 2;
        regs->reg_frm.rdo_mark_mode.p8t4_rmd_num = 2;
        regs->reg_frm.rdo_mark_mode.i8_rdo_num = 2;
        regs->reg_frm.rdo_mark_mode.iframe_i4_rdo_num = 1;
        if (slice->slice_type == H264_I_SLICE) {
            regs->reg_frm.rdo_mark_mode.iframe_i16_rdo_num = 2;
        } else {
            regs->reg_frm.rdo_mark_mode.iframe_i16_rdo_num = 1;
        }
    } break;
    case 2 : {
        // 0.71
        regs->reg_frm.rdo_mark_mode.p16_interp_num = 2;
        regs->reg_frm.rdo_mark_mode.p16t8_rdo_num = 2;
        regs->reg_frm.rdo_mark_mode.p16t4_rmd_num = 1;
        regs->reg_frm.rdo_mark_mode.p8_interp_num = 2;
        regs->reg_frm.rdo_mark_mode.p8t8_rdo_num = 1;
        regs->reg_frm.rdo_mark_mode.p8t4_rmd_num = 1;
        regs->reg_frm.rdo_mark_mode.iframe_i4_rdo_num = 1;
        if (slice->slice_type == H264_I_SLICE) {
            regs->reg_frm.rdo_mark_mode.iframe_i16_rdo_num = 2;
            regs->reg_frm.rdo_mark_mode.i8_rdo_num = 2;
        } else {
            regs->reg_frm.rdo_mark_mode.iframe_i16_rdo_num = 1;
            regs->reg_frm.rdo_mark_mode.i8_rdo_num = 1;
        }
    } break;
    case 3 : {
        // 0.73ppc
        regs->reg_frm.rdo_mark_mode.p16_interp_num = 2;
        regs->reg_frm.rdo_mark_mode.p16t8_rdo_num = 1;
        regs->reg_frm.rdo_mark_mode.p16t4_rmd_num = 1;
        regs->reg_frm.rdo_mark_mode.p8_interp_num = 2;
        regs->reg_frm.rdo_mark_mode.p8t8_rdo_num = 1;
        regs->reg_frm.rdo_mark_mode.p8t4_rmd_num = 1;
        regs->reg_frm.rdo_mark_mode.iframe_i4_rdo_num = 1;
        if (slice->slice_type == H264_I_SLICE) {
            regs->reg_frm.rdo_mark_mode.iframe_i16_rdo_num = 2;
            regs->reg_frm.rdo_mark_mode.i8_rdo_num = 2;
        } else {
            regs->reg_frm.rdo_mark_mode.iframe_i16_rdo_num = 1;
            regs->reg_frm.rdo_mark_mode.i8_rdo_num = 1;
        }
    } break;
    }
    if (slice->slice_type == H264_I_SLICE) {
        regs->reg_rc_roi.klut_ofst.chrm_klut_ofst = 6;
        lambda_idx = ctx->cfg->tune.lambda_idx_i;
    } else {
        regs->reg_rc_roi.klut_ofst.chrm_klut_ofst = (sm == MPP_ENC_SCENE_MODE_IPC) ? 9 : 6;
        lambda_idx = ctx->cfg->tune.lambda_idx_p;
    }

    if (sm == MPP_ENC_SCENE_MODE_IPC || sm == MPP_ENC_SCENE_MODE_IPC_PTZ)
        kmpp_memcpy(regs->reg_param.rdo_wgta_qp_grpa_0_51, &h264e_lambda_default[lambda_idx],
                    H264E_LAMBDA_TAB_SIZE);
    else
        kmpp_memcpy(regs->reg_param.rdo_wgta_qp_grpa_0_51, &h264e_lambda_cvr[lambda_idx],
                    H264E_LAMBDA_TAB_SIZE);

    regs->reg_frm.rdo_cfg.rect_size      = (sps->profile_idc == H264_PROFILE_BASELINE &&
                                            sps->level_idc <= H264_LEVEL_3_0) ? 1 : 0;
    regs->reg_frm.rdo_cfg.vlc_lmt        = (sps->profile_idc < H264_PROFILE_MAIN) &&
                                           !pps->entropy_coding_mode;

    regs->reg_frm.rdo_cfg.ccwa_e         = 1;
    regs->reg_frm.rdo_cfg.scl_lst_sel    = pps->scaling_list_mode;
    regs->reg_frm.rdo_cfg.atf_e          = ctx->cfg->tune.atf_str > 0;
    regs->reg_frm.rdo_cfg.atr_e          = ctx->cfg->tune.atr_str_i > 0;
    regs->reg_frm.rdo_cfg.atr_mult_sel_e = 1;
    regs->reg_frm.rdo_mark_mode.rdo_mark_mode = 0;

    hal_h264e_dbg_func("leave\n");
}

static void setup_vepu511_rc_base(HalH264eVepu511Ctx *ctx, HalVepu511RegSet *regs,
                                  H264eSps *sps, H264eSlice *slice, MppEncHwCfg *hw,
                                  EncRcTask *rc_task)
{
    EncRcTaskInfo *rc_info = &rc_task->info;
    MppEncRcCfg *rc = &ctx->cfg->rc;
    RK_S32 mb_w = sps->pic_width_in_mbs;
    RK_S32 mb_h = sps->pic_height_in_mbs;
    RK_U32 qp_target = rc_info->quality_target;
    RK_U32 qp_min = rc_info->quality_min;
    RK_U32 qp_max = rc_info->quality_max;
    RK_S32 mb_target_bits_mul_16 = (rc_info->bit_target << 4) / (mb_w * mb_h);
    MppEncBaseCfg *base = &ctx->cfg->base;
    RK_S32 mb_target_bits;
    RK_S32 negative_bits_thd;
    RK_S32 positive_bits_thd;

    hal_h264e_dbg_func("enter\n");
    hal_h264e_dbg_rc("bittarget %d qp [%d %d %d]\n", rc_info->bit_target,
                     qp_min, qp_target, qp_max);

    if (mb_target_bits_mul_16 >= 0x100000)
        mb_target_bits_mul_16 = 0x50000;

    mb_target_bits = (mb_target_bits_mul_16 * mb_w) >> 4;
    negative_bits_thd = 0 - 5 * mb_target_bits / 16;
    positive_bits_thd = 5 * mb_target_bits / 16;

    regs->reg_frm.enc_pic.pic_qp         = qp_target;
    regs->reg_frm.rc_cfg.rc_en          = 1;
    regs->reg_frm.rc_cfg.aq_en          = 1;
    regs->reg_frm.rc_cfg.rc_ctu_num     = mb_w;
    regs->reg_frm.rc_qp.rc_max_qp      = qp_max;
    regs->reg_frm.rc_qp.rc_min_qp      = qp_min;
    regs->reg_frm.rc_tgt.ctu_ebit      = mb_target_bits_mul_16;

    if ((base->smt1_en || base->smt3_en) &&
        (rc->rc_mode == MPP_ENC_RC_MODE_SMTRC || rc->rc_mode == MPP_ENC_RC_MODE_SE))
        regs->reg_frm.rc_qp.rc_qp_range = 0;
    else
        regs->reg_frm.rc_qp.rc_qp_range = (slice->slice_type == H264_I_SLICE) ?
                                          hw->qp_delta_row_i : hw->qp_delta_row;

    {
        RK_S32 fqp_min, fqp_max;

        if (slice->slice_type == H264_I_SLICE) {
            fqp_min = rc->fqp_min_i;
            fqp_max = rc->fqp_max_i;
        } else {
            fqp_min = rc->fqp_min_p;
            fqp_max = rc->fqp_max_p;
        }

        if (fqp_min == fqp_max) {
            regs->reg_frm.enc_pic.pic_qp = fqp_min;
            regs->reg_frm.rc_qp.rc_qp_range = 0;
        }
    }

    regs->reg_rc_roi.rc_adj0.qp_adj0        = -2;
    regs->reg_rc_roi.rc_adj0.qp_adj1        = -1;
    regs->reg_rc_roi.rc_adj0.qp_adj2        = 0;
    regs->reg_rc_roi.rc_adj0.qp_adj3        = 1;
    regs->reg_rc_roi.rc_adj0.qp_adj4        = 2;
    regs->reg_rc_roi.rc_adj1.qp_adj5        = 0;
    regs->reg_rc_roi.rc_adj1.qp_adj6        = 0;
    regs->reg_rc_roi.rc_adj1.qp_adj7        = 0;
    regs->reg_rc_roi.rc_adj1.qp_adj8        = 0;

    regs->reg_rc_roi.rc_dthd_0_8[0] = 4 * negative_bits_thd;
    regs->reg_rc_roi.rc_dthd_0_8[1] = negative_bits_thd;
    regs->reg_rc_roi.rc_dthd_0_8[2] = positive_bits_thd;
    regs->reg_rc_roi.rc_dthd_0_8[3] = 4 * positive_bits_thd;
    regs->reg_rc_roi.rc_dthd_0_8[4] = 0x7FFFFFFF;
    regs->reg_rc_roi.rc_dthd_0_8[5] = 0x7FFFFFFF;
    regs->reg_rc_roi.rc_dthd_0_8[6] = 0x7FFFFFFF;
    regs->reg_rc_roi.rc_dthd_0_8[7] = 0x7FFFFFFF;
    regs->reg_rc_roi.rc_dthd_0_8[8] = 0x7FFFFFFF;

    regs->reg_rc_roi.roi_qthd0.qpmin_area0    = qp_min;
    regs->reg_rc_roi.roi_qthd0.qpmax_area0    = qp_max;
    regs->reg_rc_roi.roi_qthd0.qpmin_area1    = qp_min;
    regs->reg_rc_roi.roi_qthd0.qpmax_area1    = qp_max;
    regs->reg_rc_roi.roi_qthd0.qpmin_area2    = qp_min;

    regs->reg_rc_roi.roi_qthd1.qpmax_area2    = qp_max;
    regs->reg_rc_roi.roi_qthd1.qpmin_area3    = qp_min;
    regs->reg_rc_roi.roi_qthd1.qpmax_area3    = qp_max;
    regs->reg_rc_roi.roi_qthd1.qpmin_area4    = qp_min;
    regs->reg_rc_roi.roi_qthd1.qpmax_area4    = qp_max;

    regs->reg_rc_roi.roi_qthd2.qpmin_area5    = qp_min;
    regs->reg_rc_roi.roi_qthd2.qpmax_area5    = qp_max;
    regs->reg_rc_roi.roi_qthd2.qpmin_area6    = qp_min;
    regs->reg_rc_roi.roi_qthd2.qpmax_area6    = qp_max;
    regs->reg_rc_roi.roi_qthd2.qpmin_area7    = qp_min;

    regs->reg_rc_roi.roi_qthd3.qpmax_area7    = qp_max;

    hal_h264e_dbg_func("leave\n");
}

static void setup_vepu511_io_buf(HalH264eVepu511Ctx *ctx, HalEncTask *task)
{
    HalVepu511RegSet *regs = ctx->regs_set;
    MppDev dev = ctx->dev;
    MppFrame frm = task->frame;
    KmppPacket pkt = task->packet;
    KmppShmPtr sptr;
    KmppBuffer buf_in = NULL;
    RingBuf *buf_out = task->output;
    MppFrameFormat fmt;
    RK_S32 hor_stride, ver_stride;
    RK_U32 off_in[2] = {0};
    RK_U32 off_out;

    hal_h264e_dbg_func("enter\n");

    kmpp_packet_get_length(pkt, &off_out);
    kmpp_frame_get_hor_stride(frm, &hor_stride);
    kmpp_frame_get_ver_stride(frm, &ver_stride);
    kmpp_frame_get_fmt(frm, &fmt);
    kmpp_frame_get_buffer(frm, &sptr);
    buf_in = kmpp_obj_from_shmptr(&sptr);

    if (MPP_FRAME_FMT_IS_FBC(fmt)) {
        kmpp_frame_get_fbc_offset(task->frame, &off_in[0]);
        off_in[1] = off_in[0];
    } else if (MPP_FRAME_FMT_IS_YUV(fmt)) {
        VepuFmtCfg cfg;

        vepu5xx_set_fmt(&cfg, fmt);
        switch (cfg.format) {
        case VEPU541_FMT_BGRA8888 :
        case VEPU541_FMT_BGR888 :
        case VEPU541_FMT_BGR565 : {
            off_in[0] = 0;
            off_in[1] = 0;
        } break;
        case VEPU541_FMT_YUV420SP :
        case VEPU541_FMT_YUV422SP : {
            off_in[0] = hor_stride * ver_stride;
            off_in[1] = hor_stride * ver_stride;
        } break;
        case VEPU541_FMT_YUV422P : {
            off_in[0] = hor_stride * ver_stride;
            off_in[1] = hor_stride * ver_stride * 3 / 2;
        } break;
        case VEPU541_FMT_YUV420P : {
            off_in[0] = hor_stride * ver_stride;
            off_in[1] = hor_stride * ver_stride * 5 / 4;
        } break;
        case VEPU540_FMT_YUV400 :
        case VEPU541_FMT_YUYV422 :
        case VEPU541_FMT_UYVY422 : {
            off_in[0] = 0;
            off_in[1] = 0;
        } break;
        case VEPU540C_FMT_YUV444SP : {
            off_in[0] = hor_stride * ver_stride;
            off_in[1] = hor_stride * ver_stride;
        } break;
        case VEPU540C_FMT_YUV444P : {
            off_in[0] = hor_stride * ver_stride;
            off_in[1] = hor_stride * ver_stride * 2;
        } break;
        case VEPU540C_FMT_BUTT :
        default : {
            off_in[0] = 0;
            off_in[1] = 0;
        } break;
        }
    }

    if (ctx->online) {
        regs->reg_frm.adr_src0 = 0;
        regs->reg_frm.adr_src1 = 0;
        regs->reg_frm.adr_src2 = 0;
    } else {
        RK_U64 iova = 0;

        kmpp_buffer_get_iova_by_device(buf_in, &iova, mpp_get_dev(dev));
        regs->reg_frm.adr_src0 = iova;
        regs->reg_frm.adr_src1 = iova + off_in[0];
        regs->reg_frm.adr_src2 = iova + off_in[1];
    }

    if (buf_out->cir_flag) {
        RK_U32 size = mpp_buffer_get_size(buf_out->buf);
        RK_U32 out_addr = mpp_buffer_get_iova(buf_out->buf, dev);

        regs->reg_frm.bsbb_addr = out_addr;
        regs->reg_frm.bsbs_addr = out_addr + ((buf_out->start_offset + off_out) % size);
        regs->reg_frm.bsbr_addr = out_addr + buf_out->r_pos;
        regs->reg_frm.bsbt_addr = out_addr + size;
    } else {
        RK_U32 out_addr = mpp_buffer_get_iova(buf_out->buf, dev);

        regs->reg_frm.bsbb_addr = out_addr + buf_out->start_offset;
        regs->reg_frm.bsbr_addr = out_addr;
        regs->reg_frm.bsbs_addr = out_addr + off_out;
        regs->reg_frm.bsbt_addr = out_addr + (buf_out->size - 1);
    }

    if (off_out && task->output->buf) {
        task->output->use_len = off_out;
        ring_buf_flush(task->output, 0);
    }

    hal_h264e_dbg_func("leave\n");
}

static void setup_recn_refr_wrap(HalH264eVepu511Ctx *ctx, HalVepu511RegSet *regs)
{
    MppDev dev = ctx->dev;
    H264eFrmInfo *frms = ctx->frms;
    RK_U32 recn_ref_wrap = ctx->recn_ref_wrap;
    RK_U32 ref_iova = 0;
    RK_U32 cur_is_lt = frms->curr_is_lt;
    RK_U32 refr_is_lt = frms->refr_is_lt;
    RK_U32 cur_is_non_ref = frms->curr_is_non_ref;
    RK_U32 rfp_h_bot = 0, rfp_b_bot = 0;
    RK_U32 rfp_h_top = 0, rfp_b_top = 0;
    RK_U32 rfpw_h_off = 0, rfpw_b_off = 0;
    RK_U32 rfpr_h_off = 0, rfpr_b_off = 0;
    WrapInfo *bdy_lt = &ctx->wrap_infos.body_lt;
    WrapInfo *hdr_lt = &ctx->wrap_infos.hdr_lt;
    WrapInfo *bdy = &ctx->wrap_infos.body;
    WrapInfo *hdr = &ctx->wrap_infos.hdr;

    if (recn_ref_wrap)
        ref_iova = mpp_buffer_get_iova(ctx->recn_ref_buf, dev);

    if (frms->curr_is_idr && frms->curr_idx == frms->refr_idx) {
        hal_h264e_dbg_wrap("cur is idr  lt %d\n", cur_is_lt);
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

        hal_h264e_dbg_wrap("ref type %d\n", type);
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
            if (!ctx->only_smartp) {
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

    regs->reg_frm.rfpw_h_addr = ref_iova + rfpw_h_off;
    regs->reg_frm.rfpw_b_addr = ref_iova + rfpw_b_off;

    regs->reg_frm.rfpr_h_addr = ref_iova + rfpr_h_off;
    regs->reg_frm.rfpr_b_addr = ref_iova + rfpr_b_off;

    regs->reg_frm.rfpt_h_addr = ref_iova + rfp_h_top;
    regs->reg_frm.rfpb_h_addr = ref_iova + rfp_h_bot;
    regs->reg_frm.rfpt_b_addr = ref_iova + rfp_b_top;
    regs->reg_frm.rfpb_b_addr = ref_iova + rfp_b_bot;
    regs->reg_frm.enc_pic.cur_frm_ref = !cur_is_non_ref;

    regs->reg_frm.me_ref_comb.rfpw_mode = 1;
    regs->reg_frm.me_ref_comb.rfpr_mode = 1;

    if (recn_ref_wrap) {
        RK_U32 cur_hdr_off;
        RK_U32 cur_bdy_off;

        hal_h264e_dbg_wrap("cur_is_ref %d\n", !cur_is_non_ref);
        hal_h264e_dbg_wrap("hdr[size %d top %d bot %d cur %d pre %d]\n",
                           hdr->size, hdr->top, hdr->bottom, hdr->cur_off, hdr->pre_off);
        hal_h264e_dbg_wrap("bdy [size %d top %d bot %d cur %d pre %d]\n",
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

static void setup_vepu511_recn_refr(HalH264eVepu511Ctx *ctx, HalVepu511RegSet *regs)
{
    MppDev dev = ctx->dev;
    H264eFrmInfo *frms = ctx->frms;
    HalBufs bufs = ctx->hw_recn;
    RK_S32 fbc_hdr_size = ctx->pixel_buf_fbc_hdr_size;
    RK_U32 recn_ref_wrap = ctx->recn_ref_wrap;
    HalBuf *curr = hal_bufs_get_buf(bufs, frms->curr_idx);
    HalBuf *refr = hal_bufs_get_buf(bufs, frms->refr_idx);

    hal_h264e_dbg_func("enter\n");

    if (curr && curr->cnt) {
        MppBuffer buf_thumb = curr->buf[THUMB_TYPE];

        kmpp_assert(buf_thumb);
        regs->reg_frm.dspw_addr = mpp_buffer_get_iova(buf_thumb, dev);
        if (!recn_ref_wrap) {
            MppBuffer buf_pixel = curr->buf[RECREF_TYPE];

            kmpp_assert(buf_pixel);
            regs->reg_frm.rfpw_h_addr = mpp_buffer_get_iova(buf_pixel, dev);
            regs->reg_frm.rfpw_b_addr = regs->reg_frm.rfpw_h_addr + fbc_hdr_size;
        }
    }

    if (refr && refr->cnt) {
        MppBuffer buf_thumb = refr->buf[THUMB_TYPE];

        kmpp_assert(buf_thumb);
        regs->reg_frm.dspr_addr = mpp_buffer_get_iova(buf_thumb, dev);
        if (!recn_ref_wrap) {
            MppBuffer buf_pixel = refr->buf[RECREF_TYPE];

            kmpp_assert(buf_pixel);
            regs->reg_frm.rfpr_h_addr = mpp_buffer_get_iova(buf_pixel, dev);
            regs->reg_frm.rfpr_b_addr = regs->reg_frm.rfpr_h_addr + fbc_hdr_size;
        }
    }

    if (recn_ref_wrap)
        setup_recn_refr_wrap(ctx, regs);
    else {
        regs->reg_frm.rfpt_h_addr = 0xffffffff;
        regs->reg_frm.rfpb_h_addr = 0;
        regs->reg_frm.rfpt_b_addr = 0xffffffff;
        regs->reg_frm.rfpb_b_addr  = 0;
    }

    regs->reg_frm.adr_smear_wr = mpp_buffer_get_iova(curr->buf[SMEAR_TYPE], dev);
    regs->reg_frm.adr_smear_rd = mpp_buffer_get_iova(refr->buf[SMEAR_TYPE], dev);

    hal_h264e_dbg_func("leave\n");
}

static void setup_vepu511_split(HalVepu511RegSet *regs, MppEncCfgSet *enc_cfg)
{
    MppEncSliceSplit *cfg = &enc_cfg->split;

    hal_h264e_dbg_func("enter\n");

    switch (cfg->split_mode) {
    case MPP_ENC_SPLIT_NONE : {
        regs->reg_frm.sli_splt.sli_splt = 0;
        regs->reg_frm.sli_splt.sli_splt_mode = 0;
        regs->reg_frm.sli_splt.sli_splt_cpst = 0;
        regs->reg_frm.sli_splt.sli_max_num_m1 = 0;
        regs->reg_frm.sli_splt.sli_flsh = 0;
        regs->reg_frm.sli_cnum.sli_splt_cnum_m1 = 0;

        regs->reg_frm.sli_byte.sli_splt_byte = 0;
        regs->reg_frm.enc_pic.slen_fifo = 0;
    } break;
    case MPP_ENC_SPLIT_BY_BYTE : {
        regs->reg_frm.sli_splt.sli_splt = 1;
        regs->reg_frm.sli_splt.sli_splt_mode = 0;
        regs->reg_frm.sli_splt.sli_splt_cpst = 0;
        regs->reg_frm.sli_splt.sli_max_num_m1 = 500;
        regs->reg_frm.sli_splt.sli_flsh = 1;
        regs->reg_frm.sli_cnum.sli_splt_cnum_m1 = 0;

        regs->reg_frm.sli_byte.sli_splt_byte = cfg->split_arg;
        regs->reg_frm.enc_pic.slen_fifo = cfg->split_out ? 1 : 0;
        regs->reg_ctl.int_en.vslc_done_en = cfg->split_out ? 1 : 0;
    } break;
    case MPP_ENC_SPLIT_BY_CTU : {
        regs->reg_frm.sli_splt.sli_splt = 1;
        regs->reg_frm.sli_splt.sli_splt_mode = 1;
        regs->reg_frm.sli_splt.sli_splt_cpst = 0;
        regs->reg_frm.sli_splt.sli_max_num_m1 = 500;
        regs->reg_frm.sli_splt.sli_flsh = 1;
        regs->reg_frm.sli_cnum.sli_splt_cnum_m1 = cfg->split_arg - 1;

        regs->reg_frm.sli_byte.sli_splt_byte = 0;
        regs->reg_frm.enc_pic.slen_fifo = cfg->split_out ? 1 : 0;
        regs->reg_ctl.int_en.vslc_done_en = cfg->split_out ? 1 : 0;
    } break;
    default : {
        kmpp_logi_f("invalide slice split mode %d\n", cfg->split_mode);
    } break;
    }

    hal_h264e_dbg_func("leave\n");
}

static void setup_vepu511_me(HalH264eVepu511Ctx *ctx)
{
    HalVepu511RegSet *regs = ctx->regs_set;
    Vepu511FrameCfg *reg_frm = &regs->reg_frm;
    Vepu511Param *reg_param = &regs->reg_param;
    MppEncSceneMode sm = ctx->cfg->tune.scene_mode;

    hal_h264e_dbg_func("enter\n");

    reg_frm->me_rnge.cime_srch_dwnh   = 15;
    reg_frm->me_rnge.cime_srch_uph    = 15;
    reg_frm->me_rnge.cime_srch_rgtw   = 12;
    reg_frm->me_rnge.cime_srch_lftw   = 12;
    reg_frm->me_cfg.rme_srch_h        = 3;
    reg_frm->me_cfg.rme_srch_v        = 3;
    reg_frm->me_cfg.srgn_max_num      = 72;
    reg_frm->me_cfg.cime_dist_thre    = 1024;
    reg_frm->me_cfg.rme_dis           = 0;
    reg_frm->me_cfg.fme_dis           = 0;
    reg_frm->me_rnge.dlt_frm_num      = 0x0;
    reg_frm->me_cach.cime_zero_thre   = 64;

    /* CIME: 0x1760 - 0x176C */
    reg_param->me_sqi_cfg.cime_pmv_num = 1;
    reg_param->me_sqi_cfg.cime_fuse   = 0;
    reg_param->me_sqi_cfg.move_lambda = 0;
    reg_param->me_sqi_cfg.rime_lvl_mrg     = 1;
    reg_param->me_sqi_cfg.rime_prelvl_en   = 3;
    reg_param->me_sqi_cfg.rime_prersu_en   = 0;
    reg_param->me_sqi_cfg.fme_lvl_mrg = 0;
    reg_param->cime_mvd_th.cime_mvd_th0 = 16;
    reg_param->cime_mvd_th.cime_mvd_th1 = 48;
    reg_param->cime_mvd_th.cime_mvd_th2 = 80;
    reg_param->cime_madp_th.cime_madp_th = 16;
    reg_param->cime_madp_th.ratio_consi_cfg = 13;
    reg_param->cime_madp_th.ratio_bmv_dist = 9;
    reg_param->cime_multi.cime_multi0 = 8;
    reg_param->cime_multi.cime_multi1 = 12;
    reg_param->cime_multi.cime_multi2 = 16;
    reg_param->cime_multi.cime_multi3 = 20;

    /* RFME: 0x1770 - 0x1778 */
    reg_param->rime_mvd_th.rime_mvd_th0  = 1;
    reg_param->rime_mvd_th.rime_mvd_th1  = 2;
    reg_param->rime_mvd_th.fme_madp_th   = 0;
    reg_param->rime_madp_th.rime_madp_th0 = 8;
    reg_param->rime_madp_th.rime_madp_th1 = 16;
    reg_param->rime_multi.rime_multi0 = 4;
    reg_param->rime_multi.rime_multi1 = 8;
    reg_param->rime_multi.rime_multi2 = 12;

    if (sm != MPP_ENC_SCENE_MODE_IPC) {
        /* disable subjective optimization */
        reg_param->cime_madp_th.cime_madp_th = 0;
        reg_param->rime_madp_th.rime_madp_th0 = 0;
        reg_param->rime_madp_th.rime_madp_th1 = 0;
        reg_param->cime_multi.cime_multi0 = 4;
        reg_param->cime_multi.cime_multi1 = 4;
        reg_param->cime_multi.cime_multi2 = 4;
        reg_param->cime_multi.cime_multi3 = 4;
        reg_param->rime_multi.rime_multi0 = 4;
        reg_param->rime_multi.rime_multi1 = 4;
        reg_param->rime_multi.rime_multi2 = 4;
    }

    /* 0x177C */
    reg_param->cmv_st_th.cmv_th0 = 64;
    reg_param->cmv_st_th.cmv_th1 = 96;
    reg_param->cmv_st_th.cmv_th2 = 128;
    regs->reg_rc_roi.madi_st_thd.madi_th0 = 5;
    regs->reg_rc_roi.madi_st_thd.madi_th1 = 12;
    regs->reg_rc_roi.madi_st_thd.madi_th2 = 20;
    regs->reg_rc_roi.madp_st_thd0.madp_th0 = 4 << 4;
    regs->reg_rc_roi.madp_st_thd0.madp_th1 = 9 << 4;
    regs->reg_rc_roi.madp_st_thd1.madp_th2 = 15 << 4;

    hal_h264e_dbg_func("leave\n");
}

static void setup_vepu511_ext_line_buf(HalVepu511RegSet *regs, HalH264eVepu511Ctx *ctx)
{
    if (ctx->ext_line_buf) {
        RK_U32 ext_line_buf_addr = mpp_buffer_get_iova(ctx->ext_line_buf, ctx->dev);

        regs->reg_frm.ebufb_addr = ext_line_buf_addr;
        regs->reg_frm.ebuft_addr = ext_line_buf_addr + ctx->ext_line_buf_size;

        return;
    }

    regs->reg_frm.ebufb_addr = 0;
    regs->reg_frm.ebufb_addr = 0;
}

static MPP_RET vepu511_h264e_set_dvbm(HalH264eVepu511Ctx *ctx, HalEncTask *task)
{
    HalVepu511RegSet *regs = ctx->regs_set;
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

    regs->reg_frm.enc_id.frame_id = 0;
    regs->reg_frm.enc_id.frm_id_match = 0;
    regs->reg_frm.enc_id.source_id = 0;
    regs->reg_frm.enc_id.src_id_match = 0;
    regs->reg_frm.enc_id.ch_id = 1;
    regs->reg_frm.enc_id.vinf_req_en = 1;
    regs->reg_frm.enc_id.vrsp_rtn_en = 1;

    regs->reg_ctl.int_en.dvbm_err_en = 1;

    return MPP_OK;
}

static void setup_vepu511_aq(HalH264eVepu511Ctx *ctx)
{
    MppEncCfgSet *cfg = ctx->cfg;
    MppEncHwCfg *hw = &cfg->hw;
    Vepu511RcRoiCfg *s = &ctx->regs_set->reg_rc_roi;
    RK_U8* thd = (RK_U8*)&s->aq_tthd0;
    RK_S32 *aq_step, *aq_thd;
    RK_U8 i;

    if (ctx->slice->slice_type == H264_I_SLICE) {
        aq_thd = (RK_S32 *)&hw->aq_thrd_i[0];
        aq_step = &hw->aq_step_i[0];
    } else {
        aq_thd = (RK_S32 *)&hw->aq_thrd_p[0];
        aq_step = &hw->aq_step_p[0];
    }

    for (i = 0; i < 16; i++)
        thd[i] = aq_thd[i] & 0xff;

    s->aq_stp0.aq_stp_s0 = aq_step[0] & 0x1f;
    s->aq_stp0.aq_stp_0t1 = aq_step[1] & 0x1f;
    s->aq_stp0.aq_stp_1t2 = aq_step[2] & 0x1f;
    s->aq_stp0.aq_stp_2t3 = aq_step[3] & 0x1f;
    s->aq_stp0.aq_stp_3t4 = aq_step[4] & 0x1f;
    s->aq_stp0.aq_stp_4t5 = aq_step[5] & 0x1f;
    s->aq_stp1.aq_stp_5t6 = aq_step[6] & 0x1f;
    s->aq_stp1.aq_stp_6t7 = aq_step[7] & 0x1f;
    s->aq_stp1.aq_stp_7t8 = 0;
    s->aq_stp1.aq_stp_8t9 = aq_step[8] & 0x1f;
    s->aq_stp1.aq_stp_9t10 = aq_step[9] & 0x1f;
    s->aq_stp1.aq_stp_10t11 = aq_step[10] & 0x1f;
    s->aq_stp2.aq_stp_11t12 = aq_step[11] & 0x1f;
    s->aq_stp2.aq_stp_12t13 = aq_step[12] & 0x1f;
    s->aq_stp2.aq_stp_13t14 = aq_step[13] & 0x1f;
    s->aq_stp2.aq_stp_14t15 = aq_step[14] & 0x1f;
    s->aq_stp2.aq_stp_b15 = aq_step[15] & 0x1f;
    s->aq_clip.aq16_rnge = 15;
}

static void setup_vepu511_quant(HalH264eVepu511Ctx *ctx)
{
    MppEncHwCfg *hw = &ctx->cfg->hw;
    HalVepu511RegSet *regs = ctx->regs_set;
    Vepu511Param *s = &regs->reg_param;
    RK_U8 th0 = 0, th1 = 0, th2 = 0; /* 5, 13, 27 */
    RK_U16 bias_i0 = 0, bias_i1 = 0, bias_i2 = 0, bias_i3 = 683; /* 171, 150, 120, 100 */
    RK_U16 bias_p0 = 0, bias_p1 = 0, bias_p2 = 0, bias_p3 = 341; /* 85, 80, 70, 65 */
    RK_U32 sli_type = ctx->slice->slice_type;

    if (!hw->qbias_en) {
        //TODO: set default qp

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
        if (sli_type == H264_I_SLICE) {
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

static void setup_vepu511_anti_stripe(HalH264eVepu511Ctx *ctx)
{
    HalVepu511RegSet *regs = ctx->regs_set;
    Vepu511Param *s = &regs->reg_param;
    RK_S32 str = ctx->cfg->tune.atl_str;

    s->iprd_tthdy4_0.iprd_tthdy4_0 = 1;
    s->iprd_tthdy4_0.iprd_tthdy4_1 = 3;
    s->iprd_tthdy4_1.iprd_tthdy4_2 = 6;
    s->iprd_tthdy4_1.iprd_tthdy4_3 = 8;
    s->iprd_tthdc8_0.iprd_tthdc8_0 = 1;
    s->iprd_tthdc8_0.iprd_tthdc8_1 = 3;
    s->iprd_tthdc8_1.iprd_tthdc8_2 = 6;
    s->iprd_tthdc8_1.iprd_tthdc8_3 = 8;
    s->iprd_tthdy8_0.iprd_tthdy8_0 = 1;
    s->iprd_tthdy8_0.iprd_tthdy8_1 = 3;
    s->iprd_tthdy8_1.iprd_tthdy8_2 = 6;
    s->iprd_tthdy8_1.iprd_tthdy8_3 = 8;

    if (ctx->cfg->tune.scene_mode != MPP_ENC_SCENE_MODE_IPC)
        s->iprd_tthd_ul.iprd_tthd_ul = 4095; /* disable anti-stripe */
    else
        s->iprd_tthd_ul.iprd_tthd_ul = str ? 4 : 255;

    s->iprd_wgty8.iprd_wgty8_0 = str ? 22 : 16;
    s->iprd_wgty8.iprd_wgty8_1 = str ? 23 : 16;
    s->iprd_wgty8.iprd_wgty8_2 = str ? 20 : 16;
    s->iprd_wgty8.iprd_wgty8_3 = str ? 22 : 16;
    s->iprd_wgty4.iprd_wgty4_0 = str ? 22 : 16;
    s->iprd_wgty4.iprd_wgty4_1 = str ? 26 : 16;
    s->iprd_wgty4.iprd_wgty4_2 = str ? 20 : 16;
    s->iprd_wgty4.iprd_wgty4_3 = str ? 22 : 16;
    s->iprd_wgty16.iprd_wgty16_0 = 22;
    s->iprd_wgty16.iprd_wgty16_1 = 26;
    s->iprd_wgty16.iprd_wgty16_2 = 20;
    s->iprd_wgty16.iprd_wgty16_3 = 22;
    s->iprd_wgtc8.iprd_wgtc8_0 = 18;
    s->iprd_wgtc8.iprd_wgtc8_1 = 21;
    s->iprd_wgtc8.iprd_wgtc8_2 = 20;
    s->iprd_wgtc8.iprd_wgtc8_3 = 19;
}

static void setup_vepu511_anti_ringing(HalH264eVepu511Ctx *ctx)
{
    HalVepu511RegSet *regs = ctx->regs_set;
    Vepu511SqiCfg *s = &regs->reg_sqi;
    MppEncSceneMode sm = ctx->cfg->tune.scene_mode;

    s->atr_thd.atr_qp = (sm == MPP_ENC_SCENE_MODE_IPC) ? 32 : 45;
    if (ctx->slice->slice_type == H264_I_SLICE) {
        s->atr_thd.atr_thd0 = 1;
        s->atr_thd.atr_thd1 = 2;
        s->atr_thd.atr_thd2 = 6;
        s->atr_wgt16.atr_lv16_wgt0 = 16;
        s->atr_wgt16.atr_lv16_wgt1 = 16;
        s->atr_wgt16.atr_lv16_wgt2 = 16;

        if (sm == MPP_ENC_SCENE_MODE_IPC) {
            s->atr_wgt8.atr_lv8_wgt0 = 22;
            s->atr_wgt8.atr_lv8_wgt1 = 21;
            s->atr_wgt8.atr_lv8_wgt2 = 20;
            s->atr_wgt4.atr_lv4_wgt0 = 20;
            s->atr_wgt4.atr_lv4_wgt1 = 18;
            s->atr_wgt4.atr_lv4_wgt2 = 16;
        } else {
            s->atr_wgt8.atr_lv8_wgt0 = 18;
            s->atr_wgt8.atr_lv8_wgt1 = 17;
            s->atr_wgt8.atr_lv8_wgt2 = 18;
            s->atr_wgt4.atr_lv4_wgt0 = 16;
            s->atr_wgt4.atr_lv4_wgt1 = 16;
            s->atr_wgt4.atr_lv4_wgt2 = 16;
        }
    } else {
        if (sm == MPP_ENC_SCENE_MODE_IPC) {
            s->atr_thd.atr_thd0 = 2;
            s->atr_thd.atr_thd1 = 4;
            s->atr_thd.atr_thd2 = 9;
            s->atr_wgt16.atr_lv16_wgt0 = 25;
            s->atr_wgt16.atr_lv16_wgt1 = 20;
            s->atr_wgt16.atr_lv16_wgt2 = 16;
            s->atr_wgt8.atr_lv8_wgt0 = 25;
            s->atr_wgt8.atr_lv8_wgt1 = 20;
            s->atr_wgt8.atr_lv8_wgt2 = 18;
            s->atr_wgt4.atr_lv4_wgt0 = 25;
            s->atr_wgt4.atr_lv4_wgt1 = 20;
            s->atr_wgt4.atr_lv4_wgt2 = 16;
        } else {
            s->atr_thd.atr_thd0 = 1;
            s->atr_thd.atr_thd1 = 2;
            s->atr_thd.atr_thd2 = 7;
            s->atr_wgt16.atr_lv16_wgt0 = 23;
            s->atr_wgt16.atr_lv16_wgt1 = 22;
            s->atr_wgt16.atr_lv16_wgt2 = 20;
            s->atr_wgt8.atr_lv8_wgt0 = 24;
            s->atr_wgt8.atr_lv8_wgt1 = 24;
            s->atr_wgt8.atr_lv8_wgt2 = 24;
            s->atr_wgt4.atr_lv4_wgt0 = 23;
            s->atr_wgt4.atr_lv4_wgt1 = 22;
            s->atr_wgt4.atr_lv4_wgt2 = 20;
        }
    }
}

static void setup_vepu511_anti_flicker(HalH264eVepu511Ctx *ctx)
{
    HalVepu511RegSet *regs = ctx->regs_set;
    Vepu511SqiCfg *reg = &regs->reg_sqi;
    RK_U32 str = ctx->cfg->tune.atf_str;

    static RK_U8 pskip_atf_th0[4] = { 0, 0, 1, 1 };
    static RK_U8 pskip_atf_th1[4] = { 7, 7, 10, 10 };
    static RK_U8 pskip_atf_th2[4] = { 15, 15, 15, 15 };
    static RK_U8 pskip_atf_th3[4] = { 25, 25, 25, 25 };
    static RK_U8 pskip_atf_wgt0[4] = { 16, 16, 20, 20 };
    static RK_U8 pskip_atf_wgt1[4] = { 16, 16, 15, 14 };
    static RK_U8 pskip_atf_wgt2[4] = { 16, 16, 16, 15 };
    static RK_U8 pskip_atf_wgt3[4] = { 16, 16, 16, 15 };
    static RK_U8 pskip_atf_wgt4[4] = { 16, 16, 16, 16 };
    static RK_U8 intra_atf_th0[4] = { 8, 16, 20, 20 };
    static RK_U8 intra_atf_th1[4] = { 16, 32, 40, 40 };
    static RK_U8 intra_atf_th2[4] = { 32, 56, 72, 72 };
    static RK_U8 intra_atf_wgt0[4] = { 16, 24, 28, 28 };
    static RK_U8 intra_atf_wgt1[4] = { 16, 22, 26, 26 };
    static RK_U8 intra_atf_wgt2[4] = { 16, 20, 22, 22 };
    static RK_U8 intra_atf_wgt3[4] = { 16, 16, 16, 18 };

    reg->rdo_b16_skip_atf_thd0.madp_thd0 = pskip_atf_th0[str];
    reg->rdo_b16_skip_atf_thd0.madp_thd1 = pskip_atf_th1[str];
    reg->rdo_b16_skip_atf_thd1.madp_thd2 = pskip_atf_th2[str];
    reg->rdo_b16_skip_atf_thd1.madp_thd3 = pskip_atf_th3[str];
    reg->rdo_b16_skip_atf_wgt0.wgt0 = pskip_atf_wgt0[str];
    reg->rdo_b16_skip_atf_wgt0.wgt1 = pskip_atf_wgt1[str];
    reg->rdo_b16_skip_atf_wgt0.wgt2 = pskip_atf_wgt2[str];
    reg->rdo_b16_skip_atf_wgt0.wgt3 = pskip_atf_wgt3[str];
    reg->rdo_b16_skip_atf_wgt1.wgt4 = pskip_atf_wgt4[str];

    reg->rdo_b16_inter_atf_thd0.madp_thd0 = 20;
    reg->rdo_b16_inter_atf_thd0.madp_thd1 = 40;
    reg->rdo_b16_inter_atf_thd1.madp_thd2 = 72;
    reg->rdo_b16_inter_atf_wgt.wgt0 = 16;
    reg->rdo_b16_inter_atf_wgt.wgt1 = 16;
    reg->rdo_b16_inter_atf_wgt.wgt2 = 16;
    reg->rdo_b16_inter_atf_wgt.wgt3 = 16;

    reg->rdo_b16_intra_atf_thd0.madp_thd0 = intra_atf_th0[str];
    reg->rdo_b16_intra_atf_thd0.madp_thd1 = intra_atf_th1[str];
    reg->rdo_b16_intra_atf_thd1.madp_thd2 = intra_atf_th2[str];
    reg->rdo_b16_intra_atf_wgt.wgt0 = intra_atf_wgt0[str];
    reg->rdo_b16_intra_atf_wgt.wgt1 = intra_atf_wgt1[str];
    reg->rdo_b16_intra_atf_wgt.wgt2 = intra_atf_wgt2[str];
    reg->rdo_b16_intra_atf_wgt.wgt3 = intra_atf_wgt3[str];

    reg->rdo_b16_intra_atf_cnt_thd.cnt_thd0 = 1;
    reg->rdo_b16_intra_atf_cnt_thd.cnt_thd1 = 4;
    reg->rdo_b16_intra_atf_cnt_thd.cnt_thd2 = 1;
    reg->rdo_b16_intra_atf_cnt_thd.cnt_thd3 = 4;

    reg->rdo_atf_resi_thd.big_th0 = 16;
    reg->rdo_atf_resi_thd.big_th1 = 16;
    reg->rdo_atf_resi_thd.small_th0 = 8;
    reg->rdo_atf_resi_thd.small_th1 = 8;
}

static void setup_vepu511_anti_smear(HalH264eVepu511Ctx *ctx)
{
    HalVepu511RegSet *regs = ctx->regs_set;
    Vepu511SqiCfg *reg = &regs->reg_sqi;
    H264eSlice *slice = ctx->slice;
    vepu511_h264_fbk *last_fb = &ctx->last_frame_fb;
    RK_U32 mb_cnt = last_fb->st_mb_num;
    RK_U32 *smear_cnt = last_fb->st_smear_cnt;
    RK_S32 deblur_en = ctx->cfg->tune.deblur_en;
    RK_S32 deblur_str = ctx->cfg->tune.deblur_str;
    RK_S32 delta_qp = 0;
    RK_S32 flg0 = smear_cnt[4] < (mb_cnt >> 6);
    RK_S32 flg1 = 1, flg2 = 0, flg3 = 0;
    RK_S32 smear_multi[4] = { 9, 12, 16, 16 };

    hal_h264e_dbg_func("enter\n");

    if ((smear_cnt[3] < ((5 * mb_cnt) >> 10)) ||
        (smear_cnt[3] < ((1126 * MPP_MAX3(smear_cnt[0], smear_cnt[1], smear_cnt[2])) >> 10)) ||
        (deblur_str == 6) || (deblur_str == 7))
        flg1 = 0;

    flg3 = flg1 ? 3 : (smear_cnt[4] > ((102 * mb_cnt) >> 10)) ? 2 :
           (smear_cnt[4] > ((66 * mb_cnt) >> 10)) ? 1 : 0;

    if (ctx->cfg->tune.scene_mode == MPP_ENC_SCENE_MODE_IPC) {
        reg->smear_opt_cfg.rdo_smear_en = 1;
        if (deblur_en && deblur_str > 3)
            reg->smear_opt_cfg.rdo_smear_lvl16_multi = smear_multi[flg3];
        else
            reg->smear_opt_cfg.rdo_smear_lvl16_multi = flg0 ? 9 : 12;
    } else {
        reg->smear_opt_cfg.rdo_smear_en = 0;
        reg->smear_opt_cfg.rdo_smear_lvl16_multi = 16;
    }

    if (deblur_en && deblur_str > 3) {
        flg2 = 1;
        if (smear_cnt[2] + smear_cnt[3] > (3 * smear_cnt[4] / 4))
            delta_qp = 1;
        if (smear_cnt[4] < (mb_cnt >> 4))
            delta_qp -= 8;
        else if (smear_cnt[4] < ((3 * mb_cnt) >> 5))
            delta_qp -= 7;
        else
            delta_qp -= 6;

        if (flg3 == 2)
            delta_qp = 0;
        else if (flg3 == 1)
            delta_qp = -2;
    } else {
        if (smear_cnt[2] + smear_cnt[3] > smear_cnt[4] / 2)
            delta_qp = 1;
        if (smear_cnt[4] < (mb_cnt >> 8))
            delta_qp -= (deblur_str < 2) ? 6 : 8;
        else if (smear_cnt[4] < (mb_cnt >> 7))
            delta_qp -= (deblur_str < 2) ? 5 : 6;
        else if (smear_cnt[4] < (mb_cnt >> 6))
            delta_qp -= (deblur_str < 2) ? 3 : 4;
        else
            delta_qp -= 1;
    }
    reg->smear_opt_cfg.rdo_smear_dlt_qp = delta_qp;

    if ((H264_I_SLICE == slice->slice_type) ||
        (H264_I_SLICE == last_fb->frame_type))
        reg->smear_opt_cfg.stated_mode = 1;
    else
        reg->smear_opt_cfg.stated_mode = 2;

    reg->smear_madp_thd0.madp_cur_thd0 = 0;
    reg->smear_madp_thd0.madp_cur_thd1 = flg2 ? 48 : 24;
    reg->smear_madp_thd1.madp_cur_thd2 = flg2 ? 64 : 48;
    reg->smear_madp_thd1.madp_cur_thd3 = flg2 ? 72 : 64;
    reg->smear_madp_thd2.madp_around_thd0 = flg2 ? 4095 : 16;
    reg->smear_madp_thd2.madp_around_thd1 = 32;
    reg->smear_madp_thd3.madp_around_thd2 = 48;
    reg->smear_madp_thd3.madp_around_thd3 = flg2 ? 0 : 96;
    reg->smear_madp_thd4.madp_around_thd4 = 48;
    reg->smear_madp_thd4.madp_around_thd5 = 24;
    reg->smear_madp_thd5.madp_ref_thd0 = flg2 ? 64 : 96;
    reg->smear_madp_thd5.madp_ref_thd1 = 48;

    reg->smear_cnt_thd0.cnt_cur_thd0 = flg2 ?  2 : 1;
    reg->smear_cnt_thd0.cnt_cur_thd1 = flg2 ?  5 : 3;
    reg->smear_cnt_thd0.cnt_cur_thd2 = 1;
    reg->smear_cnt_thd0.cnt_cur_thd3 = 3;
    reg->smear_cnt_thd1.cnt_around_thd0 = 1;
    reg->smear_cnt_thd1.cnt_around_thd1 = 4;
    reg->smear_cnt_thd1.cnt_around_thd2 = 1;
    reg->smear_cnt_thd1.cnt_around_thd3 = 4;
    reg->smear_cnt_thd2.cnt_around_thd4 = 0;
    reg->smear_cnt_thd2.cnt_around_thd5 = 3;
    reg->smear_cnt_thd2.cnt_around_thd6 = 0;
    reg->smear_cnt_thd2.cnt_around_thd7 = 3;
    reg->smear_cnt_thd3.cnt_ref_thd0 = 1;
    reg->smear_cnt_thd3.cnt_ref_thd1 = 3;

    reg->smear_resi_thd0.resi_small_cur_th0 = 6;
    reg->smear_resi_thd0.resi_big_cur_th0 = 9;
    reg->smear_resi_thd0.resi_small_cur_th1 = 6;
    reg->smear_resi_thd0.resi_big_cur_th1 = 9;
    reg->smear_resi_thd1.resi_small_around_th0 = 6;
    reg->smear_resi_thd1.resi_big_around_th0 = 11;
    reg->smear_resi_thd1.resi_small_around_th1 = 6;
    reg->smear_resi_thd1.resi_big_around_th1 = 8;
    reg->smear_resi_thd2.resi_small_around_th2 = 9;
    reg->smear_resi_thd2.resi_big_around_th2 = 20;
    reg->smear_resi_thd2.resi_small_around_th3 = 6;
    reg->smear_resi_thd2.resi_big_around_th3 = 20;
    reg->smear_resi_thd3.resi_small_ref_th0 = 7;
    reg->smear_resi_thd3.resi_big_ref_th0 = 16;
    reg->smear_resi_thd4.resi_th0 = flg2 ? 0 : 10;
    reg->smear_resi_thd4.resi_th1 = flg2 ? 0 : 6;

    reg->rdo_smear_st_thd.madp_cnt_th0 = flg2 ? 0 : 1;
    reg->rdo_smear_st_thd.madp_cnt_th1 = flg2 ? 0 : 5;
    reg->rdo_smear_st_thd.madp_cnt_th2 = flg2 ? 0 : 1;
    reg->rdo_smear_st_thd.madp_cnt_th3 = flg2 ? 0 : 3;

    hal_h264e_dbg_func("leave\n");
}

static void setup_vepu511_scaling_list(HalH264eVepu511Ctx *ctx)
{
    Vepu511SclCfg *s = &ctx->regs_set->reg_scl_jpgtbl.scl;
    Vepu511SclCfgExt *s_ext = &ctx->regs_set->reg_scl_jpgtbl.scl_ext;
    RK_U8 *p = (RK_U8 *)&s->tu8_intra_y[0];
    RK_U16 *q = (RK_U16 *)&s_ext->tu8_intra_y[0];
    RK_U8 idx;

    hal_h264e_dbg_func("enter\n");

    /* intra4x4 and inter4x4 is not supported on VEPU511.
     * valid range: 0x2200 ~ 0x221F
     */
    if (ctx->pps->scaling_list_mode == 1) {
        for (idx = 0; idx < 64; idx++) {
            p[idx] = vepu511_h264_cqm_jvt8i[63 - idx]; /* intra8x8 */
            p[idx + 64] = vepu511_h264_cqm_jvt8p[63 - idx]; /* inter8x8 */

            q[idx] = vepu511_h264_cqm_jvt8i_q[63 - idx]; /* intra8x8 */
            q[idx + 64] = vepu511_h264_cqm_jvt8p_q[63 - idx]; /* inter8x8 */
        }
    } else if (ctx->pps->scaling_list_mode == 2) {
        //TODO: Update scaling list for (scaling_list_mode == 2)
        kmpp_logi_f("scaling_list_mode 2 is not supported yet\n");
    }

    hal_h264e_dbg_func("leave\n");
}

static MPP_RET setup_vepu511_intra_refresh(HalH264eVepu511Ctx *ctx, HalVepu511RegSet *regs, RK_U32 refresh_idx)
{
    MPP_RET ret = MPP_OK;
    RK_U32 w = ctx->cfg->prep.width;
    RK_U32 h = ctx->cfg->prep.height;
    Vepu511FrameCfg *reg_frm = &regs->reg_frm;
    Vepu511RoiRegion *reg_regions = &regs->reg_rc_roi.roi_cfg.regions[0];
    RK_U32 refresh_num = ctx->cfg->rc.refresh_num;
    RK_U32 roi_x = 0;
    RK_U32 roi_y = 0;
    RK_U32 roi_w = 0;
    RK_U32 roi_h = 0;
    RK_U32 ctu_size = 16;

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
        reg_frm->me_rnge.cime_srch_dwnh = 4;
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
        reg_frm->me_rnge.cime_srch_rgtw = 4;
    }

    reg_regions->roi_pos_lt.roi_lt_x = MPP_ALIGN(roi_x, 16) >> 4;
    reg_regions->roi_pos_lt.roi_lt_y = MPP_ALIGN(roi_y, 16) >> 4;
    reg_regions->roi_pos_rb.roi_rb_x = MPP_ALIGN(roi_x + roi_w, 16) >> 4;
    reg_regions->roi_pos_rb.roi_rb_y = (MPP_ALIGN(roi_y + roi_h, 16) >> 4) - 1;
    reg_regions->roi_base.roi_en = 1;
    reg_regions->roi_base.roi_pri = 0x1f;
    reg_regions->roi_mdc0.h264.roi_mdc_intra16 = 1;
    reg_regions->roi_mdc1.roi_mdc_intra32 = 1;

    return ret;
}

typedef struct SymCtx_t {
    void        *hal;
    HalEncTask  *task;
} SymCtx;

static MPP_RET hal_h264e_vepu511_gen_regs(void *hal, HalEncTask *task)
{
    HalH264eVepu511Ctx *ctx = (HalH264eVepu511Ctx *)hal;
    HalVepu511RegSet *regs = ctx->regs_set;
    MppEncCfgSet *cfg = ctx->cfg;
    H264eSps *sps = ctx->sps;
    H264ePps *pps = ctx->pps;
    H264eSlice *slice = ctx->slice;
    EncRcTask *rc_task = task->rc_task;
    EncFrmStatus *frm = &rc_task->frm;
    MPP_RET ret = MPP_OK;

    hal_h264e_dbg_func("enter %p\n", hal);

    /* register setup */
    kmpp_memset(regs, 0, sizeof(*regs));

    setup_vepu511_normal(regs);
    ret = setup_vepu511_prep(regs, &ctx->cfg->prep, task);
    if (ret)
        return ret;

    setup_vepu511_vsp_filtering(ctx, task);

    if (ctx->online)
        vepu511_h264e_set_dvbm(ctx, task);
    setup_vepu511_codec(regs, sps, pps, slice);
    setup_vepu511_rdo_pred(ctx, sps, pps, slice);
    setup_vepu511_aq(ctx);
    setup_vepu511_quant(ctx);
    setup_vepu511_anti_stripe(ctx);
    setup_vepu511_anti_ringing(ctx);
    setup_vepu511_anti_flicker(ctx);
    setup_vepu511_anti_smear(ctx);

    setup_vepu511_rc_base(ctx, regs, sps, slice, &cfg->hw, rc_task);
    setup_vepu511_io_buf(ctx, task);
    setup_vepu511_recn_refr(ctx, regs);

    setup_vepu511_split(regs, cfg);
    setup_vepu511_me(ctx);
    setup_vepu511_ext_line_buf(regs, ctx);
    setup_vepu511_scaling_list(ctx);

    if (ctx->sym) {
        SymCtx sym_ctx = { ctx, task };

        kmpp_sym_run(ctx->sym, &sym_ctx, NULL);
    }

    if (ctx->osd_cfg.osd_data3) {
        vepu511_set_osd(&ctx->osd_cfg);
        ctx->osd_cfg.osd_data3 = NULL;
    }

    if (ctx->roi_data) {
        vepu511_set_roi(&regs->reg_rc_roi.roi_cfg, ctx->roi_data,
                        ctx->cfg->prep.width, ctx->cfg->prep.height);
        ctx->roi_data = NULL;
    }

    if (frm->is_i_refresh)
        setup_vepu511_intra_refresh(ctx, regs, frm->seq_idx % cfg->rc.gop);

    /* two pass register patch */
    if (frm->save_pass1)
        vepu511_h264e_save_pass1_patch(regs, ctx);

    if (frm->use_pass1)
        vepu511_h264e_use_pass1_patch(regs, ctx);

    ctx->frame_cnt++;

    hal_h264e_dbg_func("leave %p\n", hal);
    return MPP_OK;
}

static MPP_RET hal_h264e_vepu511_start(void *hal, HalEncTask *task)
{
    MPP_RET ret = MPP_OK;
    HalH264eVepu511Ctx *ctx = (HalH264eVepu511Ctx *)hal;
    HalVepu511RegSet *regs = ctx->regs_set;
    MppDevRegWrCfg cfg;
    (void) task;

    hal_h264e_dbg_func("enter %p\n", hal);

#define HAL_H264E_CFG_WR_REG(reg_set, size_set, off_set)                        \
    do {                                                                        \
        cfg.reg = (RK_U32*)&reg_set;                                        \
        cfg.size = size_set;                                                \
        cfg.offset = off_set;                                               \
        ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_REG_WR, &cfg);                \
        if (ret) {                                                          \
            kmpp_loge_f("set %s reg write failed %d\n", #reg_set, ret);         \
            return ret;                                                     \
        }                                                                   \
    } while(0);

#define HAL_H264E_CFG_RD_REG(reg_set, size_set, off_set)                        \
    do {                                                                        \
        cfg.reg = (RK_U32*)&reg_set;                                        \
        cfg.size = size_set;                                                \
        cfg.offset = off_set;                                               \
        ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_REG_RD, &cfg);                \
        if (ret) {                                                          \
            kmpp_loge_f("set %s reg read failed %d\n", #reg_set, ret);          \
            return ret;                                                     \
        }                                                                   \
    } while(0);

    do {
        /* config write regs */
        HAL_H264E_CFG_WR_REG(regs->reg_ctl,     sizeof(regs->reg_ctl),      VEPU511_CTL_OFFSET);
        HAL_H264E_CFG_WR_REG(regs->reg_frm,     sizeof(regs->reg_frm),      VEPU511_FRAME_OFFSET);
        HAL_H264E_CFG_WR_REG(regs->reg_rc_roi,  sizeof(regs->reg_rc_roi),   VEPU511_RC_ROI_OFFSET);
        HAL_H264E_CFG_WR_REG(regs->reg_param,   sizeof(regs->reg_param),    VEPU511_PARAM_OFFSET);
        HAL_H264E_CFG_WR_REG(regs->reg_sqi,     sizeof(regs->reg_sqi),      VEPU511_SQI_OFFSET);
        HAL_H264E_CFG_WR_REG(regs->reg_scl_jpgtbl, sizeof(regs->reg_scl_jpgtbl), VEPU511_SCL_JPGTBL_OFFSET);
        HAL_H264E_CFG_WR_REG(regs->reg_osd,     sizeof(regs->reg_osd),      VEPU511_OSD_OFFSET);

        /* config read regs */
        HAL_H264E_CFG_RD_REG(regs->reg_ctl.int_sta, sizeof(RK_U32), VEPU511_REG_BASE_HW_STATUS);
        HAL_H264E_CFG_RD_REG(regs->reg_st, sizeof(regs->reg_st), VEPU511_STATUS_OFFSET);

        /* send request to hardware */
        ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_CMD_SEND, NULL);
        if (ret) {
            kmpp_loge_f("send cmd failed %d\n", ret);
            break;
        }
    } while (0);

    hal_h264e_dbg_func("leave %p\n", hal);

    return ret;
}

static MPP_RET hal_h264e_vepu511_status_check(HalVepu511RegSet *regs)
{
    MPP_RET ret = MPP_OK;

    if (regs->reg_ctl.int_sta.lkt_node_done_sta)
        hal_h264e_dbg_detail("lkt_done finish");

    if (regs->reg_ctl.int_sta.enc_done_sta)
        hal_h264e_dbg_detail("enc_done finish");

    if (regs->reg_ctl.int_sta.vslc_done_sta)
        hal_h264e_dbg_detail("enc_slice finsh");

    if (regs->reg_ctl.int_sta.sclr_done_sta)
        hal_h264e_dbg_detail("safe clear finsh");

    if (regs->reg_ctl.int_sta.vbsf_oflw_sta) {
        hal_h264e_dbg_warning("bit stream overflow");
        ret = MPP_ERR_INT_BS_OVFL;
    }

    if (regs->reg_ctl.int_sta.vbuf_lens_sta)
        hal_h264e_dbg_detail("Bit stream write out set length");

    if (regs->reg_ctl.int_sta.enc_err_sta) {
        hal_h264e_dbg_warning("enc error");
        ret = MPP_NOK;
    }

    if (regs->reg_ctl.int_sta.wdg_sta) {
        hal_h264e_dbg_warning("wdg timeout");
        ret = MPP_NOK;
    }

    if (regs->reg_ctl.int_sta.vsrc_err_sta) {
        hal_h264e_dbg_warning("wrap frame error");
        ret = MPP_NOK;
    }

    return ret;
}

static MPP_RET hal_h264e_vepu511_wait(void *hal, HalEncTask *task)
{
    MPP_RET ret = MPP_OK;
    HalH264eVepu511Ctx *ctx = (HalH264eVepu511Ctx *) hal;

    hal_h264e_dbg_func("enter %p\n", hal);
    ret = mpp_dev_ioctl(ctx->dev, MPP_DEV_CMD_POLL, NULL);
    hal_h264e_dbg_func("leave %p\n", hal);

    return ret;
}

static void h264e_vepu511_update_bitrate_info(HalH264eVepu511Ctx *ctx, HalEncTask *task)
{
    vepu511_h264_fbk *fb = &ctx->feedback;
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

static MPP_RET hal_h264e_vepu511_ret_task(void * hal, HalEncTask *task)
{
    HalH264eVepu511Ctx *ctx = (HalH264eVepu511Ctx *)hal;
    HalVepu511RegSet *regs = ctx->regs_set;
    EncRcTaskInfo *rc_info = &task->rc_task->info;
    RK_U32 mb_w = ctx->sps->pic_width_in_mbs;
    RK_U32 mb_h = ctx->sps->pic_height_in_mbs;
    RK_U32 mbs = mb_w * mb_h;
    MPP_RET ret = MPP_OK;
    vepu511_h264_fbk *fb = &ctx->feedback;
    Vepu511Status *reg_st = &regs->reg_st;
    MppEncBaseCfg *base = &ctx->cfg->base;
    RK_U32 madi_th_cnt[4], madp_th_cnt[4];
    RK_U32 madi_cnt = 0, madp_cnt = 0;
    RK_U32 md_cnt;

    hal_h264e_dbg_func("enter %p\n", hal);

    ret = hal_h264e_vepu511_status_check(regs);
    if (ret)
        return ret;

    madi_th_cnt[0] = reg_st->st_madi_lt_num0.madi_th_lt_cnt0 +
                     reg_st->st_madi_rt_num0.madi_th_rt_cnt0 +
                     reg_st->st_madi_lb_num0.madi_th_lb_cnt0 +
                     reg_st->st_madi_rb_num0.madi_th_rb_cnt0;
    madi_th_cnt[1] = reg_st->st_madi_lt_num0.madi_th_lt_cnt1 +
                     reg_st->st_madi_rt_num0.madi_th_rt_cnt1 +
                     reg_st->st_madi_lb_num0.madi_th_lb_cnt1 +
                     reg_st->st_madi_rb_num0.madi_th_rb_cnt1;
    madi_th_cnt[2] = reg_st->st_madi_lt_num1.madi_th_lt_cnt2 +
                     reg_st->st_madi_rt_num1.madi_th_rt_cnt2 +
                     reg_st->st_madi_lb_num1.madi_th_lb_cnt2 +
                     reg_st->st_madi_rb_num1.madi_th_rb_cnt2;
    madi_th_cnt[3] = reg_st->st_madi_lt_num1.madi_th_lt_cnt3 +
                     reg_st->st_madi_rt_num1.madi_th_rt_cnt3 +
                     reg_st->st_madi_lb_num1.madi_th_lb_cnt3 +
                     reg_st->st_madi_rb_num1.madi_th_rb_cnt3;
    madp_th_cnt[0] = reg_st->st_madp_lt_num0.madp_th_lt_cnt0 +
                     reg_st->st_madp_rt_num0.madp_th_rt_cnt0 +
                     reg_st->st_madp_lb_num0.madp_th_lb_cnt0 +
                     reg_st->st_madp_rb_num0.madp_th_rb_cnt0;
    madp_th_cnt[1] = reg_st->st_madp_lt_num0.madp_th_lt_cnt1 +
                     reg_st->st_madp_rt_num0.madp_th_rt_cnt1 +
                     reg_st->st_madp_lb_num0.madp_th_lb_cnt1 +
                     reg_st->st_madp_rb_num0.madp_th_rb_cnt1;
    madp_th_cnt[2] = reg_st->st_madp_lt_num1.madp_th_lt_cnt2 +
                     reg_st->st_madp_rt_num1.madp_th_rt_cnt2 +
                     reg_st->st_madp_lb_num1.madp_th_lb_cnt2 +
                     reg_st->st_madp_rb_num1.madp_th_rb_cnt2;
    madp_th_cnt[3] = reg_st->st_madp_lt_num1.madp_th_lt_cnt3 +
                     reg_st->st_madp_rt_num1.madp_th_rt_cnt3 +
                     reg_st->st_madp_lb_num1.madp_th_lb_cnt3 +
                     reg_st->st_madp_rb_num1.madp_th_rb_cnt3;

    if (base->smt1_en || base->smt3_en)
        md_cnt = (12 * madp_th_cnt[3] + 11 * madp_th_cnt[2] + 8 * madp_th_cnt[1]) >> 2;
    else
        md_cnt = (24 * madp_th_cnt[3] + 22 * madp_th_cnt[2] + 17 * madp_th_cnt[1]) >> 2;
    madi_cnt = (6 * madi_th_cnt[3] + 5 * madi_th_cnt[2] + 4 * madi_th_cnt[1]) >> 2;

    /* fill rc info */
    rc_info->motion_level = 0;
    if (md_cnt * 100 > 15 * mbs)
        rc_info->motion_level = 200;
    else if (md_cnt * 100 > 5 * mbs)
        rc_info->motion_level = 100;
    else if (md_cnt * 100 > (mbs >> 2))
        rc_info->motion_level = 1;
    else
        rc_info->motion_level = 0;

    rc_info->complex_level = 0;
    if (madi_cnt * 100 > 30 * mbs)
        rc_info->complex_level = 2;
    else if (madi_cnt * 100 > 13 * mbs)
        rc_info->complex_level = 1;
    else
        rc_info->complex_level = 0;

    task->hw_length = regs->reg_st.bs_lgth_l32;
    rc_info->bit_real = task->hw_length * 8;
    rc_info->quality_real = regs->reg_st.qp_sum / mbs;
    rc_info->dsp_luma_ave = reg_st->dsp_y_sum / (mbs * 16);
    hal_h264e_dbg_regs("frame %d dsp_luma_ave %d qp_real %d motion_level %d complex_level %d\n",
                       ctx->frame_cnt - 1, rc_info->dsp_luma_ave, rc_info->quality_real,
                       rc_info->motion_level, rc_info->complex_level);

    rc_info->madi = madi_th_cnt[0] * regs->reg_rc_roi.madi_st_thd.madi_th0 +
                    madi_th_cnt[1] * (regs->reg_rc_roi.madi_st_thd.madi_th0 +
                                      regs->reg_rc_roi.madi_st_thd.madi_th1) / 2 +
                    madi_th_cnt[2] * (regs->reg_rc_roi.madi_st_thd.madi_th1 +
                                      regs->reg_rc_roi.madi_st_thd.madi_th2) / 2 +
                    madi_th_cnt[3] * regs->reg_rc_roi.madi_st_thd.madi_th2;

    madi_cnt = madi_th_cnt[0] + madi_th_cnt[1] + madi_th_cnt[2] + madi_th_cnt[3];
    if (madi_cnt)
        rc_info->madi = rc_info->madi / madi_cnt;

    rc_info->madp = madp_th_cnt[0] * regs->reg_rc_roi.madp_st_thd0.madp_th0 +
                    madp_th_cnt[1] * (regs->reg_rc_roi.madp_st_thd0.madp_th0 +
                                      regs->reg_rc_roi.madp_st_thd0.madp_th1) / 2 +
                    madp_th_cnt[2] * (regs->reg_rc_roi.madp_st_thd0.madp_th1 +
                                      regs->reg_rc_roi.madp_st_thd1.madp_th2) / 2 +
                    madp_th_cnt[3] * regs->reg_rc_roi.madp_st_thd1.madp_th2;

    madp_cnt = madp_th_cnt[0] + madp_th_cnt[1] + madp_th_cnt[2] + madp_th_cnt[3];

    if (madp_cnt)
        rc_info->madp = rc_info->madp / madp_cnt;

    rc_info->iblk4_prop = (regs->reg_st.st_pnum_i4.pnum_i4 +
                           regs->reg_st.st_pnum_i8.pnum_i8 +
                           regs->reg_st.st_pnum_i16.pnum_i16) * 256 / mbs;

    ctx->hal_rc_cfg.bit_real = rc_info->bit_real;
    ctx->hal_rc_cfg.quality_real = rc_info->quality_real;
    ctx->hal_rc_cfg.iblk4_prop = rc_info->iblk4_prop;

    task->length += task->hw_length;
    task->hal_ret.data   = &ctx->hal_rc_cfg;
    task->hal_ret.number = 1;

    fb->st_mb_num = mbs;
    fb->st_smear_cnt[0] = reg_st->st_smear_cnt0.rdo_smear_cnt0;
    fb->st_smear_cnt[1] = reg_st->st_smear_cnt0.rdo_smear_cnt1;
    fb->st_smear_cnt[2] = reg_st->st_smear_cnt1.rdo_smear_cnt2;
    fb->st_smear_cnt[3] = reg_st->st_smear_cnt1.rdo_smear_cnt3;
    fb->st_smear_cnt[4] = fb->st_smear_cnt[0] + fb->st_smear_cnt[1] +
                          fb->st_smear_cnt[2] + fb->st_smear_cnt[3];
    fb->frame_type = ctx->slice->slice_type;

    h264e_vepu511_update_bitrate_info(ctx, task);

    hal_h264e_dbg_func("leave %p\n", hal);

    return MPP_OK;
}

static MPP_RET hal_h264e_vepu511_comb_start(void *hal, HalEncTask *task, HalEncTask *jpeg_task)
{
    HalH264eVepu511Ctx *ctx = (HalH264eVepu511Ctx *) hal;
    HalVepu511RegSet *regs = (HalVepu511RegSet *) ctx->regs_set;
    Vepu511JpegCfg jpeg_cfg;
    VepuFmtCfg cfg;
    MppEncPrepCfg *prep = &ctx->cfg->prep;
    MppFrameFormat fmt = prep->format;

    hal_h264e_dbg_func("enter %p\n", hal);
    regs->reg_ctl.dtrns_map.jpeg_bus_edin = 7;
    vepu5xx_set_fmt(&cfg, fmt);
    jpeg_cfg.dev = ctx->dev;
    jpeg_cfg.jpeg_reg_base = &ctx->regs_set->reg_frm.jpeg_frame;
    jpeg_cfg.reg_tab = &regs->reg_scl_jpgtbl.jpg_tbl;
    jpeg_cfg.enc_task = jpeg_task;
    jpeg_cfg.input_fmt = &cfg;
    jpeg_cfg.online = ctx->online;
    vepu511_set_jpeg_reg(&jpeg_cfg);
    //osd part todo
    if (jpeg_task->jpeg_tlb_reg)
        kmpp_memcpy(&regs->reg_scl_jpgtbl.jpg_tbl, jpeg_task->jpeg_tlb_reg, sizeof(Vepu511JpegTable));
    if (jpeg_task->jpeg_osd_reg)
        kmpp_memcpy(&regs->reg_osd.jpeg_osd, jpeg_task->jpeg_osd_reg, sizeof(Vepu511Osd));
    hal_h264e_dbg_func("leave %p\n", hal);

    return hal_h264e_vepu511_start(hal, task);
}

static MPP_RET hal_h264e_vepu511_comb_ret_task(void *hal, HalEncTask *task, HalEncTask *jpeg_task)
{
    HalH264eVepu511Ctx *ctx = (HalH264eVepu511Ctx *) hal;
    EncRcTaskInfo *rc_info = &jpeg_task->rc_task->info;
    HalVepu511RegSet *regs = (HalVepu511RegSet *)ctx->regs_set;
    MPP_RET ret = MPP_OK;

    hal_h264e_dbg_func("enter %p\n", hal);
    ret = hal_h264e_vepu511_ret_task(hal, task);
    if (ret)
        return ret;

    if (regs->reg_ctl.int_sta.jbsf_oflw_sta)
        jpeg_task->jpeg_overflow = 1;

    jpeg_task->hw_length += regs->reg_st.jpeg_head_bits_l32;
    // setup bit length for rate control
    rc_info->bit_real = jpeg_task->hw_length * 8;

    hal_h264e_dbg_func("leave %p\n", hal);
    return ret;
}

static MPP_RET hal_h264e_vepu511_init_cfg(void *hal, MppEncCfgSet *cfg, const rk_u8 *preset)
{
    MppEncHwCfg *hw = &cfg->hw;
    MppEncFineTuneCfg *tune = &cfg->tune;
    (void)hal;

    /* setup default hardware config */
    hw->qp_delta_row_i = 1;
    hw->qp_delta_row = 2;
    hw->qbias_i = 683;
    hw->qbias_p = 341;
    hw->qbias_en = 0;
    hw->flt_str_i = 0;
    hw->flt_str_p = 0;

    if (preset && kmpp_strstr(preset, "smart")) {
        kmpp_memcpy(hw->aq_thrd_i, h264_aq_tthd_smart, sizeof(hw->aq_thrd_i));
        kmpp_memcpy(hw->aq_thrd_p, h264_aq_tthd_smart, sizeof(hw->aq_thrd_p));
        kmpp_memcpy(hw->aq_step_i, h264_I_aq_step_smart, sizeof(hw->aq_step_i));
        kmpp_memcpy(hw->aq_step_p, h264_P_aq_step_smart, sizeof(hw->aq_step_p));
    } else  {
        kmpp_memcpy(hw->aq_thrd_i, h264_aq_tthd_default, sizeof(hw->aq_thrd_i));
        kmpp_memcpy(hw->aq_thrd_p, h264_aq_tthd_default, sizeof(hw->aq_thrd_p));
        kmpp_memcpy(hw->aq_step_i, h264_I_aq_step_default, sizeof(hw->aq_step_i));
        kmpp_memcpy(hw->aq_step_p, h264_P_aq_step_default, sizeof(hw->aq_step_p));
    }

    if (preset && kmpp_strstr(preset, "ipc")) {
        tune->deblur_str = 5; /* [0, 7] */
        tune->atr_str_i = 1; /* [0, 3] */
        tune->atr_str_p = 1; /* [0, 3] */
        tune->atl_str = 1; /* [0, 3] */
        tune->lambda_idx_i = 4; /* [0, 8] */
        tune->lambda_idx_p = 4; /* [0, 8] */
        tune->atf_str = 1; /* [0, 3] */
    } else {
        tune->deblur_str = 5;
        tune->atr_str_i = 0;
        tune->atr_str_p = 0;
        tune->atl_str = 0;
        tune->lambda_idx_i = 6;
        tune->lambda_idx_p = 6;
        tune->atf_str = 0;
    }

    return MPP_OK;
}

const MppEncHalApi hal_h264e_vepu511 = {
    .name       = "hal_h264e_vepu511",
    .coding     = MPP_VIDEO_CodingAVC,
    .type       = VPU_CLIENT_RKVENC,
    .ctx_size   = sizeof(HalH264eVepu511Ctx),
    .flag       = 0,
    .init       = hal_h264e_vepu511_init,
    .deinit     = hal_h264e_vepu511_deinit,
    .prepare    = hal_h264e_vepu511_prepare,
    .get_task   = hal_h264e_vepu511_get_task,
    .gen_regs   = hal_h264e_vepu511_gen_regs,
    .start      = hal_h264e_vepu511_start,
    .wait       = hal_h264e_vepu511_wait,
    .part_start = NULL,
    .part_wait  = NULL,
    .ret_task   = hal_h264e_vepu511_ret_task,
    .comb_start = hal_h264e_vepu511_comb_start,
    .comb_ret_task = hal_h264e_vepu511_comb_ret_task,
    .init_cfg   = hal_h264e_vepu511_init_cfg,
};

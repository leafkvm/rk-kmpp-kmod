/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#include <linux/dma-buf-cache.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/delay.h>

#include "mpp_rkvenc_dvbm.h"
#include "mpp_osal.h"

#define RKVENC_SOURCE_ERR       (BIT(7))
#define RKVENC_STATUS           (0x4020)
#define RKVENC_DBG_TCTRL        (0x5078)
#define RKVENC_DBG_FRM_WORK     BIT(28)
/* dvbm regs */
#define RKVENC_DVBM_CFG         (0x60)
#define RKVENC_DVBM_EN          (BIT(0))
#define DVBM_ISP_CONNETC        (BIT(4))
#define DVBM_VEPU_CONNETC       (BIT(5))
#define RKVENC_DBG_DVBM_ISP1    (0x516c)
#define RKVENC_DBG_ISP_WORK     (BIT(27))
#define RKVENC_DBG_DVBM_CTRL    (0x5190)

#define RKVENC_DVBM_REG_S       (0x68)
#define RKVENC_DVBM_REG_E       (0x9c)
#define RKVENC_DVBM_REG_NUM     (14)

#define RKVENC_JPEG_BASE_CFG    (0x47c)
#define JRKVENC_PEGE_ENABLE     (BIT(31))

#ifndef ROCKCHIP_DVBM_ENABLE
#define ROCKCHIP_DVBM_ENABLE    1//IS_ENABLED(CONFIG_ROCKCHIP_DVBM)
#endif

#define ISP_DEBUG               0

#define ISP_IS_WORK(mpp) \
    (mpp_read(mpp, RKVENC_DBG_DVBM_ISP1) & (RKVENC_DBG_ISP_WORK|BIT(25)))

#define DVBM_VEPU_CONNECT(mpp) \
    (mpp_read(mpp, RKVENC_DVBM_CFG) & DVBM_VEPU_CONNETC)

#define VEPU_IS_ENABLE(mpp) \
    (mpp_read(mpp, RKVENC_STATUS) & BIT(0))

#define VEPU_IS_WORK(mpp) (!!(readl(mpp->reg_base + RKVENC_DBG_TCTRL) & RKVENC_DBG_FRM_WORK))

#define VEPU_IS_ONLINE_WORK(mpp) \
    (!!(readl(mpp->reg_base + RKVENC_DBG_TCTRL) & RKVENC_DBG_FRM_WORK) && \
    !!(readl(mpp->reg_base + 0x308) & BIT(16)))

#if ROCKCHIP_DVBM_ENABLE

/* 0x60 DVBM_CFG */
union dvbm_cfg {
    u32 val;
    struct {
        u32 dvbm_en             : 1;
        u32 src_badr_sel        : 1;
        u32 dvbm_vpu_fskp       : 1;
        u32 src_oflw_drop       : 1;
        u32 dvbm_isp_cnct       : 1;
        u32 dvbm_vepu_cnct      : 1;
        u32 vepu_expt_type      : 2;
        u32 vinf_dly_cycle      : 8;
        u32 ybuf_full_mgn       : 8;
        u32 ybuf_oflw_mgn       : 8;
    };
};

/* 0x0000516c reg5211 */
union dbg_dvbm_isp1 {
    u32 val;
    struct {
        u32 dbg_isp_lcnt        : 14;
        u32 dbg_isp_ltgl        : 1;
        u32 dbg_isp_fsid        : 1;
        u32 dbg_isp_fcnt        : 8;
        u32 dbg_isp_oflw        : 1;
        u32 dbg_isp_ftgl        : 1;
        u32 dbg_isp_full        : 1;
        u32 dbg_isp_work        : 1;
        u32 dbg_isp_lvld        : 1;
        u32 dbg_isp_lrdy        : 1;
        u32 dbg_isp_fvld        : 1;
        u32 dbg_isp_frdy        : 1;
    };
};

/* 0x5170 - 0x518c */
union dbg_dvbm_buf_inf0 {
    u32 val;
    struct {
        u32 dbg_isp_lcnt        : 14;
        u32 dbg_isp_llst        : 1;
        u32 dbg_isp_sofw        : 1;
        u32 dbg_isp_fcnt        : 8;
        u32 dbg_isp_fsid        : 1;
        u32 reserved            : 5;
        u32 dbg_isp_pnt         : 1;
        u32 dbg_vpu_pnt         : 1;
    };
};

union dbg_dvbm_buf_inf1 {
    u32 val;
    struct {
        u32 dbg_src_lcnt        : 14;
        u32 dbg_src_llst        : 1;
        u32 reserved            : 1;
        u32 dbg_vpu_lcnt        : 14;
        u32 dbg_vpu_llst        : 1;
        u32 dbg_vpu_vofw        : 1;
    };
};

/* 0x00005190 reg5220 */
union dbg_dvbm_ctrl {
    u32 val;
    struct {
        u32 dbg_isp_fptr        : 3;
        u32 dbg_isp_full        : 1;
        u32 dbg_src_fptr        : 3;
        u32 dbg_dvbm_ctrl       : 1;
        u32 dbg_vpu_fptr        : 3;
        u32 dbg_vpu_empt        : 1;
        u32 dbg_vpu_lvld        : 1;
        u32 dbg_vpu_lrdy        : 1;
        u32 dbg_vpu_fvld        : 1;
        u32 dbg_vpu_frdy        : 1;
        u32 dbg_fcnt_misp       : 4;
        u32 dbg_fcnt_mvpu       : 4;
        u32 dbg_fcnt_sofw       : 4;
        u32 dbg_fcnt_vofw       : 4;
    };
};

/* rv1126b vpss base address 0x21d20000*/
#define VPSS_BASE               (0x21d20000)
#define VPSS2ENC_CTL            (0)
#define VPSS2ENC_FRM            (0xcc)
/* 0x0 VPSS_CTRL */
union vpss2enc {
    u32 val;
    struct {
        u32 reserved1           : 8;
        /*
        * Online interface selection for encoder
        * 1'b0: From ISP
        * 1'b1: From VPSS
        */
        u32 vi2enc_sel          : 1;
        /*
        * Pipeline enable from VPSS to Encoder
        * 1'b0: VPSS can not be holded by Encoder
        * 1'b1: VPSS can be holded by Encoder
        */
        u32 pipe_en             : 1;
        /*
        * VPSS to Encoder line counter selection.
        * 1'b0: Line counter from channel0 scale out
        * 1'b1: Line counter from channel1 scale out
        */
        u32 cnt_sel             : 1;
        /* VPSS to Encoder path enable */
        u32 path_en             : 1;
        /*
        * Multi-sensor index
        * 3'b000: sensor0 index
        * 3'b001: sensor1 index
        * Others: reserved
        */
        u32 sensor_id           : 3;
        u32 reserved3           : 17;
    };
};

/* 0xCC VPSS2ENC_DEBUG */
union vpss2enc_frm {
    u32 val;
    struct {
        /* VPSS to Encoder line counter */
        u32 line_cnt            : 14;
        u32 reserved1           : 2;
        /* VPSS to Encoder frame counter */
        u32 frm_cnt             : 8;
        u32 reserved2           : 7;
        /* VPSS to Encoder hold flag */
        u32 hold                : 1;
    };
};

/* rv1126b isp base address 0x21d00000*/
#define ISP_BASE                (0x21d00000)
#define ISP2ENC_CTL             (0x1c)
#define ISP2ENC_SENSOR_ID       (0x408)
#define ISP2ENC_FCNT            (0x648)
#define ISP2ENC_LCNT            (0x64c)

/* 0x1C SWS_CFG */
union isp2enc {
    u32 val;
    struct {
        /*
        * Isp2enc line counter selection.
        * 1'b0: line counter from mp out
        * 1'b1: line counter from isp out
        */
        u32 cnt_sel             : 1;
        /* Isp2pp or isp2enc pipeline handshake, would hold isp pipe when enable */
        u32 pipe_en             : 1;
        u32 reserved0           : 3;
        /* ISP to Encoder path enable */
        u32 path_en             : 1;
        u32 reserved1           : 24;
        /* ISP to Encoder path enable, shadow register. */
        u32 path_en_shd         : 1;
        /* Enc2isp pipeline hold, read only */
        u32 hold                : 1;
    };
};

/* 0x408 ACQ_H_OFFS */
union isp_sensor_id {
    u32 val;
    struct {
        u32 reserved1           : 28;
        /*
        * Multi-sensor index used
        * 3'b000: Sensor0 index
        * 3'b001: Sensor1 index
        * 3'b010: Sensor2 index
        * 3'b011: Sensor3 index
        * 3'b100: Sensor4 index
        * Then same as sensor_id in old version
        */
        u32 id                  : 3;
        u32 reserved2           : 1;
    };
};

/* 0x648 DEBUG1 */
union isp2enc_fcnt {
    u32 val;
    struct {
        /*
        * ISP to encoder frame counter
        * Frame counter from mp or isp out. MUX register 0x001c[0].
        */
        u32 frm_cnt             : 8;
        u32 reserved            : 24;
    };
};

/* 0x64C DEBUG2 */
union isp2enc_lcnt {
    u32 val;
    struct {
        /*
        * ISP to encoder line counter.
        * Line counter from mp or isp out. MUX register 0x001c[0].
        */
        u32 line_cnt            : 13;
        u32 reserved1           : 19;
    };
};

struct dvbm_stride_cfg {
    u32 y_lstrid;
    u32 y_fstrid;
    u32 c_fstrid;
};

static struct dvbm_stride_cfg dvbm_stride[2] = {
    [0] = {
        .y_lstrid = 0x80,
        .y_fstrid = 0x88,
        .c_fstrid = 0x8c,
    },
    [1] = {
        .y_lstrid = 0x90,
        .y_fstrid = 0x98,
        .c_fstrid = 0x9c,
    }
};

static const char *dvbm_src[] = {
    [DVBM_SOURCE_ISP] = "isp",
    [DVBM_SOURCE_VPSS] = "vpss",
};

static int rkvenc_dvbm_buffer_deinit(struct mpp_dvbm *dvbm)
{
    struct wrap_buf *wrap_buf = &dvbm->wrap_buf;

    if (wrap_buf->buf) {
        dma_buf_unmap_attachment(wrap_buf->attach, wrap_buf->sgt, DMA_BIDIRECTIONAL);
        dma_buf_detach(wrap_buf->buf, wrap_buf->attach);
        dma_buf_put(wrap_buf->buf);
        wrap_buf->buf = NULL;
        wrap_buf->attach = NULL;
        wrap_buf->sgt = NULL;
    }

    return 0;
}

static int rkvenc_dvbm_setup(struct mpp_dvbm *dvbm, struct dvbm_isp_cfg_t *isp_cfg)
{
    struct mpp_dev *mpp = dvbm->mpp;
    u32 isp_id = isp_cfg->chan_id;
    u32 ybuf_bot;
    u32 ybuf_top;
    u32 cbuf_bot;
    u32 cbuf_top;
    u32 ybuf_start;
    u32 cbuf_start;
    int ret = 0;

    if (IS_ERR_OR_NULL(isp_cfg->buf))
        mpp_err("dvbm buffer %px is invalid\n", isp_cfg->buf);

    if (dvbm->wrap_buf.buf && (isp_cfg->buf != dvbm->wrap_buf.buf))
        rkvenc_dvbm_buffer_deinit(dvbm);

    if (isp_cfg->buf && !dvbm->wrap_buf.buf) {
        struct dma_buf_attachment *attach;
        struct sg_table *sgt;
        u32 iova = 0;

        get_dma_buf(isp_cfg->buf);
        attach = dma_buf_attach(isp_cfg->buf, mpp->dev);
        if (IS_ERR(attach)) {
            ret = PTR_ERR(attach);
            mpp_err("dma_buf_attach %p failed(%d)\n", isp_cfg->buf, ret);
            return ret;
        }
        sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
        if (IS_ERR(sgt)) {
            ret = PTR_ERR(sgt);
            mpp_err("dma_buf_map_attachment %p failed(%d)\n", isp_cfg->buf, ret);
            dma_buf_detach(isp_cfg->buf, attach);
            return ret;
        }
        iova = sg_dma_address(sgt->sgl);
        dvbm->wrap_buf.attach = attach;
        dvbm->wrap_buf.sgt = sgt;
        dvbm->wrap_buf.buf = isp_cfg->buf;
        dvbm->wrap_buf.iova = iova;
    }
    dvbm->wrap_buf.y_top_iova = dvbm->wrap_buf.iova + isp_cfg->ybuf_top;
    dvbm->wrap_buf.c_top_iova = dvbm->wrap_buf.iova + isp_cfg->cbuf_top;
    dvbm->wrap_buf.y_bot_iova = dvbm->wrap_buf.iova + isp_cfg->ybuf_bot;
    dvbm->wrap_buf.c_bot_iova = dvbm->wrap_buf.iova + isp_cfg->cbuf_bot;

    ybuf_bot = dvbm->wrap_buf.y_bot_iova;
    ybuf_top = dvbm->wrap_buf.y_top_iova;
    cbuf_bot = dvbm->wrap_buf.c_bot_iova;
    cbuf_top = dvbm->wrap_buf.c_top_iova;
    ybuf_start = dvbm->wrap_buf.y_bot_iova;
    cbuf_start = dvbm->wrap_buf.c_bot_iova;
    dvbm->wrap_line = (ybuf_top - ybuf_bot) / isp_cfg->ybuf_lstd;
    dvbm->line_stride = isp_cfg->ybuf_lstd;

    mpp_err("chan %d y [%#08x %#08x %#08x] uv [%#08x %#08x %#08x] stride [%d %d %d] wrap_line %d\n",
            isp_id, ybuf_bot, ybuf_top, ybuf_start, cbuf_bot, cbuf_top, cbuf_start,
            isp_cfg->ybuf_fstd, isp_cfg->ybuf_lstd, isp_cfg->cbuf_fstd, dvbm->wrap_line);

    /* y start addr */
    mpp_write(mpp, 0x68, ybuf_start);
    /* uv start addr */
    mpp_write(mpp, 0x6c, cbuf_start);
    /* y top addr */
    mpp_write(mpp, 0x70, ybuf_top);
    /* c top addr */
    mpp_write(mpp, 0x74, cbuf_top);
    /* y bot addr */
    mpp_write(mpp, 0x78, ybuf_bot);
    /* c bot addr */
    mpp_write(mpp, 0x7c, cbuf_bot);

    mpp_write(mpp, dvbm_stride[isp_id].y_lstrid, isp_cfg->ybuf_lstd);
    mpp_write(mpp, dvbm_stride[isp_id].y_fstrid, isp_cfg->ybuf_fstd);
    mpp_write(mpp, dvbm_stride[isp_id].c_fstrid, isp_cfg->cbuf_fstd);

    return 0;
}

static void rkvenc_dvbm_dump(struct mpp_dev *mpp)
{
    u32 s, e, i;

    if (!unlikely(mpp_dev_debug & DEBUG_DVBM_DUMP))
        return;

    s = 0x5168;
    e = 0x51bc;

    mpp_err("reg[%#x]: 0x%08x\n", 0x4020, mpp_read(mpp, 0x4020));
    for (i = s; i <= e; i += 4) {
        u32 val = mpp_read(mpp, i);
        if (val)
            mpp_err("reg[%#x]: 0x%08x\n", i, val);
    }
}

/*
 * Fix Bug for rv1126b:
 * When the vepu-dvbm connected, the dvbm maybe skip some frames to
 * match the vepu encoding frame, it will cause a bug that falsely report a full
 * signal to isp/vpss, and the isp/vpss will be holded by the encoder, and the
 * encoder will be holded by the isp/vpss, it will cause a deadlock.
 *
 * The err full signal will be cleared when the vepu encoding start.
 *
 * Solution:
 * Disable the pipe_en to make the isp/vpss work normally,
 * and after the vepu encoding start, the err full signal will be cleared.
 */
void mpp_rkvenc_dvbm_hack(struct mpp_dvbm *dvbm)
{
    unsigned long flags;

    if (!dvbm->mpp)
        return;

    spin_lock_irqsave(&dvbm->dvbm_lock, flags);
    if (dvbm->src_frm.source_id != dvbm->enc_frm.source_id ||
        dvbm->src_frm.frame_id != dvbm->enc_frm.frame_id) {
        spin_unlock_irqrestore(&dvbm->dvbm_lock, flags);
        return;
    }

    switch (dvbm->source) {
    case DVBM_SOURCE_ISP: {
        union isp2enc isp2enc = { .val = readl(dvbm->isp_base + ISP2ENC_CTL) };
        union isp2enc_lcnt lcnt = { .val = readl(dvbm->isp_base + ISP2ENC_LCNT) };

        if (isp2enc.pipe_en && isp2enc.hold && !lcnt.line_cnt) {
            /* disable pipe en to release hold */
            isp2enc.pipe_en = 0;
            writel(isp2enc.val, dvbm->isp_base + ISP2ENC_CTL);
            if (mpp_debug_unlikely(DEBUG_ISP_INFO)) {
                isp2enc.val = readl(dvbm->isp_base + ISP2ENC_CTL);
                mpp_debug(DEBUG_ISP_INFO, "isp pipe %d hold %d vepu_wrk %d\n",
                          isp2enc.pipe_en, isp2enc.hold, VEPU_IS_WORK(dvbm->mpp));
            }
        }
    } break;
    case DVBM_SOURCE_VPSS: {
        union vpss2enc vpss2enc = { .val = readl(dvbm->vpss_base + VPSS2ENC_CTL) };
        union vpss2enc_frm frm = { .val = readl(dvbm->vpss_base + VPSS2ENC_FRM) };

        if (vpss2enc.pipe_en && frm.hold && !frm.line_cnt) {
            /* disable pipe en to release hold */
            vpss2enc.pipe_en = 0;
            writel(vpss2enc.val, dvbm->vpss_base + VPSS2ENC_CTL);
            if (mpp_debug_unlikely(DEBUG_ISP_INFO)) {
                vpss2enc.val = readl(dvbm->vpss_base + VPSS2ENC_CTL);
                frm.val = readl(dvbm->vpss_base + VPSS2ENC_FRM);
                mpp_debug(DEBUG_ISP_INFO, "vpss pipe %d hold %d vepu_wrk %d\n",
                          vpss2enc.pipe_en, frm.hold, VEPU_IS_WORK(dvbm->mpp));
            }
        }
    } break;
    default: {
    } break;
    }
    spin_unlock_irqrestore(&dvbm->dvbm_lock, flags);
}

static int rkvenc_dvbm_callback(void *ctx, enum dvbm_cb_event event, void *arg)
{
    struct mpp_dvbm *dvbm = (struct mpp_dvbm *)ctx;
    struct mpp_dev *mpp = dvbm->mpp;

    switch (event) {
    case DVBM_ISP_REQ_CONNECT:
    case DVBM_VPSS_REQ_CONNECT: {
        u32 source_id = *(u32*)arg;
        union dvbm_cfg dvbm_cfg = { .val = mpp_read(mpp, RKVENC_DVBM_CFG) };

        dvbm->source = event == DVBM_ISP_REQ_CONNECT ? DVBM_SOURCE_ISP : DVBM_SOURCE_VPSS;
        if (mpp->online_mode == MPP_ENC_ONLINE_MODE_SW)
            return 0;

        mpp_err("%s %d connect dvbm %#x\n", dvbm_src[dvbm->source], source_id, dvbm_cfg.val);
        if (!VEPU_IS_ONLINE_WORK(mpp)) {
            dvbm_cfg.dvbm_isp_cnct = 1;
            dvbm_cfg.dvbm_en = 1;
            mpp_write(mpp, RKVENC_DVBM_CFG, dvbm_cfg.val);
        }
        atomic_set(&dvbm->src_fcnt, 0);
        set_bit(source_id, &dvbm->dvbm_setup);
    } break;
    case DVBM_ISP_REQ_DISCONNECT:
    case DVBM_VPSS_REQ_DISCONNECT: {
        u32 source_id = *(u32*)arg;

        if (mpp->online_mode == MPP_ENC_ONLINE_MODE_SW)
            return 0;
        clear_bit(source_id, &dvbm->dvbm_setup);
        mpp_err("%s %d disconnect 0x%lx\n",
                dvbm_src[dvbm->source], source_id, dvbm->dvbm_setup);
        if (!dvbm->dvbm_setup) {
            mpp_write(mpp, RKVENC_DVBM_CFG, 0);
            mpp->always_on = 0;
            atomic_set(&dvbm->src_fcnt, 0);
            rkvenc_dvbm_buffer_deinit(dvbm);
        }
    } break;
    case DVBM_ISP_SET_DVBM_CFG:
    case DVBM_VPSS_SET_DVBM_CFG: {
        struct dvbm_isp_cfg_t *isp_cfg = (struct dvbm_isp_cfg_t *)arg;

        if (mpp->online_mode == MPP_ENC_ONLINE_MODE_SW)
            return 0;
        mpp_power_on(mpp);
        mpp->always_on = 1;
        if (!VEPU_IS_ONLINE_WORK(mpp))
            mpp_write(mpp, RKVENC_DVBM_CFG, 0);
        rkvenc_dvbm_setup(dvbm, isp_cfg);
    } break;
    case DVBM_VEPU_NOTIFY_FRM_STR: {
        u32 id = *(u32*)arg;

        if (dvbm->isp_base && dvbm->vpss_base) {
            /* isp */
            if (dvbm->source == DVBM_SOURCE_ISP) {
                union isp2enc_fcnt fcnt = { .val = readl(dvbm->isp_base + ISP2ENC_FCNT) };
                union isp_sensor_id sensor_id = { .val = readl(dvbm->isp_base + ISP2ENC_SENSOR_ID) };

                dvbm->src_frm.source_id = sensor_id.id;
                dvbm->src_frm.frame_id = fcnt.frm_cnt;

                if (mpp_debug_unlikely(DEBUG_ISP_INFO)) {
                    union isp2enc_lcnt lcnt = { .val = readl(dvbm->isp_base + ISP2ENC_LCNT) };
                    union isp2enc isp2enc = { .val = readl(dvbm->isp_base + ISP2ENC_CTL) };
                    u32 dvbm_cfg = readl(mpp->reg_base + RKVENC_DVBM_CFG);

                    mpp_err("isp frame %d start isp2enc %d pipe %d hold %d frm [%d %d %d] dvbm %#x\n",
                            id, isp2enc.path_en, isp2enc.pipe_en, isp2enc.hold, sensor_id.id,
                            fcnt.frm_cnt, lcnt.line_cnt, dvbm_cfg);
                }
            } else {
                union vpss2enc vpss2enc = { .val = readl(dvbm->vpss_base + VPSS2ENC_CTL) };
                union vpss2enc_frm frm = { .val = readl(dvbm->vpss_base + VPSS2ENC_FRM) };

                dvbm->src_frm.source_id = vpss2enc.sensor_id;
                dvbm->src_frm.frame_id = frm.frm_cnt;

                if (mpp_debug_unlikely(DEBUG_ISP_INFO)) {
                    u32 dvbm_cfg = readl(mpp->reg_base + RKVENC_DVBM_CFG);

                    mpp_err("vpss frame %d start vpss2enc %d pipe %d lcnt_sel %d hold %d frm [%d %d %d] dvbm 0x%08x\n",
                            id, vpss2enc.path_en, vpss2enc.pipe_en, vpss2enc.cnt_sel, frm.hold, vpss2enc.sensor_id,
                            frm.frm_cnt, frm.line_cnt, dvbm_cfg);
                }
            }
            mpp_rkvenc_dvbm_hack(dvbm);
        } else
            mpp_debug(DEBUG_ISP_INFO, "%s frame %d start\n", dvbm_src[dvbm->source], id);
        atomic_inc(&dvbm->src_fcnt);
        dvbm->src_start_time = ktime_get();
        if (dvbm->src_end_time)
            dvbm->src_vblank = ktime_us_delta(dvbm->src_start_time, dvbm->src_end_time);
    } break;
    case DVBM_VEPU_NOTIFY_FRM_END: {
        u32 id = *(u32*)arg;

        dvbm->src_end_time = ktime_get();
        if (dvbm->src_end_time)
            dvbm->src_time = ktime_us_delta(dvbm->src_end_time, dvbm->src_start_time);

        if (!mpp_debug_unlikely(DEBUG_ISP_INFO))
            break;

        if (dvbm->isp_base && dvbm->vpss_base) {
            u32 dvbm_cfg = readl(mpp->reg_base + RKVENC_DVBM_CFG);

            /* isp */
            if (dvbm->source == DVBM_SOURCE_ISP) {
                union isp2enc_fcnt fcnt = { .val = readl(dvbm->isp_base + ISP2ENC_FCNT) };
                union isp2enc_lcnt lcnt = { .val = readl(dvbm->isp_base + ISP2ENC_LCNT) };
                union isp_sensor_id sensor_id = { .val = readl(dvbm->isp_base + ISP2ENC_SENSOR_ID) };
                union isp2enc isp2enc = { .val = readl(dvbm->isp_base + ISP2ENC_CTL) };

                mpp_debug(DEBUG_ISP_INFO, "isp frame %d end isp2enc %d pipe %d hold %d frm [%d %d %d] dvbm %#x\n",
                          id, isp2enc.path_en, isp2enc.pipe_en, isp2enc.hold, sensor_id.id,
                          fcnt.frm_cnt, lcnt.line_cnt, dvbm_cfg);
            } else {
                union vpss2enc vpss2enc = { .val = readl(dvbm->vpss_base + VPSS2ENC_CTL) };
                union vpss2enc_frm frm = { .val = readl(dvbm->vpss_base + VPSS2ENC_FRM) };

                mpp_debug(DEBUG_ISP_INFO, "vpss frame %d end vpss2enc %d pipe %d lcnt_sel %d hold %d frm [%d %d %d] dvbm 0x%08x\n",
                          id, vpss2enc.path_en, vpss2enc.pipe_en, vpss2enc.cnt_sel, frm.hold, vpss2enc.sensor_id,
                          frm.frm_cnt, frm.line_cnt, dvbm_cfg);
            }
        } else
            mpp_debug(DEBUG_ISP_INFO, "%s frame %d end\n", dvbm_src[dvbm->source], id);
        rkvenc_dvbm_dump(mpp);
    } break;
    default : {
    } break;
    }

    return 0;
}

int mpp_rkvenc_dvbm_deinit(struct mpp_dvbm *dvbm)
{
    if (dvbm->dvbm_port) {
        rk_dvbm_put(dvbm->dvbm_port);
        dvbm->dvbm_port = NULL;
    }

    if (dvbm->isp_base) {
        iounmap(dvbm->isp_base);
        dvbm->isp_base = NULL;
    }

    if (dvbm->vpss_base) {
        iounmap(dvbm->vpss_base);
        dvbm->vpss_base = NULL;
    }

    return 0;
}

int mpp_rkvenc_dvbm_init(struct mpp_dvbm *dvbm, struct mpp_dev *mpp)
{
    struct device_node *np_dvbm = NULL;
    struct platform_device *pdev_dvbm = NULL;
    struct device *dev = mpp->dev;

    np_dvbm = of_parse_phandle(mpp_dev_of_node(dev), "dvbm", 0);
    if (!np_dvbm || !of_device_is_available(np_dvbm))
        mpp_err("failed to get device node\n");
    else {
        pdev_dvbm = of_find_device_by_node(np_dvbm);
        dvbm->dvbm_port = rk_dvbm_get_port(pdev_dvbm, DVBM_VEPU_PORT);
        dvbm->mpp = mpp;
        of_node_put(np_dvbm);
        if (dvbm->dvbm_port) {
            struct dvbm_cb dvbm_cb;

            dvbm_cb.cb = rkvenc_dvbm_callback;
            dvbm_cb.ctx = dvbm;

            rk_dvbm_set_cb(dvbm->dvbm_port, &dvbm_cb);
            dvbm->dvbm_setup = 0;
        }
        dvbm->isp_base = ioremap(ISP_BASE, 0x650);
        if (!dvbm->isp_base)
            dev_err(mpp->dev, "isp base map failed!\n");
        /* vpss */
        dvbm->vpss_base = ioremap(VPSS_BASE, 0xf0);
        if (!dvbm->vpss_base)
            dev_err(mpp->dev, "vpss base map failed!\n");
    }

    return 0;
}

void mpp_rkvenc_dvbm_update(struct mpp_dvbm *dvbm, struct mpp_task *mpp_task, u32 online)
{
    struct wrap_buf *wrap_buf = &dvbm->wrap_buf;
    struct mpp_dev *mpp = dvbm->mpp;
    u32 frame_id = mpp_task->frame_id;
    u32 pipe_id = mpp_task->pipe_id;
    u32 enc_id;
    unsigned long flags;

    if (!mpp)
        return;

    if (!wrap_buf->buf)
        dev_err(mpp->dev, "the dvbm address do not ready!\n");

    enc_id = mpp_read(mpp, 0x308);
    if (frame_id > 0xff) {
        mpp_err("invalid frame id %d\n", frame_id);
        frame_id &= 0xff;
    }

    if (pipe_id > 1) {
        mpp_err("invalid pipe id %d\n", pipe_id);
        pipe_id &= 0x1;
    }

    /* frame id and source id match */
    if (online == MPP_ENC_ONLINE_MODE_HW) {
        enc_id |= BIT(8) | BIT(13);
        spin_lock_irqsave(&dvbm->dvbm_lock, flags);
        dvbm->enc_frm.source_id = pipe_id;
        dvbm->enc_frm.frame_id = frame_id;
        spin_unlock_irqrestore(&dvbm->dvbm_lock, flags);
    }

    enc_id |= frame_id | (pipe_id << 12);
    mpp_write_relaxed(mpp, 0x308, enc_id);

    mpp_write_relaxed(mpp, 0x4020, 0);
    /* update wrap addr */
    mpp_write_relaxed(mpp, 0x270, wrap_buf->y_top_iova);
    mpp_write_relaxed(mpp, 0x274, wrap_buf->c_top_iova);
    mpp_write_relaxed(mpp, 0x278, wrap_buf->y_bot_iova);
    mpp_write_relaxed(mpp, 0x27c, wrap_buf->c_bot_iova);
    if (online == MPP_ENC_ONLINE_MODE_SW) {
        mpp_write_relaxed(mpp, 0x280, wrap_buf->y_bot_iova);
        mpp_write_relaxed(mpp, 0x284, wrap_buf->c_bot_iova);
        mpp_write_relaxed(mpp, 0x288, wrap_buf->c_bot_iova);
    }
    if (mpp_read_relaxed(mpp, RKVENC_JPEG_BASE_CFG) & JRKVENC_PEGE_ENABLE) {
        mpp_write_relaxed(mpp, 0x418, wrap_buf->y_top_iova);
        mpp_write_relaxed(mpp, 0x41c, wrap_buf->c_top_iova);
        mpp_write_relaxed(mpp, 0x410, wrap_buf->y_bot_iova);
        mpp_write_relaxed(mpp, 0x414, wrap_buf->c_bot_iova);
        if (online == MPP_ENC_ONLINE_MODE_SW) {
            mpp_write_relaxed(mpp, 0x420, wrap_buf->y_bot_iova);
            mpp_write_relaxed(mpp, 0x424, wrap_buf->c_bot_iova);
            mpp_write_relaxed(mpp, 0x428, wrap_buf->c_bot_iova);
        }
    }
}

static void rkvenc_dvbm_show_info(struct mpp_dvbm *dvbm)
{
    struct mpp_dev *mpp = dvbm->mpp;
    u32 isp_info_addr[4] = {0x5170, 0x5178, 0x5180, 0x5188};
    u32 isp_info;
    u32 valid_idx;
    u32 dvbm_ctl = mpp_read(mpp, 0x5190);
    u32 isp1 = mpp_read(mpp, 0x516c);

    valid_idx = dvbm_ctl & 0x3;
    isp_info = mpp_read(mpp, isp_info_addr[valid_idx]);

    mpp_dbg_dvbm("fcnt %d lcnt %d\n", (isp_info >> 16) & 0xff, isp_info & 0x3fff);
    mpp_dbg_dvbm("isp0 fcnt %d isp1 0x%08x fcnt %d lcnt %d\n",
                 mpp_read(mpp, 0x5168) & 0xff, isp1, (isp1 >> 16) & 0xff, isp1 & 0x3fff);
}

int mpp_rkvenc_dvbm_connect(struct mpp_dvbm *dvbm, u32 dvbm_cfg)
{
    struct mpp_dev *mpp = dvbm->mpp;
    unsigned long flag;
    union dvbm_cfg cfg;

    if (!mpp)
        return 0;

    dvbm_cfg = dvbm_cfg ? dvbm_cfg : mpp_read(mpp, RKVENC_DVBM_CFG);
    spin_lock_irqsave(&dvbm->dvbm_lock, flag);
    if (DVBM_VEPU_CONNECT(mpp)) {
        dvbm->skip_dvbm_discnct = !!mpp->cur_task;
        goto done;
    }
    cfg.val = dvbm_cfg;
    if (!cfg.ybuf_full_mgn) {
        cfg.ybuf_full_mgn = ALIGN(dvbm->line_stride * 8, 4096) / 4096;
        cfg.vinf_dly_cycle = 2;
    }
    cfg.dvbm_vepu_cnct = 0;
    cfg.dvbm_isp_cnct = 1;
    mpp_write(mpp, RKVENC_DVBM_CFG, cfg.val);
    cfg.dvbm_vepu_cnct = 1;
    mpp_write(mpp, RKVENC_DVBM_CFG, cfg.val);
done:
    spin_unlock_irqrestore(&dvbm->dvbm_lock, flag);
    rkvenc_dvbm_show_info(dvbm);

    return 0;
}

int mpp_rkvenc_dvbm_disconnect(struct mpp_dvbm *dvbm, u32 dvbm_cfg)
{
    struct mpp_dev *mpp = dvbm->mpp;
    unsigned long flag;

    if (!mpp)
        return 0;

    dvbm_cfg = dvbm_cfg ? dvbm_cfg : mpp_read(mpp, RKVENC_DVBM_CFG);
    spin_lock_irqsave(&dvbm->dvbm_lock, flag);
    dvbm->skip_dvbm_discnct = false;
    /* check if encode online task */
    if (VEPU_IS_ENABLE(mpp) && (mpp_read(mpp, 0x308) & BIT(16)))
        goto done;

    if (!(dvbm_cfg & DVBM_VEPU_CONNETC))
        goto done;

    dvbm_cfg &= ~DVBM_VEPU_CONNETC;
    mpp_write(mpp, RKVENC_DVBM_CFG, dvbm_cfg);
done:
    spin_unlock_irqrestore(&dvbm->dvbm_lock, flag);

    return 0;
}

void mpp_rkvenc_dvbm_clear(struct mpp_dvbm *dvbm)
{
    struct mpp_dev *mpp = dvbm->mpp;
    u32 dvbm_info, dvbm_en;

    if (!mpp)
        return;

    mpp_write(mpp, 0x60, 0);
    mpp_write(mpp, 0x18, 0);
    dvbm_info = mpp_read(mpp, 0x18);
    dvbm_en = mpp_read(mpp, 0x60);
    if (dvbm_info || dvbm_en)
        pr_err("clear dvbm info failed 0x%08x 0x%08x\n",
               dvbm_info, dvbm_en);
}

static void check_dvbm_err(struct mpp_dvbm *dvbm, struct mpp_task *mpp_task)
{
    struct mpp_dev *mpp = dvbm->mpp;
    u32 dvbm_cfg = mpp_read(mpp, RKVENC_DVBM_CFG);
    struct mpp_session *session = mpp_task->session;
    char err[128];
    u32 len = 0;
    u32 size = sizeof(err) - 1;
    union st_enc_u status = { .val = mpp_read(mpp, RKVENC_STATUS) };

    if (status.vepu_src_oflw)
        len += snprintf(err + len, size - len, "wrap overflow");

    if (status.vepu_sid_nmch)
        len += snprintf(err + len, size - len, "sid mismatch");

    if (status.vepu_fcnt_nmch)
        len += snprintf(err + len, size - len, "fid mismatch");

    mpp_err("chan %d task %d frame [%d %d] %s st 0x%08x cfg %#x wrap_line %d\n",
            session->chn_id, mpp_task->task_index, mpp_task->pipe_id,
            mpp_task->frame_id, err, status.val, dvbm_cfg, dvbm->wrap_line);
    mpp_err("src time %lld us vblank %lld us\n", dvbm->src_time, dvbm->src_vblank);

    {
        u32 buf_inf0s[4] = {0x5170, 0x5178, 0x5180, 0x5188};
        u32 buf_inf1s[4] = {0x5174, 0x517c, 0x5184, 0x518c};
        union dbg_dvbm_ctrl dbg_ctl = { .val = mpp_read(mpp, RKVENC_DBG_DVBM_CTRL) };
        union dbg_dvbm_buf_inf0 buf_inf0 = { .val = mpp_read(mpp, buf_inf0s[dbg_ctl.dbg_isp_fptr & 3]) };
        union dbg_dvbm_buf_inf1 buf_inf1 = { .val = mpp_read(mpp, buf_inf1s[dbg_ctl.dbg_src_fptr & 3]) };

        mpp_err("src [%d %d %d] -> enc [%d %d %d]\n",
                buf_inf0.dbg_isp_fsid, buf_inf0.dbg_isp_fcnt, buf_inf0.dbg_isp_lcnt,
                dvbm->enc_frm.source_id, dvbm->enc_frm.frame_id, buf_inf1.dbg_vpu_lcnt);
    }
}

static int mpp_rkvenc_hw_dvbm_hdl(struct mpp_dvbm *dvbm, struct mpp_task *mpp_task)
{
    struct mpp_dev *mpp = dvbm->mpp;
    union st_enc_u enc_st = { .val = mpp_read(mpp, RKVENC_STATUS) };
    u32 dvbm_cfg = mpp_read(mpp, RKVENC_DVBM_CFG);
    struct mpp_session *session = mpp_task->session;

    if (mpp->irq_status & RKVENC_SOURCE_ERR)
        check_dvbm_err(dvbm, mpp_task);

    mpp_dbg_dvbm("chan %d task %d st 0x%08x dvbm_cfg 0x%08x\n",
                 session->chn_id, mpp_task->task_index, enc_st.val, dvbm_cfg);

    if (!(mpp->irq_status & 0x7fff) && !enc_st.dvbm_isp_oflw)
        return 1;

    if (dvbm->skip_dvbm_discnct) {
        if ((mpp->irq_status & RKVENC_SOURCE_ERR)) {
            dvbm_cfg &= ~DVBM_VEPU_CONNETC;
            mpp_write(mpp, RKVENC_DVBM_CFG, dvbm_cfg);

            mpp_write(mpp, RKVENC_STATUS, 0);

            mpp_write(mpp, RKVENC_DVBM_CFG, dvbm_cfg);
            mpp_write(mpp, RKVENC_DVBM_CFG, dvbm_cfg | DVBM_VEPU_CONNETC);
        }
        dvbm->skip_dvbm_discnct = false;
    } else {
        if (mpp->irq_status & 0x7fff) {
            dvbm_cfg &= ~DVBM_VEPU_CONNETC;
            mpp_write(mpp, RKVENC_DVBM_CFG, dvbm_cfg);
        }
        mpp_write(mpp, RKVENC_STATUS, 0);
    }

    return 0;
}

static int mpp_rkvenc_sw_dvbm_hdl(struct mpp_dvbm *dvbm, struct mpp_task *mpp_task)
{
    struct mpp_dev *mpp = dvbm->mpp;
    // struct rkvenc2_session_priv *priv = mpp_task->session->priv;
    u32 line_cnt;

    line_cnt = mpp_read(mpp, 0x18) & 0x3fff;
    mpp_rkvenc_dvbm_clear(dvbm);
    /*
    * Workaround:
    * The line cnt is updated in the mcu.
    * When line cnt is set to max value 0x3fff,
    * the cur frame has overflow.
    */
    if (line_cnt == 0x3fff) {
        mpp->irq_status |= BIT(6);
        // priv->info.wrap_overflow_cnt++;
        mpp_dbg_warning("current task %d has overflow\n", mpp_task->task_index);
    }
    /*
    * Workaround:
    * The line cnt is updated in the mcu.
    * When line cnt is set to max value 0x3ffe,
    * the cur frame source id is mismatch.
    */
    if (line_cnt == 0x3ffe) {
        mpp->irq_status |= BIT(16);
        // priv->info.wrap_source_mis_cnt++;
    }

    return 0;
}

int mpp_rkvenc_dvbm_handle(struct mpp_dvbm *dvbm, struct mpp_task *mpp_task)
{
    struct mpp_session *session = mpp_task->session;

    if (!dvbm->mpp)
        return 0;

    if (session->online == MPP_ENC_ONLINE_MODE_HW)
        return mpp_rkvenc_hw_dvbm_hdl(dvbm, mpp_task);
    else
        return mpp_rkvenc_sw_dvbm_hdl(dvbm, mpp_task);
}

void mpp_rkvenc_dvbm_reg_sav_restore(struct mpp_dvbm *dvbm, bool is_save)
{
    struct mpp_dev *mpp = dvbm->mpp;
    u32 off, i;

    if (!mpp)
        return;

    if (is_save) {
        dvbm->dvbm_cfg = mpp_read(mpp, RKVENC_DVBM_CFG);
        if (!(dvbm->dvbm_cfg & RKVENC_DVBM_EN))
            return;
        for (off = RKVENC_DVBM_REG_S; off <= RKVENC_DVBM_REG_E; off += 4) {
            i = (off - RKVENC_DVBM_REG_S) / 4;
            dvbm->dvbm_reg_save[i] = mpp_read(mpp, off);
        }
    } else {
        u32 dvbm_cfg;

        if (!(dvbm->dvbm_cfg & RKVENC_DVBM_EN))
            return;

        for (off = RKVENC_DVBM_REG_S; off <= RKVENC_DVBM_REG_E; off += 4) {
            i = (off - RKVENC_DVBM_REG_S) / 4;
            mpp_write(mpp, off, dvbm->dvbm_reg_save[i]);
        }
        dvbm_cfg = dvbm->dvbm_cfg & (~DVBM_VEPU_CONNETC);
        mpp_write(mpp, RKVENC_DVBM_CFG, dvbm_cfg);
    }
}
#endif

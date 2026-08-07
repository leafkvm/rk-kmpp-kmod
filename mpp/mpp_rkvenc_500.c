// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd
 */

#define pr_fmt(fmt) "rkvenc_500: " fmt

#include <asm/cacheflush.h>
#include <linux/delay.h>
#include <linux/devfreq.h>
#include <linux/devfreq_cooling.h>
#include <linux/iopoll.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/proc_fs.h>
#include <linux/pm_runtime.h>
#include <linux/nospec.h>
#include <linux/workqueue.h>
#ifdef CONFIG_ARM_DMA_USE_IOMMU
#include <asm/dma-iommu.h>
#endif
#include <soc/rockchip/pm_domains.h>
#include <soc/rockchip/rockchip_ipa.h>
#include <soc/rockchip/rockchip_opp_select.h>
#include <soc/rockchip/rockchip_system_monitor.h>

#include "rk-mpp.h"
#include "rk-dvbm.h"

#include "mpp_osal.h"
#include "mpp_debug.h"
#include "mpp_iommu.h"
#include "mpp_common.h"

#define RKVENC_DRIVER_NAME          "mpp_rkvenc_500"
#define RKVENC_WORK_TIMEOUT_DELAY   (200)

//#define ROCKCHIP_DVBM_ENABLE  IS_ENABLED(CONFIG_ROCKCHIP_DVBM)
#define ROCKCHIP_DVBM_ENABLE    1
#define ISP_DEBUG 0

/* irq status definition */
#define RKVENC_JPEG_OVERFLOW        (BIT(13))
#define RKVENC_SOURCE_ERR           (BIT(7))
#define RKVENC_VIDEO_OVERFLOW       (BIT(4))
#define RKVENC_SCLR_DONE_STA        (BIT(2))
#define RKVENC_ENC_DONE_STATUS      (BIT(0))
#define REC_FBC_DIS_CLASS_OFFSET    (36)

#define RKVENC_RSL                  (0x310)
#define RKVENC_JPEG_RSL             (0x440)

#define RKVENC_JPEG_BASE_CFG        (0x47c)
#define JRKVENC_PEGE_ENABLE         (BIT(31))

#define RKVENC_VIDEO_BSBT           (0x2b0)
#define RKVENC_VIDEO_BSBB           (0x2b4)
#define RKVENC_VIDEO_BSBS           (0x2b8)
#define RKVENC_VIDEO_BSBR           (0x2bc)
#define RKVENC_JPEG_BSBT            (0x400)
#define RKVENC_JPEG_BSBB            (0x404)
#define RKVENC_JPEG_BSBS            (0x408)
#define RKVENC_JPEG_BSBR            (0x40c)

#define RKVENC_STATUS               (0x4020)

/* dvbm regs */
#define RKVENC_DVBM_CFG             (0x60)
#define RKVENC_DVBM_EN              (BIT(0))
#define VEPU_CONNETC_CUR            (BIT(2))
#define DVBM_ISP_CONNETC            (BIT(4))
#define DVBM_VEPU_CONNETC           (BIT(5))
#define RKVENC_DBG_DVBM_ISP1        (0x516c)
#define RKVENC_DBG_ISP_WORK         (BIT(27))

#define RKVENC_DVBM_REG_S           (0x68)
#define RKVENC_DVBM_REG_E           (0x9c)
#define RKVENC_DVBM_REG_NUM         (14)

#define RKVENC_REG_ENC_PIC          (36)
#define RKVENC_BIT_SLEN_FIFO        BIT(30)
#define RKVENC_REG_SLI_SPLIT        (60)
#define RKVENC_BIT_SLI_SPLIT        BIT(0)

#define RKVENC_REG_SLICE_NUM_BASE   (0x4034)
#define RKVENC_REG_SLICE_LEN_BASE   (0x4038)
#define RKVENC_BIT_SLI_LST          (BIT(30))

#define ISP_IS_WORK(mpp) \
    (mpp_read(mpp, RKVENC_DBG_DVBM_ISP1) & (RKVENC_DBG_ISP_WORK|BIT(25)))

#define DVBM_VEPU_CONNECT(mpp) \
    mpp_read(mpp, RKVENC_DVBM_CFG) & DVBM_VEPU_CONNETC

#define to_rkvenc_info(info) \
        container_of(info, struct rkvenc_hw_info, hw)
#define to_rkvenc_task(ctx) \
        container_of(ctx, struct rkvenc_task, mpp_task)
#define to_rkvenc_dev(dev) \
        container_of(dev, struct rkvenc_dev, mpp)

#define task_type_str(task) (task->pp_info != NULL ? "pp" : "enc")

/* reg define for vepu_pp */
enum {
    INT_EN          = 0x20,
    INT_MSK         = 0x24,
    SRC0_ADDR       = 0x280,
    SRC1_ADDR       = 0x284,
    SRC2_ADDR       = 0x288,
    DSPW_ADDR       = 0x2a4,
    DSPR_ADDR       = 0x2a8,
    ENC_PIC         = 0x300,
    ENC_ID          = 0x308,
    ENC_RSL         = 0x310,
    SRC_FILL        = 0x314,
    SRC_FMT         = 0x318,
    PIC_OFST        = 0x330,
    SRC_STRD0       = 0x334,
    SRC_STRD1       = 0x338,
    SRC_FLT_CFG     = 0x33c,
    ME_RNGE         = 0x370,
    ME_CFG          = 0x374,
    ME_CACH         = 0x378,
    SYNT_SLI0       = 0x3bc,
    JPEG_BASE_CFG   = 0x47c,
    PP_MD_ADR       = 0x520,
    PP_OD_ADR       = 0x524,
    PP_REF_MDW_ADR  = 0x528,
    PP_REF_MDR_ADR  = 0x52c,
    PP_BASE_CFG     = 0x530,
    PP_MD_THD       = 0x534,
    PP_OD_THD       = 0x538,
};

struct rkvenc_pp_param {
    u32 int_en;
    u32 int_msk;
    u32 src0_addr;
    u32 src1_addr;
    u32 src2_addr;
    u32 dspw_addr;
    u32 dspr_addr;
    u32 enc_pic;
    u32 enc_id;
    u32 enc_rsl;
    u32 src_fill;
    u32 src_fmt;
    u32 pic_ofst;
    u32 src_strd0;
    u32 src_strd1;
    u32 src_flt_cfg;
    u32 me_rnge;
    u32 me_cfg;
    u32 me_cach;
    u32 synt_sli0;
    u32 jpeg_base_cfg;
    u32 pp_md_adr;
    u32 pp_od_adr;
    u32 pp_ref_mdw_adr;
    u32 pp_ref_mdr_adr;
    u32 pp_base_cfg;
    u32 pp_md_thd;
    u32 pp_od_thd;
};

struct rkvenc_pp_out {
    u32 luma_pix_sum_od;
};

struct rkvenc_pp_info {
    struct rkvenc_pp_param param;
    struct rkvenc_pp_out output;
};

enum RKVENC_FORMAT_TYPE {
    RKVENC_FMT_BASE         = 0x0000,
    RKVENC_FMT_H264E        = RKVENC_FMT_BASE + 0,
    RKVENC_FMT_H265E        = RKVENC_FMT_BASE + 1,
    RKVENC_FMT_JPEGE        = RKVENC_FMT_BASE + 2,
    RKVENC_FMT_BUTT,

    RKVENC_FMT_OSD_BASE     = RKVENC_FMT_BUTT,
    RKVENC_FMT_H264E_OSD    = RKVENC_FMT_OSD_BASE + 0,
    RKVENC_FMT_H265E_OSD    = RKVENC_FMT_OSD_BASE + 1,
    RKVENC_FMT_JPEGE_OSD    = RKVENC_FMT_OSD_BASE + 2,
    RKVENC_FMT_OSD_BUTT,
};

enum RKVENC_CLASS_TYPE {
    RKVENC_CLASS_BASE       = 0,    /* base */
    RKVENC_CLASS_PIC        = 1,    /* picture configure */
    RKVENC_CLASS_RC         = 2,    /* rate control */
    RKVENC_CLASS_PAR        = 3,    /* parameter */
    RKVENC_CLASS_SQI        = 4,    /* subjective Adjust */
    RKVENC_CLASS_SCL_JPGTBL = 5,    /* scaling list and jpeg table*/
    RKVENC_CLASS_OSD        = 6,    /* osd */
    RKVENC_CLASS_ST         = 7,    /* status */
    RKVENC_CLASS_DEBUG      = 8,    /* debug */
    RKVENC_CLASS_BUTT,
};

enum RKVENC_CLASS_FD_TYPE {
    RKVENC_CLASS_FD_BASE    = 0,    /* base */
    RKVENC_CLASS_FD_OSD     = 1,    /* osd */
    RKVENC_CLASS_FD_BUTT,
};

struct rkvenc_reg_msg {
    u32 base_s;
    u32 base_e;
};

struct rkvenc_hw_info {
    struct mpp_hw_info hw;
    /* for register range check */
    u32 reg_class;
    struct rkvenc_reg_msg reg_msg[RKVENC_CLASS_BUTT];
    /* for fd translate */
    u32 fd_class;
    struct {
        u32 class;
        u32 base_fmt;
    } fd_reg[RKVENC_CLASS_FD_BUTT];
    /* for get format */
    struct {
        u32 class;
        u32 base;
        u32 bitpos;
        u32 bitlen;
    } fmt_reg;
    /* register info */
    u32 enc_start_base;
    u32 enc_clr_base;
    u32 int_en_base;
    u32 int_mask_base;
    u32 int_clr_base;
    u32 int_sta_base;
    u32 enc_wdg_base;
    u32 err_mask;
};

struct rkvenc_task {
    struct mpp_task mpp_task;
    int fmt;
    struct rkvenc_hw_info *hw_info;

    /* class register */
    struct {
        u32 valid;
        u32 *data;
        u32 size;
        u32 offset;
    } reg[RKVENC_CLASS_BUTT];

    enum MPP_CLOCK_MODE clk_mode;
    u32 irq_status;
    /* req for current task */
    u32 w_req_cnt;
    struct mpp_request w_reqs[MPP_MAX_MSG_NUM];
    u32 r_req_cnt;
    struct mpp_request r_reqs[MPP_MAX_MSG_NUM];
    struct mpp_dma_buffer *table;
    u32 task_no;
    struct rkvenc_pp_info *pp_info;
    /* split fifo */
    u32 task_split;
    u32 last_slice_found;
    MppEncSliceInfo sli_info;
};

union st_enc_u {
    u32 val;
    struct {
        u32 st_enc          : 2;
        u32 st_sclr         : 1;
        u32 vepu_fbd_err    : 5;
        u32 isp_src_oflw    : 1;
        u32 vepu_src_oflw   : 1;
        u32 vepu_sid_nmch   : 1;
        u32 vepu_fcnt_nmch  : 1;
        u32 reserved        : 4;
        u32 dvbm_finf_wful  : 1;
        u32 dvbm_linf_wful  : 1;
        u32 dvbm_fsid_nmch  : 1;
        u32 dvbm_fcnt_early : 1;
        u32 dvbm_fcnt_late  : 1;
        u32 dvbm_isp_oflw   : 1;
        u32 dvbm_vepu_oflw  : 1;
        u32 isp_time_out    : 1;
        u32 dvbm_vsrc_fcnt  : 8;
    };
};

struct rkvenc_debug_info {
    /* normal info */
    u32 hw_running;

    /* err info */
    u32 wrap_overflow_cnt;
    u32 bsbuf_overflow_cnt;
    u32 wrap_source_mis_cnt;
    /* record bs buf top/bot/write/read addr */
    u32 bsbuf_info[4];
    u32 enc_err_cnt;
    u32 enc_err_irq;
    u32 enc_err_status;
};

struct rkvenc2_session_priv {
    struct rw_semaphore rw_sem;
    u32 dvbm_cfg;
    u32 dvbm_link;
    struct rkvenc_debug_info info;
};

struct rkvenc_dev {
    struct mpp_dev mpp;
    struct rkvenc_hw_info *hw_info;

    struct mpp_clk_info aclk_info;
    struct mpp_clk_info hclk_info;
    struct mpp_clk_info core_clk_info;
    u32 default_max_load;
#ifdef CONFIG_PROC_FS
    struct proc_dir_entry *procfs;
#endif
    struct reset_control *rst_a;
    struct reset_control *rst_h;
    struct reset_control *rst_core;
    atomic_t on_work;

#if ROCKCHIP_DVBM_ENABLE
    struct dvbm_port *dvbm_port;
    unsigned long dvbm_setup;
    u32 dvbm_overflow;
#endif
    u32 dvbm_reg_save[RKVENC_DVBM_REG_NUM];
    u32 dvbm_cfg;
    bool skip_dvbm_discnct;
    spinlock_t dvbm_lock;
    atomic_t isp_fcnt;
    void __iomem *vepu_qos;
    /* dump reg */
    u32 dump_reg_s;
    u32 dump_reg_e;
};

static struct rkvenc_hw_info rkvenc_500_hw_info = {
    .hw = {
        .reg_num = 254,
        .reg_id = 0,
        .reg_en = 0x10,
        .reg_start = 0,
        .reg_end = 0x5230,
    },
    .reg_class = RKVENC_CLASS_BUTT,
    .reg_msg[RKVENC_CLASS_BASE] = {
        .base_s = 0x0000,
        .base_e = 0x0120,
    },
    .reg_msg[RKVENC_CLASS_PIC] = {
        .base_s = 0x0270,
        .base_e = 0x0538,
    },
    .reg_msg[RKVENC_CLASS_RC] = {
        .base_s = 0x1000,
        .base_e = 0x113c,
    },
    .reg_msg[RKVENC_CLASS_PAR] = {
        .base_s = 0x1700,
        .base_e = 0x19cc,
    },
    .reg_msg[RKVENC_CLASS_SQI] = {
        .base_s = 0x2000,
        .base_e = 0x216c,
    },
    .reg_msg[RKVENC_CLASS_SCL_JPGTBL] = {
        .base_s = 0x2200,
        .base_e = 0x270c,
    },
    .reg_msg[RKVENC_CLASS_OSD] = {
        .base_s = 0x3000,
        .base_e = 0x3264,
    },
    .reg_msg[RKVENC_CLASS_ST] = {
        .base_s = 0x4000,
        .base_e = 0x424c,
    },
    .reg_msg[RKVENC_CLASS_DEBUG] = {
        .base_s = 0x5000,
        .base_e = 0x5230,
    },
    .fd_class = RKVENC_CLASS_FD_BUTT,
    .fd_reg[RKVENC_CLASS_FD_BASE] = {
        .class = RKVENC_CLASS_PIC,
        .base_fmt = RKVENC_FMT_BASE,
    },
    .fd_reg[RKVENC_CLASS_FD_OSD] = {
        .class = RKVENC_CLASS_OSD,
        .base_fmt = RKVENC_FMT_OSD_BASE,
    },
    .fmt_reg = {
        .class = RKVENC_CLASS_PIC,
        .base = 0x0300,
        .bitpos = 0,
        .bitlen = 2,
    },
    .enc_start_base = 0x0010,
    .enc_clr_base = 0x0014,
    .int_en_base = 0x0020,
    .int_mask_base = 0x0024,
    .int_clr_base = 0x0028,
    .int_sta_base = 0x002c,
    .enc_wdg_base = 0x0038,
    .err_mask = 0x27d0,
};

static int rkvenc_reset(struct mpp_dev *mpp);
static void rkvenc_task_timeout(struct work_struct *work_s);
static void update_online_info(struct mpp_dev *mpp, u32 pipe_id, u32 online);
static bool req_over_class(struct mpp_request *req, struct rkvenc_task *task, int class)
{
    struct rkvenc_hw_info *hw = task->hw_info;
    u32 base_s, base_e, req_e;
    bool ret;

    base_s = hw->reg_msg[class].base_s;
    base_e = hw->reg_msg[class].base_e;
    req_e = req->offset + req->size - sizeof(u32);

    ret = (req->offset <= base_e && req_e >= base_s) ? true : false;

    return ret;
}

static int rkvenc_invalid_class_msg(struct rkvenc_task *task)
{
    u32 reg_class = task->hw_info->reg_class;
    u32 i;

    for (i = 0; i < reg_class; i++) {
        if (task->reg[i].data) {
            task->reg[i].data = NULL;
            task->reg[i].valid = 0;
        }
    }

    return 0;
}

static int rkvenc_alloc_class_msg(struct rkvenc_task *task, int class)
{
    struct rkvenc_hw_info *hw = task->hw_info;

    if (!task->reg[class].data) {
        u32 base_s = hw->reg_msg[class].base_s;
        u32 base_e = hw->reg_msg[class].base_e;
        u32 class_size = base_e - base_s + sizeof(u32);

        task->reg[class].data = NULL;
        task->reg[class].size = class_size;
    }

    return 0;
}

static int rkvenc_update_req(struct rkvenc_task *task, int class, struct mpp_request *req_in,
                             struct mpp_request *req_out)
{
    struct rkvenc_hw_info *hw = task->hw_info;
    u32 base_s, base_e, req_e, s, e;

    base_s = hw->reg_msg[class].base_s;
    base_e = hw->reg_msg[class].base_e;
    req_e = req_in->offset + req_in->size - sizeof(u32);
    s = max(req_in->offset, base_s);
    e = min(req_e, base_e);

    req_out->offset = s;
    req_out->size = e - s + sizeof(u32);
    req_out->data = (u8 *)req_in->data + (s - req_in->offset);

    if (req_in->offset < base_s || req_e > base_e)
        mpp_err("warning over class, req off 0x%08x size %d\n",
                req_in->offset, req_in->size);

    return 0;
}

static int rkvenc_get_class_msg(struct rkvenc_task *task, u32 addr, struct mpp_request *msg)
{
    struct rkvenc_hw_info *hw = task->hw_info;
    bool found = false;
    int i;

    if (!msg)
        return -EINVAL;

    memset(msg, 0, sizeof(*msg));
    for (i = 0; i < hw->reg_class; i++) {
        u32 base_s = hw->reg_msg[i].base_s;
        u32 base_e = hw->reg_msg[i].base_e;

        if (addr >= base_s && addr <= base_e) {
            found = true;
            msg->offset = task->reg[i].offset;
            msg->size = task->reg[i].size;
            msg->data = task->reg[i].data;
            break;
        }
    }

    return (found ? 0 : (-EINVAL));
}

static u32 *rkvenc_get_class_reg(struct rkvenc_task *task, u32 addr)
{
    struct rkvenc_hw_info *hw = task->hw_info;
    u8 *reg = NULL;
    int i;

    for (i = 0; i < hw->reg_class; i++) {
        u32 base_s = hw->reg_msg[i].base_s;
        u32 base_e = hw->reg_msg[i].base_e;

        if (addr >= base_s && addr < base_e) {
            if (addr >= task->reg[i].offset)
                reg = (u8 *)task->reg[i].data + (addr - task->reg[i].offset);
            else
                mpp_err("get reg invalid, addr[0x%x] reg[%d].offset[0x%x]",
                        addr, i, task->reg[i].offset);
            break;
        }
    }

    return (u32 *)reg;
}

static int rkvenc_set_class_reg(struct rkvenc_task *task, u32 addr, u32 *data)
{
    struct rkvenc_hw_info *hw = task->hw_info;
    int i;

    for (i = 0; i < hw->reg_class; i++) {
        u32 base_s = hw->reg_msg[i].base_s;
        u32 base_e = hw->reg_msg[i].base_e;

        if (addr >= base_s && addr <= base_e) {
            task->reg[i].data = data;
            task->reg[i].offset = addr;
            task->reg[i].size = base_e - addr + 4;
            break;
        }
    }
    return 0;
}


static int rkvenc_extract_task_msg(struct mpp_session *session, struct rkvenc_task *task,
                                   struct mpp_task_msgs *msgs, u32 kernel_space)
{
    struct rkvenc_hw_info *hw = task->hw_info;
    struct mpp_request *req;
    u32 i, j;

    mpp_debug_enter();

    for (i = 0; i < msgs->req_cnt; i++) {
        req = &msgs->reqs[i];
        if (!req->size)
            continue;

        switch (req->cmd) {
        case MPP_CMD_SET_REG_WRITE: {
            //void *data;
            struct mpp_request *wreq;

            for (j = 0; j < hw->reg_class; j++) {
                if (!req_over_class(req, task, j))
                    continue;

                wreq = &task->w_reqs[task->w_req_cnt];
                rkvenc_update_req(task, j, req, wreq);
                rkvenc_set_class_reg(task, wreq->offset, wreq->data);
                task->reg[j].valid = 1;
                task->w_req_cnt++;
            }
        } break;
        case MPP_CMD_SET_REG_READ: {
            struct mpp_request *rreq;

            for (j = 0; j < hw->reg_class; j++) {
                if (!req_over_class(req, task, j))
                    continue;

                rreq = &task->r_reqs[task->r_req_cnt];
                rkvenc_update_req(task, j, req, rreq);
                task->reg[j].valid = 1;
                task->r_req_cnt++;
            }
        } break;
        default:
            break;
        }
    }
    mpp_debug(DEBUG_TASK_INFO, "w_req_cnt=%d, r_req_cnt=%d\n", task->w_req_cnt, task->r_req_cnt);

    mpp_debug_enter();

    return 0;
}

static int rkvenc_task_get_format(struct mpp_dev *mpp, struct rkvenc_task *task)
{
    struct rkvenc_hw_info *hw = task->hw_info;
    u32 class = hw->fmt_reg.class;
    u32 *class_reg = task->reg[class].data;
    u32 class_size = task->reg[class].size;
    u32 class_offset = task->reg[class].offset;
    u32 bitpos = hw->fmt_reg.bitpos;
    u32 bitlen = hw->fmt_reg.bitlen;
    u32 offset, val;

    if (!class_reg || !class_size) {
        mpp_err("invalid class reg %px class size %d\n", class_reg, class_size);
        return -EINVAL;
    }

    if (hw->fmt_reg.base < class_offset) {
        mpp_err("invalid fmt_reg[0x%x] class_offset[0x%x]", hw->fmt_reg.base, class_offset);
        return -EINVAL;
    }

    offset = hw->fmt_reg.base - class_offset;
    val = class_reg[offset / sizeof(u32)];
    task->fmt = (val >> bitpos) & ((1 << bitlen) - 1);

    return 0;
}

static struct rkvenc_task *rkvenc_get_task(struct mpp_session *session)
{
    struct rkvenc_task *task = NULL;
    int i;

    for (i = 0; i < MAX_TASK_CNT; i++) {
        task = (struct rkvenc_task *)session->task[i];
        if (!task->mpp_task.state)
            break;
    }

    if (i == MAX_TASK_CNT) {
        mpp_err("can't found idle task");
        task = NULL;
        return task;
    }

    for (i = 0; i < RKVENC_CLASS_BUTT; i++)
        task->reg[i].valid = 0;

    task->irq_status = 0;
    task->w_req_cnt = 0;
    task->r_req_cnt = 0;

    return task;
}

static int rkvenc_pp_extract_task_msg(struct rkvenc_task *task, struct mpp_task_msgs *msgs)
{
    struct rkvenc_pp_info *pp_info = task->pp_info;
    u32 i;

    for (i = 0; i < msgs->req_cnt; i++) {
        struct mpp_request *req = &msgs->reqs[i];

        if (!req->size)
            continue;

        switch (req->cmd) {
        case MPP_CMD_SET_REG_WRITE: {
            if (sizeof(pp_info->param) != req->size)
                mpp_err("invalid pp param size %d\n", req->size);

            memcpy(&pp_info->param, req->data, req->size);
        } break;
        case MPP_CMD_SET_REG_READ: {
            //TODO:
            memcpy(&task->r_reqs[task->r_req_cnt++], req, sizeof(*req));
        } break;
        default : {
        } break;
        }
    }

    return 0;
}

static void rkvenc_check_split_task(struct rkvenc_task *task)
{
    u32 slen_fifo_en = 0;
    u32 sli_split_en = 0;

    if (task->reg[RKVENC_CLASS_PIC].valid) {
        u32 *reg = task->reg[RKVENC_CLASS_PIC].data;

        slen_fifo_en = (reg[RKVENC_REG_ENC_PIC] & RKVENC_BIT_SLEN_FIFO) ? 1 : 0;
        sli_split_en = (reg[RKVENC_REG_SLI_SPLIT] & RKVENC_BIT_SLI_SPLIT) ? 1 : 0;
    }

    task->task_split = sli_split_en && slen_fifo_en;
}

static void *rkvenc_init_task(struct mpp_session *session, struct mpp_task_msgs *msgs)
{
    struct mpp_dev *mpp = session->mpp;
    struct rkvenc_task *task = NULL;
    struct mpp_task *mpp_task;
    u32 resolution_addr = RKVENC_RSL;
    u32 *reg_tmp;
    int ret;

    mpp_debug_enter();

    task = rkvenc_get_task(session);
    mpp_task = &task->mpp_task;
    mpp_task_init(session, mpp_task);
    mpp_task->hw_info = mpp->var->hw_info;
    mpp_task->clbk_en = 1;
    mpp_task->disable_jpeg = 0;
    task->hw_info = to_rkvenc_info(mpp_task->hw_info);
    /* extract reqs for current task */
    if (session->pp_session) {
        ret = rkvenc_pp_extract_task_msg(task, msgs);
        mpp_task->clbk_en = 0;
        return mpp_task;
    }
    ret = rkvenc_extract_task_msg(session, task, msgs, session->k_space);
    if (ret)
        goto free_task;
    mpp_task->reg = task->reg[0].data;
    /* get format */
    ret = rkvenc_task_get_format(mpp, task);
    if (ret)
        goto free_task;

    /*
     * check rec_fbc_dis flag.
     * rec fbc disable will be set when debreath pass1 frame
     */
    reg_tmp = rkvenc_get_class_reg(task, 0x300);
    if (*reg_tmp & BIT(31))
        mpp_task->clbk_en = 0;

    /* get resolution info */
    if (task->fmt == 2)
        resolution_addr = RKVENC_JPEG_RSL;
    reg_tmp = rkvenc_get_class_reg(task, resolution_addr);
    mpp_task->width = (((*reg_tmp) & 0x7ff) + 1) << 3;
    mpp_task->height = ((((*reg_tmp) >> 16) & 0x7ff) + 1) << 3;

    task->clk_mode = CLK_MODE_NORMAL;
    rkvenc_check_split_task(task);

    mpp_debug_leave();

    return mpp_task;

free_task:
    kfree(task);

    return NULL;
}

static void rkvenc_dvbm_status_dump(struct mpp_dev *mpp)
{
    int off;

    if (!mpp_debug_unlikely(DEBUG_DVBM_DUMP))
        return;

    pr_err("--- dump dvbm status --\n");
    off = 0x60;
    pr_err("reg[%#x] 0x%08x\n", off, mpp_read(mpp, off));

    // for (off = 0x68; off <= 0x9c; off += 4)
    //  pr_err("reg[%#x] 0x%08x\n", off, mpp_read(mpp, off));

    off = 0x308;
    pr_err("reg[%#x] 0x%08x\n", off, mpp_read(mpp, off));

    off = 0x4020;
    pr_err("reg[%#x] 0x%08x\n", off, mpp_read(mpp, off));

    for (off = 0x5168; off <= 0x51bc; off += 4)
        pr_err("reg[%#x] 0x%08x\n", off, mpp_read(mpp, off));
}

static void rkvenc_dump_simple_dbg(struct mpp_dev *mpp)
{
    u32 dvbm_cfg = mpp_read(mpp, 0x60);
    const char *fmt[3] = {
        "h264",
        "h265",
        "none"
    };
    u8 fmt_idx = mpp_read(mpp, 0x300) & 0x3;
    u8 jpeg_en = mpp_read(mpp, 0x47c) >> 31;

    pr_err("dvbm    - en %d sw %d\n", !!(dvbm_cfg & BIT(0)), !!(dvbm_cfg & BIT(1)));
    pr_err("fmt     - v %s jpg %d\n", fmt[fmt_idx], jpeg_en);
    if (!!(dvbm_cfg & BIT(1)))
        pr_err("vsldy   - 0x%08x\n", mpp_read(mpp, 0x18));
    pr_err("st_enc  - 0x%08x\n", mpp_read(mpp, 0x4020));
    pr_err("st_vsp0 - 0x%08x\n", mpp_read(mpp, 0x5000));
    pr_err("cycle   - 0x%08x\n", mpp_read(mpp, 0x5200));
}

static void rkvenc_reg_dump(struct mpp_dev *mpp)
{
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);

    if (unlikely(enc->dump_reg_s || enc->dump_reg_e)) {
        struct rkvenc_hw_info *hw = &rkvenc_500_hw_info;
        u32 dump_s, dump_e;
        u32 i, off;
        u32 s, e;

        dump_s = ALIGN(enc->dump_reg_s, 4);
        dump_e = ALIGN(enc->dump_reg_e, 4);
        for (i = 0; i < RKVENC_CLASS_BUTT; i++) {
            s = hw->reg_msg[i].base_s;
            e = hw->reg_msg[i].base_e;

            if (s < dump_s && e < dump_e)
                continue;
            if (s > dump_s && e > dump_e)
                return;

            s = s < dump_s ? dump_s : s;
            e = e > dump_e ? dump_e : e;
            for (off = s; off <= e; off += 4) {
                u32 val = mpp_read(mpp, off);

                if (val)
                    mpp_err("reg[%#x] 0x%08x\n", off, val);
            }
        }
    }
}

static void rkvenc_info_dump(struct mpp_dev *mpp)
{
    rkvenc_dump_simple_dbg(mpp);

    if (unlikely(mpp_dev_debug & DEBUG_DUMP_ERR_REG))
        rkvenc_reg_dump(mpp);
}

static void rkvenc_dvbm_show_info(struct mpp_dev *mpp)
{
    u32 isp_info_addr[4] = {0x5170, 0x5178, 0x5180, 0x5188};
    u32 dvbm_ctl = mpp_read(mpp, 0x5190);
    u32 isp1 = mpp_read(mpp, 0x516c);
    u32 valid_idx;
    u32 isp_info;

    valid_idx = dvbm_ctl & 0x3;
    isp_info = mpp_read(mpp, isp_info_addr[valid_idx]);

    mpp_dbg_dvbm("fcnt %d lcnt %d\n", (isp_info >> 16) & 0xff, isp_info & 0x3fff);
    mpp_dbg_dvbm("isp0 fcnt %d isp1 0x%08x fcnt %d lcnt %d\n",
                 mpp_read(mpp, 0x5168) & 0xff, isp1, (isp1 >> 16) & 0xff, isp1 & 0x3fff);
}

static int rkvenc_disconnect_dvbm(struct mpp_dev *mpp, u32 dvbm_cfg)
{
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);
    unsigned long flag;

    dvbm_cfg = dvbm_cfg ? dvbm_cfg : mpp_read(mpp, RKVENC_DVBM_CFG);
    spin_lock_irqsave(&enc->dvbm_lock, flag);
    enc->skip_dvbm_discnct = false;
    /* check if encode online task */
    if (atomic_read(&enc->on_work) && (mpp_read(mpp, 0x308) & BIT(16)))
        goto done;

    if (!(dvbm_cfg & DVBM_VEPU_CONNETC))
        goto done;

    dvbm_cfg &= ~(DVBM_VEPU_CONNETC | VEPU_CONNETC_CUR);
    mpp_write(mpp, RKVENC_DVBM_CFG, dvbm_cfg);
done:
    spin_unlock_irqrestore(&enc->dvbm_lock, flag);

    return 0;
}

static int rkvenc_connect_dvbm(struct mpp_dev *mpp, u32 dvbm_cfg)
{
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);
    unsigned long flag;
#if ROCKCHIP_DVBM_ENABLE
    bool dual_wrap = (enc->dvbm_setup & 0x3) == 0x3;
#else
    bool dual_wrap = false;
#endif

    dvbm_cfg = dvbm_cfg ? dvbm_cfg : mpp_read(mpp, RKVENC_DVBM_CFG);
    spin_lock_irqsave(&enc->dvbm_lock, flag);
    if (DVBM_VEPU_CONNECT(mpp)) {
        enc->skip_dvbm_discnct = !!atomic_read(&enc->on_work);
        goto done;
    }
    dvbm_cfg &= ~(DVBM_VEPU_CONNETC | VEPU_CONNETC_CUR);
    if (dual_wrap) {
        if (ISP_IS_WORK(mpp))
            dvbm_cfg |= DVBM_ISP_CONNETC | VEPU_CONNETC_CUR;
        else
            dvbm_cfg |= DVBM_ISP_CONNETC ;
    } else {
        if (atomic_read(&enc->isp_fcnt) > 0)
            dvbm_cfg |= DVBM_ISP_CONNETC | VEPU_CONNETC_CUR;
        else
            dvbm_cfg |= DVBM_ISP_CONNETC;
    }
    mpp_write(mpp, RKVENC_DVBM_CFG, dvbm_cfg);
    mpp_write(mpp, RKVENC_DVBM_CFG, dvbm_cfg | DVBM_VEPU_CONNETC);
done:
    spin_unlock_irqrestore(&enc->dvbm_lock, flag);
    rkvenc_dvbm_show_info(mpp);

    return 0;
}

static int rkvenc_run(struct mpp_dev *mpp, struct mpp_task *mpp_task)
{
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);
    struct rkvenc_task *task = to_rkvenc_task(mpp_task);
    struct rkvenc_hw_info *hw = enc->hw_info;
    struct mpp_session *session = mpp_task->session;
    struct rkvenc2_session_priv *priv = mpp_task->session->priv;
    struct mpp_request msg;
    struct mpp_request *req;
    u32 dvbm_cfg = 0;
    u32 i, j;

    mpp_debug_enter();

    /* update online task info */
    if (session->online) {
        u32 frame_id = mpp_task->frame_id;
        u32 pipe_id = mpp_task->pipe_id;
        u32 *enc_id;

        enc_id = rkvenc_get_class_reg(task, 0x308);
        if (frame_id > 0xff) {
            mpp_err("invalid frame id %d\n", frame_id);
            frame_id &= 0xff;
        }

        if (pipe_id > 1) {
            mpp_err("invalid pipe id %d\n", pipe_id);
            pipe_id &= 0x1;
        }

        if (session->online == MPP_ENC_ONLINE_MODE_HW)
            *enc_id |= BIT(8) | BIT(13);

        *enc_id |= frame_id | (pipe_id << 12);
    }

    for (i = 0; i < task->w_req_cnt; i++) {
        u32 s, e, off;
        u32 *regs;
        int ret;

        req = &task->w_reqs[i];
        ret = rkvenc_get_class_msg(task, req->offset, &msg);
        if (ret)
            return -EINVAL;

        s = (req->offset - msg.offset) / sizeof(u32);
        e = s + req->size / sizeof(u32);
        regs = (u32 *)msg.data;
        for (j = s; j < e; j++) {
            off = msg.offset + j * sizeof(u32);

            if (mpp_task->disable_jpeg && off == 0x47c)
                regs[j] &= (~BIT(31));

            /* dvbm cfg */
            if (off == RKVENC_DVBM_CFG) {
                dvbm_cfg = regs[j];
                if (!regs[j])
                    continue;
                if (session->online == MPP_ENC_ONLINE_MODE_HW)
                    rkvenc_connect_dvbm(mpp, dvbm_cfg);
                continue;
            }

            mpp_write_relaxed(mpp, off, regs[j]);
        }
    }

    /* init current task */
    mpp->cur_task = mpp_task;

    priv->info.hw_running = 1;
    priv->dvbm_cfg = dvbm_cfg;

    if (dvbm_cfg) {
        update_online_info(mpp, mpp_task->pipe_id, session->online);
        mpp_debug(DEBUG_RUN, "chan %d task %d pipe %d frame %d start\n",
                  session->chn_id, mpp_task->task_index, mpp_task->pipe_id, mpp_task->frame_id);
        if (session->online == MPP_ENC_ONLINE_MODE_SW) {
            wmb();
            mpp_write_relaxed(mpp, RKVENC_DVBM_CFG, dvbm_cfg);
#if ROCKCHIP_DVBM_ENABLE
            enc->dvbm_overflow = 0;
#endif
            mpp->always_on = 1;
        }
    } else {
        mpp_debug(DEBUG_RUN, "chan %d task %d start\n", session->chn_id, mpp_task->task_index);
    }

    /* Flush the register before the start the device */
    wmb();
    preempt_disable();
    if (unlikely(mpp_dev_debug & DEBUG_REG_DUMP))
        rkvenc_reg_dump(mpp);
    INIT_DELAYED_WORK(&mpp_task->timeout_work, rkvenc_task_timeout);
    schedule_delayed_work(&mpp_task->timeout_work, msecs_to_jiffies(RKVENC_WORK_TIMEOUT_DELAY));
    set_bit(TASK_STATE_RUNNING, &mpp_task->state);
    if (!dvbm_cfg || (dvbm_cfg && (session->online == MPP_ENC_ONLINE_MODE_HW)))
        mpp_write(mpp, hw->enc_start_base, 0x100);
    atomic_set(&enc->on_work, 1);

    if (dvbm_cfg)
        rkvenc_dvbm_status_dump(mpp);

    preempt_enable();

    mpp_debug_leave();

    return 0;
}

static int rkvenc_pp_run(struct mpp_dev *mpp, struct mpp_task *mpp_task)
{
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);
    struct rkvenc_task *task = to_rkvenc_task(mpp_task);
    struct rkvenc2_session_priv *priv = mpp_task->session->priv;
    struct rkvenc_pp_param *param = &task->pp_info->param;
    struct rkvenc_hw_info *hw = &rkvenc_500_hw_info;
    u32 off, s, e;
    u32 i;

    mpp_debug_enter();

    mpp_write(mpp, enc->hw_info->enc_clr_base, 0x6);
    udelay(5);
    mpp_write(mpp, enc->hw_info->enc_clr_base, 0x4);

    /* clear some reg before pp task */
    for (i = RKVENC_CLASS_PIC; i <= RKVENC_CLASS_OSD; i++) {
        s = hw->reg_msg[i].base_s;
        e = hw->reg_msg[i].base_e;
        for (off = s; off <= e; off += 4)
            mpp_write_relaxed(mpp, off, 0);
    }

    mpp_write_relaxed(mpp, INT_EN, param->int_en);
    mpp_write_relaxed(mpp, INT_MSK, param->int_msk);
    mpp_write_relaxed(mpp, SRC0_ADDR, param->src0_addr);
    mpp_write_relaxed(mpp, SRC1_ADDR, param->src1_addr);
    mpp_write_relaxed(mpp, SRC2_ADDR, param->src2_addr);
    mpp_write_relaxed(mpp, DSPW_ADDR, param->dspw_addr);
    mpp_write_relaxed(mpp, DSPR_ADDR, param->dspr_addr);
    mpp_write_relaxed(mpp, ENC_PIC, param->enc_pic);
    mpp_write_relaxed(mpp, ENC_ID, param->enc_id);
    mpp_write_relaxed(mpp, ENC_RSL, param->enc_rsl);
    mpp_write_relaxed(mpp, SRC_FILL, param->src_fill);
    mpp_write_relaxed(mpp, SRC_FMT, param->src_fmt);
    mpp_write_relaxed(mpp, PIC_OFST, param->pic_ofst);
    mpp_write_relaxed(mpp, SRC_STRD0, param->src_strd0);
    mpp_write_relaxed(mpp, SRC_STRD1, param->src_strd1);
    mpp_write_relaxed(mpp, SRC_FLT_CFG, param->src_flt_cfg);
    mpp_write_relaxed(mpp, ME_RNGE, param->me_rnge);
    mpp_write_relaxed(mpp, ME_CFG, param->me_cfg);
    mpp_write_relaxed(mpp, ME_CACH, param->me_cach);
    mpp_write_relaxed(mpp, SYNT_SLI0, param->synt_sli0);
    mpp_write_relaxed(mpp, JPEG_BASE_CFG, param->jpeg_base_cfg);
    mpp_write_relaxed(mpp, PP_MD_ADR, param->pp_md_adr);
    mpp_write_relaxed(mpp, PP_OD_ADR, param->pp_od_adr);
    mpp_write_relaxed(mpp, PP_REF_MDW_ADR, param->pp_ref_mdw_adr);
    mpp_write_relaxed(mpp, PP_REF_MDR_ADR, param->pp_ref_mdr_adr);
    mpp_write_relaxed(mpp, PP_BASE_CFG, param->pp_base_cfg);
    mpp_write_relaxed(mpp, PP_MD_THD, param->pp_md_thd);
    mpp_write_relaxed(mpp, PP_OD_THD, param->pp_od_thd);

    /* init current task */
    mpp->cur_task = mpp_task;

    priv->info.hw_running = 1;
    /* Flush the register before the start the device */
    wmb();
    preempt_disable();
    INIT_DELAYED_WORK(&mpp_task->timeout_work, rkvenc_task_timeout);
    schedule_delayed_work(&mpp_task->timeout_work, msecs_to_jiffies(RKVENC_WORK_TIMEOUT_DELAY));
    set_bit(TASK_STATE_RUNNING, &mpp_task->state);
    mpp_debug(DEBUG_RUN, "chan %d pp_task %d start\n",
              mpp_task->session->chn_id, mpp_task->task_index);
    mpp_write(mpp, 0x10, 0x100);
    atomic_set(&enc->on_work, 1);
    preempt_enable();

    mpp_debug_leave();

    return 0;
}

static void rkvenc_check_bs_overflow(struct mpp_dev *mpp)
{
    struct mpp_task *task = mpp->cur_task;
    struct rkvenc2_session_priv *priv = task->session->priv;
    u32 w_adr, r_adr, top_adr, bot_adr;

    if (!(mpp->irq_status & RKVENC_ENC_DONE_STATUS)) {
        if (mpp->irq_status & RKVENC_JPEG_OVERFLOW) {
            r_adr = mpp_read(mpp, RKVENC_JPEG_BSBR);
            w_adr = mpp_read(mpp, RKVENC_JPEG_BSBS);
            top_adr = mpp_read(mpp, RKVENC_JPEG_BSBT);
            bot_adr = mpp_read(mpp, RKVENC_JPEG_BSBB);

            priv->info.bsbuf_info[0] = top_adr;
            priv->info.bsbuf_info[1] = bot_adr;
            priv->info.bsbuf_info[2] = w_adr;
            priv->info.bsbuf_info[3] = r_adr;

            mpp_write(mpp, RKVENC_JPEG_BSBS, w_adr);
            mpp_write(mpp, RKVENC_JPEG_BSBR, r_adr | 0xc);

            mpp_dbg_warning("task %d jpeg bs overflow, buf[t:%#x b:%#x w:%#x r:%#x]\n",
                            task->task_index, top_adr, bot_adr, w_adr, r_adr);
#if ROCKCHIP_DVBM_ENABLE
            mpp->overflow_status = mpp->irq_status;
#endif
        }
        if (mpp->irq_status & RKVENC_VIDEO_OVERFLOW) {
            r_adr = mpp_read(mpp, RKVENC_VIDEO_BSBR);
            w_adr = mpp_read(mpp, RKVENC_VIDEO_BSBS);
            top_adr = mpp_read(mpp, RKVENC_VIDEO_BSBT);
            bot_adr = mpp_read(mpp, RKVENC_VIDEO_BSBB);

            priv->info.bsbuf_info[0] = top_adr;
            priv->info.bsbuf_info[1] = bot_adr;
            priv->info.bsbuf_info[2] = w_adr;
            priv->info.bsbuf_info[3] = r_adr;

            mpp_write(mpp, RKVENC_VIDEO_BSBS, w_adr);
            mpp_write(mpp, RKVENC_VIDEO_BSBR, r_adr | 0xc);

            mpp_dbg_warning("task %d video bs overflow, buf[t:%#x b:%#x w:%#x r:%#x]\n",
                            task->task_index, top_adr, bot_adr, w_adr, r_adr);
#if ROCKCHIP_DVBM_ENABLE
            mpp->overflow_status = mpp->irq_status;
#endif
        }
    }
}

#if ROCKCHIP_DVBM_ENABLE
static void rkvenc_clear_dvbm_info(struct mpp_dev *mpp)
{
    u32 dvbm_info, dvbm_en;

    mpp_write(mpp, 0x60, 0);
    mpp_write(mpp, 0x18, 0);
    dvbm_info = mpp_read(mpp, 0x18);
    dvbm_en = mpp_read(mpp, 0x60);
    if (dvbm_info || dvbm_en)
        pr_err("clear dvbm info failed 0x%08x 0x%08x\n", dvbm_info, dvbm_en);
}
#else
static void rkvenc_clear_dvbm_info(struct mpp_dev *mpp)
{
}
#endif

int rkvenc_500_hw_dvbm_handle(struct mpp_dev *mpp, struct mpp_task *mpp_task)
{
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);
    struct mpp_session *session = mpp_task->session;
    u32 enc_st = mpp_read(mpp, RKVENC_STATUS);
    u32 dvbm_cfg = mpp_read(mpp, RKVENC_DVBM_CFG);

    if (mpp->irq_status & RKVENC_SOURCE_ERR) {
        if (atomic_read(&enc->isp_fcnt) <= 1) {
            dvbm_cfg &= ~DVBM_VEPU_CONNETC;
            mpp_write(mpp, RKVENC_DVBM_CFG, dvbm_cfg);
            mpp_write(mpp, RKVENC_STATUS, 0);
            dvbm_cfg |= VEPU_CONNETC_CUR;
            mpp_write(mpp, RKVENC_DVBM_CFG, dvbm_cfg);
            mpp_write(mpp, RKVENC_DVBM_CFG, dvbm_cfg | DVBM_VEPU_CONNETC);
            mpp_write(mpp, 0x10, 0x100);
            return 1;
        }
        mpp_err("chan %d task %d pipe %d frame id %d err 0x%08x cfg %#x 0x%08x\n",
                session->chn_id, mpp_task->task_index, mpp_task->pipe_id,
                mpp_task->frame_id, enc_st, dvbm_cfg, mpp_read(mpp, 0x516c));
    }
    mpp_dbg_dvbm("st enc 0x%08x\n", enc_st);

    if (enc->skip_dvbm_discnct) {
        if ((mpp->irq_status & RKVENC_SOURCE_ERR)) {
            dvbm_cfg &= ~DVBM_VEPU_CONNETC;
            mpp_write(mpp, RKVENC_DVBM_CFG, dvbm_cfg);

            mpp_write(mpp, RKVENC_STATUS, 0);

            dvbm_cfg |= VEPU_CONNETC_CUR;
            mpp_write(mpp, RKVENC_DVBM_CFG, dvbm_cfg);
            mpp_write(mpp, RKVENC_DVBM_CFG, dvbm_cfg | DVBM_VEPU_CONNETC);
        }
        enc->skip_dvbm_discnct = false;
    } else {
        if (mpp->irq_status & 0x7fff) {
            dvbm_cfg &= ~DVBM_VEPU_CONNETC;
            mpp_write(mpp, RKVENC_DVBM_CFG, dvbm_cfg);
        }
        mpp_write(mpp, RKVENC_STATUS, 0);
    }
    if (!(mpp->irq_status & 0x7fff))
        return 1;

    return 0;
}

int rkvenc_500_soft_dvbm_handle(struct mpp_dev *mpp, struct mpp_task *mpp_task)
{
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);
    struct rkvenc2_session_priv *priv = mpp_task->session->priv;
    u32 line_cnt;

    line_cnt = mpp_read(mpp, 0x18) & 0x3fff;
    rkvenc_clear_dvbm_info(mpp);
    /*
    * Workaround:
    * The line cnt is updated in the mcu.
    * When line cnt is set to max value 0x3fff,
    * the cur frame has overflow.
    */
    if (line_cnt == 0x3fff) {
#if ROCKCHIP_DVBM_ENABLE
        enc->dvbm_overflow = 1;
#endif
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
        priv->info.wrap_source_mis_cnt++;
    }
#if ROCKCHIP_DVBM_ENABLE
    if (enc->dvbm_overflow) {
        mpp->irq_status |= BIT(6);
        enc->dvbm_overflow = 0;
        priv->info.wrap_overflow_cnt++;
    }
#endif

    return 0;
}

static void rkvenc_read_slice_len(struct mpp_task *mpp_task, struct rkvenc_task *task,
                                  struct mpp_dev *mpp, u32 sli_num)
{
    struct mpp_session *session = mpp_task->session;
    u32 reg_st_slen;
    u32 i;

    for (i = 0; i < sli_num; i++) {
        reg_st_slen = mpp_read_relaxed(mpp, RKVENC_REG_SLICE_LEN_BASE);
        task->sli_info.length = reg_st_slen & 0x3fffffff;
        task->sli_info.last = !!(reg_st_slen & RKVENC_BIT_SLI_LST);
        task->last_slice_found = task->sli_info.last;

        mpp_debug(DEBUG_SLICE, "wr %3d reg_st_slen %#x len %d %s\n",
                  i, reg_st_slen, task->sli_info.length,
                  task->sli_info.last ? "last" : "continue");

        if (session->callback && mpp_task->clbk_en)
            session->callback(session->chn_id, MPP_VCODEC_EVENT_SLICE, &task->sli_info.val);
    }
}

irqreturn_t rkvenc_500_irq(int irq, void *param)
{
    struct mpp_dev *mpp = param;
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);
    struct rkvenc_hw_info *hw = enc->hw_info;
    struct mpp_task *mpp_task = mpp_taskqueue_get_running_task(mpp->queue);
    struct rkvenc_task *task;
    struct mpp_session *session;
    struct rkvenc2_session_priv *priv;
    union st_enc_u enc_st;
    u32 sli_num;

    mpp_debug_enter();

    if (!mpp_task) {
        dev_err(mpp->dev, "found null task in irq\n");
        return IRQ_HANDLED;
    }

    task = to_rkvenc_task(mpp_task);
    session = mpp_task->session;
    priv = mpp_task->session->priv;

    /* get irq status */
    sli_num = mpp_read_relaxed(mpp, RKVENC_REG_SLICE_NUM_BASE);
    mpp->irq_status = mpp_read(mpp, hw->int_sta_base);
    enc_st.val = mpp_read(mpp, RKVENC_STATUS);

    pr_info("irq status %x enc st %x\n", mpp->irq_status, enc_st.val);

    if (!mpp->irq_status)
        return IRQ_NONE;

    /* check irq status */
    rkvenc_check_bs_overflow(mpp);

    /* clear irq regs */
    mpp_write(mpp, hw->int_mask_base, 0x100);
    mpp_write(mpp, hw->int_clr_base, mpp->irq_status);
    mpp_write(mpp, hw->int_sta_base, 0);

    /* slice fifo */
    if (task && task->task_split) {
        if (sli_num) {
            rkvenc_read_slice_len(mpp_task, task, mpp, sli_num);
            /* clear irq regs */
            mpp_write(mpp, hw->int_mask_base, 0x100);
            mpp_write(mpp, hw->int_clr_base, mpp->irq_status);
            mpp_write(mpp, hw->int_sta_base, 0);
            if ((mpp->irq_status & RKVENC_ENC_DONE_STATUS) == 0) {
                mpp_debug(DEBUG_SLICE, "mpp->irq_status[0x%08x] slice done return.\n",
                          mpp->irq_status);
                return IRQ_HANDLED;
            }
        } else {
            if (task->last_slice_found == 0 &&
                (mpp->irq_status & RKVENC_ENC_DONE_STATUS)) {
                mpp_debug(DEBUG_SLICE, "send empty slice info packet.\n");
                task->sli_info.last = 1;
                task->sli_info.length = 0;
                task->last_slice_found = 1;
                if (session->callback && mpp_task->clbk_en)
                    session->callback(session->chn_id, MPP_VCODEC_EVENT_SLICE,
                                      &task->sli_info.val);
            }
        }
    }

    if (priv->dvbm_cfg) {
        if (session->online == MPP_ENC_ONLINE_MODE_SW) {
            rkvenc_500_soft_dvbm_handle(mpp, mpp_task);
        } else {
            if (rkvenc_500_hw_dvbm_handle(mpp, mpp_task))
                return IRQ_HANDLED;
        }
    }

    if (test_and_set_bit(TASK_STATE_HANDLE, &mpp_task->state)) {
        mpp_err("task %d has been handled state %#lx\n",
                mpp_task->task_index, mpp_task->state);
        return IRQ_HANDLED;
    }
    cancel_delayed_work(&mpp_task->timeout_work);
    set_bit(TASK_STATE_IRQ, &mpp_task->state);

    if (mpp->overflow_status)
        priv->info.bsbuf_overflow_cnt++;
    task->irq_status = (mpp->irq_status | mpp->overflow_status);

    mpp_debug(DEBUG_IRQ_STATUS, "chan %d %s task %d irq_status 0x%08x st 0x%08x\n",
              session->chn_id, task_type_str(task), mpp_task->task_index,
              task->irq_status, enc_st.val);

    mpp_time_diff(mpp_task);
    /* get enc task info */
    if (mpp->dev_ops->finish)
        mpp->dev_ops->finish(mpp, mpp_task);
    if (mpp->irq_status & enc->hw_info->err_mask) {
        mpp_dbg_warning("chan %d %s task %d irq_status 0x%08x st 0x%08x\n",
                        session->chn_id, task_type_str(task), mpp_task->task_index,
                        task->irq_status, enc_st.val);
        priv->info.enc_err_irq = task->irq_status;
        priv->info.enc_err_status = enc_st.val;
        if (enc_st.vepu_src_oflw)
            priv->info.wrap_overflow_cnt++;
        priv->info.enc_err_cnt++;
        if (mpp->hw_ops->reset)
            mpp->hw_ops->reset(mpp);
    }
    priv->info.hw_running = 0;
    mpp->overflow_status = 0;
    set_bit(TASK_STATE_DONE, &mpp_task->state);
    mpp_taskqueue_pop_running(mpp->queue, mpp_task);
    wake_up(&mpp_task->wait);
    up(&mpp->work_sem);
    mpp_taskqueue_trigger_work(mpp);
    if (session->callback && mpp_task->clbk_en)
        session->callback(session->chn_id, MPP_VCODEC_EVENT_FRAME, NULL);

    mpp_debug_leave();

    return IRQ_HANDLED;
}

static int rkvenc_finish(struct mpp_dev *mpp, struct mpp_task *mpp_task)
{
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);
    struct rkvenc_task *task = to_rkvenc_task(mpp_task);
    struct mpp_request *req;
    u32 *reg;
    u32 i, j;

    mpp_debug_enter();

    if (task->pp_info) {
        task->pp_info->output.luma_pix_sum_od = mpp_read(mpp, 0x4070);
        mpp_write(mpp, 0x530, 0);
        atomic_set(&enc->on_work, 0);
        return 0;
    }

    for (i = 0; i < task->r_req_cnt; i++) {
        u32 off;

        req = &task->r_reqs[i];
        reg = (u32 *)req->data;
        for (j = 0; j < req->size / sizeof(u32); j++) {
            off =  req->offset + j * sizeof(u32);
            reg[j] = mpp_read_relaxed(mpp, off);
            if (off == task->hw_info->int_sta_base)
                reg[j] = task->irq_status;
        }
    }
    /* revert hack for irq status */
    reg = rkvenc_get_class_reg(task, task->hw_info->int_sta_base);
    if (reg)
        *reg = task->irq_status;

    atomic_set(&enc->on_work, 0);

    mpp_debug_leave();

    return 0;
}

static int rkvenc_result(struct mpp_dev *mpp, struct mpp_task *mpp_task)
{
    struct rkvenc_task *task = to_rkvenc_task(mpp_task);
    (void)mpp;

    mpp_debug_enter();

    if (task->pp_info) {
        struct mpp_request *req;
        int i;

        for (i = 0; i < task->r_req_cnt; i++) {
            req = &task->r_reqs[i];
            memcpy(req->data, (u8 *)&task->pp_info->output, req->size);
        }
    }

    if (test_bit(TASK_STATE_TIMEOUT, &mpp_task->state))
        return -1;

    if (atomic_read(&mpp_task->abort_request))
        return -1;

    mpp_debug_leave();

    return 0;
}

static int rkvenc_free_task(struct mpp_session *session, struct mpp_task *mpp_task)
{
    struct rkvenc_task *task = to_rkvenc_task(mpp_task);

    mpp_task_finalize(session, mpp_task);
    rkvenc_invalid_class_msg(task);
    task->task_split = 0;
    task->last_slice_found = 0;

    return 0;
}

static int rkvenc_unbind_jpeg_task(struct mpp_session *session)
{
    struct mpp_dev *mpp = session->mpp;
    struct mpp_taskqueue *queue = mpp->queue;
    struct mpp_task *task, *n;
    unsigned long flags, flags1;

    spin_lock_irqsave(&queue->dev_lock, flags);
    spin_lock_irqsave(&session->pending_lock, flags1);

    list_for_each_entry_safe(task, n, &session->pending_list, pending_link) {
        if (test_bit(TASK_STATE_RUNNING, &task->state))
            continue;

        pr_err("task %d disable_jpeg\n", task->task_index);
        task->disable_jpeg = 1;
    }

    spin_unlock_irqrestore(&session->pending_lock, flags1);
    spin_unlock_irqrestore(&queue->dev_lock, flags);

    return 0;
}

static int rkvenc_control(struct mpp_session *session, struct mpp_request *req)
{
    switch (req->cmd) {
    case MPP_CMD_UNBIND_JPEG_TASK: {
        return rkvenc_unbind_jpeg_task(session);
    } break;
    case MPP_CMD_VEPU_CONNECT_DVBM: {
        u32 dvbm_cfg = mpp_read(session->mpp, RKVENC_DVBM_CFG);
        u32 connect = *(u32*)req->data;

        if (session->online == MPP_ENC_ONLINE_MODE_SW)
            return 0;

        if (connect)
            return rkvenc_connect_dvbm(session->mpp, dvbm_cfg);
        else
            return rkvenc_disconnect_dvbm(session->mpp, dvbm_cfg);
    } break;
    default: {
        mpp_err("unknown mpp ioctl cmd %x\n", req->cmd);
    } break;
    }

    return 0;
}

static int rkvenc_free_session(struct mpp_session *session)
{
    if (session) {
        u32 i = 0;

        for (i = 0 ; i < MAX_TASK_CNT; i++) {
            struct rkvenc_task *task = (struct rkvenc_task *)session->task[i];
            if (task) {
                rkvenc_invalid_class_msg(task);
                if (task->pp_info)
                    kfree(task->pp_info);
                kfree(task);
            }
        }
    }
    if (session && session->priv) {
        kfree(session->priv);
        session->priv = NULL;
    }

    return 0;
}

static int rkvenc_pre_alloc_task(struct mpp_session *session)
{
    struct mpp_dev *mpp = session->mpp;
    int i, j, ret = 0;

    for (i = 0; i < MAX_TASK_CNT; i++) {
        struct rkvenc_task *task = kzalloc(sizeof(struct rkvenc_task), GFP_KERNEL);

        if (!task) {
            mpp_err("alloc task %d failed\n", i);
            ret = -ENOMEM;
            goto fail;
        }

        task->hw_info = to_rkvenc_info(mpp->var->hw_info);
        for (j = 0; j < task->hw_info->reg_class; j++) {
            ret = rkvenc_alloc_class_msg(task, j);
            if (ret) {
                mpp_err("alloc task %d class %d msg failed\n", i, j);
                goto fail;
            }
        }
        if (session->pp_session) {
            struct rkvenc_pp_info *pp_info = kzalloc(sizeof(*pp_info), GFP_KERNEL);

            if (!pp_info) {
                mpp_err("alloc pp info %d failed\n", i);
                ret = -ENOMEM;
                goto fail;
            }
            task->pp_info = pp_info;
        }
        session->task[i] = task;
    }

    return ret;
fail:
    for (i = 0 ; i < MAX_TASK_CNT; i++) {
        struct rkvenc_task *task = (struct rkvenc_task *)session->task[i];

        if (task) {
            rkvenc_invalid_class_msg(task);
            if (task->pp_info)
                kfree(task->pp_info);
            kfree(task);
        }
    }

    return ret;
}

static int rkvenc_init_session(struct mpp_session *session)
{
    struct rkvenc2_session_priv *priv;
    struct mpp_dev *mpp = NULL;
    int ret = 0;

    if (!session) {
        mpp_err("session is null\n");
        return -EINVAL;
    }

    mpp = session->mpp;
    priv = kzalloc(sizeof(*priv), GFP_KERNEL);
    if (!priv) {
        mpp_err("alloc priv failed\n");
        return -ENOMEM;
    }

    init_rwsem(&priv->rw_sem);
    session->priv = priv;
    ret = rkvenc_pre_alloc_task(session);
    if (ret)
        goto fail;

    return 0;

fail:
    if (session->priv)
        kfree(session->priv);

    return -ENOMEM;
}

static void rkvenc_task_timeout(struct work_struct *work_s)
{
    struct mpp_task *task = container_of(to_delayed_work(work_s), struct mpp_task, timeout_work);
    struct mpp_session *session;
    struct mpp_dev *mpp;
    struct rkvenc2_session_priv *priv;
    u32 clbk_en = task->clbk_en;

    if (test_and_set_bit(TASK_STATE_HANDLE, &task->state)) {
        mpp_err("task has been handled\n");
        return;
    }

    if (!task->session) {
        mpp_err("task %p, task->session is null.\n", task);
        return;
    }
    session = task->session;

    mpp_err("chan %d %s task %d state %#lx processing time out!\n",
            session->chn_id, task_type_str(to_rkvenc_task(task)),
            task->task_index, task->state);

    if (!session->mpp) {
        mpp_err("session %p, session->mpp is null.\n", session);
        return;
    }

    mpp = session->mpp;

    disable_irq(mpp->irq);
    rkvenc_info_dump(mpp);
    set_bit(TASK_STATE_TIMEOUT, &task->state);

    priv = session->priv;
    priv->info.hw_running = 0;
    mpp_time_diff(task);
    set_bit(TASK_STATE_DONE, &task->state);
    mpp_taskqueue_pop_running(mpp->queue, task);

    if (mpp->dev_ops->finish)
        mpp->dev_ops->finish(mpp, task);
    if (mpp->hw_ops->reset)
        mpp->hw_ops->reset(mpp);
    if (session->online == MPP_ENC_ONLINE_MODE_SW)
        rkvenc_clear_dvbm_info(mpp);
    else
        rkvenc_disconnect_dvbm(mpp, 0);
    up(&mpp->work_sem);
    enable_irq(mpp->irq);
    mpp_taskqueue_trigger_work(mpp);
    if (session->callback && clbk_en)
        session->callback(session->chn_id, MPP_VCODEC_EVENT_FRAME, NULL);
}

static void mpp_rkvenc_worker(struct kthread_work *work_s)
{
    struct mpp_dev *mpp = container_of(work_s, struct mpp_dev, work);
    struct mpp_taskqueue *queue = mpp->queue;
    struct mpp_task *mpp_task;
    unsigned long flags;

    mpp_debug_enter();
    spin_lock_irqsave(&queue->dev_lock, flags);

    /* 1. check reset process */
    if (atomic_read(&mpp->reset_request) && !list_empty(&queue->running_list)) {
        disable_irq(mpp->irq);
        mpp_dev_reset(mpp);
        enable_irq(mpp->irq);
    }
    spin_unlock_irqrestore(&queue->dev_lock, flags);

    mpp_power_on(mpp);

    spin_lock_irqsave(&queue->dev_lock, flags);
    if (!list_empty(&queue->running_list))
        goto done;

    /* 2. check and process pending mpp_task */
    mpp_task = mpp_taskqueue_get_pending_task(queue);
    if (!mpp_task)
        goto done;

    if (atomic_read(&mpp->suspend_en))
        goto done;
    down(&mpp->work_sem);
    mpp_taskqueue_pending_to_run(queue, mpp_task);

    mpp_time_record(mpp_task);
    set_bit(TASK_STATE_START, &mpp_task->state);
    if (mpp_task->session->pp_session)
        rkvenc_pp_run(mpp, mpp_task);
    else
        rkvenc_run(mpp, mpp_task);

done:
    spin_unlock_irqrestore(&queue->dev_lock, flags);
    if (list_empty(&queue->running_list))
        mpp_power_off(mpp);
    mpp_session_clean_detach(queue);
}

#ifdef CONFIG_PROC_FS
static int rkvenc_procfs_remove(struct mpp_dev *mpp)
{
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);

    if (enc->procfs) {
        proc_remove(enc->procfs);
        enc->procfs = NULL;
    }

    return 0;
}

static int rkvenc_dump_session(struct mpp_session *session, struct seq_file *seq)
{
    struct rkvenc2_session_priv *priv = session->priv;
    struct rkvenc_debug_info *info = &priv->info;

    down_read(&priv->rw_sem);
    /* item name */
    seq_puts(seq, "\n--------hw session infos");
    seq_puts(seq, "------------------------------------------------------\n");
    seq_printf(seq, "%8s|", "ID");
    seq_printf(seq, "%8s|", "device");
    seq_printf(seq, "%15s|%10s|%12s|%10s|%15s|%13s|%13s|%13s",
               "hw_running", "online", "wrap_ovfl", "bs_ovfl", "wrap_src_mis",
               "enc_err_cnt", "enc_err_irq", "enc_err_st");
    seq_puts(seq, "\n");

    seq_printf(seq, "%8d|", session->chn_id);
    seq_printf(seq, "%8s|", mpp_device_name[session->device_type]);
    seq_printf(seq, "%15d|%10d|%12d|%10d|%15d|%13d|%13x|%13x",
               info->hw_running, session->online, info->wrap_overflow_cnt,
               info->bsbuf_overflow_cnt, info->wrap_source_mis_cnt,
               info->enc_err_cnt, info->enc_err_irq, info->enc_err_status);
    seq_puts(seq, "\n");
    if (info->bsbuf_overflow_cnt) {
        seq_puts(seq, "\n--------bitstream buffer addr");
        seq_puts(seq, "------------------------------------------------------\n");
        seq_printf(seq, "%10s|%10s|%10s|%10s|\n",
                   "top", "bot", "wr", "rd");
        seq_printf(seq, "%10x|%10x|%10x|%10x|\n",
                   info->bsbuf_info[0], info->bsbuf_info[1],
                   info->bsbuf_info[2], info->bsbuf_info[3]);
    }
    up_read(&priv->rw_sem);

    return 0;
}

static int rkvenc_show_session_info(struct seq_file *seq, void *offset)
{
    struct mpp_session *session = NULL, *n;
    struct mpp_dev *mpp = seq->private;

    mutex_lock(&mpp->srv->session_lock);
    list_for_each_entry_safe(session, n, &mpp->srv->session_list, session_link) {
        if (session->device_type != MPP_DEVICE_RKVENC)
            continue;
        if (!session->priv)
            continue;
        if (mpp->dev_ops->dump_session)
            mpp->dev_ops->dump_session(session, seq);
    }
    mutex_unlock(&mpp->srv->session_lock);

    return 0;
}

static int rkvenc_procfs_init(struct mpp_dev *mpp)
{
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);
    struct device_node *of_node;
    char name[32];

    if (!mpp->dev || !mpp_dev_of_node(mpp->dev) || !mpp->srv || !mpp->srv->procfs)
        return -EINVAL;

    of_node = mpp_dev_of_node(mpp->dev);
    if (!of_node->name)
        return -EINVAL;

    snprintf(name, sizeof(name) - 1, "%s%d", of_node->name, mpp->core_id);

    enc->procfs = proc_mkdir(name, mpp->srv->procfs);
    if (IS_ERR_OR_NULL(enc->procfs)) {
        mpp_err("failed on open procfs\n");
        enc->procfs = NULL;
        return -EIO;
    }
    /* for debug */
    mpp_procfs_create_clk_rw("aclk", 0644, enc->procfs, &enc->aclk_info);
    mpp_procfs_create_clk_rw("clk_core", 0644, enc->procfs, &enc->core_clk_info);
    mpp_procfs_create_clk_rw("hclk", 0644, enc->procfs, &enc->hclk_info);
    mpp_procfs_create_u32("dump_regs", 0644, enc->procfs, &mpp->dump_regs);
    mpp_procfs_create_u32("dump_reg_s", 0644, enc->procfs, &enc->dump_reg_s);
    mpp_procfs_create_u32("dump_reg_e", 0644, enc->procfs, &enc->dump_reg_e);
    /* for show session info */
    proc_create_single_data("sessions-info", 0444, enc->procfs, rkvenc_show_session_info, mpp);

    return 0;
}

#else
static inline int rkvenc_procfs_remove(struct mpp_dev *mpp)
{
    return 0;
}

static inline int rkvenc_procfs_init(struct mpp_dev *mpp)
{
    return 0;
}

static inline int rkvenc_dump_session(struct mpp_session *session, struct seq_file *seq)
{
    return 0;
}
#endif

static int rkvenc_init(struct mpp_dev *mpp)
{
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);
    struct device_node *of_node = mpp_dev_of_node(mpp->dev);
    int ret;

    /* Get clock info from dtsi */
    ret = mpp_get_clk_info(mpp, &enc->aclk_info, "aclk_vcodec");
    if (ret)
        mpp_err("failed on clk_get aclk_vcodec\n");
    ret = mpp_get_clk_info(mpp, &enc->hclk_info, "hclk_vcodec");
    if (ret)
        mpp_err("failed on clk_get hclk_vcodec\n");
    ret = mpp_get_clk_info(mpp, &enc->core_clk_info, "clk_core");
    if (ret)
        mpp_err("failed on clk_get clk_core\n");

    /* Get normal max workload from dtsi */
    of_property_read_u32(of_node, "rockchip,default-max-load", &enc->default_max_load);
    /* Set default rates */
    mpp_set_clk_info_rate_hz(&enc->aclk_info, CLK_MODE_DEFAULT, 300 * MHZ);
    mpp_set_clk_info_rate_hz(&enc->core_clk_info, CLK_MODE_DEFAULT, 600 * MHZ);

    /* Get reset control from dtsi */
    enc->rst_a = mpp_reset_control_get(mpp, RST_TYPE_A, "video_a");
    if (!enc->rst_a)
        mpp_err("No aclk reset resource define\n");
    enc->rst_h = mpp_reset_control_get(mpp, RST_TYPE_H, "video_h");
    if (!enc->rst_h)
        mpp_err("No hclk reset resource define\n");
    enc->rst_core = mpp_reset_control_get(mpp, RST_TYPE_CORE, "video_core");
    if (!enc->rst_core)
        mpp_err("No core reset resource define\n");

    return 0;
}

static int rkvenc_reset(struct mpp_dev *mpp)
{
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);
    struct rkvenc_hw_info *hw = enc->hw_info;
    u32 rst_status = 0;
    int ret = 0;

    mpp_debug_enter();

    mpp_write(mpp, hw->int_mask_base, 0x3FF);
    mpp_write(mpp, hw->enc_clr_base, 0x5);
    ret = readl_relaxed_poll_timeout(mpp->reg_base + hw->int_sta_base,
                                     rst_status, rst_status & RKVENC_SCLR_DONE_STA, 0, 1000);
    if (ret)
        mpp_err("soft reset timeout %#08x\n", mpp_read(mpp, 0x2c));

    mpp_write(mpp, hw->enc_clr_base, 0x6);
    udelay(5);
    mpp_write(mpp, hw->enc_clr_base, 0x4);
    mpp_write(mpp, hw->int_clr_base, 0xffffffff);
    mpp_write(mpp, hw->int_sta_base, 0);

    mpp_debug_leave();

    return ret;
}

static int rkvenc_clk_on(struct mpp_dev *mpp)
{
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);

    mpp_clk_safe_enable(enc->aclk_info.clk);
    mpp_clk_safe_enable(enc->hclk_info.clk);
    mpp_clk_safe_enable(enc->core_clk_info.clk);

    return 0;
}

static int rkvenc_clk_off(struct mpp_dev *mpp)
{
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);

    clk_disable_unprepare(enc->aclk_info.clk);
    clk_disable_unprepare(enc->hclk_info.clk);
    clk_disable_unprepare(enc->core_clk_info.clk);

    return 0;
}

static int rkvenc_set_freq(struct mpp_dev *mpp, struct mpp_task *mpp_task)
{
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);
    struct rkvenc_task *task = to_rkvenc_task(mpp_task);

    mpp_clk_set_rate(&enc->aclk_info, task->clk_mode);
    mpp_clk_set_rate(&enc->core_clk_info, task->clk_mode);

    return 0;
}

#if ROCKCHIP_DVBM_ENABLE
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

static int rkvenc_dvbm_setup(struct mpp_dev *mpp, struct dvbm_isp_cfg_t *isp_cfg)
{
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);
    u32 isp_id = isp_cfg->chan_id;
    u32 ybuf_bot = isp_cfg->ybuf_bot + isp_cfg->dma_addr;
    u32 ybuf_top = isp_cfg->ybuf_top + isp_cfg->dma_addr;
    u32 cbuf_bot = isp_cfg->cbuf_bot + isp_cfg->dma_addr;
    u32 cbuf_top = isp_cfg->cbuf_top + isp_cfg->dma_addr;
    u32 ybuf_start = ybuf_bot;
    u32 cbuf_start = cbuf_bot;

    mpp_err("chan %d y [%#08x %#08x %#08x] uv [%#08x %#08x %#08x] stride [%d %d %d]\n",
            isp_id, ybuf_bot, ybuf_top, ybuf_start, cbuf_bot, cbuf_top, cbuf_start,
            isp_cfg->ybuf_fstd, isp_cfg->ybuf_lstd, isp_cfg->cbuf_fstd);

    if (!enc->dvbm_setup) {
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
    }

    mpp_write(mpp, dvbm_stride[isp_id].y_lstrid, isp_cfg->ybuf_lstd);
    mpp_write(mpp, dvbm_stride[isp_id].y_fstrid, isp_cfg->ybuf_fstd);
    mpp_write(mpp, dvbm_stride[isp_id].c_fstrid, isp_cfg->cbuf_fstd);

    return 0;
}

int rkvenc_dvbm_callback(void *ctx, enum dvbm_cb_event event, void *arg)
{
    struct mpp_dev *mpp = (struct mpp_dev *)ctx;
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);

    switch (event) {
    case DVBM_ISP_REQ_CONNECT: {
        u32 isp_id = *(u32*)arg;

        if (mpp->online_mode == MPP_ENC_ONLINE_MODE_SW)
            return 0;

        mpp_debug(DEBUG_ISP_INFO, "isp %d connect\n", isp_id);
        mpp_write(mpp, RKVENC_DVBM_CFG, DVBM_ISP_CONNETC | RKVENC_DVBM_EN | VEPU_CONNETC_CUR);

        if (!enc->dvbm_setup)
            atomic_set(&enc->isp_fcnt, 0);

        set_bit(isp_id, &enc->dvbm_setup);
    } break;
    case DVBM_ISP_REQ_DISCONNECT: {
        u32 isp_id = *(u32*)arg;

        if (mpp->online_mode == MPP_ENC_ONLINE_MODE_SW)
            return 0;

        clear_bit(isp_id, &enc->dvbm_setup);
        mpp_debug(DEBUG_ISP_INFO, "isp %d disconnect 0x%lx\n", isp_id, enc->dvbm_setup);

        if (!enc->dvbm_setup) {
            mpp_write(mpp, RKVENC_DVBM_CFG, 0);
            mpp->always_on = 0;
            atomic_set(&enc->isp_fcnt, 0);
        }
    } break;
    case DVBM_ISP_SET_DVBM_CFG: {
        struct dvbm_isp_cfg_t *isp_cfg = (struct dvbm_isp_cfg_t *)arg;

        if (mpp->online_mode == MPP_ENC_ONLINE_MODE_SW)
            return 0;

        mpp_power_on(mpp);
        mpp->always_on = 1;
        rkvenc_dvbm_setup(mpp, isp_cfg);
    } break;
    case DVBM_VEPU_NOTIFY_FRM_STR: {
        u32 id = *(u32*)arg;

#if ISP_DEBUG
        if (mpp->isp_base)
            mpp_debug(DEBUG_ISP_INFO, "isp frame %d start addr[%#x %#x]\n", id,
                      readl(mpp->isp_base), readl(mpp->isp_base + 4));
        else
#endif
            mpp_debug(DEBUG_ISP_INFO, "isp frame %d start\n", id);

        atomic_inc(&enc->isp_fcnt);
    } break;
    case DVBM_VEPU_NOTIFY_FRM_END: {
        u32 id = *(u32*)arg;

        mpp_debug(DEBUG_ISP_INFO, "isp frame %d end\n", id);
    } break;
    default : {
    } break;
    }

    return 0;
}

static int rkvenc_dvbm_deinit(struct mpp_dev *mpp)
{
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);

    if (!IS_ENABLED(CONFIG_ROCKCHIP_DVBM))
        return 0;

    if (enc->dvbm_port) {
        rk_dvbm_put(enc->dvbm_port);
        enc->dvbm_port = NULL;
    }

    return 0;
}

static int rkvenc_dvbm_init(struct mpp_dev *mpp)
{
    struct device *dev = mpp->dev;
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);
    struct platform_device *pdev_dvbm = NULL;
    struct device_node *np_dvbm = NULL;

    if (!IS_ENABLED(CONFIG_ROCKCHIP_DVBM))
        return 0;

    np_dvbm = of_parse_phandle(mpp_dev_of_node(dev), "dvbm", 0);
    if (!np_dvbm || !of_device_is_available(np_dvbm))
        mpp_err("failed to get device node\n");
    else {
        pdev_dvbm = of_find_device_by_node(np_dvbm);
        enc->dvbm_port = rk_dvbm_get_port(pdev_dvbm, DVBM_VEPU_PORT);
        of_node_put(np_dvbm);
        if (enc->dvbm_port) {
            struct dvbm_cb dvbm_cb;

            dvbm_cb.cb = rkvenc_dvbm_callback;
            dvbm_cb.ctx = enc;

            rk_dvbm_set_cb(enc->dvbm_port, &dvbm_cb);
            enc->dvbm_setup = 0;
        }
    }

    return 0;
}

static void update_online_info(struct mpp_dev *mpp, u32 pipe_id, u32 online)
{
    struct dvbm_addr_cfg dvbm_adr;

    dvbm_adr.chan_id = pipe_id;
    rk_dvbm_ctrl(NULL, DVBM_VEPU_GET_ADR, &dvbm_adr);
    if (!dvbm_adr.ybuf_bot || !dvbm_adr.cbuf_bot)
        dev_err(mpp->dev, "the dvbm address do not ready!\n");

    /* update wrap addr */
    mpp_write_relaxed(mpp, 0x270, dvbm_adr.ybuf_top);
    mpp_write_relaxed(mpp, 0x274, dvbm_adr.cbuf_top);
    mpp_write_relaxed(mpp, 0x278, dvbm_adr.ybuf_bot);
    mpp_write_relaxed(mpp, 0x27c, dvbm_adr.cbuf_bot);
    if (online == MPP_ENC_ONLINE_MODE_SW) {
        mpp_write_relaxed(mpp, 0x280, dvbm_adr.ybuf_sadr);
        mpp_write_relaxed(mpp, 0x284, dvbm_adr.cbuf_sadr);
        mpp_write_relaxed(mpp, 0x288, dvbm_adr.cbuf_sadr);
    }
    if (mpp_read_relaxed(mpp, RKVENC_JPEG_BASE_CFG) & JRKVENC_PEGE_ENABLE) {
        mpp_write_relaxed(mpp, 0x410, dvbm_adr.ybuf_bot);
        mpp_write_relaxed(mpp, 0x414, dvbm_adr.cbuf_bot);
        mpp_write_relaxed(mpp, 0x418, dvbm_adr.ybuf_top);
        mpp_write_relaxed(mpp, 0x41c, dvbm_adr.cbuf_top);
        if (online == MPP_ENC_ONLINE_MODE_SW) {
            mpp_write_relaxed(mpp, 0x420, dvbm_adr.ybuf_sadr);
            mpp_write_relaxed(mpp, 0x424, dvbm_adr.cbuf_sadr);
            mpp_write_relaxed(mpp, 0x428, dvbm_adr.cbuf_sadr);
        }
    }
}

#else

static inline int rkvenc_dvbm_init(struct mpp_dev *mpp)
{
    return 0;
}

static inline int rkvenc_dvbm_deinit(struct mpp_dev *mpp)
{
    return 0;
}

static inline void update_online_info(struct mpp_dev *mpp, u32 pipe_id, u32 online)
{
}

#endif

static struct mpp_hw_ops rkvenc_hw_ops = {
    .init = rkvenc_init,
    .clk_on = rkvenc_clk_on,
    .clk_off = rkvenc_clk_off,
    .set_freq = rkvenc_set_freq,
    .reset = rkvenc_reset,
};

static struct mpp_dev_ops rkvenc_dev_ops_v2 = {
    .alloc_task = rkvenc_init_task,
    .prepare = NULL,
    .run = rkvenc_run,
    .irq = NULL,
    .isr = NULL,
    .finish = rkvenc_finish,
    .result = rkvenc_result,
    .free_task = rkvenc_free_task,
    .ioctl = rkvenc_control,
    .init_session = rkvenc_init_session,
    .free_session = rkvenc_free_session,
    .dump_session = rkvenc_dump_session,
};

static const struct mpp_dev_var rkvenc_rv1103b_data = {
    .device_type = MPP_DEVICE_RKVENC,
    .hw_info = &rkvenc_500_hw_info.hw,
    .hw_ops = &rkvenc_hw_ops,
    .dev_ops = &rkvenc_dev_ops_v2,
};

static const struct of_device_id mpp_rkvenc_dt_match[] = {
    {
        .compatible = "rockchip,rkv-encoder-rv1103b",
        .data = &rkvenc_rv1103b_data,
    },
    {},
};

static int rkvenc_probe_default(struct platform_device *pdev)
{
    KmppDevData *data;
    struct device *dev = &pdev->dev;
    struct rkvenc_dev *enc = NULL;
    struct mpp_dev *mpp = NULL;
    const struct of_device_id *match = NULL;
    int ret = 0;

    data = pdev_set_mpp(pdev, enc);
    if (!data)
        return -ENOMEM;

    mpp = &enc->mpp;

    if (mpp_dev_of_node(dev)) {
        match = of_match_device(mpp_rkvenc_dt_match, &pdev->dev);
        if (!match) {
            dev_err(dev, "dt match failed!\n");
            return -ENODEV;
        }
        mpp->var = (struct mpp_dev_var *)match->data;
    }

    ret = mpp_dev_probe(mpp, pdev);
    if (ret)
        return ret;

#if ISP_DEBUG
    mpp->isp_base = ioremap(0x20d01660, 0x8);
    if (!mpp->isp_base)
        dev_err(mpp->dev, "isp base map failed!\n");
#endif
    /* config vepu qos to 0x101 */
    enc->vepu_qos = ioremap(0x20350008, 4);
    if (enc->vepu_qos)
        writel(0x101, enc->vepu_qos);
    else
        dev_err(enc->mpp.dev, "vepu_qos map failed!\n");
    kthread_init_work(&mpp->work, mpp_rkvenc_worker);
    ret = devm_request_threaded_irq(dev, mpp->irq, rkvenc_500_irq, NULL,
                                    IRQF_ONESHOT, dev_name(dev), mpp);
    if (ret) {
        dev_err(dev, "register interrupter runtime failed\n");
        goto failed_get_irq;
    }

    rkvenc_dvbm_init(mpp);

    enc->hw_info = to_rkvenc_info(mpp->var->hw_info);
    enc->dump_reg_s = enc->hw_info->hw.reg_start;
    enc->dump_reg_s = enc->hw_info->hw.reg_end;
    spin_lock_init(&enc->dvbm_lock);
    rkvenc_procfs_init(mpp);
    mpp_dev_register_srv(mpp, mpp->srv);

    return 0;

failed_get_irq:
    mpp_dev_remove(mpp);

    return ret;
}

static int rkvenc_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    int ret;

    dev_info(dev, "probing start\n");

    ret = rkvenc_probe_default(pdev);

    dev_info(dev, "probing finish\n");

    return ret;
}

static int rkvenc_remove(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct rkvenc_dev *enc = (struct rkvenc_dev *)pdev_get_legacy(pdev);

#if ISP_DEBUG
    if (enc->mpp.isp_base)
        iounmap(enc->mpp.isp_base);
#endif
    if (enc->vepu_qos)
        iounmap(enc->vepu_qos);
    dev_info(dev, "remove device\n");
    mpp_dev_remove(&enc->mpp);
    rkvenc_procfs_remove(&enc->mpp);
    rkvenc_dvbm_deinit(&enc->mpp);

    return 0;
}

static void rkvenc_shutdown(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct rkvenc_dev *enc = (struct rkvenc_dev *)pdev_get_legacy(pdev);
    struct mpp_dev *mpp = &enc->mpp;
    int val;
    int ret;

    dev_info(dev, "shutdown device\n");

    atomic_inc(&mpp->srv->shutdown_request);
    ret = readx_poll_timeout(atomic_read, &mpp->task_count, val, val == 0, 1000, 200000);
    if (ret == -ETIMEDOUT)
        dev_err(dev, "wait total running time out\n");

    dev_info(dev, "shutdown success\n");
}

static void rkvenc_dvbm_reg_sav_restore(struct mpp_dev *mpp, bool is_save)
{
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);
    u32 off, i;

    if (is_save) {
        enc->dvbm_cfg = mpp_read(mpp, RKVENC_DVBM_CFG);
        if (!(enc->dvbm_cfg & RKVENC_DVBM_EN))
            return;
        for (off = RKVENC_DVBM_REG_S; off <= RKVENC_DVBM_REG_E; off += 4) {
            i = (off - RKVENC_DVBM_REG_S) / 4;
            enc->dvbm_reg_save[i] = mpp_read(mpp, off);
        }
    } else {
        u32 dvbm_cfg;

        if (!(enc->dvbm_cfg & RKVENC_DVBM_EN))
            return;

        for (off = RKVENC_DVBM_REG_S; off <= RKVENC_DVBM_REG_E; off += 4) {
            i = (off - RKVENC_DVBM_REG_S) / 4;
            mpp_write(mpp, off, enc->dvbm_reg_save[i]);
        }
        dvbm_cfg = enc->dvbm_cfg & (~DVBM_VEPU_CONNETC);
        mpp_write(mpp, RKVENC_DVBM_CFG, dvbm_cfg);
        atomic_set(&enc->isp_fcnt, 0);
    }
}

static int __maybe_unused rkvenc_runtime_suspend(struct device *dev)
{
    struct rkvenc_dev *enc = (struct rkvenc_dev *)dev_to_legacy(dev);
    struct mpp_dev *mpp = &enc->mpp;

    mpp_debug(DEBUG_POWER, "%s suspend device ++\n", dev_name(dev));
    if (!atomic_xchg(&mpp->suspend_en, 1))
        down(&mpp->work_sem);

    rkvenc_dvbm_reg_sav_restore(mpp, true);
    mpp_debug(DEBUG_POWER, "%s suspend device --\n", dev_name(dev));

    return 0;
}

static int __maybe_unused rkvenc_runtime_resume(struct device *dev)
{
    struct rkvenc_dev *enc = (struct rkvenc_dev *)dev_to_legacy(dev);
    struct mpp_dev *mpp = &enc->mpp;

    mpp_debug(DEBUG_POWER, "%s resume device ++\n", dev_name(dev));
    if (atomic_xchg(&mpp->suspend_en, 0))
        up(&mpp->work_sem);

    rkvenc_dvbm_reg_sav_restore(mpp, false);
    mpp_taskqueue_trigger_work(mpp);
    mpp_debug(DEBUG_POWER, "%s resume device --\n", dev_name(dev));

    return 0;
}

static const struct dev_pm_ops rkvenc_pm_ops = {
    MPP_SET_SYSTEM_SLEEP_PM_OPS(rkvenc_runtime_suspend, rkvenc_runtime_resume)
};

struct platform_driver rockchip_rkvenc500_driver = {
    .probe = rkvenc_probe,
    .remove = rkvenc_remove,
    .shutdown = rkvenc_shutdown,
    .driver = {
        .name = RKVENC_DRIVER_NAME,
        .of_match_table = of_match_ptr(mpp_rkvenc_dt_match),
        .pm = &rkvenc_pm_ops,
    },
};

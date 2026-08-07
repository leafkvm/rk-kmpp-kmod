// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2022 Rockchip Electronics Co., Ltd
 *
 * author:
 *  Ding Wei, leo.ding@rock-chips.com
 */

#define pr_fmt(fmt) "rkvenc_540c: " fmt

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

#define RKVENC_DRIVER_NAME              "mpp_rkvenc_540c"

#define ROCKCHIP_DVBM_ENABLE            1

#define RKVENC_SESSION_MAX_BUFFERS      40

/* irq status definition */
#define RKVENC_DVBM_DISCONNECT          (BIT(15))
#define RKVENC_JPEG_OVERFLOW            (BIT(13))
#define RKVENC_VIDEO_OVERFLOW           (BIT(4))
#define RKVENC_SLICE_DONE_STATUS        (BIT(3))
#define RKVENC_ENC_DONE_STATUS          (BIT(0))
#define REC_FBC_DIS_CLASS_OFFSET        (36)

#define RKVENC_JPEG_BASE_CFG            (0x47c)
#define JRKVENC_PEGE_ENABLE             (BIT(31))

#define RKVENC_VIDEO_BSBT               (0x2b0)
#define RKVENC_VIDEO_BSBB               (0x2b4)
#define RKVENC_VIDEO_BSBS               (0x2b8)
#define RKVENC_VIDEO_BSBR               (0x2bc)
#define RKVENC_JPEG_BSBT                (0x400)
#define RKVENC_JPEG_BSBB                (0x404)
#define RKVENC_JPEG_BSBS                (0x40c)
#define RKVENC_JPEG_BSBR                (0x408)

#define RKVENC_REG_EXT_LINE_BUF_BASE    (22)

#define RKVENC_REG_ENC_PIC              (36)
#define RKVENC_BIT_ENC_STND             (BIT(0))
#define RKVENC_BIT_VAL_H264             (0)
#define RKVENC_BIT_VAL_H265             (1)
#define RKVENC_BIT_SLEN_FIFO            BIT(30)
#define RKVENC_REG_SLI_SPLIT            (60)
#define RKVENC_BIT_SLI_SPLIT            BIT(0)
#define RKVENC_BIT_SLI_FLUSH            BIT(15)

#define RKVENC_REG_SLICE_NUM_BASE       (0x4034)
#define RKVENC_REG_SLICE_LEN_BASE       (0x4038)

#define to_rkvenc_info(info) \
        container_of(info, struct rkvenc_hw_info, hw)
#define to_rkvenc_task(ctx) \
        container_of(ctx, struct rkvenc_task, mpp_task)
#define to_rkvenc_dev(dev) \
        container_of(dev, struct rkvenc_dev, mpp)

enum RKVENC_MODE {
    RKVENC_MODE_ONEFRAME        = 0,
    RKVENC_MODE_LINK_ADD        = 1,
    RKVENC_MODE_LINK_ONEFRAME   = 2,

    RKVDEC_MODE_BUTT,
};

enum RKVENC_FORMAT_TYPE {
    RKVENC_FMT_BASE             = 0x0000,
    RKVENC_FMT_H264E            = RKVENC_FMT_BASE + 0,
    RKVENC_FMT_H265E            = RKVENC_FMT_BASE + 1,
    RKVENC_FMT_JPEGE            = RKVENC_FMT_BASE + 2,
    RKVENC_FMT_BUTT,

    RKVENC_FMT_OSD_BASE = RKVENC_FMT_BUTT,
    RKVENC_FMT_H264E_OSD        = RKVENC_FMT_OSD_BASE + 0,
    RKVENC_FMT_H265E_OSD        = RKVENC_FMT_OSD_BASE + 1,
    RKVENC_FMT_JPEGE_OSD        = RKVENC_FMT_OSD_BASE + 2,
    RKVENC_FMT_OSD_BUTT,
};

enum RKVENC_CLASS_TYPE {
    RKVENC_CLASS_CTL            = 0,    /* base */
    RKVENC_CLASS_PIC            = 1,    /* picture configure */
    RKVENC_CLASS_RC             = 2,    /* rate control */
    RKVENC_CLASS_PAR            = 3,    /* parameter */
    RKVENC_CLASS_SQI            = 4,    /* subjective Adjust */
    RKVENC_CLASS_SCL_JPGTBL     = 5,    /* scaling list and jpeg table */
    RKVENC_CLASS_OSD            = 6,    /* osd */
    RKVENC_CLASS_ST             = 7,    /* status */
    RKVENC_CLASS_DEBUG          = 8,    /* debug */
    RKVENC_CLASS_BUTT,
};

enum RKVENC_CLASS_FD_TYPE {
    RKVENC_CLASS_FD_BASE        = 0,    /* base */
    RKVENC_CLASS_FD_OSD         = 1,    /* osd */
    RKVENC_CLASS_FD_BUTT,
};

struct rkvenc_reg_msg {
    u32 base_s;
    u32 base_e;
    /* class base for link */
    u32 link_s;
    /* class end for link */
    u32 link_e;
    /* class bytes for link */
    u32 link_len;
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
    u32 st_enc;
    u32 dvbm_cfg;
    /* for link */
    u32 link_stop_mask;
    u32 link_addr_base;
    u32 link_status_base;
    u32 link_node_base;
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
    /* split output / slice mode info */
    u32 task_split;
    u32 last_slice_found;
    /* fix BUG: count slice length in each frame when slice fifo */
    u32 slice_total_len;
    MppEncSliceInfo sli_info;
};

struct rkvenc_link_header {
    struct {
        u32 node_core_id    : 2;
        u32 node_int        : 1;
        u32 reserved        : 1;
        u32 task_id         : 12;
        u32 reserved1       : 16;
    } node_cfg;
    union {
        u32 valid           : 1;
        u32 lkt_addr        : 32;
    } pic_cfg;
    union {
        u32 valid           : 1;
        u32 lkt_addr        : 32;
    } rc_cfg;
    union {
        u32 valid           : 1;
        u32 lkt_addr        : 32;
    } param_cfg;
    union {
        u32 valid           : 1;
        u32 lkt_addr        : 32;
    } sqi_cfg;
    union {
        u32 valid           : 1;
        u32 lkt_addr        : 32;
    } scal_cfg;
    union {
        u32 valid           : 1;
        u32 lkt_addr        : 32;
    } osd_cfg;
    union {
        u32 valid           : 1;
        u32 lkt_addr        : 32;
    } status_cfg;
    union {
        u32 valid           : 1;
        u32 next_addr       : 32;
    } next_node;
};

struct rkvenc_link_status {
    u32 cfg_done_num        : 8;
    u32 cfg_num             : 8;
    u32 int_num             : 8;
    u32 enc_num             : 8;
};

struct rkvenc_link_dev {
    struct rkvenc_dev *enc;
    u32 table_num;
    struct mpp_dma_buffer *table;
    u32 class_off[RKVENC_CLASS_BUTT];

    /* list for used link table */
    struct list_head used_list;
    /* list for unused link table */
    struct list_head unused_list;
    /* lock for the above table lists */
    struct mutex list_mutex;
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
    u32 enc_err_status;
};

struct rkvenc2_session_priv {
    struct rw_semaphore rw_sem;
    u32 dvbm_en;
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
    struct dvbm_port *port;
    u32 dvbm_overflow;
#endif
    u32 line_cnt;
    u32 frm_id_start;
    u32 frm_id_end;

    /* for link mode */
    u32 task_capacity;
    struct rkvenc_link_dev *link;
    enum RKVENC_MODE link_mode;
    atomic_t link_task_cnt;
    u32 link_run;
    u32 video_wr_addr;
    u32 jpeg_wr_addr;
};

static struct rkvenc_hw_info rkvenc_rv1106_hw_info = {
    .hw = {
        .reg_num = 254,
        .reg_id = 0,
        .reg_en = 4,
        .reg_start = 160,
        .reg_end = 253,
    },
    .reg_class = RKVENC_CLASS_BUTT,
    .reg_msg[RKVENC_CLASS_CTL] = {
        .base_s = 0x0000,
        .base_e = 0x0120,
    },
    .reg_msg[RKVENC_CLASS_PIC] = {
        .base_s = 0x0270,
        .base_e = 0x0480,
        .link_len = 532,
    },
    .reg_msg[RKVENC_CLASS_RC] = {
        .base_s = 0x1000,
        .base_e = 0x110c,
        .link_len = 272,
    },
    .reg_msg[RKVENC_CLASS_PAR] = {
        .base_s = 0x1700,
        .base_e = 0x19cc,
        .link_len = 720,
    },
    .reg_msg[RKVENC_CLASS_SQI] = {
        .base_s = 0x2000,
        .base_e = 0x20fc,
        .link_len = 256,
    },
    .reg_msg[RKVENC_CLASS_SCL_JPGTBL] = {
        .base_s = 0x21e0,
        .base_e = 0x2dfc,
        .link_len = 3104,
    },
    .reg_msg[RKVENC_CLASS_OSD] = {
        .base_s = 0x3000,
        .base_e = 0x326c,
        .link_len = 624,
    },
    .reg_msg[RKVENC_CLASS_ST] = {
        .base_s = 0x4000,
        .base_e = 0x424c,
        .link_len = 592,
    },
    .reg_msg[RKVENC_CLASS_DEBUG] = {
        .base_s = 0x5000,
        .base_e = 0x5354,
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
    .st_enc = 0x4020,
    .dvbm_cfg = 0x60,
    .link_stop_mask = 0x0140,
    .link_addr_base = 0x0070,
    .link_status_base = 0x4024,
    .link_node_base = 0x4028,
};

static int rkvenc_reset(struct mpp_dev *mpp);
static void rkvenc_task_timeout(struct work_struct *work_s);
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
            //kfree(task->reg[i].data);
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

        /*data = kzalloc(class_size, GFP_KERNEL);
        if (!data)
            return -ENOMEM;*/
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
    u32 i, j;

    mpp_debug_enter();

    for (i = 0; i < msgs->req_cnt; i++) {
        struct mpp_request *req = &msgs->reqs[i];

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
        case MPP_CMD_SET_REG_ADDR_OFFSET: {
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

    if (!class_reg || !class_size)
        return -EINVAL;

    if (hw->fmt_reg.base < class_offset) {
        mpp_err("invalid fmt_reg[0x%x] class_offset[0x%x]", hw->fmt_reg.base, class_offset);
        return -EINVAL;
    }

    offset = hw->fmt_reg.base - class_offset;
    val = class_reg[offset / sizeof(u32)];
    task->fmt = (val >> bitpos) & ((1 << bitlen) - 1);

    return 0;
}

static void *task_init(struct rkvenc_task *task)
{
    int i = 0;

    for (i = 0; i < RKVENC_CLASS_BUTT; i++)
        task->reg[i].valid = 0;

    task->irq_status = 0;
    task->w_req_cnt = 0;
    task->r_req_cnt = 0;
    return 0;
}

static void rkvenc_dump_simple_dbg(struct mpp_dev *mpp)
{
    pr_err("dvbm_en     -   %d\n", mpp_read(mpp, 0x60) & 0x1);
    pr_err("fmt         -   %d\n", mpp_read(mpp, 0x300) & 0x3);
    pr_err("jpg_en      -   %d\n", mpp_read(mpp, 0x47c) >> 31);
    pr_err("vsldy       -   0x%08x\n", mpp_read(mpp, 0x18));
    pr_err("st_wdg      -   0x%08x\n", mpp_read(mpp, 0x5000));
    pr_err("st_ppl      -   0x%08x\n", mpp_read(mpp, 0x5004));
    pr_err("st_vsp0     -   0x%08x\n", mpp_read(mpp, 0x5008));
    pr_err("st_vsp1     -   0x%08x\n", mpp_read(mpp, 0x500c));
    pr_err("vepu_cycle  -   0x%08x\n", mpp_read(mpp, 0x5200));
}

int rkvenc_dump_dbg(struct mpp_dev *mpp)
{
    u32 i;

    rkvenc_dump_simple_dbg(mpp);

    if (!unlikely(mpp_dev_debug & DEBUG_DUMP_ERR_REG))
        return 0;

    pr_info("=== %s ===\n", __func__);

    for (i = 0; i < RKVENC_CLASS_BUTT; i++) {
        u32 j, s, e;

        s = rkvenc_rv1106_hw_info.reg_msg[i].base_s;
        e = rkvenc_rv1106_hw_info.reg_msg[i].base_e;
#if 1
        if ((i == RKVENC_CLASS_RC) ||
            (i == RKVENC_CLASS_PAR) ||
            (i == RKVENC_CLASS_SQI) ||
            (i == RKVENC_CLASS_SCL_JPGTBL) ||
            (i == RKVENC_CLASS_OSD))
            continue;
#endif
        for (j = s; j <= e; j += 4)
            pr_info("reg[0x%0x] = 0x%08x\n", j, mpp_read(mpp, j));
    }

    return 0;
}

//#if ROCKCHIP_DVBM_ENABLE
#if 1
#define VEPU_LINE_CNT_UNMARK    (~GENMASK(13, 0))
#define VEPU_LINE_CNT           GENMASK(13, 0)
#define VEPU_FRAME_CNT          GENMASK(21, 14)
#define VEPU_FRAME_CNT_OFF      (14)
#define VEPU_LDLY_REG           (0x18)
#define VEPU_DVBM_ID_REG        (0x308)
#define VEPU_DBG_WRK            (0x5004)
#define VEPU_DBG_POS0           (0x5008)
#define VEPU_DBG_POS1           (0x500c)

static int rkvenc_check_overflow(struct rkvenc_dev *enc, struct dvbm_isp_frm_info *info,
                                 u32 cur_fcnt, u32 cur_lcnt)
{
    u32 pos0_y = mpp_read(&enc->mpp, VEPU_DBG_POS0) >> 16;
    u32 pos1_y = mpp_read(&enc->mpp, VEPU_DBG_POS1) >> 16;
    u32 vepu_wrk = mpp_read(&enc->mpp, VEPU_DBG_WRK);
    u32 vepu_lcnt = 16 * max(pos0_y, pos1_y);

    /* 1. check whether isp frame is cur enc frame or not */
    if (cur_fcnt != info->frame_cnt || cur_lcnt >= info->line_cnt) {
        /* 1.1 check overflow */
        if (info->line_cnt > 0 && cur_fcnt != info->frame_cnt) {
            if (info->line_cnt >= info->wrap_line) {
                pr_err("overflow vepu[frm: %d line: %d %d] isp[frm: %d line: %d]\n",
                       cur_fcnt, vepu_lcnt, cur_lcnt,
                       info->frame_cnt, info->line_cnt);
                enc->dvbm_overflow = 1;
            }
            rkvenc_dump_dbg(&enc->mpp);
        }
        /* 1.2 hack for isp interruption disorder */
        if (cur_lcnt < info->max_line_cnt) {
            u32 val = mpp_read(&enc->mpp, VEPU_LDLY_REG);

            val &= VEPU_LINE_CNT_UNMARK;
            val |= info->max_line_cnt;
            mpp_write(&enc->mpp, VEPU_LDLY_REG, val);
            wmb();
            pr_err("line cnt disorder set max %d [%d %d]->[%d %d]\n",
                   info->max_line_cnt, cur_fcnt, cur_lcnt, info->frame_cnt, info->line_cnt);
        }
        return 1;
    }

    /* check overflow */
    if (atomic_read(&enc->on_work)) {
        if ((info->line_cnt - vepu_lcnt) > info->wrap_line) {
            pr_err("overflow vepu[frm: %d line: %d %d] isp[frm: %d line: %d] wrk 0x%08x\n",
                   cur_fcnt, vepu_lcnt, cur_lcnt,
                   info->frame_cnt, info->line_cnt, vepu_wrk);
            enc->dvbm_overflow = 1;
        }
    } else {
        if (info->line_cnt > info->wrap_line) {
            pr_err("overflow vepu[frm: %d line: %d %d] isp[frm: %d line: %d] wrk 0x%08x\n",
                   cur_fcnt, vepu_lcnt, cur_lcnt,
                   info->frame_cnt, info->line_cnt, vepu_wrk);
            enc->dvbm_overflow = 1;
        }
    }

    return 0;
}

static int rkvenc_callback(void* ctx, enum dvbm_cb_event event, void* arg)
{
    struct rkvenc_dev *enc = (struct rkvenc_dev *)ctx;

    if (!enc)
        return 0;

    switch (event) {
    case DVBM_VEPU_REQ_CONNECT : {
        u32 connect = *(u32*)arg;
        unsigned long val = mpp_read(&enc->mpp, 0x18);

        if (!connect)
            clear_bit(24, &val);
        mpp_write(&enc->mpp, 0x18, val);
    } break;
    case DVBM_VEPU_NOTIFY_FRM_STR: {
        enc->frm_id_start = *(u32*)arg;
    } break;
    case DVBM_VEPU_NOTIFY_FRM_END: {
        enc->frm_id_end = *(u32*)arg;
    } break;
    case DVBM_VEPU_NOTIFY_FRM_INFO: {
        struct dvbm_isp_frm_info *info = (struct dvbm_isp_frm_info*)arg;
        u32 cur_fcnt;
        u32 cur_lcnt;
        u32 val;

        /* 1. get cur vepu frame info */
        val = mpp_read(&enc->mpp, VEPU_LDLY_REG);
        cur_fcnt = (val & VEPU_FRAME_CNT) >> VEPU_FRAME_CNT_OFF;
        cur_lcnt = val & VEPU_LINE_CNT;

        /* 2. check overflow */
        if (rkvenc_check_overflow(enc, info, cur_fcnt, cur_lcnt))
            return 0;
        /* 3. config frame info to vepu */
        val &= VEPU_LINE_CNT_UNMARK;
        val |= info->line_cnt;
        enc->line_cnt = info->line_cnt;
        mpp_write(&enc->mpp, VEPU_LDLY_REG, val);
        wmb();

        mpp_dbg_dvbm("frame cnt %d line cnt %d\n", info->frame_cnt, info->line_cnt);

        if (val != mpp_read(&enc->mpp, VEPU_LDLY_REG)) {
            pr_err("set frame info failed! [%d %d] -> [%d %d]\n",
                   cur_fcnt, cur_lcnt, info->frame_cnt, info->line_cnt);
        }
    } break;
    case DVBM_VEPU_NOTIFY_DUMP: {
        rkvenc_dump_dbg(&enc->mpp);
    } break;
    default : {
    } break;
    }
    return 0;
}

static void update_online_info(struct mpp_dev *mpp, u32 pipe_id)
{
    struct dvbm_addr_cfg dvbm_adr;
    u32 val;

    dvbm_adr.chan_id = pipe_id;
    rk_dvbm_ctrl(NULL, DVBM_VEPU_GET_ADR, &dvbm_adr);
    if (!dvbm_adr.ybuf_bot || !dvbm_adr.cbuf_bot)
        dev_err(mpp->dev, "the dvbm address do not ready!\n");
    /*
     * use frame_id bit field to store isp pipe id to identify
     * which isp channel is bound to the current wrap
     */
    val = mpp_read_relaxed(mpp, 0x308) & (~GENMASK(7, 0));
    mpp_write_relaxed(mpp, 0x308, val | pipe_id);

    /* update wrap addr */
    mpp_write_relaxed(mpp, 0x270, dvbm_adr.ybuf_top);
    mpp_write_relaxed(mpp, 0x274, dvbm_adr.cbuf_top);
    mpp_write_relaxed(mpp, 0x278, dvbm_adr.ybuf_bot);
    mpp_write_relaxed(mpp, 0x27c, dvbm_adr.cbuf_bot);
    mpp_write_relaxed(mpp, 0x280, dvbm_adr.ybuf_sadr);
    mpp_write_relaxed(mpp, 0x284, dvbm_adr.cbuf_sadr);
    mpp_write_relaxed(mpp, 0x288, dvbm_adr.cbuf_sadr);
    if (mpp_read_relaxed(mpp, RKVENC_JPEG_BASE_CFG) & JRKVENC_PEGE_ENABLE) {
        mpp_write_relaxed(mpp, 0x410, dvbm_adr.ybuf_bot);
        mpp_write_relaxed(mpp, 0x414, dvbm_adr.cbuf_bot);
        mpp_write_relaxed(mpp, 0x418, dvbm_adr.ybuf_top);
        mpp_write_relaxed(mpp, 0x41c, dvbm_adr.cbuf_top);
        mpp_write_relaxed(mpp, 0x420, dvbm_adr.ybuf_sadr);
        mpp_write_relaxed(mpp, 0x424, dvbm_adr.cbuf_sadr);
        mpp_write_relaxed(mpp, 0x428, dvbm_adr.cbuf_sadr);
    }
}
#endif

static void rkvenc_check_split_task(struct rkvenc_task *task)
{
    u32 slen_fifo_en = 0;
    u32 sli_split_en = 0;

    if (task->reg[RKVENC_CLASS_PIC].valid) {
        u32 *reg = task->reg[RKVENC_CLASS_PIC].data;
        u32 enc_stnd = reg[RKVENC_REG_ENC_PIC] & RKVENC_BIT_ENC_STND;

        slen_fifo_en = (reg[RKVENC_REG_ENC_PIC] & RKVENC_BIT_SLEN_FIFO) ? 1 : 0;
        sli_split_en = (reg[RKVENC_REG_SLI_SPLIT] & RKVENC_BIT_SLI_SPLIT) ? 1 : 0;

        /*
         * FIXUP: rkvenc2 hardware bug:
         * H.264 encoding has bug when external line buffer and slice flush both
         * are enabled.
         */
        if (sli_split_en && slen_fifo_en &&
            enc_stnd == RKVENC_BIT_VAL_H264 &&
            reg[RKVENC_REG_EXT_LINE_BUF_BASE])
            reg[RKVENC_REG_SLI_SPLIT] &= ~RKVENC_BIT_SLI_FLUSH;
    }

    task->task_split = sli_split_en && slen_fifo_en;
}

static void *rkvenc_alloc_task(struct mpp_session *session, struct mpp_task_msgs *msgs)
{
    struct mpp_dev *mpp = session->mpp;
    struct rkvenc_task *task = NULL;
    struct mpp_task *mpp_task;
    int ret;
    u32 i;

    mpp_debug_enter();


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

    task_init(task);
    mpp_task = &task->mpp_task;
    mpp_task_init(session, mpp_task);
    mpp_task->hw_info = mpp->var->hw_info;
    mpp_task->clbk_en = 1;
    mpp_task->disable_jpeg = 0;
    task->hw_info = to_rkvenc_info(mpp_task->hw_info);
    /* extract reqs for current task */
    ret = rkvenc_extract_task_msg(session, task, msgs, session->k_space);
    if (ret)
        goto free_task;
    mpp_task->reg = task->reg[0].data;
    /* get format */
    ret = rkvenc_task_get_format(mpp, task);
    if (ret)
        goto free_task;
    /* check rec fbc is disable */
    {
        u32 val = task->reg[RKVENC_CLASS_PIC].data[REC_FBC_DIS_CLASS_OFFSET];
        if (val & 0x80000000)
            mpp_task->clbk_en = 0;
    }

    {
        u32 *wh_reg = rkvenc_get_class_reg(task, 0x310);

        mpp_task->width = (((*wh_reg) & 0x7ff) + 1) << 3;
        mpp_task->height = (((*wh_reg) >> 16) + 1) << 3;
    }

    task->clk_mode = CLK_MODE_NORMAL;

    rkvenc_check_split_task(task);
    mpp_debug_leave();

    return mpp_task;

free_task:
    kfree(task);

    return NULL;
}

static int rkvenc_link_fill_table(struct rkvenc_link_dev *link, struct rkvenc_task *task,
                                  struct mpp_dma_buffer *table)
{
    struct rkvenc_hw_info *hw = task->hw_info;
    struct rkvenc_link_header *hdr;
    struct rkvenc_reg_msg *msg;
    u32 *tb_reg = (u32 *)table->vaddr;
    u32 i, off, s, e, len, si, di;

    //mpp_err("table->iova=%pad\n", &table->iova);
    /* set class data addr valid */
    hdr = (struct rkvenc_link_header *)table->vaddr;
    hdr->node_cfg.node_int = 1;
    // hdr->base_cfg.valid = task->reg[RKVENC_CLASS_BASE].valid;
    hdr->pic_cfg.valid = task->reg[RKVENC_CLASS_PIC].valid;
    hdr->rc_cfg.valid = task->reg[RKVENC_CLASS_RC].valid;
    hdr->param_cfg.valid = task->reg[RKVENC_CLASS_PAR].valid;
    hdr->sqi_cfg.valid = task->reg[RKVENC_CLASS_SQI].valid;
    hdr->scal_cfg.valid = task->reg[RKVENC_CLASS_SCL_JPGTBL].valid;
    hdr->osd_cfg.valid = task->reg[RKVENC_CLASS_OSD].valid;
    hdr->status_cfg.valid = task->reg[RKVENC_CLASS_ST].valid;
    hdr->next_node.valid = 1;
    // mpp_err("hdr->base_cfg.valid=%d\n", hdr->base_cfg.valid);
    // mpp_err("hdr->pic_cfg.valid=%d\n", hdr->pic_cfg.valid);
    // mpp_err("hdr->rc_cfg.valid=%d\n", hdr->rc_cfg.valid);
    // mpp_err("hdr->param_cfg.valid=%d\n", hdr->param_cfg.valid);
    // mpp_err("hdr->sqi_cfg.valid=%d\n", hdr->sqi_cfg.valid);
    // mpp_err("hdr->scal_cfg.valid=%d\n", hdr->scal_cfg.valid);
    // mpp_err("hdr->osd_cfg.valid=%d\n", hdr->osd_cfg.valid);
    // mpp_err("hdr->status_cfg.valid=%d\n", hdr->status_cfg.valid);
    // mpp_err("hdr->next_node.valid=%d\n", hdr->next_node.valid);

    /* set register data */
    for (i = 0; i < hw->reg_class; i++) {
        msg = &hw->reg_msg[i];
        //mpp_err("class=%d, link_len=%d, reg_valid=%d, reg_size=%d\n",
        //  i, msg->link_len, task->reg[i].valid, task->reg[i].size);
        if (!msg->link_len || !task->reg[i].valid)
            continue;
        off = link->class_off[i];
        s = msg->link_s ? msg->link_s : msg->base_s;
        e = msg->link_e ? msg->link_e : msg->base_e;
        len = e - s + sizeof(u32);

        di = off / sizeof(u32);
        si = (s - msg->base_s) / sizeof(u32);
        //mpp_err("off=%d, s=%08x, e=%08x, len=%d, di=%d, si=%d, size=%d\n",
        //  off, s, e, len, di, si, task->reg[i].size);
        memcpy(&tb_reg[di], &task->reg[i].data[si], len);
    }
    /* memset status regs for read */
    msg = &hw->reg_msg[RKVENC_CLASS_ST];
    di = link->class_off[RKVENC_CLASS_ST] / sizeof(u32);
    memset(&tb_reg[di], 0, msg->link_len);
    dma_sync_single_for_device(link->enc->mpp.dev, table->iova, table->size, DMA_FROM_DEVICE);
    dma_sync_single_for_cpu(link->enc->mpp.dev, table->iova, table->size, DMA_FROM_DEVICE);

    return 0;
}

static void *rkvenc_prepare(struct mpp_dev *mpp, struct mpp_task *mpp_task)
{
    struct mpp_taskqueue *queue = mpp->queue;
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);
    struct rkvenc_task *task = to_rkvenc_task(mpp_task);
    struct mpp_task *out_task = NULL;

    mpp_debug_enter();

    if (atomic_read(&mpp->reset_request) > 0)
        goto done;
    if (atomic_read(&mpp_task->session->release_request) > 0)
        goto done;

    switch (enc->link_mode) {
    case RKVENC_MODE_ONEFRAME: {
        unsigned long flags;
        spin_lock_irqsave(&queue->running_lock, flags);
        out_task = list_empty(&queue->running_list) ? mpp_task : NULL;
        spin_unlock_irqrestore(&queue->running_lock, flags);
    } break;
    case RKVENC_MODE_LINK_ONEFRAME: {
        struct mpp_dma_buffer *table;
        struct rkvenc_link_dev *link = enc->link;
        unsigned long flags;

        spin_lock_irqsave(&queue->running_lock, flags);
        out_task = list_empty(&queue->running_list) ? mpp_task : NULL;
        spin_unlock_irqrestore(&queue->running_lock, flags);
        if (!out_task)
            goto done;

        /* fill task entry */
        mutex_lock(&link->list_mutex);
        table = list_first_entry_or_null(&link->unused_list, struct mpp_dma_buffer, link);
        if (table) {
            rkvenc_link_fill_table(link, task, table);
            // mutex_lock(&link->list_mutex);
            list_move_tail(&table->link, &link->used_list);
            // mutex_unlock(&link->list_mutex);
            task->table = table;
            out_task = mpp_task;
        }
        mutex_unlock(&link->list_mutex);
    } break;
    case RKVENC_MODE_LINK_ADD: {
        struct rkvenc_link_dev *link = enc->link;
        struct mpp_dma_buffer *table;

        if (test_bit(TASK_STATE_LINK_FILLED, &mpp_task->state)) {
            out_task = mpp_task;
            mpp_err("xxxx task filled, mpp_state=%lx\n", mpp_task->state);
            goto done;
        }
        /* fill task entry */
        mutex_lock(&link->list_mutex);
        table = list_first_entry_or_null(&link->unused_list, struct mpp_dma_buffer, link);
        //mpp_err("table=%px\n", table);
        if (table) {
            rkvenc_link_fill_table(link, task, table);
            // mutex_lock(&link->list_mutex);
            list_move_tail(&table->link, &link->used_list);
            // mutex_unlock(&link->list_mutex);
            task->table = table;
            out_task = mpp_task;
            set_bit(TASK_STATE_LINK_FILLED, &mpp_task->state);
        }
        mutex_unlock(&link->list_mutex);
    } break;
    default:
        break;
    }
done:
    mpp_debug_leave();

    return out_task;
}

static int rkvenc_run_start_link(struct mpp_dev *mpp, struct rkvenc_task *task)
{
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);
    struct rkvenc_hw_info *hw = enc->hw_info;
    u32 base_s, base_e, s, e;
    u32 j, *regs;

    /* inital hardware */
    base_s = hw->reg_msg[RKVENC_CLASS_CTL].base_s;
    base_e = hw->reg_msg[RKVENC_CLASS_CTL].base_e;
    s = base_s / sizeof(u32);
    e = base_e / sizeof(u32);
    regs = (u32 *)task->reg[RKVENC_CLASS_CTL].data;
    //mpp_err("s=%d, e=%d\n", s, e);
    for (j = s; j <= e; j++) {
        if (j == hw->hw.reg_en)
            continue;

        mpp_write(mpp, j * sizeof(u32), regs[j]);
    }

    /* set interrupt enable */
    mpp_write(mpp, hw->int_en_base, 0xffff);
    /* set interrupt enable */
    // mpp_write_relaxed(mpp, hw->int_mask_base, 0x3FF);
    /* set link start addr */
    mpp_write(mpp, hw->link_addr_base, task->table->iova);

    return 0;
}

static int rkvenc_run(struct mpp_dev *mpp, struct mpp_task *mpp_task)
{
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);
    struct rkvenc_task *task = to_rkvenc_task(mpp_task);
    struct rkvenc_hw_info *hw = enc->hw_info;
    struct rkvenc2_session_priv *priv = mpp_task->session->priv;

    mpp_debug_enter();

    enc->line_cnt = 0;
    switch (enc->link_mode) {
    case RKVENC_MODE_ONEFRAME: {
        struct mpp_request msg;
        struct mpp_request *req;
        u32 enc_start_val = 0;
        u32 dvbm_en = 0;
        u32 st_ppl = 0;
        u32 i, j;

        if (task->task_split) {
            /* fix BUG: enc reset when slice fifo */
            mpp_write(mpp, hw->enc_clr_base, 0x2);
            udelay(5);
            mpp_write(mpp, hw->enc_clr_base, 0x0);
        }

        for (i = 0; i < task->w_req_cnt; i++) {
            int ret;
            u32 s, e, off;
            u32 *regs;

            req = &task->w_reqs[i];
            ret = rkvenc_get_class_msg(task, req->offset, &msg);
            if (ret)
                return -EINVAL;

            s = (req->offset - msg.offset) / sizeof(u32);
            e = s + req->size / sizeof(u32);
            regs = (u32 *)msg.data;
            for (j = s; j < e; j++) {
                off = msg.offset + j * sizeof(u32);
                if (off == hw->enc_start_base) {
                    enc_start_val = regs[j];
                    continue;
                }
                if (off == RKVENC_VIDEO_BSBS)
                    enc->video_wr_addr = regs[j];
                if (off == RKVENC_JPEG_BSBS)
                    enc->jpeg_wr_addr = regs[j];
                if (off == hw->dvbm_cfg) {
                    dvbm_en = regs[j];
                    continue;
                }
                if (mpp_task->disable_jpeg && off == 0x47c)
                    regs[j] &= (~BIT(31));
                mpp_write_relaxed(mpp, off, regs[j]);
            }
        }

        /* init current task */
        mpp->cur_task = mpp_task;
        {
            u32 enc_rsl = mpp_read(mpp, 0x310);
            u32 width = ((enc_rsl & 0x7ff) + 1) << 3;
            u32 height = ((enc_rsl >> 16) + 1) << 3;

            mpp_debug(DEBUG_RUN, "%s session %d task %d fmt %d jpeg_en %08x dvbm_en %d w/h [%d %d]\n",
                      __func__, mpp_task->session->index, mpp_task->task_index, task->fmt,
                      mpp_read(mpp, 0x47c) >> 31, dvbm_en, width, height);
        }

        /* check enc status before start */
        st_ppl = mpp_read(mpp, 0x5004);

        if (st_ppl & BIT(10))
            mpp_err("enc started status %08x\n", st_ppl);

        //if (IS_ENABLED(ROCKCHIP_DVBM_ENABLE)) {
        if (1) {
            if (dvbm_en) {
                enc->dvbm_overflow = 0;
                update_online_info(mpp, mpp_task->pipe_id);
                priv->dvbm_link = 1;
                wmb();
                mpp_write_relaxed(mpp, hw->dvbm_cfg, dvbm_en);
                mpp->always_on = 1;
            }
        }

        priv->dvbm_en = dvbm_en;
        priv->info.hw_running = 1;
        /* Flush the register before the start the device */
        wmb();
        preempt_disable();
        INIT_DELAYED_WORK(&mpp_task->timeout_work, rkvenc_task_timeout);
        schedule_delayed_work(&mpp_task->timeout_work, msecs_to_jiffies(200));
        set_bit(TASK_STATE_RUNNING, &mpp_task->state);
        if (!dvbm_en)
            mpp_write(mpp, hw->enc_start_base, enc_start_val);
        atomic_set(&enc->on_work, 1);
        preempt_enable();
    } break;
    case RKVENC_MODE_LINK_ONEFRAME: {
        atomic_set(&enc->link_task_cnt, 0);
        rkvenc_run_start_link(mpp, task);
        task->task_no = atomic_inc_return(&enc->link_task_cnt);
        /* init current task */
        mpp->cur_task = mpp_task;
        wmb();
        /* link mode and one frame */
        if (enc->link_run)
            mpp_write(mpp, hw->enc_start_base, 0x201);
    } break;
    case RKVENC_MODE_LINK_ADD: {
        u32 link_status = mpp_read(mpp, hw->link_status_base);
        //struct rkvenc_link_status *status = (struct rkvenc_link_status *)&link_status;

        //mpp_err("num_cfg_done=%d, num_cfg=%d, num_int=%d, num_enc_done=%d\n",
        //  status->cfg_done_num, status->cfg_num, status->int_num, status->enc_num);
        if (!link_status) {
            atomic_set(&enc->link_task_cnt, 0);
            rkvenc_run_start_link(mpp, task);
            task->task_no = atomic_inc_return(&enc->link_task_cnt);
            wmb();
            /* link start mode */
            if (enc->link_run)
                mpp_write(mpp, hw->enc_start_base, 0x201);
        } else {
            task->task_no = atomic_inc_return(&enc->link_task_cnt);
            wmb();
            /* link add mode */
            if (enc->link_run)
                mpp_write(mpp, hw->enc_start_base, 0x301);
        }
    } break;
    default : {
    } break;
    }

    mpp_debug_leave();

    return 0;
}

static void rkvenc_check_bs_overflow(struct mpp_dev *mpp)
{
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);
    struct mpp_task *task = mpp->cur_task;
    struct rkvenc2_session_priv *priv = task->session->priv;
    u32 r_adr, top_adr, bot_adr;

    if (!(mpp->irq_status & RKVENC_ENC_DONE_STATUS)) {
        if (mpp->irq_status & RKVENC_JPEG_OVERFLOW) {
            /* the w/r address need to be read in reversed*/
            r_adr = mpp_read(mpp, RKVENC_JPEG_BSBS);
            top_adr = mpp_read(mpp, RKVENC_JPEG_BSBT);
            bot_adr = mpp_read(mpp, RKVENC_JPEG_BSBB);

            priv->info.bsbuf_info[0] = top_adr;
            priv->info.bsbuf_info[1] = bot_adr;
            priv->info.bsbuf_info[2] = enc->jpeg_wr_addr;
            priv->info.bsbuf_info[3] = r_adr;
            mpp_dbg_warning("task %d jpeg bs overflow, buf[t:%#x b:%#x w:%#x r:%#x]\n",
                            task->task_index, top_adr, bot_adr, enc->jpeg_wr_addr, r_adr);
            if (enc->jpeg_wr_addr == r_adr)
                enc->jpeg_wr_addr += 1;
            if (enc->jpeg_wr_addr >= top_adr)
                enc->jpeg_wr_addr = mpp_read(mpp, RKVENC_JPEG_BSBB);
            mpp_write(mpp, RKVENC_JPEG_BSBS, enc->jpeg_wr_addr);
            mpp_write(mpp, RKVENC_JPEG_BSBR, r_adr + 0xc);
            mpp->overflow_status = mpp->irq_status;
        }
        if (mpp->irq_status & RKVENC_VIDEO_OVERFLOW) {
            r_adr = mpp_read(mpp, RKVENC_VIDEO_BSBR);
            top_adr = mpp_read(mpp, RKVENC_VIDEO_BSBT);
            bot_adr = mpp_read(mpp, RKVENC_VIDEO_BSBB);
            priv->info.bsbuf_info[0] = top_adr;
            priv->info.bsbuf_info[1] = bot_adr;
            priv->info.bsbuf_info[2] = enc->video_wr_addr;
            priv->info.bsbuf_info[3] = r_adr;

            mpp_dbg_warning("task %d video bs overflow, buf[t:%#x b:%#x w:%#x r:%#x]\n",
                            task->task_index, top_adr, bot_adr, enc->video_wr_addr, r_adr);
            if (enc->video_wr_addr == r_adr)
                enc->video_wr_addr += 1;
            if (enc->video_wr_addr >= top_adr)
                enc->video_wr_addr = mpp_read(mpp, RKVENC_VIDEO_BSBB);
            mpp_write(mpp, RKVENC_VIDEO_BSBS, enc->video_wr_addr);
            mpp_write(mpp, RKVENC_VIDEO_BSBR, r_adr + 0xc);
            mpp->overflow_status = mpp->irq_status;
        }
    }
}

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

static void rkvenc_read_slice_len(struct mpp_task *mpp_task, struct rkvenc_task *task,
                                  struct mpp_dev *mpp)
{
    struct mpp_session *session = mpp_task->session;
    u32 frame_size;
    u32 last;

    last = mpp->irq_status & RKVENC_ENC_DONE_STATUS;

    if (last) {
        frame_size = mpp_read_relaxed(mpp, 0x4000);
        if (frame_size >= task->slice_total_len)
            task->sli_info.length = frame_size - task->slice_total_len;
        else {
            mpp_err("read slice len fail, frame %d slice_total_len[%d]",
                    frame_size, task->slice_total_len);
            task->sli_info.length = 0;
        }
        task->last_slice_found = 1;
        task->sli_info.last = 1;
    } else {
        task->sli_info.val = mpp_read_relaxed(mpp, RKVENC_REG_SLICE_LEN_BASE);
        task->slice_total_len += task->sli_info.val;
    }

    if (session->callback && mpp_task->clbk_en)
        session->callback(session->chn_id, MPP_VCODEC_EVENT_SLICE, &task->sli_info.val);
}

irqreturn_t mpp_rkvenc_irq(int irq, void *param)
{
    struct mpp_dev *mpp = param;
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);
    struct rkvenc_hw_info *hw = enc->hw_info;
    struct mpp_task *mpp_task = mpp_taskqueue_get_running_task(mpp->queue);
    struct rkvenc_task *task;
    struct mpp_session *session;
    struct rkvenc2_session_priv *priv;
    u32 sli_num;

    mpp_debug_enter();

    /* get irq status */
    sli_num = mpp_read_relaxed(mpp, RKVENC_REG_SLICE_NUM_BASE);
    mpp->irq_status = mpp_read(mpp, hw->int_sta_base);
    if (!mpp->irq_status)
        return IRQ_NONE;
    /* clear irq regs */
    mpp_write(mpp, hw->int_mask_base, 0x100);
    mpp_write(mpp, hw->int_clr_base, mpp->irq_status);
    mpp_write(mpp, hw->int_sta_base, 0);

    /* check irq status */
    if (mpp->irq_status == RKVENC_DVBM_DISCONNECT)
        return IRQ_HANDLED;
    rkvenc_check_bs_overflow(mpp);
    if (!mpp_task) {
        dev_err(mpp->dev, "found null task in irq\n");
        return IRQ_HANDLED;
    }

    task = to_rkvenc_task(mpp_task);
    session = mpp_task->session;
    priv = mpp_task->session->priv;

    /* slice fifo */
    if ((mpp->irq_status & enc->hw_info->err_mask) == 0) {
        if (task && task->task_split) {
            if (sli_num) {
                rkvenc_read_slice_len(mpp_task, task, mpp);
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
    }

    if (test_and_set_bit(TASK_STATE_HANDLE, &mpp_task->state)) {
        mpp_err("task %d has been handled state %#lx\n", mpp_task->task_index, mpp_task->state);
        return IRQ_HANDLED;
    }
    cancel_delayed_work(&mpp_task->timeout_work);
    set_bit(TASK_STATE_IRQ, &mpp_task->state);
    /* get wrap info */
    enc->line_cnt = mpp_read(mpp, 0x18) & 0x3fff;
    rkvenc_clear_dvbm_info(mpp);
    /* check irq status again */
    if (priv->dvbm_en) {
        /*
        * Workaround:
        * The line cnt is updated in the mcu.
        * When line cnt is set to max value 0x3fff,
        * the cur frame has overflow.
        */
        if (enc->line_cnt == 0x3fff) {
            enc->dvbm_overflow = 1;
            mpp_dbg_warning("current frame has overflow\n");
        }
        /*
        * Workaround:
        * The line cnt is updated in the mcu.
        * When line cnt is set to max value 0x3ffe,
        * the cur frame source id is mismatch.
        */
        if (enc->line_cnt == 0x3ffe) {
            mpp->irq_status |= BIT(16);
            priv->info.wrap_source_mis_cnt++;
        }
        if (enc->dvbm_overflow) {
            mpp->irq_status |= BIT(6);
            enc->dvbm_overflow = 0;
            priv->info.wrap_overflow_cnt++;
        }
    }
    if (mpp->overflow_status)
        priv->info.bsbuf_overflow_cnt++;
    task->irq_status = (mpp->irq_status | mpp->overflow_status);
    mpp_debug(DEBUG_IRQ_STATUS, "task %d fmt %d dvbm_en %d irq_status 0x%08x\n",
              mpp_task->task_index, task->fmt, priv->dvbm_en, task->irq_status);

    mpp_time_diff(mpp_task);
    set_bit(TASK_STATE_DONE, &mpp_task->state);
    mpp_taskqueue_pop_running(mpp->queue, mpp_task);
    /* get enc task info */
    if (mpp->dev_ops->finish)
        mpp->dev_ops->finish(mpp, mpp_task);
    if (mpp->irq_status & enc->hw_info->err_mask) {
        mpp_dbg_warning("task %d fmt %d dvbm_en %d irq_status 0x%08x\n",
                        mpp_task->task_index, task->fmt, priv->dvbm_en, task->irq_status);
        priv->info.enc_err_status = task->irq_status;
        priv->info.enc_err_cnt++;
        if (mpp->hw_ops->reset)
            mpp->hw_ops->reset(mpp);
    }
    priv->info.hw_running = 0;
    mpp->overflow_status = 0;
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

    mpp_debug_enter();

    switch (enc->link_mode) {
    case RKVENC_MODE_ONEFRAME:
    case RKVENC_MODE_LINK_ONEFRAME: {
        struct mpp_request *req;
        u32 *reg;
        u32 i, j;

        if (mpp->irq_status & 0x100 || mpp->dump_regs)
            rkvenc_dump_dbg(mpp);

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
    } break;
    default : {
    } break;
    }
    atomic_set(&enc->on_work, 0);

    mpp_debug_leave();

    return 0;
}

static u32 *rkvenc_link_get_class_reg(struct rkvenc_link_dev *link,
                                      struct rkvenc_task *task, u32 addr)
{
    struct rkvenc_hw_info *hw = task->hw_info;
    u8 *reg = NULL;
    int i;

    for (i = 0; i < hw->reg_class; i++) {
        u32 base_s = hw->reg_msg[i].base_s;
        u32 base_e = hw->reg_msg[i].base_e;

        if (addr >= base_s && addr < base_e) {
            u32 off = link->class_off[i] + addr - base_s;

            reg = (u8 *)task->table->vaddr + off;
            break;
        }
    }

    return (u32 *)reg;
}

static int rkvenc_result(struct mpp_dev *mpp, struct mpp_task *mpp_task)
{
    struct rkvenc_dev *enc = to_rkvenc_dev(mpp);
    struct rkvenc_task *task = to_rkvenc_task(mpp_task);

    mpp_debug_enter();

    switch (enc->link_mode) {
    case RKVENC_MODE_ONEFRAME:
    case RKVENC_MODE_LINK_ONEFRAME: {
        if (test_bit(TASK_STATE_TIMEOUT, &mpp_task->state) ||
            test_bit(TASK_STATE_ABORT, &mpp_task->state))
            return -1;
    } break;
    case RKVENC_MODE_LINK_ADD: {
        struct mpp_request *req;
        u32 *reg;
        u32 i;

        for (i = 0; i < task->r_req_cnt; i++) {
            req = &task->r_reqs[i];
            reg = rkvenc_link_get_class_reg(enc->link, task, req->offset);
            if (!reg)
                return -EINVAL;
            if (!mpp_task->session->k_space) {
                if (copy_to_user(req->data, reg, req->size)) {
                    mpp_err("copy_to_user reg fail\n");
                    return -EIO;
                }
            } else
                memcpy(req->data, (u8 *)reg, req->size);
        }
    } break;
    default : {
    } break;
    }

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
    task->slice_total_len = 0;

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
    case MPP_CMD_SEND_CODEC_INFO: {
    } break;
    case MPP_CMD_UNBIND_JPEG_TASK: {
        return rkvenc_unbind_jpeg_task(session);
    } break;;
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
                kfree(task);
            }
        }

#if 0
        if (IS_ENABLED(ROCKCHIP_DVBM_ENABLE)) {
            struct rkvenc2_session_priv *priv =
                (struct rkvenc2_session_priv *)session->priv;

            if (priv->dvbm_en)
                session->mpp->always_on = 0;
        }
#endif
    }
    if (session && session->priv) {
        kfree(session->priv);
        session->priv = NULL;
    }

    return 0;
}

static int rkvenc_init_session(struct mpp_session *session)
{
    struct rkvenc2_session_priv *priv;
    struct mpp_dev *mpp = NULL;
    struct rkvenc_task *task = NULL;
    int j = 0, ret = 0;
    u32 i = 0;

    if (!session) {
        mpp_err("session is null\n");
        return -EINVAL;
    }

    mpp = session->mpp;
    priv = kzalloc(sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    init_rwsem(&priv->rw_sem);
    session->priv = priv;

    for (i = 0; i < MAX_TASK_CNT; i++) {
        task = kzalloc(sizeof(*task), GFP_KERNEL);
        if (!task)
            goto fail;

        task->hw_info = to_rkvenc_info(mpp->var->hw_info);
        {
            struct rkvenc_hw_info *hw = task->hw_info;

            for (j = 0; j < hw->reg_class; j++) {
                ret = rkvenc_alloc_class_msg(task, j);
                if (ret)
                    goto fail;
            }
        }
        session->task[i] = task;
    }
    return 0;

fail:
    if (session->priv)
        kfree(session->priv);
    if (task) {
        rkvenc_invalid_class_msg(task);
        kfree(task);
    }
    for (i = 0 ; i < MAX_TASK_CNT; i++) {
        struct rkvenc_task *task = (struct rkvenc_task *)session->task[i];

        if (task) {
            rkvenc_invalid_class_msg(task);
            kfree(task);
        }
    }
    return -ENOMEM;
}

static void rkvenc_task_timeout(struct work_struct *work_s)
{
    struct mpp_task *task = container_of(to_delayed_work(work_s), struct mpp_task, timeout_work);
    struct mpp_session *session;
    struct mpp_dev *mpp;
    u32 clbk_en = task->clbk_en;

    if (!task->session) {
        mpp_err("task %p, task->session is null.\n", task);
        return;
    }
    session = task->session;

    if (!session->mpp) {
        mpp_err("session %p, session->mpp is null.\n", session);
        return;
    }

    mpp = session->mpp;

    disable_irq(mpp->irq);

    if (test_and_set_bit(TASK_STATE_HANDLE, &task->state)) {
        mpp_err("task has been handled\n");
        return;
    }

    mpp_err("task %d state %#lx processing time out!\n", task->task_index, task->state);

    rkvenc_dump_dbg(mpp);

    set_bit(TASK_STATE_TIMEOUT, &task->state);

    mpp_time_diff(task);
    set_bit(TASK_STATE_DONE, &task->state);
    mpp_taskqueue_pop_running(mpp->queue, task);
    if (mpp->hw_ops->reset)
        mpp->hw_ops->reset(mpp);
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
    rkvenc_run(mpp, mpp_task);

done:
    spin_unlock_irqrestore(&queue->dev_lock, flags);
    if (list_empty(&queue->running_list))
        mpp_power_off(mpp);
    mpp_session_clean_detach(queue);

    mpp_debug_leave();
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

    down_read(&priv->rw_sem);
    /* item name */
    seq_puts(seq, "\n--------hw session infos");
    seq_puts(seq, "------------------------------------------------------\n");
    seq_printf(seq, "%8s|", "ID");
    seq_printf(seq, "%8s|", "device");
    seq_printf(seq, "%15s|%10s|%12s|%10s|%15s|%13s|%13s",
               "hw_running", "online", "wrap_ovfl", "bs_ovfl",
               "wrap_src_mis", "enc_err_cnt", "enc_err_st");
    seq_puts(seq, "\n");
    /* item data*/
    seq_printf(seq, "%8d|", session->chn_id);
    seq_printf(seq, "%8s|", mpp_device_name[session->device_type]);
    seq_printf(seq, "%15d|%10d|%12d|%10d|%15d|%13d|%13x",
               priv->info.hw_running, priv->dvbm_en ? 1 : 0, priv->info.wrap_overflow_cnt,
               priv->info.bsbuf_overflow_cnt, priv->info.wrap_source_mis_cnt,
               priv->info.enc_err_cnt, priv->info.enc_err_status);
    seq_puts(seq, "\n");

    if (priv->info.bsbuf_overflow_cnt) {
        seq_puts(seq, "\n--------bitstream buffer addr");
        seq_puts(seq, "------------------------------------------------------\n");
        seq_printf(seq, "%10s|%10s|%10s|%10s|\n",
                   "top", "bot", "wr", "rd");
        seq_printf(seq, "%10x|%10x|%10x|%10x|\n",
                   priv->info.bsbuf_info[0], priv->info.bsbuf_info[1],
                   priv->info.bsbuf_info[2], priv->info.bsbuf_info[3]);
    }
    up_read(&priv->rw_sem);

    return 0;
}

static int rkvenc_show_session_info(struct seq_file *seq, void *offset)
{
    struct mpp_dev *mpp = seq->private;
    struct mpp_session *session, *n;

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
    /* for show session info */
    proc_create_single_data("sessions-info", 0444, enc->procfs, rkvenc_show_session_info, mpp);
    mpp_procfs_create_u32("link_mode", 0644, enc->procfs, &enc->link_mode);

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

    mpp_debug_enter();

    /* safe reset */
    mpp_write(mpp, hw->int_mask_base, 0x3FF);
    mpp_write(mpp, hw->enc_clr_base, 0x1);
    udelay(5);
    mpp_write(mpp, hw->enc_clr_base, 0x3);
    mpp_write(mpp, hw->int_clr_base, 0xffffffff);
    mpp_write(mpp, hw->int_sta_base, 0);
    mpp_write(mpp, hw->enc_clr_base, 0);

    rkvenc_clear_dvbm_info(mpp);

    mpp_debug_leave();

    return 0;
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

static struct mpp_hw_ops rkvenc_hw_ops = {
    .init = rkvenc_init,
    .clk_on = rkvenc_clk_on,
    .clk_off = rkvenc_clk_off,
    .set_freq = rkvenc_set_freq,
    .reset = rkvenc_reset,
};

static struct mpp_dev_ops rkvenc_dev_ops_v2 = {
    .alloc_task = rkvenc_alloc_task,
    .prepare = rkvenc_prepare,
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
    .dump_dev = rkvenc_dump_dbg,
};

static const struct mpp_dev_var rkvenc_rv1106_data = {
    .device_type = MPP_DEVICE_RKVENC,
    .hw_info = &rkvenc_rv1106_hw_info.hw,
    .hw_ops = &rkvenc_hw_ops,
    .dev_ops = &rkvenc_dev_ops_v2,
};

static const struct of_device_id mpp_rkvenc_dt_match[] = {
    {
        .compatible = "rockchip,rkv-encoder-rv1106",
        .data = &rkvenc_rv1106_data,
    },
    {},
};

static int rkvenc_link_remove(struct rkvenc_dev *enc, struct rkvenc_link_dev *link)
{
    mpp_debug_enter();

    if (link) {
        struct mpp_dma_buffer *table = NULL, *n;

        mutex_lock(&link->list_mutex);
        list_for_each_entry_safe(table, n, &link->used_list, link) {
            list_del_init(&table->link);
            mpp_dma_free(table);
        }
        list_for_each_entry_safe(table, n, &link->unused_list, link) {
            list_del_init(&table->link);
            mpp_dma_free(table);
        }
        mutex_unlock(&link->list_mutex);
        link->table_num = 0;
    }

    mpp_debug_leave();

    return 0;
}

static int rkvenc_link_alloc_table(struct rkvenc_dev *enc, struct rkvenc_link_dev *link)
{
    struct rkvenc_hw_info *hw = enc->hw_info;
    struct mpp_dev *mpp = &enc->mpp;
    struct rkvenc_link_header *hdr;
    struct  mpp_dma_buffer *table, *prev;
    u32 table_size;
    int ret;
    int i;

    mpp_debug_enter();

    /* calc table class offset info */
    table_size =  roundup(sizeof(struct rkvenc_link_header), 128);
    memset(link->class_off, 0, sizeof(link->class_off));
    for (i = 0; i < hw->reg_class; i++) {
        if (!hw->reg_msg[i].link_len)
            continue;

        //mpp_err("class_off[%d]=%08x\n", i, table_size);
        link->class_off[i] = table_size;
        table_size += roundup(hw->reg_msg[i].link_len, 128);
    }
    //mpp_err("table_size=%d, dev_name=%s\n", table_size, dev_name(mpp->dev));
    /* alloc and init table node */
    link->table_num = enc->task_capacity;
    for (i = 0; i < link->table_num; i++) {
        table = mpp_dma_alloc(mpp->dev, table_size);
        if (!table) {
            dev_err(mpp->dev, "dma alloc failed\n");
            ret = -ENOMEM;
            goto err_free_node;
        }
        hdr = (struct rkvenc_link_header *)table->vaddr;
        // hdr->base_cfg.lkt_addr = table->iova + link->class_off[RKVENC_CLASS_BASE];
        hdr->pic_cfg.lkt_addr = table->iova + link->class_off[RKVENC_CLASS_PIC];
        hdr->rc_cfg.lkt_addr = table->iova + link->class_off[RKVENC_CLASS_RC];
        hdr->param_cfg.lkt_addr = table->iova + link->class_off[RKVENC_CLASS_PAR];
        hdr->sqi_cfg.lkt_addr = table->iova + link->class_off[RKVENC_CLASS_SQI];
        hdr->scal_cfg.lkt_addr = table->iova + link->class_off[RKVENC_CLASS_SCL_JPGTBL];
        hdr->osd_cfg.lkt_addr = table->iova + link->class_off[RKVENC_CLASS_OSD];
        hdr->status_cfg.lkt_addr = table->iova + link->class_off[RKVENC_CLASS_ST];

        INIT_LIST_HEAD(&table->link);
        mutex_lock(&link->list_mutex);
        /* init link next table addr */
        if (!list_empty(&link->unused_list)) {
            prev = list_last_entry(&link->unused_list, struct mpp_dma_buffer, link);
            hdr = (struct rkvenc_link_header *)prev->vaddr;
            hdr->next_node.valid = 1;
            hdr->next_node.next_addr = table->iova;
        }
        list_add_tail(&table->link, &link->unused_list);
        mutex_unlock(&link->list_mutex);
    }
    /* init first node pointer the last, then it loop */
    mutex_lock(&link->list_mutex);
    table = list_first_entry(&link->unused_list, struct mpp_dma_buffer, link);
    prev = list_last_entry(&link->unused_list, struct mpp_dma_buffer, link);
    hdr = (struct rkvenc_link_header *)prev->vaddr;
    hdr->next_node.valid = 1;
    hdr->next_node.next_addr = table->iova;
    mutex_unlock(&link->list_mutex);

    mpp_debug_leave();

    return 0;

err_free_node:
    rkvenc_link_remove(enc, link);
    return ret;
}

static int rkvenc_link_init(struct platform_device *pdev, struct rkvenc_dev *enc)
{
    struct device *dev = &pdev->dev;
    struct device_node *np = mpp_dev_of_node(dev);;
    struct rkvenc_link_dev *link;
    int ret;

    mpp_debug_enter();

    /* read link table capacity */
    ret = of_property_read_u32(np, "rockchip,task-capacity", &enc->task_capacity);
    if (ret) {
        /* only oneframe mode */
        enc->link_mode = RKVENC_MODE_ONEFRAME;
        return ret;
    }
    if (enc->task_capacity > 1) {
        enc->link_mode = RKVENC_MODE_LINK_ADD;
        dev_info(dev, "%d task capacity link mode detected\n", enc->task_capacity);
    } else {
        enc->task_capacity = 1;
        enc->link_mode = RKVENC_MODE_LINK_ONEFRAME;
    }

    /* alloc link device data */
    link = devm_kzalloc(dev, sizeof(*link), GFP_KERNEL);
    if (!link)
        return -ENOMEM;

    atomic_set(&enc->link_task_cnt, 0);
    mutex_init(&link->list_mutex);
    INIT_LIST_HEAD(&link->used_list);
    INIT_LIST_HEAD(&link->unused_list);
    ret = rkvenc_link_alloc_table(enc, link);
    if (ret)
        return ret;

    enc->link = link;
    link->enc = enc;

    mpp_debug_leave();

    return 0;
}

static int rkvenc_probe_default(struct platform_device *pdev)
{
    KmppDevData *data;
    struct device *dev = &pdev->dev;
    const struct of_device_id *match = NULL;
    struct rkvenc_dev *enc = NULL;
    struct mpp_dev *mpp = NULL;
    int ret = 0;

    data = pdev_set_data(pdev, enc);
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

    kthread_init_work(&mpp->work, mpp_rkvenc_worker);
    ret = devm_request_threaded_irq(dev, mpp->irq, mpp_rkvenc_irq, NULL,
                                    IRQF_SHARED, dev_name(dev), mpp);
    if (ret) {
        dev_err(dev, "register interrupter runtime failed\n");
        goto failed_get_irq;
    }
    enc->hw_info = to_rkvenc_info(mpp->var->hw_info);
    rkvenc_procfs_init(mpp);
    mpp_dev_register_srv(mpp, mpp->srv);

    if (IS_ENABLED(ROCKCHIP_DVBM_ENABLE)) {
        struct device_node *np_dvbm = NULL;
        struct platform_device *pdev_dvbm = NULL;

        np_dvbm = of_parse_phandle(mpp_dev_of_node(dev), "dvbm", 0);
        if (!np_dvbm || !of_device_is_available(np_dvbm))
            mpp_err("failed to get device node\n");
        else {
            pdev_dvbm = of_find_device_by_node(np_dvbm);
            enc->port = rk_dvbm_get_port(pdev_dvbm, DVBM_VEPU_PORT);
            of_node_put(np_dvbm);
            if (enc->port) {
                struct dvbm_cb dvbm_cb;

                dvbm_cb.cb = rkvenc_callback;
                dvbm_cb.ctx = enc;

                rk_dvbm_set_cb(enc->port, &dvbm_cb);
            }
        }
    }

    enc->link_mode = RKVENC_MODE_ONEFRAME;
    /* init for link device */
    rkvenc_link_init(pdev, enc);
    enc->link_run = 1;

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

    dev_info(dev, "remove device\n");
    if (IS_ENABLED(ROCKCHIP_DVBM_ENABLE)) {
        if (enc->port) {
            rk_dvbm_put(enc->port);
            enc->port = NULL;
        }
    }
    mpp_dev_remove(&enc->mpp);
    rkvenc_procfs_remove(&enc->mpp);
    rkvenc_link_remove(enc, enc->link);

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

static int __maybe_unused rkvenc_runtime_suspend(struct device *dev)
{
    struct rkvenc_dev *enc = (struct rkvenc_dev *)dev_to_legacy(dev);
    struct mpp_dev *mpp = &enc->mpp;

    mpp_debug(DEBUG_POWER, "%s suspend device ++\n", dev_name(dev));
    if (!atomic_xchg(&mpp->suspend_en, 1))
        down(&mpp->work_sem);

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
    mpp_taskqueue_trigger_work(mpp);
    mpp_debug(DEBUG_POWER, "%s resume device --\n", dev_name(dev));

    return 0;
}

static const struct dev_pm_ops rkvenc_pm_ops = {
    MPP_SET_SYSTEM_SLEEP_PM_OPS(rkvenc_runtime_suspend, rkvenc_runtime_resume)
};

struct platform_driver rockchip_rkvenc540c_driver = {
    .probe = rkvenc_probe,
    .remove = rkvenc_remove,
    .shutdown = rkvenc_shutdown,
    .driver = {
        .name = RKVENC_DRIVER_NAME,
        .of_match_table = of_match_ptr(mpp_rkvenc_dt_match),
        .pm = &rkvenc_pm_ops,
    },
};

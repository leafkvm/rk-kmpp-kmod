/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __ROCKCHIP_MPP_DVBM_H__
#define __ROCKCHIP_MPP_DVBM_H__

#include <linux/dma-buf.h>
#include <linux/types.h>
#include <soc/rockchip/rockchip_dvbm.h>

#include "mpp_common.h"
#include "mpp_debug.h"

#define RKVENC_DVBM_REG_NUM (14)

union st_enc_u {
    u32 val;
    struct {
        u32 st_enc             : 2;
        u32 st_sclr            : 1;
        u32 vepu_fbd_err       : 1;
        u32 vepu_vsp_err       : 1;
        u32 reserved           : 3;
        u32 isp_src_oflw       : 1;
        u32 vepu_src_oflw      : 1;
        u32 vepu_sid_nmch      : 1;
        u32 vepu_fcnt_nmch     : 1;
        u32 reserved1          : 4;
        u32 dvbm_finf_wful     : 1;
        u32 dvbm_linf_wful     : 1;
        u32 dvbm_fsid_nmch     : 1;
        u32 dvbm_fcnt_early    : 1;
        u32 dvbm_fcnt_late     : 1;
        u32 dvbm_isp_oflw      : 1;
        u32 dvbm_vepu_oflw     : 1;
        u32 isp_time_out       : 1;
        u32 anti_info_oflw     : 1;
        u32 anti_vsrc_oflw     : 1;
        u32 reserved2          : 1;
        u32 dvbm_vsrc_sid      : 1;
        u32 dvbm_vsrc_fcnt     : 4;
    };
};

enum dvbm_source {
    DVBM_SOURCE_ISP,
    DVBM_SOURCE_VPSS,
    DVBM_SOURCE_MAX,
};

struct wrap_buf {
    struct dma_buf *buf;
    struct dma_buf_attachment *attach;
    struct sg_table *sgt;
    u32 iova;
    u32 y_top_iova;
    u32 y_bot_iova;
    u32 c_top_iova;
    u32 c_bot_iova;
};

struct wrap_frame {
    u32 source_id;
    u32 frame_id;
};

struct mpp_dvbm {
    struct mpp_dev *mpp;
    struct dvbm_port *dvbm_port;
    u32 source;
    unsigned long dvbm_setup;
    u32 dvbm_reg_save[RKVENC_DVBM_REG_NUM];
    u32 dvbm_cfg;
    bool skip_dvbm_discnct;
    spinlock_t dvbm_lock;
    atomic_t src_fcnt;
    u32 line_stride;
    u32 wrap_line;
    struct wrap_buf wrap_buf;
    struct wrap_frame src_frm;
    struct wrap_frame enc_frm;
    ktime_t src_start_time;
    ktime_t src_end_time;
    s64 src_time;
    s64 src_vblank;
    /* debug for source */
    void __iomem *isp_base;
    void __iomem *vpss_base;

};

int mpp_rkvenc_dvbm_init(struct mpp_dvbm *dvbm, struct mpp_dev *mpp);
int mpp_rkvenc_dvbm_deinit(struct mpp_dvbm *dvbm);
void mpp_rkvenc_dvbm_update(struct mpp_dvbm *dvbm, struct mpp_task *mpp_task, u32 online);
int mpp_rkvenc_dvbm_connect(struct mpp_dvbm *dvbm, u32 dvbm_cfg);
int mpp_rkvenc_dvbm_disconnect(struct mpp_dvbm *dvbm, u32 dvbm_cfg);
int mpp_rkvenc_dvbm_handle(struct mpp_dvbm *dvbm, struct mpp_task *mpp_task);
void mpp_rkvenc_dvbm_clear(struct mpp_dvbm *dvbm);
void mpp_rkvenc_dvbm_hack(struct mpp_dvbm *dvbm);
void mpp_rkvenc_dvbm_reg_sav_restore(struct mpp_dvbm *dvbm, bool is_save);

#endif

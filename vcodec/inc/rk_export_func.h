/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 */

#ifndef __RK_EXPORT_FUNC_H__
#define __RK_EXPORT_FUNC_H__

#include "legacy_rockit.h"
#include "kmpp_session.h"

struct mpp_frame_infos {
    RK_U32  width;
    RK_U32  height;
    RK_U32  hor_stride;
    RK_U32  ver_stride;
    RK_U32  hor_stride_pixel;
    RK_U32  offset_x;
    RK_U32  offset_y;
    RK_U32  fmt;
    RK_U32  fd;
    RK_U64  pts;
    RK_S32  jpeg_chan_id;
    void    *osd_buf;
    RK_S32  mpi_buf_id;
    void    *jpg_combo_osd_buf;
    RK_U32  is_gray;
    RK_U32  is_full;
    RK_U32  phy_addr;
    RK_U64  dts;
    void    *pp_info;
    /* num of pskip need to gen after normal frame */
    RK_U32  pskip_num;
    union {
        RK_U32 val;
        struct {
            RK_U32  eos : 1;
            RK_U32  pskip : 1;
            RK_U32  idr_request : 1;
        };
    };
    /* pp combo */
    RK_S32 pp_chn;
    RK_S32 md_en;
    RK_S32 od_en;
    RK_S32 pp_frm_cnt;
    struct pp_md_cfg md_cfg;
    struct pp_od_cfg od_cfg;
    RK_U32 md_index;
    RK_U32 od_index;
    RK_U32 reserved[8];
};

#define VENC_MAX_PACK_INFO_NUM 8

struct venc_pack_info {
    RK_U32      flag;
    RK_U32      temporal_id;
    RK_U32      packet_offset;
    RK_U32      packet_len;
};

struct venc_packet {
    RK_S64               u64priv_data;  //map mpi_id
    RK_U64               u64packet_addr; //packet address in kernel space
    RK_U32               len;
    RK_U32               buf_size;

    RK_U64               u64pts;
    RK_U64               u64dts;
    RK_U32               flag;
    RK_U32               temporal_id;
    RK_U32               offset;
    RK_U32               data_num;
    struct venc_pack_info       packet[8];
};

extern int mpp_vcodec_clear_buf_resource(void);
extern int mpp_vcodec_run_task(RK_U32 chan_id, RK_S64 pts, RK_S64 dts);

#endif

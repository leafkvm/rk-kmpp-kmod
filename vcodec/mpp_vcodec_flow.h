/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 */

#ifndef __ROCKCHIP_MPP_VCODEC_FLOW_H__
#define __ROCKCHIP_MPP_VCODEC_FLOW_H__

#include "rk_export_func.h"
#include "rk_venc_cmd.h"
#include "mpp_vcodec_base.h"

enum mpp_vcodec_event_type {
    MPP_VCODEC_EVENT_FRAME,
    MPP_VCODEC_EVENT_SLICE,
    MPP_VCODEC_EVENT_BUTT,
};

int mpp_vcodec_enc_routine(void *param);
void *mpp_vcodec_dec_routine(void *param);
void mpp_vcodec_enc_int_handle(RK_S32 chan_id, RK_S32 event, void *param);
int mpp_vcodec_enc_run_task(RK_U32 chan_id, RK_S64 pts, RK_S64 dts);
int mpp_vcodec_enc_set_online_mode(RK_U32 chan_id, RK_U32 mode);
void mpp_vcodec_enc_add_packet_list(struct mpp_chan *chan_entry, KmppPacket packet);

#endif

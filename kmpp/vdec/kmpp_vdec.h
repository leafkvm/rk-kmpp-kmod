/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_VDEC_H__
#define __KMPP_VDEC_H__

#include "kmpp_osal.h"

#include "kmpi_vdec.h"
#include "kmpp_vdec_debug.h"
#include "kmpp_vdec_objs_impl.h"

/* parser / hal api common flag */
#define HAS_FRAME_INPUT     (1 << 0)
#define HAS_FRAME_OUTPUT    (1 << 1)
#define HAS_SLICE_INPUT     (1 << 2)
#define HAS_SLICE_OUTPUT    (1 << 3)

#define HAS_RANDOM_INPUT    (1 << 4)
#define HAS_RING_BUF        (1 << 5)
#define HAS_HW_PARSER       (1 << 6)
#define HAS_LINKED_LIST     (1 << 7)

typedef void* KmppVdec;

typedef void* VdecParser;
typedef struct ParserJob_t ParserJob;
typedef struct ParserOut_t ParserOut;

typedef void* VdecHal;
typedef struct VdecHalTask_t VdecHalTask;

typedef struct KmppVdecTaskImpl_t {
    kmpp_list_head      list;
    rk_s32              idx;
    rk_u32              stage;

    KmppVdec            vdec;

    rk_s32              parser_out_idx;
    rk_s32              hal_task_idx;

    ParserOut           *parser_out;
    VdecHalTask         *hal_task;
} KmppVdecTask;

#define KMPP_OBJ_NAME               vdec_impl
#define KMPP_OBJ_INTF_TYPE          KmppVdec
#include "kmpp_obj_func.h"

rk_s32 vdec_cfg_init(KmppVdecCfgImpl *cfg);

rk_s32 vdec_impl_init(KmppVdec vdec, KmppVdecInitCfg cfg);
rk_s32 vdec_impl_deinit(KmppVdec vdec);
rk_s32 vdec_impl_reset(KmppVdec vdec);
rk_s32 vdec_impl_start(KmppVdec vdec);
rk_s32 vdec_impl_stop(KmppVdec vdec);
rk_s32 vdec_impl_suspend(KmppVdec vdec);
rk_s32 vdec_impl_resume(KmppVdec vdec);

rk_s32 vdec_impl_get_cfg(KmppVdec vdec, KmppVdecCfg cfg);
rk_s32 vdec_impl_set_cfg(KmppVdec vdec, KmppVdecCfg cfg);

rk_s32 vdec_impl_decode(KmppVdec vdec, KmppPacket pkt, KmppFrame *frame);
rk_s32 vdec_impl_get_pkt(KmppVdec vdec, KmppPacket *pkt);
rk_s32 vdec_impl_put_pkt(KmppVdec vdec, KmppPacket pkt);
rk_s32 vdec_impl_get_frm(KmppVdec vdec, KmppFrame *frm);
rk_s32 vdec_impl_put_frm(KmppVdec vdec, KmppFrame frm);

#endif /* __KMPP_VDEC_H__ */

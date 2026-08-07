/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __VDEC_HAL_H__
#define __VDEC_HAL_H__

#include "kmpp_vdec.h"

typedef struct VdecHalCfg_s {
    /* input */
    KmppVdec        vdec;
    MppCodingType   coding;
    rk_s32          task_req;

    /* output */
    rk_s32          ctx_size;
    rk_s32          task_cnt;
} VdecHalCfg;

struct VdecHalTask_t {
    rk_s32          index;
    rk_u32          task_id;
    rk_s32          reg_idx;

    /* parser output is part of vdec input */
    ParserOut       *out;
};

/*
 * Vdec hal work flow
 * query (use coding type and request task cnt to get ctx size)
 * init
 *
 * get_task
 * setup parser out pointer
 * put_task
 * poll_task
 *
 * deinit
 */

rk_s32 vdec_hal_query(VdecHalCfg *cfg);
rk_s32 vdec_hal_init(VdecHal hal, VdecHalCfg *cfg);
rk_s32 vdec_hal_deinit(VdecHal hal);

rk_s32 vdec_hal_reset(VdecHal hal);
rk_s32 vdec_hal_start(VdecHal hal);
rk_s32 vdec_hal_stop(VdecHal hal);

/* try get valid task slot */
rk_s32 vdec_hal_get_task(VdecHal hal, VdecHalTask **task);
/* generate protocol registers, setup address registers and put task to the pending queue */
rk_s32 vdec_hal_put_task(VdecHal hal, VdecHalTask *task);
/* update slice info to frame task */
rk_s32 vdec_hal_update_task(VdecHal hal, VdecHalTask *task);
/* poll task for hardware ready */
rk_s32 vdec_hal_poll_task(VdecHal hal, VdecHalTask *task);

#endif /* __VDEC_HAL_H__ */

/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#ifndef __MPP_DEVICE_H__
#define __MPP_DEVICE_H__

#include "kmpp_log.h"
#include "mpp_dev_defs.h"
#include "kmpp_session.h"

typedef enum MppDevIoctlCmd_e {
    /* hardware operation setup config */
    MPP_DEV_REG_WR,
    MPP_DEV_REG_RD,

    MPP_DEV_CMD_SEND,
    MPP_DEV_CMD_POLL,
    MPP_DEV_CMD_RUN_TASK,

    MPP_DEV_IOCTL_CMD_BUTT,
} MppDevIoctlCmd;

/* for MPP_DEV_REG_WR */
typedef struct MppDevRegWrCfg_t {
    void *reg;
    RK_U32 size;
    RK_U32 offset;
} MppDevRegWrCfg;

/* for MPP_DEV_REG_RD */
typedef struct MppDevRegRdCfg_t {
    void *reg;
    RK_U32 size;
    RK_U32 offset;
} MppDevRegRdCfg;

/* slice fifo */
typedef union MppEncSliceInfo_u {
    RK_U32 val;
    struct {
        RK_U32 length  : 31;
        RK_U32 last    : 1;
    };
} MppEncSliceInfo;

typedef struct MppDevCfg_t {
    MppClientType type;
    rk_s32 chan_id;
    rk_s32 online;
    KmppSessionHook hook;
} MppDevCfg;

typedef struct MppDevApi_t {
    const char *name;
    RK_U32 ctx_size;
    MPP_RET(*init) (void *ctx, MppDevCfg *cfg);
    MPP_RET(*deinit) (void *ctx);

    /* config the cmd on preparing */
    MPP_RET(*reg_wr) (void *ctx, MppDevRegWrCfg * cfg);
    MPP_RET(*reg_rd) (void *ctx, MppDevRegRdCfg * cfg);

    /* send cmd to hardware */
    MPP_RET(*cmd_send) (void *ctx);

    /* poll cmd from hardware */
    MPP_RET(*cmd_poll) (void *ctx);

    struct device *(*chnl_get_dev)(void *ctx);
    RK_S32 (*run_task)(void *ctx, void *param);
    RK_S32 (*chnl_check_running)(void *ctx);
    RK_S32 (*control)(void *ctx, RK_U32 cmd, void *param);
} MppDevApi;

typedef void *MppDev;

#ifdef __cplusplus
extern "C" {
#endif

MPP_RET mpp_dev_init(MppDev *ctx, MppDevCfg *cfg);
MPP_RET mpp_dev_deinit(MppDev ctx);

MPP_RET mpp_dev_ioctl(MppDev ctx, RK_S32 cmd, void *param);

RK_U32 mpp_dev_get_iova_address(MppDev ctx, MppBuffer mpp_buf, RK_U32 reg_idx);

struct device * mpp_get_dev(MppDev ctx);

RK_S32 mpp_dev_chnl_check_running(MppDev ctx);
RK_S32 mpp_dev_chnl_unbind_jpeg_task(MppDev ctx);
RK_S32 mpp_dev_chnl_control(MppDev ctx, RK_S32 cmd, void *param);

#ifdef __cplusplus
}
#endif

#endif /* __MPP_DEVICE_H__ */

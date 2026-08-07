/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2019 Rockchip Electronics Co., Ltd.
 */

#ifndef __MPP_SERVICE_H__
#define __MPP_SERVICE_H__

#include "rk_type.h"
#include "legacy_rockit.h"

#include <asm/ioctl.h>

/* Use 'v' as magic number */
#define MPP_IOC_MAGIC               'v'
#define MPP_IOC_CFG_V1              _IOW(MPP_IOC_MAGIC, 1, unsigned int)
#define MAX_REQ_NUM                 16

#if __SIZEOF_POINTER__ == 4
#define REQ_DATA_PTR(ptr) ((RK_U32)ptr)
#elif __SIZEOF_POINTER__ == 8
#define REQ_DATA_PTR(ptr) ((RK_U64)ptr)
#endif

/* define flags for mpp_request */
#define MPP_FLAGS_MULTI_MSG         (0x00000001)
#define MPP_FLAGS_LAST_MSG          (0x00000002)
#define MPP_FLAGS_REG_FD_NO_TRANS   (0x00000004)
#define MPP_FLAGS_SCL_FD_NO_TRANS   (0x00000008)
#define MPP_FLAGS_REG_OFFSET_ALONE  (0x00000010)

typedef enum MppServiceCmdType_e {
    MPP_CMD_QUERY_BASE              = 0,
    MPP_CMD_PROBE_HW_SUPPORT        = MPP_CMD_QUERY_BASE + 0,
    MPP_CMD_QUERY_HW_ID             = MPP_CMD_QUERY_BASE + 1,
    MPP_CMD_QUERY_CMD_SUPPORT       = MPP_CMD_QUERY_BASE + 2,
    MPP_CMD_QUERY_BUTT,

    MPP_CMD_INIT_BASE               = 0x100,
    MPP_CMD_INIT_CLIENT_TYPE        = MPP_CMD_INIT_BASE + 0,
    MPP_CMD_INIT_DRIVER_DATA        = MPP_CMD_INIT_BASE + 1,
    MPP_CMD_INIT_TRANS_TABLE        = MPP_CMD_INIT_BASE + 2,
    MPP_CMD_INIT_BUTT,

    MPP_CMD_SEND_BASE               = 0x200,
    MPP_CMD_SET_REG_WRITE           = MPP_CMD_SEND_BASE + 0,
    MPP_CMD_SET_REG_READ            = MPP_CMD_SEND_BASE + 1,
    MPP_CMD_SET_REG_ADDR_OFFSET     = MPP_CMD_SEND_BASE + 2,
    MPP_CMD_SET_RCB_INFO            = MPP_CMD_SEND_BASE + 3,
    MPP_CMD_SET_SESSION_FD          = MPP_CMD_SEND_BASE + 4,
    MPP_CMD_SEND_BUTT,

    MPP_CMD_POLL_BASE               = 0x300,
    MPP_CMD_POLL_HW_FINISH          = MPP_CMD_POLL_BASE + 0,
    MPP_CMD_POLL_HW_IRQ             = MPP_CMD_POLL_BASE + 1,
    MPP_CMD_POLL_BUTT,

    MPP_CMD_CONTROL_BASE            = 0x400,
    MPP_CMD_RESET_SESSION           = MPP_CMD_CONTROL_BASE + 0,
    MPP_CMD_TRANS_FD_TO_IOVA        = MPP_CMD_CONTROL_BASE + 1,
    MPP_CMD_RELEASE_FD              = MPP_CMD_CONTROL_BASE + 2,
    MPP_CMD_SEND_CODEC_INFO         = MPP_CMD_CONTROL_BASE + 3,
    MPP_CMD_SET_ERR_REF_HACK            = MPP_CMD_CONTROL_BASE + 4,
    MPP_CMD_CONTROL_BUTT,

    MPP_CMD_KMPP_CONTROL_BASE       = 0x500,
    MPP_CMD_UNBIND_JPEG_TASK        = MPP_CMD_KMPP_CONTROL_BASE + 0,
    MPP_CMD_VEPU_CONNECT_DVBM       = MPP_CMD_KMPP_CONTROL_BASE + 1,
    MPP_CMD_VEPU_SET_ONLINE_MODE    = MPP_CMD_KMPP_CONTROL_BASE + 2,
    MPP_CMD_VEPU_CLEAR_ONLINE_TASK  = MPP_CMD_KMPP_CONTROL_BASE + 3,
    MPP_CMD_KMPP_CONTROL_BUTT,

    MPP_CMD_BUTT,
} MppServiceCmdType;

typedef struct mppReqV1_t {
    RK_U32 cmd;
    RK_U32 flag;
    RK_U32 size;
    RK_U32 offset;
    RK_U64 data_ptr;
} MppReqV1;

typedef struct MppTaskInfo_t {
    /* indentify the id of isp pipe */
    RK_U32 pipe_id;
    /* indentify the frame id */
    RK_U32 frame_id;

    RK_U32 width;
    RK_U32 height;
} MppTaskInfo;

#ifdef  __cplusplus
extern "C" {
#endif

#ifdef  __cplusplus
}
#endif

#endif /* __MPP_SERVICE_H__ */

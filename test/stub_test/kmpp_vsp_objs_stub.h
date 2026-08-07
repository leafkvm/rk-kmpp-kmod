/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_VSP_OBJS_STUB_H__
#define __KMPP_VSP_OBJS_STUB_H__

#include "kmpp_stub.h"
#include "kmpp_vsp_objs.h"

#define KMPP_OBJ_NAME           kmpp_vsp_init_cfg
#define KMPP_OBJ_INTF_TYPE      KmppVspInitCfg
#define KMPP_OBJ_ENTRY_TABLE    KMPP_VSP_INIT_CFG_ENTRY_TABLE
#include "kmpp_obj_func_stub.h"

#define KMPP_OBJ_NAME               kmpp_vsp_pp_rt_cfg
#define KMPP_OBJ_INTF_TYPE          KmppVspRtCfg
#define KMPP_OBJ_ENTRY_TABLE        KMPP_VSP_PP_CFG_ENTRY_TABLE
#include "kmpp_obj_func_stub.h"

#define KMPP_OBJ_NAME               kmpp_vsp_pp_rt_out
#define KMPP_OBJ_INTF_TYPE          KmppVspRtOut
#define KMPP_OBJ_ENTRY_TABLE        KMPP_VSP_PP_OUT_ENTRY_TABLE
#include "kmpp_obj_func_stub.h"

#endif /*__KMPP_VSP_OBJS_STUB_H__*/

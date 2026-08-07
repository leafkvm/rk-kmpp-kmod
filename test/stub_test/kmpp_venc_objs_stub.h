/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_VENC_OBJS_STUB_H__
#define __KMPP_VENC_OBJS_STUB_H__

#include "kmpp_stub.h"
#include "kmpp_venc_objs.h"

#define KMPP_OBJ_NAME           kmpp_venc_init_cfg
#define KMPP_OBJ_INTF_TYPE      KmppVencInitCfg
#define KMPP_OBJ_ENTRY_TABLE    KMPP_VENC_INIT_CFG_ENTRY_TABLE

#include "kmpp_obj_func_stub.h"

#define kmpp_venc_init_cfg_dump_f(cfg) kmpp_venc_init_cfg_dump(cfg, __FUNCTION__)

/* ------------------------ kmpp notify infos ------------------------- */
#define KMPP_OBJ_NAME           kmpp_venc_ntfy
#define KMPP_OBJ_INTF_TYPE      KmppVencNtfy
#define KMPP_OBJ_ENTRY_TABLE    KMPP_NOTIFY_CFG_ENTRY_TABLE

#include "kmpp_obj_func_stub.h"

/* ------------------------ kmpp static config ------------------------- */
#define KMPP_OBJ_NAME           kmpp_venc_st_cfg
#define KMPP_OBJ_INTF_TYPE      KmppVencStCfg

#include "kmpp_obj_func_stub.h"

/* ------------------------ kmpp reference config --------------------- */
#define KMPP_OBJ_NAME           kmpp_ref_st_cfg
#define KMPP_OBJ_INTF_TYPE      KmppRefStCfg
#define KMPP_OBJ_ENTRY_TABLE    KMPP_REF_ST_CFG_ENTRY_TABLE
#include "kmpp_obj_func_stub.h"

#define KMPP_OBJ_NAME           kmpp_ref_lt_cfg
#define KMPP_OBJ_INTF_TYPE      KmppRefLtCfg
#define KMPP_OBJ_ENTRY_TABLE    KMPP_REF_LT_CFG_ENTRY_TABLE
#include "kmpp_obj_func_stub.h"

#define KMPP_OBJ_NAME           kmpp_ref_cfg
#define KMPP_OBJ_INTF_TYPE      KmppRefCfg
#define KMPP_OBJ_ENTRY_TABLE    KMPP_REF_CFG_ENTRY_TABLE
#include "kmpp_obj_func_stub.h"

#endif /*__KMPP_VENC_OBJS_STUB_H__*/

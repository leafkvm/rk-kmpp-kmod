/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPI_H__
#define __KMPI_H__

#include "kmpi_defs.h"

/* string - value config for control flow */
typedef void* KmppSysCfg;

rk_s32 kmpp_install(void);
rk_s32 kmpp_uninstall(void);

/* global parameter query / setup including environment paramters */
rk_s32 kmpp_sys_query(KmppSysCfg cfg);
rk_s32 kmpp_sys_setup(KmppSysCfg cfg);

#include "kmpi_venc.h"
#include "kmpi_vdec.h"
#include "kmpi_vsp.h"

#endif /*__KMPI_H__*/

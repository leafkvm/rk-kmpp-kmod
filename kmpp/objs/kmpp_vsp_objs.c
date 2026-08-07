/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_vsp_objs"

#include "kmpp_log.h"

#include "kmpp_obj.h"
#include "kmpp_vsp_objs_impl.h"

static rk_s32 kmpp_vsp_init_cfg_impl_dump(void *entry)
{
    KmppVspInitCfgImpl *impl = (KmppVspInitCfgImpl*)entry;

    if (!impl) {
        kmpp_loge_f("invalid param frame NULL\n");
        return rk_nok;
    }

    kmpp_logi("name         %s\n", impl->name);
    kmpp_logi("max width    %d\n", impl->max_width);
    kmpp_logi("max height   %d\n", impl->max_height);

    return rk_ok;
}

#define KMPP_OBJ_NAME               kmpp_vsp_init_cfg
#define KMPP_OBJ_INTF_TYPE          KmppVspInitCfg
#define KMPP_OBJ_IMPL_TYPE          KmppVspInitCfgImpl
#define KMPP_OBJ_ENTRY_TABLE        KMPP_VSP_INIT_CFG_ENTRY_TABLE
#define KMPP_OBJ_FUNC_DUMP          kmpp_vsp_init_cfg_impl_dump
#include "kmpp_obj_helper.h"

static rk_s32 kmpp_vsp_pp_cfg_dump(void *entry)
{
    KmppVspPpCfg *impl = (KmppVspPpCfg*)entry;

    if (!impl) {
        kmpp_loge_f("invalid param frame NULL\n");
        return rk_nok;
    }

    kmpp_logi("width        %d\n", impl->width);
    kmpp_logi("height       %d\n", impl->height);

    return rk_ok;
}

#define KMPP_OBJ_NAME               kmpp_vsp_pp_rt_cfg
#define KMPP_OBJ_INTF_TYPE          KmppVspRtCfg
#define KMPP_OBJ_IMPL_TYPE          KmppVspPpCfg
#define KMPP_OBJ_ENTRY_TABLE        KMPP_VSP_PP_CFG_ENTRY_TABLE
#define KMPP_OBJ_FUNC_DUMP          kmpp_vsp_pp_cfg_dump
#define KMPP_OBJ_HIERARCHY_ENABLE
#include "kmpp_obj_helper.h"

static rk_s32 kmpp_vsp_pp_out_dump(void *entry)
{
    KmppVspPpOut *impl = (KmppVspPpOut*)entry;

    if (!impl) {
        kmpp_loge_f("invalid param frame NULL\n");
        return rk_nok;
    }

    kmpp_logi("od:flag      %d\n", impl->od.flag );
    kmpp_logi("od:pix_sum   %d\n", impl->od.pix_sum);

    return rk_ok;
}

#define KMPP_OBJ_NAME               kmpp_vsp_pp_rt_out
#define KMPP_OBJ_INTF_TYPE          KmppVspRtOut
#define KMPP_OBJ_IMPL_TYPE          KmppVspPpOut
#define KMPP_OBJ_ENTRY_TABLE        KMPP_VSP_PP_OUT_ENTRY_TABLE
#define KMPP_OBJ_FUNC_DUMP          kmpp_vsp_pp_out_dump
#include "kmpp_obj_helper.h"

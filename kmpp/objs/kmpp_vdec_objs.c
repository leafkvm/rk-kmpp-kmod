/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_vdec_objs"

#include "kmpp_log.h"
#include "kmpp_macro.h"
#include "kmpp_module.h"

#include "kmpp_mod_id.h"
#include "kmpp_obj.h"
#include "kmpp_vdec.h"

static rk_s32 kmpp_vdec_cfg_impl_init(void *entry, KmppObj obj, KmppFsDev *file, const rk_u8 *caller)
{
    return vdec_cfg_init((KmppVdecCfgImpl*)entry);
}

static rk_s32 kmpp_vdec_init_cfg_impl_dump(void *entry)
{
    KmppVdecBaseCfg *base = (KmppVdecBaseCfg*)entry;

    if (!base) {
        kmpp_loge_f("invalid param frame NULL\n");
        return rk_nok;
    }

    kmpp_logi("type             %d\n", base->type);
    kmpp_logi("coding           %d\n", base->coding);
    kmpp_logi("hw_type          %d\n", base->hw_type);
    kmpp_logi("batch_mode       %d\n", base->batch_mode);
    kmpp_logi("out_fmt          %d\n", base->out_fmt);
    kmpp_logi("fast_out         %d\n", base->fast_out);
    kmpp_logi("fast_parse       %d\n", base->fast_parse);
    kmpp_logi("split_parse      %d\n", base->split_parse);
    kmpp_logi("internal_pts     %d\n", base->internal_pts);
    kmpp_logi("sort_pts         %d\n", base->sort_pts);
    kmpp_logi("disable_error    %d\n", base->disable_error);
    kmpp_logi("enable_vproc     %d\n", base->enable_vproc);
    kmpp_logi("enable_fast_play %d\n", base->enable_fast_play);
    kmpp_logi("enable_hdr_meta  %d\n", base->enable_hdr_meta);
    kmpp_logi("enable_thumbnail %d\n", base->enable_thumbnail);
    kmpp_logi("enable_mvc       %d\n", base->enable_mvc);
    kmpp_logi("disable_dpb_chk  %d\n", base->disable_dpb_chk);
    kmpp_logi("disable_thread   %d\n", base->disable_thread);
    kmpp_logi("codec_mode       %d\n", base->codec_mode);
    kmpp_logi("dis_err_clr_mark %d\n", base->dis_err_clr_mark);

    return rk_ok;
}

#define KMPP_OBJ_NAME               kmpp_vdec_init_cfg
#define KMPP_OBJ_INTF_TYPE          KmppVdecInitCfg
#define KMPP_OBJ_IMPL_TYPE          KmppVdecCfgImpl
#define KMPP_OBJ_ENTRY_TABLE        KMPP_VDEC_INIT_CFG_ENTRY_TABLE
#define KMPP_OBJ_FUNC_INIT          kmpp_vdec_cfg_impl_init
#define KMPP_OBJ_FUNC_DUMP          kmpp_vdec_init_cfg_impl_dump
#define KMPP_OBJ_ACCESS_DISABLE
#define KMPP_OBJ_HIERARCHY_ENABLE
#include "kmpp_obj_helper.h"

#define KMPP_OBJ_NAME               kmpp_vdec_cfg
#define KMPP_OBJ_INTF_TYPE          KmppVdecCfg
#define KMPP_OBJ_IMPL_TYPE          KmppVdecCfgImpl
#define KMPP_OBJ_FUNC_INIT          kmpp_vdec_cfg_impl_init
#define KMPP_OBJ_ACCESS_DISABLE
#define KMPP_OBJ_HIERARCHY_ENABLE
#define KMPP_OBJ_SHARE_DISABLE
#include "kmpp_obj_helper.h"

static rk_s32 kmpp_vdec_objs_init(void)
{
    kmpp_vdec_init_cfg_objdef_get();
    kmpp_vdec_cfg_objdef_get();

    return rk_ok;
}

static rk_s32 kmpp_vdec_objs_deinit(void)
{
    kmpp_vdec_init_cfg_objdef_put();
    kmpp_vdec_cfg_objdef_put();

    return rk_ok;
}

KMPP_MODULE_ADD(kmpp, KMPP_MOD_VDEC_OBJS, vdec_objs, kmpp_vdec_objs_init, kmpp_vdec_objs_deinit)

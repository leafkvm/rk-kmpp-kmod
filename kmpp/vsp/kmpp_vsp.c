/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#include "kmpp_log.h"
#include "kmpp_module.h"
#include "kmpp_mod_id.h"
#include "kmpp_string.h"

#include "kmpp_vsp_impl.h"
#include "kmpp_vsp_objs.h"

#include "vepu500_pp.h"
#include "vepu511_pp.h"

static KmppVspApi *vsp_apis[] = {
#ifdef RKVEPU500_PP_ENABLE
    &vepu500_pp_api,
#endif
#ifdef RKVEPU511_PP_ENABLE
    &vepu511_pp_api,
#endif
    NULL,
};

static rk_s32 kmpp_vsp_objs_init(void)
{
    kmpp_vsp_init_cfg_objdef_get();
    kmpp_vsp_pp_rt_cfg_objdef_get();
    kmpp_vsp_pp_rt_out_objdef_get();

    return rk_ok;
}

static rk_s32 kmpp_vsp_objs_deinit(void)
{
    kmpp_vsp_init_cfg_objdef_put();
    kmpp_vsp_pp_rt_cfg_objdef_put();
    kmpp_vsp_pp_rt_out_objdef_put();

    return rk_ok;
}

KMPP_MODULE_ADD(kmpp, KMPP_MOD_VSP_OBJS, vsp_objs, kmpp_vsp_objs_init, kmpp_vsp_objs_deinit)

KmppVspApi *kmpp_vsp_get_api(const rk_u8 *name)
{
    rk_s32 i;

    for (i = 0; vsp_apis[i]; i++) {
        KmppVspApi *api = vsp_apis[i];

        if (!api)
            break;

        kmpp_logd_f("compare api vs name: %s - %s\n", api->name, name);

        if (!kmpp_strcmp(api->name, name))
            return api;
    }

    kmpp_loge_f("vsp api %s not found\n", name);

    return NULL;
}

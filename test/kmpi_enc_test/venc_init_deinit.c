/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#include "rk_mpi_cmd.h"
#include "mpp_buffer.h"

#include "kmpp_osal.h"
#include "kmpi_venc.h"

#include "kmpp_obj.h"
#include "kmpp_venc_objs.h"

#define MAX_TYPE_COUNT 3

rk_s32 test_init_deinit(const rk_u8 *name, rk_u32 flag)
{
    static MppCodingType test_types[MAX_TYPE_COUNT] = {
        MPP_VIDEO_CodingAVC,
        MPP_VIDEO_CodingHEVC,
        MPP_VIDEO_CodingMJPEG,
    };
    static const rk_u8 *test_type_str[MAX_TYPE_COUNT] = {
        "h264",
        "h265",
        "jpeg",
    };
    KmppCtx ctx[MAX_TYPE_COUNT];
    KmppVencInitCfg cfg;
    rk_s32 ret = rk_ok;
    rk_u32 i;

    kmpp_memset(ctx, 0, sizeof(ctx));

    ret |= kmpp_venc_init_cfg_get(&cfg);
    ret |= kmpp_venc_init_cfg_set_ntfy_mode(cfg, 0);
    if (ret) {
        kmpp_loge("%s kmpp_venc_init_cfg setup failed ret %d\n", name, ret);
        return ret;
    }

    for (i = 0; i < MAX_TYPE_COUNT; i++) {
        MppCodingType type = test_types[i];

        kmpp_venc_init_cfg_set_coding(cfg, type);

        ret |= kmpp_venc_init(&ctx[i], cfg);
        if (ret) {
            kmpp_loge("%s kmpp_venc_init %s failed ret %d\n",
                      name, test_type_str[i], ret);
            break;
        }
    }

    if (ret)
        goto done;

    for (i = 0; i < MAX_TYPE_COUNT; i++) {
        ret |= kmpp_venc_deinit(ctx[i], NULL);
        ctx[i] = NULL;
        if (ret) {
            kmpp_loge("%s kmpp_venc_deinit %s failed ret %d\n",
                      name, test_type_str[i], ret);
            break;
        }
    }

done:
    for (i = 0; i < MAX_TYPE_COUNT; i++) {
        if (ctx[i]) {
            kmpp_venc_deinit(ctx[i], NULL);
            ctx[i] = NULL;
        }
    }

    kmpp_venc_init_cfg_put(cfg);

    return ret;
}

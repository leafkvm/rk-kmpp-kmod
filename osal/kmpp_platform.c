/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_plat"

#include <linux/of.h>
#include <linux/of_device.h>

#include "kmpp_module.h"
#include "kmpp_osal_id.h"
#include "kmpp_platform.h"

static struct device_node *root = NULL;
static const rk_u8 *compatible = NULL;

static rk_s32 kmpp_plat_init(void)
{
    if (!root)
        root = of_find_node_opts_by_path("/", NULL);

    if (root) {
        const char *val = NULL;

        of_property_read_string(root, "compatible", &val);
        compatible = val;
    }

    return compatible ? rk_ok : rk_nok;
}

static rk_s32 kmpp_plat_deinit(void)
{
    if (root) {
        of_node_put(root);
        root = NULL;
    }

    return rk_ok;
}

KMPP_MODULE_ADD(kmpp_osal, KMPP_OSAL_PLAT, plat, kmpp_plat_init, kmpp_plat_deinit);

const rk_u8 *kmpp_plat_get_name(void)
{
    return compatible;
}
EXPORT_SYMBOL(kmpp_plat_get_name);

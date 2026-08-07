/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "kmpp_dev"

#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/interrupt.h>
#include <linux/of_platform.h>
#include <soc/rockchip/rockchip_iommu.h>

#include "kmpp_list.h"
#include "kmpp_atomic.h"
#include "kmpp_mem.h"
#include "kmpp_log.h"
#include "kmpp_mutex.h"
#include "kmpp_module.h"
#include "kmpp_osal_id.h"
#include "kmpp_dev_impl.h"
#include "kmpp_string.h"

void *kmpp_dev_get_device(KmppDev *dev)
{
    KmppDevImpl *impl = dev_to_impl(dev);

    return dev ? impl->device : NULL;
}
EXPORT_SYMBOL(kmpp_dev_get_device);

const rk_u8 *kmpp_device_name(void *device)
{
    struct device *dev = device;

    return (const rk_u8 *)dev_name(dev);
}
EXPORT_SYMBOL(kmpp_device_name);

/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "stub_test"

#include <linux/module.h>
#include <linux/kernel.h>

#include "kmpp_sym_stub.h"
#include "kmpp_obj_stub.h"

#include "kmpi_venc_stub.h"
#include "kmpp_venc_objs_stub.h"
#include "kmpi_vsp_stub.h"
#include "kmpp_vsp_objs_stub.h"

#include "mpp_buffer_stub.h"
#include "kmpp_buffer_stub.h"
#include "kmpp_frame_stub.h"
#include "kmpp_meta_stub.h"

#include "rk_export_func_stub.h"

int stub_test_init(void)
{
    printk("stub test start\n");

    return 0;
}

void stub_test_exit(void)
{
    printk("stub test end\n");
}

module_init(stub_test_init);
module_exit(stub_test_exit);

MODULE_AUTHOR("rockchip");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_VERSION("1.0");
/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#include <linux/poll.h>

#include "kmpp_log.h"

#include "kmpp_poll.h"
#include "kmpp_wait_impl.h"

void kmpp_poll_wait(KmppFsDev *dev, KmppWait *wait, void *poll_table)
{
    if (!dev || !wait || !poll_table) {
        kmpp_err_f("Invalid parameters: dev=%p, wait=%p, poll_table=%p\n",
                   dev, wait, poll_table);
        return;
    }

    poll_wait(dev->file_data, wait->wait, poll_table);
}
EXPORT_SYMBOL(kmpp_poll_wait);

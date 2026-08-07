/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_WAIT_H__
#define __KMPP_WAIT_H__

#include "rk_type.h"

typedef rk_s32 (*KmppWaitCondFunc)(const void *param);

typedef struct KmppWait_t KmppWait;

rk_s32 kmpp_wait_init(KmppWait **wait);
void   kmpp_wait_deinit(KmppWait **wait);

rk_s32 kmpp_wait_size(void);
rk_s32 kmpp_wait_assign(KmppWait **wait, void *buf, rk_s32 size);

rk_s32 kmpp_wait_interruptible(KmppWait *wait, KmppWaitCondFunc func, void *param);
rk_s32 kmpp_wait_uninterruptible(KmppWait *wait, KmppWaitCondFunc func, void *param);
rk_s32 kmpp_wait_timeout_interruptible(KmppWait *wait, KmppWaitCondFunc func, void *param, rk_u32 ms);
rk_s32 kmpp_wait_timeout_uninterruptible(KmppWait *wait, KmppWaitCondFunc func, void *param, rk_u32 ms);

void   kmpp_wait_wakeup(KmppWait *wait);

#endif /* __KMPP_WAIT_H__ */

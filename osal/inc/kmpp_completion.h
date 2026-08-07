/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_COMPLETION_H__
#define __KMPP_COMPLETION_H__

#include "rk_type.h"

typedef struct KmppCompletion_t KmppCompletion;

void    kmpp_completion_init(KmppCompletion **completion);
void    kmpp_completion_deinit(KmppCompletion **completion);

rk_s32  kmpp_completion_size(void);
rk_s32  kmpp_completion_assign(KmppCompletion **completion, void *buf, rk_s32 size);

void    kmpp_completion_reinit(KmppCompletion *completion);
void    kmpp_wait_for_completion(KmppCompletion *completion);
rk_s32  kmpp_wait_for_completion_interruptible(KmppCompletion *completion);
rk_s32  kmpp_wait_for_completion_timeout(KmppCompletion *completion, rk_s32 timeout);

rk_s32  kmpp_try_wait_for_completion(KmppCompletion *completion);
rk_s32  kmpp_complete_done(KmppCompletion *completion);
void    kmpp_complete(KmppCompletion *completion);
void    kmpp_complete_all(KmppCompletion *completion);

#endif /* __KMPP_COMPLETION_H__ */
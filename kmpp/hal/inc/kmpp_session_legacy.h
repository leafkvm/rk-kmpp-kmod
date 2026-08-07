/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_SESSION_LEGACY_H__
#define __KMPP_SESSION_LEGACY_H__

#include "mpp_common.h"
#include "kmpp_session.h"

typedef struct mpp_session KmppSessionOld;

rk_s32 kmpp_session_init_legacy(KmppSessionOld *session, KmppSession obj);
rk_s32 kmpp_session_deinit_legacy(KmppSessionOld *session);

rk_s32 kmpp_session_ioctl_legacy(KmppSessionOld *session, void *arg);
rk_s32 kmpp_session_setup_legacy(KmppSessionOld *session, KmppSessionCfg *cfg);
rk_s32 kmpp_session_is_running_legacy(KmppSessionOld *session);
rk_s32 kmpp_session_run_task_legacy(KmppSessionOld *session, void *param);
rk_s32 kmpp_session_add_req_legacy(KmppSessionOld *session, void *param);
void *kmpp_session_get_dev_legacy(KmppSessionOld *session);

#endif /* __KMPP_SESSION_LEGACY_H__ */

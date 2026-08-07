/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_SESSION_H__
#define __KMPP_SESSION_H__

typedef void (*KmppSessionHook)(rk_s32 chan_id, rk_s32 event, void *param);

typedef struct KmppSessionCfg_t {
    rk_u32 client_type;
    rk_s32 chan_id;
    rk_s32 online;
    KmppSessionHook hook;
} KmppSessionCfg;

typedef void* KmppSession;

#define KMPP_OBJ_NAME           kmpp_session
#define KMPP_OBJ_INTF_TYPE      KmppSession
#include "kmpp_obj_func.h"

rk_s32 kmpp_session_setup(KmppSession obj, KmppSessionCfg *cfg);
rk_s32 kmpp_session_is_running(KmppSession obj);
rk_s32 kmpp_session_run_task(KmppSession obj, void *param);
rk_s32 kmpp_session_add_req(KmppSession obj, void *reqs);
void *kmpp_session_get_dev(KmppSession obj);

#endif /* __KMPP_SESSION_H__ */

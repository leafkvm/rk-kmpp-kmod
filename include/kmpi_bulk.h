/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPI_BULK_H__
#define __KMPI_BULK_H__

#include "kmpi.h"

typedef void *KmppBulkTask;
typedef void *KmppBulkTaskCfg;

rk_s32 kmpp_bulk_task_init(KmppBulkTask *task, KmppBulkTaskCfg *cfg);
rk_s32 kmpp_bulk_task_deinit(KmppBulkTask task);

/* KmppChanId or KmppCtx */
rk_s32 kmpp_dec_bulk_task_push(KmppBulkTask task, KmppCtx ctx, KmppPacket pkt);
rk_s32 kmpp_enc_bulk_task_push(KmppBulkTask task, KmppCtx ctx, KmppFrame frm);
rk_s32 kmpp_vsp_bulk_task_push(KmppBulkTask task, KmppCtx ctx, KmppFrame frm);

rk_s32 kmpp_vdec_bulk_task_pop(KmppBulkTask task, KmppCtx *ctx, KmppFrame *frm);
rk_s32 kmpp_venc_bulk_task_pop(KmppBulkTask task, KmppCtx *ctx, KmppPacket *pkt);
rk_s32 kmpp_vsp_bulk_task_pop(KmppBulkTask task, KmppCtx *ctx, KmppFrame *frm);

rk_s32 kmpp_vdec_put_bulk(KmppBulkTask task);
rk_s32 kmpp_vdec_get_bulk(KmppBulkTask task);
rk_s32 kmpp_venc_put_bulk(KmppBulkTask task);
rk_s32 kmpp_venc_get_bulk(KmppBulkTask task);
rk_s32 kmpp_vsp_put_bulk(KmppBulkTask task);
rk_s32 kmpp_vsp_get_bulk(KmppBulkTask task);

#endif /*__KMPI_BULK_H__*/

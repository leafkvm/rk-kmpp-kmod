
/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __VDEC_PARSER_API_H__
#define __VDEC_PARSER_API_H__

#include "kmpp_list.h"

#include "vdec_parser.h"

typedef struct ParserApi_t {
    kmpp_list_head      list;

    const rk_u8         *name;
    MppCodingType       coding;
    rk_u32              ctx_size;
    rk_u64              flag;

    rk_s32 (*init)(void *ctx, ParserCfg *cfg);
    rk_s32 (*deinit)(void *ctx);
    rk_s32 (*reset)(void *ctx);
    rk_s32 (*start)(void *ctx);
    rk_s32 (*stop)(void *ctx);

    rk_s32 (*get_job)(void *ctx, ParserJob **job, rk_s32 size);
    rk_s32 (*put_job)(void *ctx, ParserJob *job);

    rk_s32 (*get_out)(void *ctx, ParserOut **out);
    rk_s32 (*poll_out)(void *ctx, ParserOut *out);
    rk_s32 (*put_out)(void *ctx, ParserOut *out);

    rk_s32 (*get_frm)(void *ctx, KmppFrame *frm);
} ParserApi;

rk_s32 parser_add(ParserApi *api);
rk_s32 parser_del(ParserApi *api);

#endif /* __VDEC_PARSER_API_H__ */

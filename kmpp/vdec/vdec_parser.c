
/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#include "kmpp_osal.h"

#include "vdec_parser_api.h"

typedef struct ParserImpl_t {
    ParserApi   *api;
    void        *ctx;
} ParserImpl;

static kmpp_list_head list_parser_api = KMPP_LIST_HEAD_INIT(list_parser_api);

rk_s32 parser_add(ParserApi *api)
{
    if (api) {
        /* TODO: check soc type first to valid device only */
        kmpp_list_add_tail(&api->list, &list_parser_api);
    }

    return rk_ok;
}

rk_s32 parser_del(ParserApi *api)
{
    if (api)
        kmpp_list_del_init(&api->list);

    return rk_ok;
}

rk_s32 parser_query(ParserCfg *cfg)
{
    MppCodingType coding = cfg->coding;
    ParserApi *api = NULL;
    ParserApi *pos;

    cfg->ctx_size = sizeof(VdecParser);

    kmpp_list_for_each_entry(pos, &list_parser_api, ParserApi, list) {
        if (pos->coding == coding) {
            api = pos;
            break;
        }
    }

    if (!api) {
        kmpp_logi_f("no parser api for coding %d\n", coding);
        return rk_nok;
    }

    cfg->ctx_size += api->ctx_size;
    cfg->flag = api->flag;

    return rk_ok;
}

rk_s32 parser_init(VdecParser parser, ParserCfg *cfg)
{
    ParserImpl *impl = (ParserImpl *)parser;
    ParserApi *api = NULL;
    ParserApi *pos;

    cfg->ctx_size = sizeof(VdecParser);

    kmpp_list_for_each_entry(pos, &list_parser_api, ParserApi, list) {
        if (pos->coding == cfg->coding) {
            api = pos;
            break;
        }
    }

    impl->ctx = (void *)(impl + 1);
    impl->api = api;

    if (api && api->init)
        return api->init(impl->ctx, cfg);

    return rk_ok;
}

rk_s32 parser_deinit(VdecParser parser)
{
    ParserImpl *impl = (ParserImpl *)parser;
    ParserApi *api = impl->api;

    if (api && api->deinit)
        return api->deinit(impl->ctx);

    return rk_ok;
}

rk_s32 parser_reset(VdecParser parser)
{
    ParserImpl *impl = (ParserImpl *)parser;
    ParserApi *api = impl->api;

    if (api && api->reset)
        return api->reset(impl->ctx);

    return rk_ok;
}

rk_s32 parser_start(VdecParser parser)
{
    ParserImpl *impl = (ParserImpl *)parser;
    ParserApi *api = impl->api;

    if (api && api->start)
        return api->start(impl->ctx);

    return rk_ok;
}

rk_s32 parser_stop(VdecParser parser)
{
    ParserImpl *impl = (ParserImpl *)parser;
    ParserApi *api = impl->api;

    if (api && api->stop)
        return api->stop(impl->ctx);

    return rk_ok;
}

rk_s32 parser_get_job(VdecParser parser, ParserJob **job, rk_s32 size)
{
    ParserImpl *impl = (ParserImpl *)parser;
    ParserApi *api = impl->api;

    if (api && api->get_job)
        return api->get_job(impl->ctx, job, size);

    return rk_ok;
}

rk_s32 parser_put_job(VdecParser parser, ParserJob *job)
{
    ParserImpl *impl = (ParserImpl *)parser;
    ParserApi *api = impl->api;

    if (api && api->put_job)
        return api->put_job(impl->ctx, job);

    return rk_ok;
}

rk_s32 parser_get_out(VdecParser parser, ParserOut **task)
{
    ParserImpl *impl = (ParserImpl *)parser;
    ParserApi *api = impl->api;

    if (api && api->get_out)
        return api->get_out(impl->ctx, task);

    return rk_ok;
}

rk_s32 parser_poll_out(VdecParser parser, ParserOut *task)
{
    ParserImpl *impl = (ParserImpl *)parser;
    ParserApi *api = impl->api;

    if (api && api->poll_out)
        return api->poll_out(impl->ctx, task);

    return rk_ok;
}

rk_s32 parser_put_out(VdecParser parser, ParserOut *task)
{
    ParserImpl *impl = (ParserImpl *)parser;
    ParserApi *api = impl->api;

    if (api && api->put_out)
        return api->put_out(impl->ctx, task);

    return rk_ok;
}

rk_s32 parser_get_frm(VdecParser parser, KmppFrame *frm)
{
    ParserImpl *impl = (ParserImpl *)parser;
    ParserApi *api = impl->api;

    if (api && api->get_frm)
        return api->get_frm(impl->ctx, frm);

    return rk_ok;
}

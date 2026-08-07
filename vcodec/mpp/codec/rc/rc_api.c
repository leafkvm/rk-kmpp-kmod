/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2016 Rockchip Electronics Co., Ltd.
 */

#define MODULE_TAG "rc_api"

#include <linux/kernel.h>

#include "mpp_mem.h"
#include "mpp_2str.h"

#include "kmpp_env.h"
#include "kmpp_list.h"
#include "kmpp_mutex.h"
#include "kmpp_module.h"
#include "kmpp_mod_id.h"
#include "kmpp_string.h"

#include "rc_debug.h"
#include "rc_api.h"

#include "h264e_rc.h"
#include "h265e_rc.h"
#include "jpege_rc.h"

#define get_srv_rc_api_srv(caller) \
    ({ \
        MppRcApiSrv *__tmp; \
        if (!rc_api_srv) { \
            rc_api_srv_init(); \
        } \
        if (rc_api_srv) { \
            __tmp = rc_api_srv; \
        } else { \
            kmpp_loge("mpp rc api srv not init at %s : %s\n", __FUNCTION__, caller); \
            __tmp = NULL; \
        } \
        __tmp; \
    })

// use class to register RcApi
typedef struct RcApiNode_t {
    /* list to list in MppRcApiSrv */
    kmpp_list_head      list;
    char                name[64];
    MppCodingType       type;
    RcApiBrief          brief;
    RcApi               api;
} RcApiNode;

typedef struct MppRcApiSrv_t {
    KmppMutex           *lock;
    KmppEnvNode         env;
    RK_U32              api_cnt;
    /* list for list in RcApiNode */
    kmpp_list_head      list;
} MppRcApiSrv;

static MppRcApiSrv *rc_api_srv = NULL;
RK_U32 kmpp_rc_debug = 0;

static rk_s32 rc_api_srv_init(void)
{
    MppRcApiSrv *srv = rc_api_srv;
    rk_s32 lock_size = kmpp_mutex_size();

    //mpp_env_get_u32("rc_debug", &kmpp_rc_debug, 0);

    if (srv)
        return rk_ok;

    srv = kmpp_calloc_atomic(sizeof(MppRcApiSrv) + lock_size);
    if (!srv) {
        kmpp_loge_f("failed to create rc api srv\n");
        return rk_nok;
    }

    rc_api_srv = srv;

    kmpp_mutex_assign(&srv->lock, (void *)(srv + 1), lock_size);
    KMPP_INIT_LIST_HEAD(&srv->list);
    srv->api_cnt = 0;

    {
        KmppEnvInfo env;

        env.type = KmppEnv_u32;
        env.readonly = 0;
        env.name = "kmpp_rc_debug";
        env.val = &kmpp_rc_debug;
        env.env_show = NULL;

        kmpp_env_add(kmpp_env_debug(), &srv->env, &env);
    }

    /* add all default rc apis */
    rc_api_register(&default_h264e);
    rc_api_register(&default_h265e);
    rc_api_register(&default_jpege);

    return rk_ok;
}

static rk_s32 rc_api_srv_deinit(void)
{
    MppRcApiSrv *srv = rc_api_srv;

    if (!srv)
        return rk_ok;

    kmpp_mutex_lock(srv->lock);

    if (srv->api_cnt) {
        RcApiNode *pos, *n;

        kmpp_list_for_each_entry_safe(pos, n, &srv->list, RcApiNode, list) {
            rc_dbg_api("%-5s rc api %s is removed\n",
                       strof_coding_type(pos->type), pos->name);

            kmpp_list_del_init(&pos->list);
            kmpp_free(pos);
            srv->api_cnt--;
        }

        kmpp_assert(srv->api_cnt == 0);
    }

    kmpp_mutex_unlock(srv->lock);
    kmpp_mutex_deinit(&srv->lock);
    kmpp_env_del(kmpp_env_debug(), srv->env);
    kmpp_free(srv);
    rc_api_srv = NULL;

    return rk_ok;
}

KMPP_MODULE_ADD(kmpp, KMPP_MOD_RC_API, rc_api, rc_api_srv_init, rc_api_srv_deinit)

static RcApi *_rc_api_get(MppRcApiSrv *srv, MppCodingType type, const char *name)
{
    if (srv->api_cnt && name) {
        RcApiNode *pos, *n;

        kmpp_list_for_each_entry_safe(pos, n, &srv->list, RcApiNode, list) {
            if (type == pos->type &&
                !kmpp_strncmp(name, pos->name, sizeof(pos->name) - 1)) {
                rc_dbg_api("%-5s rc api %s is selected\n",
                           strof_coding_type(type), pos->name);
                return &pos->api;
            }
        }
    }

    rc_dbg_api("%-5s rc api %s can not be found\n", strof_coding_type(type), name);

    return NULL;
}

static void set_node_api(RcApiNode *node, const RcApi *api)
{
    node->api = *api;
    node->type = api->type;

    kmpp_strncpy(node->name, api->name, sizeof(node->name) - 1);
    node->api.name = api->name;

    node->brief.type = api->type;
    node->brief.name = api->name;
}

rk_s32 rc_api_register(const RcApi *api)
{
    MppRcApiSrv *srv = get_srv_rc_api_srv(__FUNCTION__);
    RcApiNode *node = NULL;
    RcApi *node_api = NULL;

    if (!api) {
        kmpp_loge_f("unable to register NULL api\n");
        return rk_nok;
    }

    if (!srv)
        return rk_nok;

    kmpp_mutex_lock(srv->lock);

    /* search for same node for replacement */
    node_api = _rc_api_get(srv, api->type, api->name);

    if (!node_api) {
        node = mpp_malloc(RcApiNode, 1);
        if (!node) {
            kmpp_loge_f("failed to create api node\n");
            kmpp_mutex_unlock(srv->lock);
            return rk_nok;
        }

        KMPP_INIT_LIST_HEAD(&node->list);
        kmpp_list_add_tail(&node->list, &srv->list);

        srv->api_cnt++;
        rc_dbg_api("%-5s rc api %s is added\n", strof_coding_type(api->type), api->name);
    } else {
        node = container_of(node_api, RcApiNode, api);
        rc_dbg_api("%-5s rc api %s is updated\n", strof_coding_type(api->type), api->name);
    }

    set_node_api(node, api);
    kmpp_mutex_unlock(srv->lock);

    return rk_ok;
}

rk_s32 rc_api_unregister(const RcApi *api)
{
    MppRcApiSrv *srv = get_srv_rc_api_srv(__FUNCTION__);
    RcApiNode *node = NULL;
    RcApi *node_api = NULL;

    if (!api) {
        kmpp_loge_f("unable to unregister NULL api\n");
        return rk_nok;
    }

    if (!srv)
        return rk_nok;

    kmpp_mutex_lock(srv->lock);

    node_api = _rc_api_get(srv, api->type, api->name);

    if (node_api) {
        node = container_of(node_api, RcApiNode, api);

        rc_dbg_api("%-5s rc api %s is removed\n",
                   strof_coding_type(api->type), api->name);
        kmpp_list_del_init(&node->list);
        kmpp_free(node);
        srv->api_cnt--;
    } else {
        kmpp_loge("%-5s rc api %s is not registered\n",
                  strof_coding_type(api->type), api->name);
    }

    kmpp_mutex_unlock(srv->lock);

    return rk_ok;
}

RcApi *rc_api_get(MppCodingType type, const char *name)
{
    MppRcApiSrv *srv = get_srv_rc_api_srv(__FUNCTION__);

    if (!srv)
        return NULL;

    return _rc_api_get(srv, type, name);
}

rk_s32 rc_api_get_all(MppRcApiSrv *srv, RcApiBrief *brief, RK_S32 *count, RK_S32 max_count)
{
    RcApiNode *pos, *n;
    RK_S32 cnt = 0;

    kmpp_mutex_lock(srv->lock);

    kmpp_list_for_each_entry_safe(pos, n, &srv->list, RcApiNode, list) {
        if (cnt >= max_count)
            break;

        brief[cnt++] = pos->brief;
    }

    *count = cnt;
    kmpp_mutex_unlock(srv->lock);

    return rk_ok;
}

rk_s32 rc_api_get_by_type(MppRcApiSrv *srv, RcApiBrief *brief, RK_S32 *count,
                          RK_S32 max_count, MppCodingType type)
{
    RcApiNode *pos, *n;
    RK_S32 cnt = 0;

    kmpp_mutex_lock(srv->lock);

    kmpp_list_for_each_entry_safe(pos, n, &srv->list, RcApiNode, list) {
        if (cnt >= max_count)
            break;

        if (pos->type != type)
            continue;

        brief[cnt++] = pos->brief;
    }

    *count = cnt;
    kmpp_mutex_unlock(srv->lock);

    return rk_ok;
}

rk_s32 rc_brief_get_all(RcApiQueryAll *query)
{
    MppRcApiSrv *srv = rc_api_srv;
    RcApiBrief *brief;
    RK_S32 *count;
    RK_S32 max_count;

    if (!srv)
        return rk_nok;

    if (!query) {
        kmpp_loge_f("invalide NULL query input\n");
        return MPP_ERR_NULL_PTR;
    }

    brief = query->brief;
    count = &query->count;
    max_count = query->max_count;

    if (!brief || max_count <= 0) {
        kmpp_loge_f("invalide brief buffer %p max count %d\n", brief, max_count);
        return rk_nok;
    }

    return rc_api_get_all(srv, brief, count, max_count);
}

rk_s32 rc_brief_get_by_type(RcApiQueryType *query)
{
    MppRcApiSrv *srv = rc_api_srv;
    RcApiBrief *brief;
    RK_S32 *count;
    RK_S32 max_count;
    MppCodingType type;

    if (!srv)
        return rk_nok;

    if (!query) {
        kmpp_loge_f("invalide NULL query input\n");
        return MPP_ERR_NULL_PTR;
    }

    brief = query->brief;
    count = &query->count;
    max_count = query->max_count;
    type = query->type;

    if (!brief || max_count <= 0) {
        kmpp_loge_f("invalide brief buffer %p max count %d type %x\n",
                    brief, max_count, type);
        return rk_nok;
    }

    return rc_api_get_by_type(srv, brief, count, max_count, type);
}

#include <linux/export.h>

EXPORT_SYMBOL(rc_api_register);
EXPORT_SYMBOL(rc_api_unregister);
EXPORT_SYMBOL(rc_brief_get_all);
EXPORT_SYMBOL(rc_brief_get_by_type);
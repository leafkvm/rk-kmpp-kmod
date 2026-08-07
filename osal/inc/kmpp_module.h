/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_MODULE_H__
#define __KMPP_MODULE_H__

#include "rk_type.h"

#define MODULE_TO_STR(x)        #x
#define MODULE_TO_INFO(mod, x)  __##mod##_##x##_info
#define MODULE_TO_CTX(x)        __##x##_ctx
#define MODULE_SET(x)           __##x##_set
#define MODULE_SIZE(x)          sizeof(MODULE_SET(x))

#define KMPP_MODULE_ADD(mod, id, name, init, deinit) \
    KmppModInfo MODULE_TO_INFO(mod, name) = { \
        sizeof(MODULE_TO_INFO(mod, name)), \
        id, \
        MODULE_TO_STR(name), \
        init, \
        deinit, \
    };

#define KMPP_MODULE_EXTERN(mod, name) \
    extern KmppModInfo MODULE_TO_INFO(mod, name);

#define KMPP_MODULE_ARRAY(mod, name) \
    &MODULE_TO_INFO(mod, name),

#define KMPP_MODULE_ARRAYS(mod, TABLE) \
    TABLE(mod, KMPP_MODULE_EXTERN) \
    KmppModInfo *MODULE_SET(mod)[] = { \
    TABLE(mod, KMPP_MODULE_ARRAY) \
    };

#define KMPP_MODULE(mod) \
    static KmppModule MODULE_TO_CTX(mod) = NULL;

#define KMPP_MODULE_INIT(mod) \
    ({ \
        void *start = MODULE_SET(mod); \
        void *end = start + MODULE_SIZE(mod); \
        MODULE_TO_CTX(mod) = kmpp_module_init(MODULE_TO_STR(mod), start, end); \
    })

#define KMPP_MODULE_DEINIT(mod) \
    ({ \
        kmpp_module_deinit(MODULE_TO_CTX(mod)); \
        MODULE_TO_CTX(mod) = NULL; \
    })

typedef rk_s32 (*KmppModuleFunc)(void);
typedef void* KmppModule;

typedef struct KmppModInfo_t {
    rk_u32          size;
    rk_u32          id;
    const rk_u8     *name;
    KmppModuleFunc  init;
    KmppModuleFunc  deinit;
} KmppModInfo;

KmppModule kmpp_module_init(const rk_u8 *name, void *start, void *end);
rk_s32 kmpp_module_deinit(KmppModule mod);

#endif /*__KMPP_MODULE_H__*/

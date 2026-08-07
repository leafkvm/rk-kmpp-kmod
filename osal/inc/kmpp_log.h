/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_LOG_H__
#define __KMPP_LOG_H__

#include "rk_type.h"
#include "rk_log_def.h"

#ifndef MODULE_TAG
#define MODULE_TAG rk_null
#endif

/*
 * _c function will add condition check
 * _f function will add function name to the log
 * _cf function will add both function name and condition check
 */

/*
 * mpp runtime log system usage:
 * kmpp_logf is for fatal logging. For use when aborting
 * kmpp_loge is for error logging. For use with unrecoverable failures.
 * kmpp_logw is for warning logging. For use with recoverable failures.
 * kmpp_logi is for informational logging.
 * kmpp_logd is for debug logging.
 * kmpp_logv is for verbose logging
 */

#define kmpp_logf(fmt, ...)  _kmpp_log(RK_LOG_FATAL,   MODULE_TAG, fmt, NULL, ## __VA_ARGS__)
#define kmpp_loge(fmt, ...)  _kmpp_log(RK_LOG_ERROR,   MODULE_TAG, fmt, NULL, ## __VA_ARGS__)
#define kmpp_logw(fmt, ...)  _kmpp_log(RK_LOG_WARN,    MODULE_TAG, fmt, NULL, ## __VA_ARGS__)
#define kmpp_logi(fmt, ...)  _kmpp_log(RK_LOG_INFO,    MODULE_TAG, fmt, NULL, ## __VA_ARGS__)
#define kmpp_logd(fmt, ...)  _kmpp_log(RK_LOG_DEBUG,   MODULE_TAG, fmt, NULL, ## __VA_ARGS__)
#define kmpp_logv(fmt, ...)  _kmpp_log(RK_LOG_VERBOSE, MODULE_TAG, fmt, NULL, ## __VA_ARGS__)

#define kmpp_logf_f(fmt, ...)  _kmpp_log(RK_LOG_FATAL,   MODULE_TAG, fmt, __FUNCTION__, ## __VA_ARGS__)
#define kmpp_loge_f(fmt, ...)  _kmpp_log(RK_LOG_ERROR,   MODULE_TAG, fmt, __FUNCTION__, ## __VA_ARGS__)
#define kmpp_logw_f(fmt, ...)  _kmpp_log(RK_LOG_WARN,    MODULE_TAG, fmt, __FUNCTION__, ## __VA_ARGS__)
#define kmpp_logi_f(fmt, ...)  _kmpp_log(RK_LOG_INFO,    MODULE_TAG, fmt, __FUNCTION__, ## __VA_ARGS__)
#define kmpp_logd_f(fmt, ...)  _kmpp_log(RK_LOG_DEBUG,   MODULE_TAG, fmt, __FUNCTION__, ## __VA_ARGS__)
#define kmpp_logv_f(fmt, ...)  _kmpp_log(RK_LOG_VERBOSE, MODULE_TAG, fmt, __FUNCTION__, ## __VA_ARGS__)

#define kmpp_logf_c(cond, fmt, ...)  do { if (cond) kmpp_logf(fmt, ## __VA_ARGS__); } while (0)
#define kmpp_loge_c(cond, fmt, ...)  do { if (cond) kmpp_loge(fmt, ## __VA_ARGS__); } while (0)
#define kmpp_logw_c(cond, fmt, ...)  do { if (cond) kmpp_logw(fmt, ## __VA_ARGS__); } while (0)
#define kmpp_logi_c(cond, fmt, ...)  do { if (cond) kmpp_logi(fmt, ## __VA_ARGS__); } while (0)
#define kmpp_logd_c(cond, fmt, ...)  do { if (cond) kmpp_logd(fmt, ## __VA_ARGS__); } while (0)
#define kmpp_logv_c(cond, fmt, ...)  do { if (cond) kmpp_logv(fmt, ## __VA_ARGS__); } while (0)

#define kmpp_logf_cf(cond, fmt, ...) do { if (cond) kmpp_logf_f(fmt, ## __VA_ARGS__); } while (0)
#define kmpp_loge_cf(cond, fmt, ...) do { if (cond) kmpp_loge_f(fmt, ## __VA_ARGS__); } while (0)
#define kmpp_logw_cf(cond, fmt, ...) do { if (cond) kmpp_logw_f(fmt, ## __VA_ARGS__); } while (0)
#define kmpp_logi_cf(cond, fmt, ...) do { if (cond) kmpp_logi_f(fmt, ## __VA_ARGS__); } while (0)
#define kmpp_logd_cf(cond, fmt, ...) do { if (cond) kmpp_logd_f(fmt, ## __VA_ARGS__); } while (0)
#define kmpp_logv_cf(cond, fmt, ...) do { if (cond) kmpp_logv_f(fmt, ## __VA_ARGS__); } while (0)

/*
 * mpp runtime log system usage:
 * kmpp_err is for error status message, it will print for sure.
 * kmpp_log is for important message like open/close/reset/flush, it will print too.
 */

#define kmpp_log(fmt, ...) kmpp_logi(fmt, ## __VA_ARGS__)
#define kmpp_err(fmt, ...) kmpp_loge(fmt, ## __VA_ARGS__)

#define kmpp_log_f(fmt, ...) kmpp_logi_f(fmt, ## __VA_ARGS__)
#define kmpp_err_f(fmt, ...) kmpp_loge_f(fmt, ## __VA_ARGS__)

#define kmpp_log_c(cond, fmt, ...)  do { if (cond) kmpp_log(fmt, ## __VA_ARGS__); } while (0)
#define kmpp_log_cf(cond, fmt, ...) do { if (cond) kmpp_log_f(fmt, ## __VA_ARGS__); } while (0)

#define kmpp_dbg(debug, flag, fmt, ...)   kmpp_log_c((debug) & (flag), fmt, ## __VA_ARGS__)
#define kmpp_dbg_f(debug, flag, fmt, ...) kmpp_log_cf((debug) & (flag), fmt, ## __VA_ARGS__)

#define kmpp_assert(cond) do {                                          \
    if (!(cond)) {                                                      \
        kmpp_logf("Assertion %s failed at %s:%d\n",                     \
                   #cond, __FUNCTION__, __LINE__);                      \
    }                                                                   \
} while (0)

void _kmpp_log(int level, const char *tag, const char *fmt, const char *func, ...);

void kmpp_set_log_level(int level);
int kmpp_get_log_level(void);

#endif /* __KMPP_LOG_H__ */

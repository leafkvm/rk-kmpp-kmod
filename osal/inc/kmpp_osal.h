/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_OSAL_H__
#define __KMPP_OSAL_H__

#include "kmpp_list.h"
#include "kmpp_macro.h"

#include "kmpp_atomic.h"
#include "kmpp_completion.h"
#include "kmpp_dmabuf.h"
#include "kmpp_dmaheap.h"
#include "kmpp_bitops.h"
#include "kmpp_cls.h"
#include "kmpp_dev.h"
#include "kmpp_env.h"
#include "kmpp_file.h"
#include "kmpp_mem.h"
#include "kmpp_log.h"
#include "kmpp_mutex.h"
#include "kmpp_platform.h"
#include "kmpp_sema.h"
#include "kmpp_rwsem.h"
#include "kmpp_module.h"
#include "kmpp_spinlock.h"
#include "kmpp_thread.h"
#include "kmpp_sthd.h"
#include "kmpp_string.h"
#include "kmpp_time.h"
#include "kmpp_wait.h"
#include "kmpp_delay.h"
#include "kmpp_uaccess.h"

int kmpp_osal_init(void);
void kmpp_osal_exit(void);

#endif /* __KMPP_OSAL_H__ */

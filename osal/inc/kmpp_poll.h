/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_POLL_H__
#define __KMPP_POLL_H__

#include "kmpp_cls.h"
#include "kmpp_wait.h"

#define KMPP_POLLIN         0x0001
#define KMPP_POLLPRI        0x0002
#define KMPP_POLLOUT        0x0004
#define KMPP_POLLERR        0x0008
#define KMPP_POLLHUP        0x0010
#define KMPP_POLLNVAL       0x0020
#define KMPP_POLLRDNORM     0x0040
#define KMPP_POLLRDBAND     0x0080

void kmpp_poll_wait(KmppFsDev *dev, KmppWait *wait, void *poll_table);

#endif /* __KMPP_POLL_H__ */

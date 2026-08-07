/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_SHM_H__
#define __KMPP_SHM_H__

#include "kmpp_trie.h"
#include "kmpp_sys_defs.h"

/*
 * kernel share memory layout
 *
 * |---------------------|
 * |     KmppObjShm      |
 * |---------------------|
 * |     KmppShmImpl     |
 * |---------------------|
 * |     share memory    |
 * |---------------------|
 *
 * KmppObjShm is the ioctl userspace access object
 * kobj_uaddr in KmppObjShm is the KmppObjShm struct userspace address
 * kobj_kaddr in KmppObjShm is the KmppObjShm struct kernel address
 *
 * KmppObjShm is at the head of KmppShmImpl and KmppShmImpl can only
 * be seen by kernel. Userspace should not access KmppShmImpl.
 *
 * The share memory is the actual memory access by both kernel and userspace.
 * Also the share memory address will be assigned to KmppObjImpl entry.
 */

rk_s32 kmpp_shm_add_trie_info(KmppTrie trie);

KmppFsDev *kmpp_shm_get_objs_file(void);
rk_s32 kmpp_shm_get(KmppShm *shm, KmppFsDev *file, const rk_u8 *name, rk_u32 size);
rk_s32 kmpp_shm_put(KmppShm shm);

/*
 * set late deinit mode
 * when late deinit mode is enabled the shm will be released later
 * when the associated file is closed
 */
rk_s32 kmpp_shm_set_late_deinit(KmppShm shm, rk_u32 enable);

rk_s32 kmpp_shm_dump(KmppShm shm, const rk_u8 *caller);
rk_s32 kmpp_shm_check(KmppShm shm, const rk_u8 *caller);
KmppShm kmpp_shm_from_shmptr(KmppShmPtr *shmptr, const rk_u8 *caller);

#define kmpp_shm_dump_f(shm)        kmpp_shm_dump(shm, __FUNCTION__)
#define kmpp_shm_check_f(shm)       kmpp_shm_check(shm, __FUNCTION__)
#define kmpp_shm_from_shmptr_f(shm) kmpp_shm_from_shmptr(shm, __FUNCTION__)

KmppShmPtr *kmpp_shm_to_shmptr(KmppShm shm);

void *kmpp_shm_get_kbase(KmppShm shm);
void *kmpp_shm_get_kaddr(KmppShm shm);
rk_ul kmpp_shm_get_ubase(KmppShm shm);
rk_u64 kmpp_shm_get_uaddr(KmppShm shm);

rk_s32 kmpp_shm_set_kpriv(KmppShm shm, void *priv);
void *kmpp_shm_get_kpriv(KmppShm shm);
rk_u64 kmpp_shm_get_upriv(KmppShm shm);
rk_s32 kmpp_shm_get_entry_size(KmppShm shm);

#endif /* __KMPP_SHM_H__ */
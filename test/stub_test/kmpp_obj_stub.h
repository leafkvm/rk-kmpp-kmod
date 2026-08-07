/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_OBJ_STUB_H__
#define __KMPP_OBJ_STUB_H__

#include "kmpp_stub.h"
#include "kmpp_obj.h"

rk_s32 kmpp_objdef_create(KmppObjDef *def, const rk_u8 *name)
STUB_FUNC_RET_ZERO(kmpp_objdef_create);

rk_s32 kmpp_objdef_get(KmppObjDef *def, rk_s32 priv_size, rk_s32 size, const rk_u8 *name)
STUB_FUNC_RET_ZERO(kmpp_objdef_get);

rk_s32 kmpp_objdef_put(KmppObjDef def)
STUB_FUNC_RET_ZERO(kmpp_objdef_put);

rk_s32 kmpp_objdef_find(KmppObjDef *def, const rk_u8 *name)
STUB_FUNC_RET_ZERO(kmpp_objdef_find);

rk_s32 kmpp_objdef_add_entry(KmppObjDef def, const rk_u8 *name, KmppEntry *tbl)
STUB_FUNC_RET_ZERO(kmpp_objdef_add_entry);

rk_s32 kmpp_objdef_add_hook(KmppObjDef def, const rk_u8 *name, KmppObjHook hook)
STUB_FUNC_RET_ZERO(kmpp_objdef_add_hook);

rk_s32 kmpp_objdef_add_init(KmppObjDef def, KmppObjInit init)
STUB_FUNC_RET_ZERO(kmpp_objdef_add_init);

rk_s32 kmpp_objdef_add_deinit(KmppObjDef def, KmppObjDeinit deinit)
STUB_FUNC_RET_ZERO(kmpp_objdef_add_deinit);

rk_s32 kmpp_objdef_add_ioctl(KmppObjDef def, KmppObjIoctls *ioctls)
STUB_FUNC_RET_ZERO(kmpp_objdef_add_ioctl);

rk_s32 kmpp_objdef_add_dump(KmppObjDef def, KmppObjDump dump)
STUB_FUNC_RET_ZERO(kmpp_objdef_add_dump);

/* NOTE: get entry / index / offset by name can only used for non __xxxx elements */
rk_s32 kmpp_objdef_get_entry(KmppObjDef def, const rk_u8 *name, KmppEntry **tbl)
STUB_FUNC_RET_ZERO(kmpp_objdef_get_entry);

rk_s32 kmpp_objdef_get_index(KmppObjDef def, const rk_u8 *name)
STUB_FUNC_RET_ZERO(kmpp_objdef_get_index);

rk_s32 kmpp_objdef_get_offset(KmppObjDef def, const rk_u8 *name)
STUB_FUNC_RET_ZERO(kmpp_objdef_get_offset);

rk_s32 kmpp_objdef_get_hook(KmppObjDef def, const rk_u8 *name)
STUB_FUNC_RET_ZERO(kmpp_objdef_get_hook);

rk_s32 kmpp_objdef_ioctl(KmppObjDef def, KmppFsDev *file, KmppIoc ioc)
STUB_FUNC_RET_ZERO(kmpp_objdef_ioctl);

rk_s32 kmpp_objdef_dump(KmppObjDef def, rk_u32 flag)
STUB_FUNC_RET_ZERO(kmpp_objdef_dump);

void kmpp_objdef_dump_all(void)
STUB_FUNC_RET_VOID(kmpp_objdef_dump_all);

const rk_u8 *kmpp_objdef_get_name(KmppObjDef def)
STUB_FUNC_RET_NULL(kmpp_objdef_get_name);

rk_s32 kmpp_objdef_get_buf_size(KmppObjDef def)
STUB_FUNC_RET_ZERO(kmpp_objdef_get_buf_size);

rk_s32 kmpp_objdef_get_entry_size(KmppObjDef def)
STUB_FUNC_RET_ZERO(kmpp_objdef_get_entry_size);

KmppTrie kmpp_objdef_get_trie(KmppObjDef def, rk_u32 id)
STUB_FUNC_RET_NULL(kmpp_objdef_get_trie);

rk_s32 kmpp_objdef_share(KmppObjDef def)
STUB_FUNC_RET_ZERO(kmpp_objdef_share);

rk_s32 kmpp_objdefset_get(KmppObjDefSet **defs, rk_u32 id)
STUB_FUNC_RET_ZERO(kmpp_objdefset_get);

rk_s32 kmpp_objdefset_put(KmppObjDefSet *defs)
STUB_FUNC_RET_ZERO(kmpp_objdefset_put);

void kmpp_objdefset_dump(KmppObjDefSet *defs)
STUB_FUNC_RET_VOID(kmpp_objdefset_dump);

rk_s32 kmpp_obj_get(KmppObj *obj, KmppObjDef def, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_obj_get);

rk_s32 kmpp_obj_get_by_name(KmppObj *obj, const rk_u8 *name, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_obj_get_by_name);

rk_s32 kmpp_obj_assign(KmppObj *obj, KmppObjDef def, void *buf, rk_s32 size, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_obj_assign);

rk_s32 kmpp_obj_get_share(KmppObj *obj, KmppObjDef def, KmppFsDev *file, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_obj_get_share);

rk_s32 kmpp_obj_put(KmppObj obj, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_obj_put);

rk_s32 kmpp_obj_check(KmppObj obj, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_obj_check);

rk_s32 kmpp_obj_reset(KmppObj obj, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_obj_reset);

void *kmpp_obj_to_entry(KmppObj obj)
STUB_FUNC_RET_NULL(kmpp_obj_to_entry);

void *kmpp_obj_to_flags(KmppObj obj)
STUB_FUNC_RET_NULL(kmpp_obj_to_flags);

KmppShm kmpp_obj_to_shm(KmppObj obj)
STUB_FUNC_RET_NULL(kmpp_obj_to_shm);

rk_s32 kmpp_obj_to_shmptr(KmppObj obj, KmppShmPtr *sptr)
STUB_FUNC_RET_ZERO(kmpp_obj_to_shmptr);

KmppObj kmpp_obj_from_shmptr(KmppShmPtr *sptr)
STUB_FUNC_RET_NULL(kmpp_obj_from_shmptr);

/* value access function */
rk_s32 kmpp_obj_set_s32(KmppObj obj, const rk_u8 *name, rk_s32 val)
STUB_FUNC_RET_ZERO(kmpp_obj_set_s32);

rk_s32 kmpp_obj_get_s32(KmppObj obj, const rk_u8 *name, rk_s32 *val)
STUB_FUNC_RET_ZERO(kmpp_obj_get_s32);

rk_s32 kmpp_obj_set_u32(KmppObj obj, const rk_u8 *name, rk_u32 val)
STUB_FUNC_RET_ZERO(kmpp_obj_set_u32);

rk_s32 kmpp_obj_get_u32(KmppObj obj, const rk_u8 *name, rk_u32 *val)
STUB_FUNC_RET_ZERO(kmpp_obj_get_u32);

rk_s32 kmpp_obj_set_s64(KmppObj obj, const rk_u8 *name, rk_s64 val)
STUB_FUNC_RET_ZERO(kmpp_obj_set_s64);

rk_s32 kmpp_obj_get_s64(KmppObj obj, const rk_u8 *name, rk_s64 *val)
STUB_FUNC_RET_ZERO(kmpp_obj_get_s64);

rk_s32 kmpp_obj_set_u64(KmppObj obj, const rk_u8 *name, rk_u64 val)
STUB_FUNC_RET_ZERO(kmpp_obj_set_u64);

rk_s32 kmpp_obj_get_u64(KmppObj obj, const rk_u8 *name, rk_u64 *val)
STUB_FUNC_RET_ZERO(kmpp_obj_get_u64);

rk_s32 kmpp_obj_set_st(KmppObj obj, const rk_u8 *name, void *val)
STUB_FUNC_RET_ZERO(kmpp_obj_set_st);

rk_s32 kmpp_obj_get_st(KmppObj obj, const rk_u8 *name, void *val)
STUB_FUNC_RET_ZERO(kmpp_obj_get_st);

rk_s32 kmpp_obj_tbl_set_s32(KmppObj obj, KmppEntry *tbl, rk_s32 val)
STUB_FUNC_RET_ZERO(kmpp_obj_tbl_set_s32);

rk_s32 kmpp_obj_tbl_get_s32(KmppObj obj, KmppEntry *tbl, rk_s32 *val)
STUB_FUNC_RET_ZERO(kmpp_obj_tbl_get_s32);

rk_s32 kmpp_obj_tbl_set_u32(KmppObj obj, KmppEntry *tbl, rk_u32 val)
STUB_FUNC_RET_ZERO(kmpp_obj_tbl_set_u32);

rk_s32 kmpp_obj_tbl_get_u32(KmppObj obj, KmppEntry *tbl, rk_u32 *val)
STUB_FUNC_RET_ZERO(kmpp_obj_tbl_get_u32);

rk_s32 kmpp_obj_tbl_set_s64(KmppObj obj, KmppEntry *tbl, rk_s64 val)
STUB_FUNC_RET_ZERO(kmpp_obj_tbl_set_s64);

rk_s32 kmpp_obj_tbl_get_s64(KmppObj obj, KmppEntry *tbl, rk_s64 *val)
STUB_FUNC_RET_ZERO(kmpp_obj_tbl_get_s64);

rk_s32 kmpp_obj_tbl_set_u64(KmppObj obj, KmppEntry *tbl, rk_u64 val)
STUB_FUNC_RET_ZERO(kmpp_obj_tbl_set_u64);

rk_s32 kmpp_obj_tbl_get_u64(KmppObj obj, KmppEntry *tbl, rk_u64 *val)
STUB_FUNC_RET_ZERO(kmpp_obj_tbl_get_u64);

rk_s32 kmpp_obj_tbl_set_st(KmppObj obj, KmppEntry *tbl, void *val)
STUB_FUNC_RET_ZERO(kmpp_obj_tbl_set_st);

rk_s32 kmpp_obj_tbl_get_st(KmppObj obj, KmppEntry *tbl, void *val)
STUB_FUNC_RET_ZERO(kmpp_obj_tbl_get_st);

rk_s32 kmpp_obj_set_kobj(KmppObj obj, const rk_u8 *name, KmppObj val)
STUB_FUNC_RET_ZERO(kmpp_obj_set_kobj);

rk_s32 kmpp_obj_get_kobj(KmppObj obj, const rk_u8 *name, KmppObj *val)
STUB_FUNC_RET_ZERO(kmpp_obj_get_kobj);

rk_s32 kmpp_obj_set_kptr(KmppObj obj, const rk_u8 *name, void *val)
STUB_FUNC_RET_ZERO(kmpp_obj_set_kptr);

rk_s32 kmpp_obj_get_kptr(KmppObj obj, const rk_u8 *name, void **val)
STUB_FUNC_RET_ZERO(kmpp_obj_get_kptr);

rk_s32 kmpp_obj_set_kfp(KmppObj obj, const rk_u8 *name, void *val)
STUB_FUNC_RET_ZERO(kmpp_obj_set_kfp);

rk_s32 kmpp_obj_get_kfp(KmppObj obj, const rk_u8 *name, void **val)
STUB_FUNC_RET_ZERO(kmpp_obj_get_kfp);

rk_s32 kmpp_obj_tbl_set_kobj(KmppObj obj, KmppEntry *tbl, KmppObj val)
STUB_FUNC_RET_ZERO(kmpp_obj_tbl_set_kobj);

rk_s32 kmpp_obj_tbl_get_kobj(KmppObj obj, KmppEntry *tbl, KmppObj *val)
STUB_FUNC_RET_ZERO(kmpp_obj_tbl_get_kobj);

rk_s32 kmpp_obj_tbl_set_kptr(KmppObj obj, KmppEntry *tbl, void *val)
STUB_FUNC_RET_ZERO(kmpp_obj_tbl_set_kptr);

rk_s32 kmpp_obj_tbl_get_kptr(KmppObj obj, KmppEntry *tbl, void **val)
STUB_FUNC_RET_ZERO(kmpp_obj_tbl_get_kptr);

rk_s32 kmpp_obj_tbl_set_kfp(KmppObj obj, KmppEntry *tbl, void *val)
STUB_FUNC_RET_ZERO(kmpp_obj_tbl_set_kfp);

rk_s32 kmpp_obj_tbl_get_kfp(KmppObj obj, KmppEntry *tbl, void **val)
STUB_FUNC_RET_ZERO(kmpp_obj_tbl_get_kfp);

rk_s32 kmpp_obj_set_shm(KmppObj obj, const rk_u8 *name, KmppShmPtr *val)
STUB_FUNC_RET_ZERO(kmpp_obj_set_shm);

rk_s32 kmpp_obj_get_shm(KmppObj obj, const rk_u8 *name, KmppShmPtr *val)
STUB_FUNC_RET_ZERO(kmpp_obj_get_shm);

rk_s32 kmpp_obj_tbl_set_shm(KmppObj obj, KmppEntry *tbl, KmppShmPtr *val)
STUB_FUNC_RET_ZERO(kmpp_obj_tbl_set_shm);

rk_s32 kmpp_obj_tbl_get_shm(KmppObj obj, KmppEntry *tbl, KmppShmPtr *val)
STUB_FUNC_RET_ZERO(kmpp_obj_tbl_get_shm);

rk_s32 kmpp_obj_test(KmppObj obj, const rk_u8 *name)
STUB_FUNC_RET_ZERO(kmpp_obj_test);

rk_s32 kmpp_obj_tbl_test(KmppObj obj, KmppEntry *tbl)
STUB_FUNC_RET_ZERO(kmpp_obj_tbl_test);

rk_s32 kmpp_obj_update(KmppObj dst, KmppObj src)
STUB_FUNC_RET_ZERO(kmpp_obj_update);

rk_s32 kmpp_obj_update_entry(void *entry, KmppObj src)
STUB_FUNC_RET_ZERO(kmpp_obj_update_entry);

rk_s32 kmpp_obj_run(KmppObj obj, const rk_u8 *name, void *arg)
STUB_FUNC_RET_ZERO(kmpp_obj_run);

rk_s32 kmpp_obj_tbl_run(KmppObj obj, KmppEntry *tbl, void *arg)
STUB_FUNC_RET_ZERO(kmpp_obj_tbl_run);

rk_s32 kmpp_obj_idx_run(KmppObj obj, rk_s32 idx, void *arg, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_obj_idx_run);

rk_s32 kmpp_obj_dump(KmppObj obj, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_obj_dump);

const rk_u8 *strof_ELEM_TYPE(ElemType type)
STUB_FUNC_RET_NULL(strof_ELEM_TYPE);

#endif /* __KMPP_OBJ_STUB_H__ */
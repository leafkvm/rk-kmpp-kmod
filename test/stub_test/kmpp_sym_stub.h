/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2025 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_SYM_STUB_H__
#define __KMPP_SYM_STUB_H__

#include "kmpp_stub.h"
#include "kmpp_sym.h"

rk_s32 kmpp_sym_init(void)
STUB_FUNC_RET_ZERO(kmpp_sym_init);

rk_s32 kmpp_sym_deinit(void)
STUB_FUNC_RET_ZERO(kmpp_sym_deinit);

rk_s32 kmpp_symdef_get_f(KmppSymDef *def, const rk_u8 *name, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_symdef_get_f);

rk_s32 kmpp_symdef_put_f(KmppSymDef def, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_symdef_put_f);

rk_u32 kmpp_symdef_add(KmppSymDef def, const rk_u8 *name, KmppFuncPtr func)
STUB_FUNC_RET_ZERO(kmpp_symdef_add);

rk_s32 kmpp_symdef_install(KmppSymDef def)
STUB_FUNC_RET_ZERO(kmpp_symdef_install);

rk_s32 kmpp_symdef_uninstall(KmppSymDef def)
STUB_FUNC_RET_ZERO(kmpp_symdef_uninstall);

rk_u32 kmpp_syms_get_f(KmppSyms *syms, const rk_u8 *name, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_syms_get_f);

rk_u32 kmpp_syms_put_f(KmppSyms syms, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_syms_put_f);

rk_u32 kmpp_sym_get(KmppSym *sym, KmppSyms syms, const rk_u8 *name)
STUB_FUNC_RET_ZERO(kmpp_sym_get);

rk_u32 kmpp_sym_put(KmppSym sym)
STUB_FUNC_RET_ZERO(kmpp_sym_put);

rk_s32 kmpp_sym_run_f(KmppSym sym, void *arg, rk_s32 *ret, const rk_u8 *caller)
STUB_FUNC_RET_ZERO(kmpp_sym_run_f);

void kmpp_syms_dump(KmppSyms syms, const rk_u8 *caller)
STUB_FUNC_RET_VOID(kmpp_syms_dump);

void kmpp_syms_dump_all(const rk_u8 *caller)
STUB_FUNC_RET_VOID(kmpp_syms_dump_all);

#endif /* __KMPP_SYM_STUB_H__ */
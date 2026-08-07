/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_SYM_H__
#define __KMPP_SYM_H__

#include "rk_type.h"

typedef void* KmppSymDef;
typedef void* KmppSyms;
typedef void* KmppSym;

typedef rk_s32 (*KmppFuncPtr)(void *arg, const rk_u8 *caller);

/* register side get sym for config */
rk_s32 kmpp_symdef_get_f(KmppSymDef *def, const rk_u8 *name, const rk_u8 *caller);
rk_s32 kmpp_symdef_put_f(KmppSymDef def, const rk_u8 *caller);
#define kmpp_symdef_get(def, name) kmpp_symdef_get_f(def, name, __FUNCTION__)
#define kmpp_symdef_put(def) kmpp_symdef_put_f(def, __FUNCTION__)

/* pass pointer between different modules */
rk_u32 kmpp_symdef_add(KmppSymDef def, const rk_u8 *name, KmppFuncPtr func);

/* install to sys module */
rk_s32 kmpp_symdef_install(KmppSymDef def);
/* uninstall from sys module */
rk_s32 kmpp_symdef_uninstall(KmppSymDef def);

/* user side functions */
rk_u32 kmpp_syms_get_f(KmppSyms *syms, const rk_u8 *name, const rk_u8 *caller);
rk_u32 kmpp_syms_put_f(KmppSyms syms, const rk_u8 *caller);
#define kmpp_syms_get(syms, name) kmpp_syms_get_f(syms, name, __FUNCTION__)
#define kmpp_syms_put(syms) kmpp_syms_put_f(syms, __FUNCTION__)

rk_u32 kmpp_sym_get(KmppSym *sym, KmppSyms syms, const rk_u8 *name);
rk_u32 kmpp_sym_put(KmppSym sym);

/* run the callback function */
rk_s32 kmpp_sym_run_f(KmppSym sym, void *arg, rk_s32 *ret, const rk_u8 *caller);
#define kmpp_sym_run(sym, arg, ret) kmpp_sym_run_f(sym, arg, ret, __FUNCTION__)

void kmpp_syms_dump(KmppSyms syms, const rk_u8 *caller);
void kmpp_syms_dump_all(const rk_u8 *caller);
#define kmpp_syms_dump_f(syms) kmpp_syms_dump(syms, __FUNCTION__)
#define kmpp_syms_dump_all_f() kmpp_syms_dump_all(__FUNCTION__)

#endif /* __KMPP_SYM_H__ */
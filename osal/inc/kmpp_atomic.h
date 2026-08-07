/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_ATOMIC_H__
#define __KMPP_ATOMIC_H__

#include "rk_type.h"

#define KMPP_FETCH_ADD          __sync_fetch_and_add
#define KMPP_FETCH_SUB          __sync_fetch_and_sub
#define KMPP_FETCH_OR           __sync_fetch_and_or
#define KMPP_FETCH_AND          __sync_fetch_and_and
#define KMPP_FETCH_XOR          __sync_fetch_and_xor
#define KMPP_FETCH_NAND         __sync_fetch_and_nand

#define KMPP_ADD_FETCH          __sync_add_and_fetch
#define KMPP_SUB_FETCH          __sync_sub_and_fetch
#define KMPP_OR_FETCH           __sync_or_and_fetch
#define KMPP_AND_FETCH          __sync_and_and_fetch
#define KMPP_XOR_FETCH          __sync_xor_and_fetch
#define KMPP_NAND_FETCH         __sync_nand_and_fetch

#define KMPP_BOOL_CAS           __sync_bool_compare_and_swap
#define KMPP_VAL_CAS            __sync_val_compare_and_swap

#define KMPP_SYNC               __sync_synchronize
#define KMPP_SYNC_TEST_SET      __sync_lock_test_and_set
#define KMPP_SYNC_CLR           __sync_lock_release

typedef struct KmppAtomic_t KmppAtomic;

rk_s32 kmpp_atomic_size(void);
rk_s32 kmpp_atomic_init(KmppAtomic **atomic);
rk_s32 kmpp_atomic_assign(KmppAtomic **atomic, void *buf, rk_s32 size);
void   kmpp_atomic_deinit(KmppAtomic **atomic);

rk_s32 kmpp_atomic_read(KmppAtomic *v);
void   kmpp_atomic_set(KmppAtomic *v, rk_s32 i);

void kmpp_atomic_inc(KmppAtomic *v);
void kmpp_atomic_dec(KmppAtomic *v);
rk_s32 kmpp_atomic_fetch_inc(KmppAtomic *v);
rk_s32 kmpp_atomic_fetch_dec(KmppAtomic *v);
rk_s32 kmpp_atomic_inc_return(KmppAtomic *v);
rk_s32 kmpp_atomic_dec_return(KmppAtomic *v);
rk_s32 kmpp_atomic_dec_and_test(KmppAtomic *v);

rk_s32 kmpp_atomic_xchg(KmppAtomic *v, rk_s32 i);

#define kmpp_cmpxchg(ptr, oldval, newval) cmpxchg(ptr, oldval, newval)

#define kmpp_force_cmpxchg(ptr, oldval, newval) \
({ \
        typeof(*ptr) __val; \
        while ((__val = cmpxchg(ptr, oldval, newval)) && (*ptr != newval)); \
        __val; \
})

#endif /* __KMPP_ATOMIC_H__ */

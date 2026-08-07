/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/atomic.h>
#include <linux/slab.h>

#include "kmpp_mem.h"
#include "kmpp_atomic.h"
#include "kmpp_string.h"

struct KmppAtomic_t {
    rk_u32 need_free;
    atomic_t atomic[];
};

rk_s32 kmpp_atomic_size(void)
{
    return sizeof(KmppAtomic) + sizeof(atomic_t);
}
EXPORT_SYMBOL(kmpp_atomic_size);

rk_s32 kmpp_atomic_init(KmppAtomic **atomic)
{
    if (atomic) {
        rk_s32 size = kmpp_atomic_size();
        KmppAtomic *p = kmpp_calloc(size);

        *atomic = NULL;
        if (!p)
            return rk_nok;

        p->need_free = 1;

        *atomic = p;
        return 0;
    }

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_atomic_init);

rk_s32 kmpp_atomic_assign(KmppAtomic **atomic, void *buf, rk_s32 size)
{
    if (atomic && buf && size >= kmpp_atomic_size()) {
        KmppAtomic *p = (KmppAtomic *)buf;

        p->need_free = 0;

        *atomic = p;
        return rk_ok;
    }

    return rk_nok;
}
EXPORT_SYMBOL(kmpp_atomic_assign);

void kmpp_atomic_deinit(KmppAtomic **atomic)
{
    if (atomic) {
        KmppAtomic *p = kmpp_cmpxchg(atomic, *atomic, NULL);

        if (p) {
            if (p->need_free)
                kmpp_free(p);
            else
                kmpp_memset(p, 0, kmpp_atomic_size());
        }
    }
}
EXPORT_SYMBOL(kmpp_atomic_deinit);

rk_s32 kmpp_atomic_read(KmppAtomic *v)
{
    if (v)
        return atomic_read(v->atomic);

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_atomic_read);

void kmpp_atomic_set(KmppAtomic *v, rk_s32 val)
{
    if (v)
        atomic_set(v->atomic, val);
}
EXPORT_SYMBOL(kmpp_atomic_set);

void kmpp_atomic_inc(KmppAtomic *v)
{
    if (v)
        atomic_inc(v->atomic);
}
EXPORT_SYMBOL(kmpp_atomic_inc);

void kmpp_atomic_dec(KmppAtomic *v)
{
    if (v)
        atomic_dec(v->atomic);
}
EXPORT_SYMBOL(kmpp_atomic_dec);

rk_s32 kmpp_atomic_fetch_inc(KmppAtomic *v)
{
    if (v)
        return atomic_fetch_inc(v->atomic);

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_atomic_fetch_inc);

rk_s32 kmpp_atomic_fetch_dec(KmppAtomic *v)
{
    if (v)
        return atomic_fetch_dec(v->atomic);

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_atomic_fetch_dec);

rk_s32 kmpp_atomic_inc_return(KmppAtomic *v)
{
    if (v)
        return atomic_inc_return(v->atomic);

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_atomic_inc_return);

rk_s32 kmpp_atomic_dec_return(KmppAtomic *v)
{
    if (v)
        return atomic_dec_return(v->atomic);

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_atomic_dec_return);

rk_s32 kmpp_atomic_dec_and_test(KmppAtomic *v)
{
    if (v)
        return atomic_dec_and_test(v->atomic);

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_atomic_dec_and_test);

rk_s32 kmpp_atomic_xchg(KmppAtomic *v, rk_s32 i)
{
    if (v)
        return atomic_xchg(v->atomic, i);

    return -EINVAL;
}
EXPORT_SYMBOL(kmpp_atomic_xchg);
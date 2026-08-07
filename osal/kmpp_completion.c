/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#include <linux/kernel.h>
#include <linux/completion.h>

#include "kmpp_mem.h"
#include "kmpp_atomic.h"
#include "kmpp_completion.h"

struct KmppCompletion_t {
    rk_u32 need_free;
    void *completion;
};

void kmpp_completion_init(KmppCompletion **completion)
{
    if (completion) {
        KmppCompletion *p = kmpp_calloc(sizeof(KmppCompletion) + sizeof(struct completion));

        *completion = NULL;
        if (p) {
            struct completion *s = (struct completion *)(p + 1);

            init_completion(s);
            p->need_free = 1;
            p->completion = (void *)s;
            *completion = p;
        }
    }
}
EXPORT_SYMBOL(kmpp_completion_init);

void kmpp_completion_deinit(KmppCompletion **completion)
{
    if (completion) {
        KmppCompletion *p = KMPP_FETCH_AND(completion, NULL);

        if (p && p->need_free)
            kmpp_free(p);
    }
}
EXPORT_SYMBOL(kmpp_completion_deinit);

rk_s32 kmpp_completion_size(void)
{
    return sizeof(KmppCompletion) + sizeof(struct completion);
}
EXPORT_SYMBOL(kmpp_completion_size);

rk_s32 kmpp_completion_assign(KmppCompletion **completion, void *buf, rk_s32 size)
{
    if (completion && buf && size >= sizeof(KmppCompletion) + sizeof(struct completion)) {
        KmppCompletion *p = (KmppCompletion *)buf;
        struct completion *s = (struct completion *)(p + 1);

        init_completion(s);
        p->need_free = 0;
        p->completion = (void *)s;
        *completion = p;
        return rk_ok;
    }

    if (completion)
        *completion = NULL;

    return rk_nok;
}
EXPORT_SYMBOL(kmpp_completion_assign);

void kmpp_completion_reinit(KmppCompletion *completion)
{
    if (completion && completion->completion)
        reinit_completion((struct completion *)completion->completion);
}
EXPORT_SYMBOL(kmpp_completion_reinit);

void kmpp_wait_for_completion(KmppCompletion *completion)
{
    if (completion && completion->completion)
        wait_for_completion((struct completion *)completion->completion);
}
EXPORT_SYMBOL(kmpp_wait_for_completion);

rk_s32 kmpp_wait_for_completion_interruptible(KmppCompletion *completion)
{
    if (!completion || !completion->completion)
        return rk_nok;

    return wait_for_completion_interruptible((struct completion *)completion->completion);
}
EXPORT_SYMBOL(kmpp_wait_for_completion_interruptible);

rk_s32 kmpp_wait_for_completion_timeout(KmppCompletion *completion, rk_s32 timeout)
{
    if (!completion || !completion->completion)
        return rk_nok;

    return wait_for_completion_timeout((struct completion *)completion->completion, timeout);
}
EXPORT_SYMBOL(kmpp_wait_for_completion_timeout);

rk_s32 kmpp_try_wait_for_completion(KmppCompletion *completion)
{
    if (!completion || !completion->completion)
        return rk_nok;

    return try_wait_for_completion((struct completion *)completion->completion);
}
EXPORT_SYMBOL(kmpp_try_wait_for_completion);

rk_s32 kmpp_complete_done(KmppCompletion *completion)
{
    if (!completion || !completion->completion)
        return rk_nok;

    return completion_done((struct completion *)completion->completion);
}
EXPORT_SYMBOL(kmpp_complete_done);

void kmpp_complete(KmppCompletion *completion)
{
    if (completion && completion->completion)
        complete((struct completion *)completion->completion);
}
EXPORT_SYMBOL(kmpp_complete);

void kmpp_complete_all(KmppCompletion *completion)
{
    if (completion && completion->completion)
        complete_all((struct completion *)completion->completion);
}
EXPORT_SYMBOL(kmpp_complete_all);
/* SPDX-License-Identifier: Apache-2.0 OR MIT */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 */

#ifndef __KMPP_LIST_H__
#define __KMPP_LIST_H__

#include "rk_type.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

typedef struct kmpp_list_head_t kmpp_list_head;

struct kmpp_list_head_t {
    kmpp_list_head *next, *prev;
};

#define KMPP_LIST_HEAD_INIT(name) { &(name), &(name) }

#define KMPP_LIST_HEAD(name) \
    kmpp_list_head name = KMPP_LIST_HEAD_INIT(name)

#define KMPP_INIT_LIST_HEAD(ptr) do { \
        (ptr)->next = (ptr); (ptr)->prev = (ptr); \
    } while (0)

#define kmpp_list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
         pos = n, n = pos->next)

#define kmpp_list_entry(ptr, type, member) \
    ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

#define kmpp_list_first_entry(ptr, type, member) \
        kmpp_list_entry((ptr)->next, type, member)

#define kmpp_list_last_entry(ptr, type, member) \
        kmpp_list_entry((ptr)->prev, type, member)

#define kmpp_list_first_entry_or_null(ptr, type, member) ({ \
        kmpp_list_head *head__ = (ptr); \
        kmpp_list_head *pos__ = head__->next; \
        pos__ != head__ ? kmpp_list_entry(pos__, type, member) : NULL; \
})

#define kmpp_list_next_entry(pos, type, member) \
        kmpp_list_entry((pos)->member.next, type, member)

#define kmpp_list_prev_entry(pos, type, member) \
        kmpp_list_entry((pos)->member.prev, type, member)

#define kmpp_list_for_each_entry(pos, head, type, member) \
    for (pos = kmpp_list_entry((head)->next, type, member); \
         &pos->member != (head); \
         pos = kmpp_list_next_entry(pos, type, member))

#define kmpp_list_for_each_entry_safe(pos, n, head, type, member) \
    for (pos = kmpp_list_first_entry(head, type, member),  \
         n = kmpp_list_next_entry(pos, type, member); \
         &pos->member != (head);                    \
         pos = n, n = kmpp_list_next_entry(n, type, member))

#define kmpp_list_for_each_entry_reverse(pos, head, type, member) \
    for (pos = kmpp_list_last_entry(head, type, member); \
         &pos->member != (head); \
         pos = kmpp_list_prev_entry(pos, type, member))

#define kmpp_list_for_each_entry_safe_reverse(pos, n, head, type, member) \
    for (pos = kmpp_list_last_entry(head, type, member),  \
         n = kmpp_list_prev_entry(pos, type, member); \
         &pos->member != (head);                    \
         pos = n, n = kmpp_list_prev_entry(n, type, member))

static __inline void __kmpp_list_add(kmpp_list_head * _new,
                                     kmpp_list_head * prev,
                                     kmpp_list_head * next)
{
    next->prev = _new;
    _new->next = next;
    _new->prev = prev;
    prev->next = _new;
}

static __inline void kmpp_list_add(kmpp_list_head *_new, kmpp_list_head *head)
{
    __kmpp_list_add(_new, head, head->next);
}

static __inline void kmpp_list_add_tail(kmpp_list_head *_new, kmpp_list_head *head)
{
    __kmpp_list_add(_new, head->prev, head);
}

static __inline void __kmpp_list_del(kmpp_list_head * prev, kmpp_list_head * next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void kmpp_list_replace(kmpp_list_head *old, kmpp_list_head *_new)
{
    _new->next = old->next;
    _new->next->prev = _new;
    _new->prev = old->prev;
    _new->prev->next = _new;
}

static inline void kmpp_list_replace_init(kmpp_list_head *old, kmpp_list_head *_new)
{
    kmpp_list_replace(old, _new);
    KMPP_INIT_LIST_HEAD(old);
}

static inline void kmpp_list_swap(kmpp_list_head *entry1, kmpp_list_head *entry2)
{
    kmpp_list_head *pos = entry2->prev;

    __kmpp_list_del(entry2->prev, entry2->next);
    kmpp_list_replace(entry1, entry2);
    if (pos == entry1)
        pos = entry2;
    kmpp_list_add(entry1, pos);
}

static __inline void kmpp_list_del_init(kmpp_list_head *entry)
{
    __kmpp_list_del(entry->prev, entry->next);

    KMPP_INIT_LIST_HEAD(entry);
}

static __inline void kmpp_list_move(kmpp_list_head *list, kmpp_list_head *head)
{
    __kmpp_list_del(list->prev, list->next);
    kmpp_list_add(list, head);
}

static __inline void kmpp_list_move_tail(kmpp_list_head *list, kmpp_list_head *head)
{
    __kmpp_list_del(list->prev, list->next);
    kmpp_list_add_tail(list, head);
}

static inline int kmpp_list_is_first(const kmpp_list_head *list, const kmpp_list_head *head)
{
    return list->prev == head;
}

static inline int kmpp_list_is_last(const kmpp_list_head *list, const kmpp_list_head *head)
{
    return list->next == head;
}

static inline int kmpp_list_empty(const kmpp_list_head *head)
{
    return head->next == head;
}

static inline void __kmpp_list_splice(const kmpp_list_head *list,
                                      kmpp_list_head *prev,
                                      kmpp_list_head *next)
{
    kmpp_list_head *first = list->next;
    kmpp_list_head *last = list->prev;

    first->prev = prev;
    prev->next = first;

    last->next = next;
    next->prev = last;
}

static inline void kmpp_list_splice(const kmpp_list_head *list, kmpp_list_head *head)
{
    if (!kmpp_list_empty(list))
        __kmpp_list_splice(list, head, head->next);
}

static inline void kmpp_list_splice_tail(kmpp_list_head *list, kmpp_list_head *head)
{
    if (!kmpp_list_empty(list))
        __kmpp_list_splice(list, head->prev, head);
}

static inline void kmpp_list_splice_init(kmpp_list_head *list, kmpp_list_head *head)
{
    if (!kmpp_list_empty(list)) {
        __kmpp_list_splice(list, head, head->next);
        KMPP_INIT_LIST_HEAD(list);
    }
}

static inline void kmpp_list_splice_tail_init(kmpp_list_head *list, kmpp_list_head *head)
{
    if (!kmpp_list_empty(list)) {
        __kmpp_list_splice(list, head->prev, head);
        KMPP_INIT_LIST_HEAD(list);
    }
}

typedef RK_S32 (*kmpp_list_cmp_func_t)(void *, const kmpp_list_head *, const kmpp_list_head *);

void kmpp_list_sort(void *priv, kmpp_list_head *head, kmpp_list_cmp_func_t cmp);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif /* __KMPP_LIST_H__ */

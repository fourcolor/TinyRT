#pragma once

#include <stddef.h>

#define container_of(ptr, type, member) ((type *)((char *)(ptr)-offsetof(type, member)))

struct list_head
{
    struct list_head *next;
    struct list_head *prev;
};

typedef struct list_head list_head_t;
typedef struct list_head list_node_t;

#define LIST_HEAD_INIT(name)                                                                       \
    {                                                                                              \
        &(name), &(name)                                                                           \
    }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

static inline void __list_add(struct list_head *node, struct list_head *prev,
                              struct list_head *next)
{
    next->prev = node;
    node->next = next;
    node->prev = prev;
    prev->next = node;
}

static inline void list_add(struct list_head *node, struct list_head *head)
{
    __list_add(node, head, head->next);
}

static inline void list_add_tail(struct list_head *node, struct list_head *head)
{
    __list_add(node, head->prev, head);
}

static inline void list_add_before(struct list_head *node, struct list_head *pos)
{
    __list_add(node, pos->prev, pos);
}

static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    entry->next = entry;
    entry->prev = entry;
}

static inline void list_del_init(struct list_head *entry)
{
    list_del(entry);
    INIT_LIST_HEAD(entry);
}

static inline void list_replace(struct list_head *old, struct list_head *node)
{
    node->next = old->next;
    node->next->prev = node;
    node->prev = old->prev;
    node->prev->next = node;
}

static inline void list_replace_init(struct list_head *old, struct list_head *node)
{
    list_replace(old, node);
    INIT_LIST_HEAD(old);
}

static inline void list_move(struct list_head *node, struct list_head *head)
{
    __list_del(node->prev, node->next);
    list_add(node, head);
}

static inline void list_move_tail(struct list_head *node, struct list_head *head)
{
    __list_del(node->prev, node->next);
    list_add_tail(node, head);
}

static inline int list_empty(const struct list_head *head)
{
    return head->next == head;
}

static inline int list_is_singular(const struct list_head *head)
{
    return !list_empty(head) && head->next == head->prev;
}

static inline void __list_splice(const struct list_head *list, struct list_head *prev,
                                 struct list_head *next)
{
    struct list_head *first = list->next;
    struct list_head *last = list->prev;

    first->prev = prev;
    prev->next = first;
    last->next = next;
    next->prev = last;
}

static inline void list_splice(const struct list_head *list, struct list_head *head)
{
    if (!list_empty(list))
    {
        __list_splice(list, head, head->next);
    }
}

static inline void list_splice_tail(const struct list_head *list, struct list_head *head)
{
    if (!list_empty(list))
    {
        __list_splice(list, head->prev, head);
    }
}

static inline void list_splice_init(struct list_head *list, struct list_head *head)
{
    if (!list_empty(list))
    {
        __list_splice(list, head, head->next);
        INIT_LIST_HEAD(list);
    }
}

static inline void list_splice_tail_init(struct list_head *list, struct list_head *head)
{
    if (!list_empty(list))
    {
        __list_splice(list, head->prev, head);
        INIT_LIST_HEAD(list);
    }
}

#define list_entry(ptr, type, member) container_of(ptr, type, member)

#define list_first_entry(ptr, type, member) list_entry((ptr)->next, type, member)

#define list_last_entry(ptr, type, member) list_entry((ptr)->prev, type, member)

#define list_next_entry(pos, member) list_entry((pos)->member.next, __typeof__(*(pos)), member)

#define list_prev_entry(pos, member) list_entry((pos)->member.prev, __typeof__(*(pos)), member)

#define list_for_each(pos, head) for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_prev(pos, head) for (pos = (head)->prev; pos != (head); pos = pos->prev)

#define list_for_each_safe(pos, n, head)                                                           \
    for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)

#define list_for_each_prev_safe(pos, n, head)                                                      \
    for (pos = (head)->prev, n = pos->prev; pos != (head); pos = n, n = pos->prev)

#define list_for_each_entry(pos, head, member)                                                     \
    for (pos = list_entry((head)->next, __typeof__(*pos), member); &pos->member != (head);         \
         pos = list_next_entry(pos, member))

#define list_for_each_entry_reverse(pos, head, member)                                             \
    for (pos = list_entry((head)->prev, __typeof__(*pos), member); &pos->member != (head);         \
         pos = list_prev_entry(pos, member))

#define list_for_each_entry_safe(pos, n, head, member)                                             \
    for (pos = list_entry((head)->next, __typeof__(*pos), member),                                 \
        n = list_next_entry(pos, member);                                                          \
         &pos->member != (head); pos = n, n = list_next_entry(n, member))

#define list_for_each_entry_safe_reverse(pos, n, head, member)                                     \
    for (pos = list_entry((head)->prev, __typeof__(*pos), member),                                 \
        n = list_prev_entry(pos, member);                                                          \
         &pos->member != (head); pos = n, n = list_prev_entry(n, member))

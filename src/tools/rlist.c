#include "rlist.h"

void rlist_init(rlist_t *head)
{
    head->prev = head;
    head->next = head;
}

/* 在 node 与 node->next 之间插入 new_node */
static void __rlist_insert(rlist_t *node, rlist_t *new_node)
{
    new_node->prev = node;
    new_node->next = node->next;
    node->next->prev = new_node;
    node->next = new_node;
}

void rlist_insert_after(rlist_t *node, rlist_t *new_node)
{
    __rlist_insert(node, new_node);
}

void rlist_insert_before(rlist_t *node, rlist_t *new_node)
{
    __rlist_insert(node->prev, new_node);
}

void rlist_push_head(rlist_t *head, rlist_t *node)
{
    __rlist_insert(head, node);
}

void rlist_push_tail(rlist_t *head, rlist_t *node)
{
    __rlist_insert(head->prev, node);
}

rlist_t *rlist_remove(rlist_t *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;

    node->prev = node;
    node->next = node;

    return node;
}

rlist_t *rlist_pop_head(rlist_t *head)
{
    rlist_t *node;

    if (head->next == head)
    {
        return (rlist_t *)0;
    }

    node = head->next;
    rlist_remove(node);

    return node;
}

rlist_t *rlist_pop_tail(rlist_t *head)
{
    rlist_t *node;

    if (head->prev == head)
    {
        return (rlist_t *)0;
    }

    node = head->prev;
    rlist_remove(node);

    return node;
}

int rlist_is_empty(const rlist_t *head)
{
    return head->next == head;
}

unsigned int rlist_len(const rlist_t *head)
{
    unsigned int len = 0;
    const rlist_t *pos;

    for (pos = head->next; pos != head; pos = pos->next)
    {
        len++;
    }

    return len;
}

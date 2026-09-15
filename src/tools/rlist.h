#ifndef __RLIST_H__
#define __RLIST_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 侵入式双向环形链表 (ring list)
 *
 * 节点内嵌在使用者结构体中，不涉及任何动态内存分配：
 *
 *     typedef struct {
 *         int        data;
 *         rlist_t    node;      // 链表节点
 *     } my_item_t;
 *
 * 通过 RLIST_CONTAINER_OF() 可由节点反推出宿主结构体地址。
 */

/** 链表节点 */
typedef struct rlist_node
{
    struct rlist_node *prev;
    struct rlist_node *next;
} rlist_t;

/** 由成员指针 ptr 反推出宿主结构体指针（type 为宿主结构体类型，member 为节点成员名） */
#define RLIST_CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - (unsigned long)&((type *)0)->member))

/** 初始化链表头 */
void rlist_init(rlist_t *head);

/** 在 node 之后插入 new_node */
void rlist_insert_after(rlist_t *node, rlist_t *new_node);

/** 在 node 之前插入 new_node */
void rlist_insert_before(rlist_t *node, rlist_t *new_node);

/** 头插（head 为链表头） */
void rlist_push_head(rlist_t *head, rlist_t *node);

/** 尾插（head 为链表头） */
void rlist_push_tail(rlist_t *head, rlist_t *node);

/** 从链表中摘除 node，返回其自身指针；调用方保证 node 在链表中 */
rlist_t *rlist_remove(rlist_t *node);

/** 从链表头取出一节点（摘除并返回），链表为空时返回 NULL */
rlist_t *rlist_pop_head(rlist_t *head);

/** 从链表尾取出一节点（摘除并返回），链表为空时返回 NULL */
rlist_t *rlist_pop_tail(rlist_t *head);

/** 链表是否为空 */
int rlist_is_empty(const rlist_t *head);

/** 返回链表长度（O(n)） */
unsigned int rlist_len(const rlist_t *head);

/**
 * 遍历链表：pos 为 rlist_t* 游标，head 为链表头。
 * 遍历过程中允许摘除 pos 所在节点（先取 next 再使用 pos）。
 */
#define RLIST_FOR_EACH(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

/** 遍历链表，pos 为 rlist_t* 游标，n 为 rlist_t* 临时变量（可安全摘除 pos） */
#define RLIST_FOR_EACH_SAFE(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
         pos = n, n = pos->next)

/**
 * 遍历并取宿主结构体：pos 为宿主结构体指针（type*），head 为链表头，
 * member 为节点成员名。
 */
#define RLIST_FOR_EACH_ENTRY(pos, head, type, member) \
    for (pos = RLIST_CONTAINER_OF((head)->next, type, member); \
         &pos->member != (head); \
         pos = RLIST_CONTAINER_OF(pos->member.next, type, member))

#ifdef __cplusplus
}
#endif

#endif /* __RLIST_H__ */

#include <string.h>
#include "stm32f10x.h"
#include "task.h"

/*
 * 任务管理：add（入就绪表）/ delete（出表）/ delay（pend 到延时队列）。
 * 就绪表 __task_list 和当前任务指针定义在 time_slice_core.c，
 * 调度决策（挑下一个任务）也归 core，本文件只负责维护任务的"在哪个表里"。
 */

/* 延时队列：睡着等节拍的任务挂在上面，由 task_delay_scan 每拍扫描 */
static rlist_t __delay_list;

void task_init(void)
{
    rlist_init(&__delay_list);
}

int task_add(task_block_t *tcb)
{
    if (tcb == NULL)
    {
        return -1;
    }

    rlist_insert_after(&__task_list, &tcb->task_node);
    memset(tcb->buf, 0, tcb->buf_size);

    return 0;
}

/**
 * 删除任务。无论它在就绪表还是延时队列里，一个 rlist_remove 都能摘掉
 * （侵入式链表的节点同一时刻只会在一个表里）。
 *
 * 删除"正在运行的任务"也是安全的：把 __current_task 置空后，
 * 下一次 PendSV 切换时 core 会像"第一次切换"一样直接取就绪表第一个任务，
 * 被删任务的栈不会再被恢复。
 */
int task_delete(task_block_t *tcb)
{
    if (tcb == NULL)
    {
        return -1;
    }

    rlist_remove(&tcb->task_node);

    if (tcb == __current_task)
    {
        __current_task = NULL;
    }

    return 0;
}


#include "stm32f10x.h"
#include "stm32f10x_it.h"
#include <stddef.h>
#include "time_slice_core.h"
#include "task.h"
#include "port.h"


task_block_t *__current_task = NULL;     /* 当前正在运行的任务 */

/**
 * 从任务链表中取出下一个要运行的任务（轮转调度）
 * @cur  当前正在运行的任务；传 NULL 表示调度刚开始，取第一个任务
 * @return 下一个任务的 TCB；链表为空时返回 NULL
 */
task_block_t *time_slice_get_next(task_block_t *cur)
{
    rlist_t *node;

    if (rlist_is_empty(&__task_list))
    {
        return NULL;
    }

    if (cur == NULL)
    {
        node = __task_list.next;            /* 还没有任务在跑 → 取第一个任务 */
    }
    else
    {
        node = cur->task_node.next;         /* 从当前任务的下一个开始轮转 */
    }

    /*
     * 两种情况都"重新从第一个就绪任务开始"：
     *   node == &__task_list   ：cur 是就绪表里最后一个任务，环形绕回；
     *   node == &cur->task_node：cur 已经把自己摘出了就绪表（task_delay/
     *                            task_delete 之后 rlist_remove 会把节点
     *                            的 next 指回自己），不能再从 cur 找起。
     */
    if (node == &__task_list || node == &cur->task_node)
    {
        node = __task_list.next;            /* 从第一个任务开始 */
    }

    return RLIST_CONTAINER_OF(node, task_block_t, task_node);
}


/**
 * 启动systick作为系统心跳
 */
int time_slice_core_init(void)
{
    port_hardware_init(1);
    rlist_init(&__task_list);
    task_init();                            /* 延时队列归 task.c 管，这里统一完成模块初始化 */

    return 0;
}


/**
 * 启动调度：配置并打开 SysTick 心跳。
 * 心跳第一次溢出 → SysTick_Handler → 触发 PendSV → 切进第一个任务，
 * 从此 main() 永远不会再被运行（它已被切出且不在任务链表里）。
 */
int time_slice_start_schedule(void)
{
    if (rlist_is_empty(&__task_list))
    {
        return -1;                          /* 一个任务都没有，开心跳也没意义 */
    }  

    start_first_task(&__task_list);         /* 立刻切进第一个任务，本函数不再返回 */

    return 0;                               /* 不会执行到，只为消编译警告 */
}



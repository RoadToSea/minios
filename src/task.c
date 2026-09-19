#include <string.h>
#include "task.h"
#include "minios_config.h"

/* 当前正在运行的任务 */
task_block_t *__current_task = NULL;
/* 延时队列：睡着等节拍的任务挂在上面，由 task_delay_scan 每拍扫描 */
static rlist_t __delay_list;
static volatile uint32_t __tick_count;

static inline task_block_t* __get_cur_task(void)
{
    return __current_task;
}

static inline void __set_cur_task(task_block_t *cur)
{
    __current_task = cur;
}

void task_init(void)
{
    rlist_init(&__delay_list);
    __tick_count = 0;
}

int task_add(task_block_t *tcb)
{
    if (tcb == NULL)
    {
        return -1;
    }

    rlist_init(&tcb->task_node);
    rlist_init(&tcb->delay_node);
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
    uint32_t primask;

    if (tcb == NULL)
    {
        return -1;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    rlist_remove(&tcb->task_node);

    if (tcb->delay_node.next != &tcb->delay_node)
    {
        rlist_remove(&tcb->delay_node);
    }

    if (tcb == __get_cur_task())
    {
        __set_cur_task(NULL);
    }

    __set_PRIMASK(primask);

    return 0;
}

/**
 * @brief   任务延时函数
 *
 * @param   延时时间ms
 * @return
 */
int task_delay(uint32_t ms)
{
    uint32_t primask;
    task_block_t *tcb = __get_cur_task();
    if (tcb == NULL || ms == 0)
    {
        return -1;
    }

    /* 进入临界区,为什么不使用互斥锁是因为__dealy_list是共享资源,可能会被task_delay_scan在Systick中断中调用,在中断中调用互斥锁会在中断中等待导致系统死锁 */
    primask = __get_PRIMASK();
    __disable_irq();

    /* 当前 SysTick 为 1ms 一拍，因此 ms 直接作为节拍数使用。 */
    tcb->delay_expire = __tick_count + MS_TO_TICK(ms);

    /* 任务从就绪表移出，但保留 task_node 自环，供调度器识别。 */
    rlist_remove(&tcb->task_node);
    rlist_push_tail(&__delay_list, &tcb->delay_node);

    /* 让当前任务尽快切出；PendSV 中仍可保存它的栈指针。 */
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;

    __set_PRIMASK(primask);

    return 0;
}

void task_delay_scan(void)
{
    rlist_t *pos;
    rlist_t *next;

    ++__tick_count;

    RLIST_FOR_EACH_SAFE(pos, next, &__delay_list)
    {
        task_block_t *tcb = RLIST_CONTAINER_OF(pos, task_block_t, delay_node);

        /* 有符号差值比较可以正确处理 uint32_t 节拍回绕。 */
        if ((int32_t)(__tick_count - tcb->delay_expire) >= 0)
        {
            rlist_remove(&tcb->delay_node);
            rlist_push_tail(&__task_list, &tcb->task_node);
        }
    }
}

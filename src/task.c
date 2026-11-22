#include <string.h>
#include "task.h"
#include "port.h"
#include "minios_config.h"
#include "minios.h"

/* 当前正在运行的任务 */
task_block_t *__current_task = NULL;
/* 延时队列：睡着等节拍的任务挂在上面，由 task_delay_scan 每拍扫描 */
static rlist_t __delay_list;
/* 就绪队列：存放各优先级的就绪任务（运行态任务亦保留于所属队列队首） */
static rlist_t __ready_list[MINIOS_PRIORITY_MAX_NUM];
/* 优先级就绪位图：bit N 为 1 表示优先级 N 的链表非空 */
static uint32_t __ready_priority_mask = 0U;

static uint32_t __tick_count = 0U;

static inline task_block_t *__get_cur_task(void)
{
    return __current_task;
}

static inline void __set_cur_task(task_block_t *cur)
{
    __current_task = cur;
}

static inline int __tick_reached(uint32_t now, uint32_t expire)
{
    return (int32_t)(now - expire) >= 0;
}

static inline uint32_t __get_highest_priority(uint32_t mask)
{
    if (mask == 0U)
    {
        /* 空闲任务永远常驻，mask 理论上绝不可能为 0 */
        while (1)
        {
            __BKPT(0);
        }
    }
    return port_ctz(mask);
}

static inline void __ready_enqueue(task_block_t *tcb)
{
    rlist_push_tail(&__ready_list[tcb->priority], &tcb->task_node);
    __ready_priority_mask |= (1U << tcb->priority);
}

static inline void __ready_dequeue(task_block_t *tcb)
{
    rlist_remove(&tcb->task_node);
    if (rlist_is_empty(&__ready_list[tcb->priority]))
    {
        __ready_priority_mask &= ~(1U << tcb->priority);
    }
}

uint32_t minios_get_tick(void)
{
    return __tick_count;
}

void task_init(void)
{
    __current_task = NULL;
    __ready_priority_mask = 0U;
    __tick_count = 0U;
    rlist_init(&__delay_list);
    for (uint32_t i = 0U; i < MINIOS_PRIORITY_MAX_NUM; i++)
    {
        rlist_init(&__ready_list[i]);
    }
}

int task_has_ready(void)
{
    return __ready_priority_mask != 0U;
}

minios_err_t task_add(task_block_t *tcb)
{
    uint32_t primask;
    uintptr_t top;

    if (tcb == NULL || tcb->func == NULL || tcb->buf == NULL ||
        tcb->buf_size < 64U || tcb->priority >= MINIOS_PRIORITY_MAX_NUM)
    {
        return MINIOS_EINVAL;
    }

    top = ((uintptr_t)tcb->buf + tcb->buf_size) & ~(uintptr_t)0x7U;
    if (top <= (uintptr_t)tcb->buf || (top - (uintptr_t)tcb->buf) < 64U)
    {
        return MINIOS_EINVAL;
    }

    if (tcb->task_node.next != NULL && tcb->task_node.next != &tcb->task_node)
    {
        return MINIOS_EBUSY;
    }

    rlist_init(&tcb->task_node);
    memset(tcb->buf, 0, tcb->buf_size);
    task_build_first_frame(tcb);

    primask = __get_PRIMASK();
    __disable_irq();

    tcb->state = TASK_STATE_READY;
    __ready_enqueue(tcb);
    if (__current_task != NULL && tcb->priority < __current_task->priority)
    {
        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    }

    __set_PRIMASK(primask);
    __DSB();
    __ISB();
    return MINIOS_EOK;
}

static int __task_detach(task_block_t *tcb)
{
    if (tcb->state == TASK_STATE_READY || tcb->state == TASK_STATE_RUNNING)
    {
        __ready_dequeue(tcb);
        return 0;
    }
    if (tcb->state == TASK_STATE_DELAY)
    {
        rlist_remove(&tcb->task_node);
        return 0;
    }
    return -1;
}

minios_err_t task_delete(task_block_t *tcb)
{
    uint32_t primask;

    if (tcb == NULL)
    {
        return MINIOS_EINVAL;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    if (tcb->state == TASK_STATE_DELETED ||
        tcb->task_node.next == NULL || tcb->task_node.prev == NULL ||
        __task_detach(tcb) != 0)
    {
        __set_PRIMASK(primask);
        return MINIOS_EINVAL;
    }

    tcb->state = TASK_STATE_DELETED;

    if (tcb == __get_cur_task())
    {
        __set_cur_task(NULL);
        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
        __set_PRIMASK(primask);

        /* 仅在线程模式下阻断自杀任务继续执行；中断内调用须立即返回以便 PendSV 接管 */
        if (__get_IPSR() == 0U)
        {
            while (1)
            {
                __WFI();
            }
        }
        return MINIOS_EOK;
    }

    __set_PRIMASK(primask);
    return MINIOS_EOK;
}

static rlist_t *__find_pre_expire_node(uint32_t tick)
{
    rlist_t *node = NULL;
    task_block_t *tcb = NULL;

    RLIST_FOR_EACH(node, &__delay_list)
    {
        tcb = RLIST_CONTAINER_OF(node, task_block_t, task_node);
        if ((int32_t)(tcb->delay_expire - tick) > 0)
        {
            return node->prev;
        }
    }

    return __delay_list.prev;
}

minios_err_t task_delay(uint32_t ms)
{
    uint32_t primask;
    task_block_t *tcb;
    rlist_t *pre_expired_node;

    if (__get_IPSR() != 0U)
    {
        return MINIOS_EISR;
    }

    /* 单次延时不得超过 0x7FFFFFFF 个 Tick，否则有符号差值比较将发生回绕误判 */
    if (ms == 0U || MS_TO_TICK(ms) > 0x7FFFFFFFU)
    {
        return MINIOS_EINVAL;
    }

    tcb = __get_cur_task();
    if (tcb == NULL || tcb->state != TASK_STATE_RUNNING)
    {
        return MINIOS_ERROR;
    }

    /* 临界区保护：__delay_list 与 __ready_list 会被 SysTick 中断中的 task_delay_scan 访问 */
    primask = __get_PRIMASK();
    __disable_irq();

    __ready_dequeue(tcb);
    tcb->delay_expire = __tick_count + MS_TO_TICK(ms);
    tcb->state = TASK_STATE_DELAY;
    pre_expired_node = __find_pre_expire_node(tcb->delay_expire);
    rlist_insert_after(pre_expired_node, &tcb->task_node);
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;

    __set_PRIMASK(primask);
    __DSB();
    __ISB();
    return MINIOS_EOK;
}

static void __delay_wake_expired(void)
{
    rlist_t *first_expired;
    task_block_t *tcb;

    while (!rlist_is_empty(&__delay_list))
    {
        first_expired = RLIST_PEEK_FIRST(&__delay_list);
        tcb = RLIST_CONTAINER_OF(first_expired, task_block_t, task_node);

        if (!__tick_reached(__tick_count, tcb->delay_expire))
        {
            /* 延时队列按到期时间升序排列，队首未到期则后续节点必未到期 */
            break;
        }

        rlist_remove(&tcb->task_node);
        tcb->state = TASK_STATE_READY;
        __ready_enqueue(tcb);
    }
}

int tick_inc_handler(void)
{
    uint32_t primask;
    uint32_t priority;
    rlist_t *head;
    task_block_t *highest;
    int ret = MINIOS_EOK;

    primask = __get_PRIMASK();
    __disable_irq();

    __tick_count++;
    __delay_wake_expired();

    priority = __get_highest_priority(__ready_priority_mask);
    head = &__ready_list[priority];

    /* 最高优先级存在多任务轮转，或队首任务非当前运行任务时触发切换 */
    if (head->next != head->prev)
    {
        ret = MINIOS_ETASK_TOG;
    }
    else
    {
        highest = RLIST_CONTAINER_OF(head->next, task_block_t, task_node);
        if (__current_task == NULL || highest != __current_task)
        {
            ret = MINIOS_ETASK_TOG;
        }
    }

    __set_PRIMASK(primask);
    return ret;
}

/*
 * 不能加 static：仅由 PendSV_Handler 内联汇编引用，加 static 会被编译器优化删除。
 */
uint32_t minios_task_switch(uint32_t sp)
{
    uint32_t primask;
    task_block_t *next;
    uint32_t task_priority;

    /* PendSV 优先级低于 SysTick，须关中断避免切出期间被 SysTick 并发修改就绪表与位图 */
    primask = __get_PRIMASK();
    __disable_irq();

    if (__current_task != NULL)
    {
        __current_task->stack_ptr = (void *)sp;

        /* 仅当任务仍为运行态时才轮转至同优先级队尾；已主动休眠的任务位于延时队列中 */
        if (__current_task->state == TASK_STATE_RUNNING)
        {
            rlist_t *cur_head = &__ready_list[__current_task->priority];
            __current_task->state = TASK_STATE_READY;
            if (cur_head->next != cur_head->prev)
            {
                rlist_remove(&__current_task->task_node);
                rlist_push_tail(cur_head, &__current_task->task_node);
            }
        }
    }

    task_priority = __get_highest_priority(__ready_priority_mask);
    next = RLIST_PEEK_FIRST_ENTRY(&__ready_list[task_priority],
                                  task_block_t, task_node);

    __current_task = next;
    __current_task->state = TASK_STATE_RUNNING;

    __set_PRIMASK(primask);
    return (uint32_t)next->stack_ptr;
}

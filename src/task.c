#include <string.h>
#include "task.h"
#include "minios_config.h"


/* 当前正在运行的任务 */
task_block_t *__current_task = NULL;
/* 延时队列：睡着等节拍的任务挂在上面，由 task_delay_scan 每拍扫描 */
rlist_t __delay_list;
rlist_t __ready_list = {0};

static uint32_t __tick_count = 0;

static inline task_block_t* __get_cur_task(void)
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

    rlist_init(&tcb->task_node);
    rlist_push_tail(&__ready_list, &tcb->task_node);    /* 尾插：先添加的先运行（FIFO） */
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

    if (tcb->task_node.next != &tcb->task_node)
    {
        rlist_remove(&tcb->task_node);
    }

    if (tcb == __get_cur_task())
    {
        __set_cur_task(NULL);
    }

    __set_PRIMASK(primask);

    return 0;
}

static rlist_t* __find_pre_expire_node(uint32_t tick)
{
    rlist_t *node = NULL;
    task_block_t *tcb = NULL;
    RLIST_FOR_EACH(node,&__delay_list){
        tcb = RLIST_CONTAINER_OF(node,task_block_t,task_node);
        if(tcb->delay_expire>tick){
            break;
        }
    }
    return node;
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
    rlist_t *pre_expired_node = NULL;
    if (tcb == NULL || ms == 0)
    {
        return -1;
    }

    /* 进入临界区,为什么不使用互斥锁是因为__dealy_list是共享资源,可能会被task_delay_scan在Systick中断中调用,在中断中调用互斥锁会在中断中等待导致系统死锁 */
    primask = __get_PRIMASK();
    __disable_irq();

    /* 当前 SysTick 为 1ms 一拍，因此 ms 直接作为节拍数使用。 */
    tcb->delay_expire =  __tick_count +MS_TO_TICK(ms);
    if (tcb->state == TASK_STATE_DELAYED)
    {
        /* 已在延时队列（任务被"复活"过）：摘下来按新到期时间重排，防止节点重复插入成环 */
        rlist_remove(&tcb->task_node);
    }
    tcb->state = TASK_STATE_DELAYED;    /* 标记延时态：PendSV 切出时据此不放回就绪表 */
    pre_expired_node = __find_pre_expire_node(tcb->delay_expire);
    if(pre_expired_node){
        /* 不需要自己从 ready_list 摘除：被调度进去时已被 pop_head 摘出 */
        rlist_insert_after(pre_expired_node,&tcb->task_node);
    }

    /* 让当前任务尽快切出；PendSV 中仍可保存它的栈指针。 */
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;

    __set_PRIMASK(primask);

    return 0;
}

void task_delay_scan(void)
{
    rlist_t *first_expired;
    task_block_t *tcb;

    __tick_count++;
    /* task has expired */
    while(!rlist_is_empty(&__delay_list)){
        first_expired = RLIST_PEEK_FIRST(&__delay_list);
        tcb = RLIST_CONTAINER_OF(first_expired,task_block_t,task_node);

        if (!__tick_reached(__tick_count, tcb->delay_expire))
        {
            break;          /* 队首未到期（列表按到期时间有序），后面更不会到 */
        }

        rlist_remove(&tcb->task_node);
        tcb->state = TASK_STATE_READY;      /* 恢复"可被放回就绪表尾部"的身份 */
        rlist_push_tail(&__ready_list, &tcb->task_node);
    }
}

#include <string.h>
#include "task.h"
#include "port.h"
#include "minios_config.h"


/* 当前正在运行的任务 */
task_block_t *__current_task = NULL;
/* 延时队列：睡着等节拍的任务挂在上面，由 task_delay_scan 每拍扫描 */
rlist_t __delay_list;
rlist_t __ready_list;

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
    rlist_init(&__ready_list);
}

int task_add(task_block_t *tcb)
{
    if (tcb == NULL)
    {
        return -1;
    }

    rlist_init(&tcb->task_node);

    rlist_push_tail(&__ready_list, &tcb->task_node);    /* 尾插：先添加的先运行（FIFO） */
    tcb->state = TASK_STATE_READY;

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

    if (tcb->task_node.next != NULL && tcb->task_node.prev != NULL)
    {
        rlist_remove(&tcb->task_node);
    }
    tcb->state = TASK_STATE_DELETED;

    /* 如果删除的任务是当前正在运行的任务，需要触发任务中断 */
    if (tcb == __get_cur_task())
    {
        __set_cur_task(NULL);
        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
        __set_PRIMASK(primask);

        /* 阻断执行流：绝对不允许自杀任务继续向下返回或执行用户代码 */
        while (1)
        {
            __WFI();
        }
    }

    __set_PRIMASK(primask);
    return 0;
}

static rlist_t* __find_pre_expire_node(uint32_t tick)
{
    rlist_t *node =NULL;
    task_block_t *tcb = NULL;
    RLIST_FOR_EACH(node,&__delay_list){
        tcb = RLIST_CONTAINER_OF(node,task_block_t,task_node);
        /* last node delay tick is less than insert, current node tick is bigger than insert*/
        if ((int32_t)(tcb->delay_expire - tick) > 0){
            return node->prev;
        }
    }

    /* the whole delay list is less than insert, return list tail (empty -> head itself) */
    return __delay_list.prev;
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

    /* 单次延时不得超过 0x7FFFFFFF 个 Tick, 否则会产生溢出 */
    if(ms == 0 || MS_TO_TICK(ms)>  0x7FFFFFFFU){
        return -1;
    }

    if (tcb == NULL || tcb->state != TASK_STATE_RUNNING)
    {
        while (1) { __BKPT(0); }
    }

    /* 进入临界区,为什么不使用互斥锁是因为__dealy_list是共享资源,可能会被task_delay_scan在Systick中断中调用,在中断中调用互斥锁会在中断中等待导致系统死锁 */
    primask = __get_PRIMASK();
    __disable_irq();

    tcb->delay_expire =  __tick_count +MS_TO_TICK(ms);
    tcb->state = TASK_STATE_DELAY;    /* 标记延时态：PendSV 切出时据此不放回就绪表 */
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



/**
 * PendSV_Handler 的汇编会调用本函数完成"存旧任务、挑新任务"。
 * 注意：不能加 static——它只被内联汇编字符串引用，加 static 会被
 * 编译器当作"没人用"而优化删除，导致链接错误。
 *
 * @sp   进入 PendSV 时保存完 r4-r11 之后的任务栈顶（指向 r4 的槽位）
 * @return 下一个任务的同位置栈顶（汇编拿它去恢复 r4-r11）
 */
uint32_t minios_task_switch(uint32_t sp)
{
    task_block_t *next;

    if (__current_task != NULL)
    {
        __current_task->stack_ptr = (void *)sp;     /* 保存切出任务的栈顶 */

        /*
         * pop_head 调度下，任务被调度进去时就已摘出就绪表，切出时必须
         * 放回队尾，否则就绪表一轮转就空了。主动 task_delay 的任务已把
         * 自己挂到延时队列（state == DELAYED），不能再放回就绪表。
         */
        if (__current_task->state == TASK_STATE_RUNNING)
        {
            __current_task->state = TASK_STATE_READY;
            rlist_push_tail(&__ready_list, &__current_task->task_node);
        }
    }

    /*
     * 必须先判空再 pop：pop_head 对空表返回 NULL，而
     * RLIST_CONTAINER_OF(NULL,...) 得到 0-4=0xFFFFFFFC，不是 NULL，
     * 后面无法用 next==NULL 拦住，会拿着垃圾栈顶去恢复 → HardFault。
     */
    if (rlist_is_empty(&__ready_list))
    {
        return sp;                                  /* 无任务可切：原样返回，当前任务继续跑 */
    }

    next = RLIST_CONTAINER_OF(rlist_pop_head(&__ready_list),
                              task_block_t, task_node);

    __current_task = next;                          /* 更新"当前任务"标记 */

    __current_task->state = TASK_STATE_RUNNING;

    return (uint32_t)next->stack_ptr;               /* 返回下一个任务的栈顶 */
}

#include <string.h>
#include "stm32f10x.h"
#include "stm32f10x_it.h"
#include "time_slice_core.h"

rlist_t __task_list = {0};

static task_block_t *__current_task = NULL;     /* 当前正在运行的任务 */

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

    if (node == &__task_list)               /* cur 是链表里最后一个任务 */
    {
        node = __task_list.next;            /* 环形链表绕回，重新从第一个任务开始 */
    }

    return RLIST_CONTAINER_OF(node, task_block_t, task_node);
}

/**
 * PendSV_Handler 的汇编会调用本函数完成"存旧任务、挑新任务"。
 * 注意：不能加 static——它只被内联汇编字符串引用，加 static 会被
 * 编译器当作"没人用"而优化删除，导致链接错误。
 *
 * @sp   进入 PendSV 时保存完 r4-r11 之后的任务栈顶（指向 r4 的槽位）
 * @return 下一个任务的同位置栈顶（汇编拿它去恢复 r4-r11）
 */
uint32_t time_slice_switch(uint32_t sp)
{
    task_block_t *next;

    if (__current_task != NULL)
    {
        __current_task->stack_ptr = (void *)sp;     /* 保存切出任务的栈顶 */
    }

    next = time_slice_get_next(__current_task);     /* 轮转挑下一个任务 */

    if (next == NULL)
    {
        return sp;                                  /* 兜底：无任务则原样返回 */
    }

    __current_task = next;                          /* 更新"当前任务"标记 */

    return (uint32_t)next->stack_ptr;               /* 返回下一个任务的栈顶 */
}

/**
 * 启动systick作为系统心跳
 */
int time_slice_core_init(void)
{
    RCC_SYSCLKConfig(RCC_SYSCLKSource_HSE);
    NVIC_SetPriority(SysTick_IRQn,2);
    NVIC_SetPriority(PendSV_IRQn, 0xFF);    /* PendSV 设为最低优先级：切换不能打断别的中断 */
    

    rlist_init(&__task_list);

    return 0;
}



/**
 * 在任务栈顶构造"初始假帧"。
 *
 * 为什么需要假帧：PendSV 切进一个任务的唯一方式是"异常返回"——硬件从
 * 任务栈上弹出 16 个字当作寄存器恢复。一个从没运行过的任务栈上是空的，
 * 所以必须预先摆好一整帧，让第一次切换看起来就像"这个任务刚被切出去过"。
 */
static void task_build_first_frame(task_block_t * tcb)
{
    uint32_t *sp;

    sp = (uint32_t *)((uint32_t)tcb->buf + tcb->buf_size);  /* 栈从高地址往低长 */
    sp = (uint32_t *)((uint32_t)sp & ~0x7u);                /* 8 字节对齐（AAPCS 要求） */

    /* ---- 硬件自动保存区（异常返回时硬件弹走的 8 个字）---- */
    *--sp = 0x01000000u;                    /* xPSR：只有 T 位(bit24)=1，非 Thumb 会 HardFault */
    *--sp = (uint32_t)tcb->func;            /* PC  ："返回"即跳进任务函数，这是假帧的灵魂 */
    *--sp = 0xFFFFFFFDu;                    /* LR  ：任务函数若 return，会触发异常返回（防呆，约定任务不许 return） */
    *--sp = 0;                              /* R12 */
    *--sp = 0;                              /* R3  */
    *--sp = 0;                              /* R2  */
    *--sp = 0;                              /* R1  */
    *--sp = (uint32_t)tcb->func_arg;        /* R0  ：任务函数的第一个参数，C 传参走 R0 */

    /* ---- 手动保存区（PendSV 汇编 ldmia 弹出的 8 个字）---- */
    *--sp = 0;                              /* R11 */
    *--sp = 0;                              /* R10 */
    *--sp = 0;                              /* R9  */
    *--sp = 0;                              /* R8  */
    *--sp = 0;                              /* R7  */
    *--sp = 0;                              /* R6  */
    *--sp = 0;                              /* R5  */
    *--sp = 0;                              /* R4  */

    tcb->stack_ptr = sp;                    /* 指向 R4 槽位，与 PendSV 恢复流程的约定一致 */
}

/**
 * 切换到第一个任务：给链表里所有任务建好假帧后，立刻触发一次 PendSV。
 * 本函数被调用的瞬间 CPU 就跳进第一个任务，永远不会返回。
 */
void time_slice_switch_first_task(void)
{
    task_block_t *pos;

    /*
     * 第一次进 PendSV 时 main 还跑在 MSP 上，PSP 里是个无效值（复位后
     * 未初始化）。而 PendSV 一进来就 stmdb 往 PSP 指向的栈里压 r4-r11，
     * PSP 无效 → 总线错误 → HardFault。
     * 所以先给 PSP 指一块临时的合法内存，让第一次"保存"有的放矢——
     * 保存的内容反正没人要（切进任务后就作废）。
     */
    static uint32_t first_psp_scratch[16];  /* 只需 8 个字，翻倍留余量 */
    __set_PSP((uint32_t)(first_psp_scratch + 16));

    RLIST_FOR_EACH_ENTRY(pos, &__task_list, task_block_t, task_node)
    {
        task_build_first_frame(pos);
    }

    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;     /* 置 PendSV 挂起 → 线程模式立即被抢占，开始第一次切换 */
}


int time_slice_add_task(task_block_t * tcb)
{
    if(tcb == NULL){
        return -1;
    }

    rlist_insert_after(&__task_list,&tcb->task_node);
    memset(tcb->buf,0,tcb->buf_size);

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

    SysTick_Config(SystemCoreClock / 1000); /* 1ms 心跳：每 72000 个时钟周期中断一次 */
    // 开启调度
    NVIC_EnableIRQ(SysTick_IRQn);

    time_slice_switch_first_task();         /* 立刻切进第一个任务，本函数不再返回 */

    return 0;                               /* 不会执行到，只为消编译警告 */
}

void SysTick_Handler(void)
{
    __asm volatile
    (
        "ldr r0, =0xE000ED04 \n"    /* ICSR 寄存器地址（中断控制与状态寄存器） */
        "ldr r1, =0x10000000 \n"    /* PENDSVSET 位（bit28）：置 1 请求挂起 PendSV */
        "str r1, [r0]        \n"    /* 写入 ICSR，触发 PendSV 挂起 */
    );
}

/*
 * naked：告诉编译器不要生成任何函数开头/结尾的压栈出栈代码。
 * 本函数的"栈"完全由下面的汇编手工管理，编译器若插手必然破坏切换。
 */
__attribute__((naked)) void PendSV_Handler(void)
{
    __asm volatile
    (
        /* ================= 保存当前任务 ================= */
        "mrs    r0, psp             \n"    /* 读出当前任务的 PSP（硬件帧顶） */
        "stmdb  r0!, {r4-r11}       \n"    /* 手动压 r4-r11（满递减），r0 落到 r4 槽位 */

        /* ================= 交给 C 调度 ===== ============ */
        "bl     time_slice_switch   \n"    /* 传入 r0=旧栈顶，返回 r0=新栈顶
                                              安全性：r0-r3/r12 已被硬件自动压栈，
                                              C 函数随便用这几个寄存器 */

        /* ================= 恢复下一个任务 ================= */
        "ldmia  r0!, {r4-r11}       \n"    /* 弹出 r4-r11，r0 推进到硬件帧首 */
        "msr    psp, r0             \n"    /* PSP 指向新任务的硬件帧 */
        "ldr    r3, =0xFFFFFFFD     \n"    /* EXC_RETURN：回线程模式、用 PSP */
        "bx     r3                  \n"    /* 异常返回，硬件自动恢复 r0-r3,r12,lr,pc,xPSR */
    );
}
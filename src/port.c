#include "minios_config.h"
#include "port.h"
#include "task.h"
#include <stddef.h>


static uint32_t tick_reload = 0;    /* SysTick 重装值：init 时只存，start 时才启动*/

void port_hardware_init(uint32_t beat_ms)
{
    // RCC_SYSCLKConfig(RCC_SYSCLKSource_HSE);
    NVIC_SetPriority(SysTick_IRQn,2);
    NVIC_SetPriority(PendSV_IRQn, 0xFF);    /* PendSV 设为最低优先级：切换不能打断别的中断 */

    /* 每拍计数值：start_first_task 里以 tick_reload-1 写入 SysTick->LOAD */
    tick_reload = SYSTICK_CLOCK_HZ / SYSTICK_RATE_HZ;

    /*
     * 此刻 PSP 还没指到合法内存，心跳一旦触发 PendSV，handler 会对
     * 无效 PSP 执行 stmdb 压栈 → HardFault。所以这里只装重装值，
     * SysTick 留到 start_first_task 里假帧全部建好之后再启动。
     */
    SysTick->CTRL = 0;                      /* 关闭Systick运行 */
    SysTick->LOAD = tick_reload;
    SysTick->VAL  = 0;
}

/**
 * 在任务栈顶构造"初始假帧"。
 *
 * 为什么需要假帧：PendSV 切进一个任务的唯一方式是"异常返回"——硬件从
 * 任务栈上弹出 16 个字当作寄存器恢复。一个从没运行过的任务栈上是空的，
 * 所以必须预先摆好一整帧，让第一次切换看起来就像"这个任务刚被切出去过"。
 */
void task_build_first_frame(task_block_t * tcb)
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

void start_first_task(rlist_t *task_head)
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

    RLIST_FOR_EACH_ENTRY(pos, task_head, task_block_t, task_node)
    {
        task_build_first_frame(pos);
    }

    /*
     * 假帧全部就绪、PSP scratch 已指到合法内存，现在启动心跳才安全。
     * 从这一刻起第一个 SysTick 中断会挂起 PendSV，触发第一次切换。
     */
    SysTick->LOAD = tick_reload - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk     /* 内核时钟 */
                  | SysTick_CTRL_TICKINT_Msk       /* 计数到 0 触发中断 */
                  | SysTick_CTRL_ENABLE_Msk;       /* 开火 */

    // NVIC_EnableIRQ(SysTick_IRQn);           /* 开启Systick中断*/
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;     /* 置 PendSV 挂起 → 线程模式立即被抢占，开始第一次切换 */
}


void SysTick_Handler(void)
{
    task_delay_scan();                      /* 扫延时队列，到期任务移回就绪表 */

    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;     /* 置 PendSV 挂起，请求一次上下文切换 */
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
        "bl     minios_task_switch  \n"    /* 传入 r0=旧栈顶，返回 r0=新栈顶
                                              安全性：r0-r3/r12 已被硬件自动压栈，
                                              C 函数随便用这几个寄存器 */

        /* ================= 恢复下一个任务 ================= */
        "ldmia  r0!, {r4-r11}       \n"    /* 弹出 r4-r11，r0 推进到硬件帧首 */
        "msr    psp, r0             \n"    /* PSP 指向新任务的硬件帧 */
        "ldr    r3, =0xFFFFFFFD     \n"    /* EXC_RETURN：回线程模式、用 PSP */
        "bx     r3                  \n"    /* 异常返回，硬件自动恢复 r0-r3,r12,lr,pc,xPSR */
    );
}


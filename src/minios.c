#include <stddef.h>
#include "stm32f10x.h"
#include "stm32f10x_it.h"
#include "minios.h"
#include "minios_config.h"
#include "task.h"
#include "port.h"



/*
 * 空闲任务：就绪表里永远保留的最后一张牌。
 * 有了它，就绪表不会为空，minios_task_switch 的 pop_head 永远取得到
 * 合法任务；其他任务全部 task_delay 时 CPU 停在 WFI 上等下一次
 * 心跳，而不是把睡着的任务"复活"来空转。
 */
static uint8_t idle_buf[128];
static void idle_loop(void *arg)
{
    (void)arg;
    while (1)
    {
        __WFI();                                /* 等中断（下一次 SysTick 心跳） */
    }
}
static task_block_t idle_task = {
    .name     = "idle",
    .func     = idle_loop,
    .buf      = idle_buf,
    .buf_size = sizeof(idle_buf),
    .priority = MINIOS_PRIORITY_MAX_NUM-1
};




/**
 * 启动systick作为系统心跳
 */
minios_err_t minios_init(void)
{
    port_hardware_init(1);
    task_init();                            
    return MINIOS_EOK;
}


/**
 * 启动调度：配置并打开 SysTick 心跳。
 * 心跳第一次溢出 → SysTick_Handler → 触发 PendSV → 切进第一个任务，
 * 从此 main() 永远不会再被运行（它已被切出且不在任务链表里）。
 */
minios_err_t minios_start(void)
{
    if (!task_has_ready())
    {
        return MINIOS_ERROR;                /* 一个任务都没有，开心跳也没意义 */
    }

    if (task_add(&idle_task) != MINIOS_EOK) /* 空闲任务置于最低优先级：无用户任务就绪时兜底 */
    {
        return MINIOS_ERROR;
    }
    start_first_task();                     /* 立刻切进最高优先级任务，本函数不再返回 */

    return MINIOS_EOK;                      /* 不会执行到，只为消编译警告 */
}



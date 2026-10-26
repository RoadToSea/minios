/*
 * ************************************************
 *
 *              MiniOS Time-Slicing Demo
 *
 *  CPU: STM32F103C8
 *  PIN: PC13
 *
 *  Stage 4: Demonstrates time-slice round-robin scheduling
 *  driven by SysTick and PendSV between taskA and taskB.
 *
 * ************************************************
 */

#include <stdio.h>
#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "system_stm32f10x.h"
#include "time_slice_core.h"
#include "task.h"
#include "usart_printf.h"

void handleA(void *arg);
void handleB(void *arg);

rlist_t nodeA;
uint8_t buf_A[128] = {0};
task_block_t taskA = {
    .name     = "taskA",
    .func     = handleA,
    .buf      = buf_A,
    .buf_size = 128};

rlist_t nodeB;
uint8_t buf_B[128] = {0};
task_block_t taskB = {
    .name     = "taskB",
    .func     = handleB,
    .buf      = buf_B,
    .buf_size = 128};

static void SystemConfig(void)
{
    SystemInit();

    SysTick->CTRL = 0;
    SCB->ICSR     = SCB_ICSR_PENDSTCLR_Msk;
    SCB->CCR |= SCB_CCR_STKALIGN_Msk;
}

volatile uint32_t cntA = 0;
volatile uint32_t cntB = 0;

void handleA(void *arg)
{
    while (1) {
        cntA++; /* 断点打这里，每次被调度进来就 +1 */
    }
}

void handleB(void *arg)
{
    while (1) {
        cntB++; /* 断点打这里，每次被调度进来就 +1 */
    }
}

int main(void)
{
    SystemConfig();
    USART_Printf_Init(115200); /* printf -> USART1 (PA9-TX/PA10-RX) */
    printf("system start, clock = %lu Hz\r\n", SystemCoreClock);

    /* 初始化轮转任务调度 (PendSV / SysTick, 进入 PSP 任务多栈环境) */
    time_slice_core_init();
    task_add(&taskA);
    task_add(&taskB);
    time_slice_start_schedule(); /* 从这一行之后，CPU 就属于任务了 */

    while (1) {
    }
}

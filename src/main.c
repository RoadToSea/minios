/*
 * ************************************************
 *
 *              STM32 blink + debug probe demo
 *
 *  CPU: STM32F103C8
 *  PIN: PC13
 *
 *  The function-call / interrupt-call observation code
 *  lives in src/debug_probe.c and is exposed through
 *  src/debug_probe.h; the RTOS multi-thread test lives
 *  in src/rtos_test.c. The bare-metal / RTOS switch is
 *  src/probe_config.h (RTOS_TEST_ENABLED).
 *
 *  Add the probe* symbols from debug_probe.h to the
 *  debugger watch view to observe both contexts:
 *    - thread mode (bare metal: MSP / RTOS: PSP)
 *    - handler mode (TIM2 IRQ, always MSP)
 *
 *  How the vectors connect (startup_stm32f10x_md.s):
 *    g_pfnVectors[44] = TIM2_IRQHandler   (16 core + 28 TIM2_IRQn)
 *    g_pfnVectors[11] = SVC_Handler       -> RTX irq_armv7m.S
 *    g_pfnVectors[14] = PendSV_Handler    -> RTX context switch
 *    g_pfnVectors[15] = SysTick_Handler   -> RTX tick / osDelay
 *  This project always compiles irq_armv7m.S, so those three
 *  vectors are bound to the RTX code in both modes; in
 *  bare-metal mode they simply never fire (SysTick is
 *  disabled in SystemConfig and no SVC/PendSV is issued).
 *
 *  Suggested stepping exercise:
 *    1. breakpoint on DebugProbe_RunNormalCall - step over
 *       NVIC_SetPendingIRQ and watch control jump straight to
 *       TIM2_IRQHandler (vector fetch, not a normal call)
 *    2. step through TIM2_IRQHandler into ProbeIrqBody -
 *       compare SP (MSP), IPSR, LR (EXC_RETURN) with the
 *       probeNormalRegs values captured just before
 *    3. single-step the exception return (bx lr with
 *       EXC_RETURN in LR) and see PC land back on the
 *       __NOP() loop in main(), SP restored
 *
 * ************************************************
 */

#include <stdio.h>
#include "stm32f10x.h"
#include "cmsis_os2.h"
#include "stm32f10x_rcc.h"
#include "system_stm32f10x.h"
#include "debug_probe.h"
#include "rtos_test.h"
#include "probe_config.h"
#include "time_slice_core.h"
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

    SysTick_Config(SystemCoreClock / 1000);

    NVIC_DisableIRQ(TIM2_IRQn);
    NVIC_ClearPendingIRQ(TIM2_IRQn);
    NVIC_SetPriority(TIM2_IRQn, 2);
    NVIC_EnableIRQ(TIM2_IRQn);
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

    /* 1. 触发并观测普通模式下的中断 (TIM2 IRQ, MSP 栈环境) */
    DebugProbe_RunNormalCall(0x11223344, 0x55667788);

    /* 2. 初始化轮转任务调度 (PendSV / SysTick, 进入 PSP 任务多栈环境) */
    time_slice_core_init();
    time_slice_add_task(&taskA);
    time_slice_add_task(&taskB);
    time_slice_start_schedule(); /* 从这一行之后，CPU 就属于任务了 */

    while (1) {
    }
}

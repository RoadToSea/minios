

#include <stdio.h>
#include "stm32f10x.h"
#include "cmsis_os2.h"
#include "stm32f10x_rcc.h"
#include "system_stm32f10x.h"
#include "minios.h"
#include "task.h"
#include "usart_printf.h"

void handleA(void *arg);
void handleB(void *arg);

#define TASKA_BUFSIZE 1024
uint8_t buf_A[TASKA_BUFSIZE] = {0};
task_block_t taskA = {
    .name     = "taskA",
    .priority = 1U,
    .func     = handleA,
    .buf      = buf_A,
    .buf_size = TASKA_BUFSIZE};

#define TASKB_BUFSIZE 1024
uint8_t buf_B[TASKB_BUFSIZE] = {0};
task_block_t taskB = {
    .name     = "taskB",
    .priority = 2U,
    .func     = handleB,
    .buf      = buf_B,
    .buf_size = TASKB_BUFSIZE};

static void SystemConfig(void)
{
    SystemInit();

    SysTick->CTRL = 0;
    SCB->ICSR     = SCB_ICSR_PENDSTCLR_Msk;
    SCB->CCR |= SCB_CCR_STKALIGN_Msk;

    // SysTick_Config(SystemCoreClock / 1000);

    NVIC_DisableIRQ(TIM2_IRQn);
    NVIC_ClearPendingIRQ(TIM2_IRQn);
    NVIC_SetPriority(TIM2_IRQn, 2);
    NVIC_EnableIRQ(TIM2_IRQn);
}

volatile uint32_t cntA = 0;
volatile uint32_t cntB = 0;

void handleA(void *arg)
{
    (void)arg;
    while (1) {
        cntA++; /* 断点打这里，每次被调度进来就 +1 */
        printf("cntA is :%lu\n", (unsigned long)cntA);
        task_delay(500);
    }
}

void handleB(void *arg)
{
    (void)arg;
    while (1) {
        cntB++; /* 断点打这里，每次被调度进来就 +1 */
        printf("cntB is :%lu\n", (unsigned long)cntB);
        task_delay(100); /* 睡 500ms：期间 A 独占 CPU，到期后 B 回到就绪表尾部 */
    }
}

int main(void)
{
    SystemConfig();
    USART_Printf_Init(115200); /* printf -> USART1 (PA9-TX/PA10-RX) */
    printf("system start, clock = %lu Hz\r\n", SystemCoreClock);

    /* 2. 初始化轮转任务调度 (PendSV / SysTick, 进入 PSP 任务多栈环境) */
    minios_init();
    task_add(&taskA);
    task_add(&taskB);
    minios_start(); /* 从这一行之后，CPU 就属于任务了 */

    while (1) {
    }
}

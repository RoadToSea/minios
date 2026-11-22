#include <stdio.h>
#include "stm32f10x.h"
#include "system_stm32f10x.h"
#include "minios_err.h"
#include "usart_printf.h"

#if defined(TEST_CASE_PREEMPT)
extern minios_err_t test_sched_preempt_run(void);
#endif

static void board_init(void)
{
    SystemInit();

    /* 调度器尚未就绪前关闭 SysTick，避免残留配置在进入内核前提前触发异常 */
    SysTick->CTRL = 0U;
    SCB->ICSR     = SCB_ICSR_PENDSTCLR_Msk;
    SCB->CCR     |= SCB_CCR_STKALIGN_Msk;

    USART_Printf_Init(115200U);
}

int main(void)
{
    minios_err_t err;

    board_init();
    printf("[test] runner start, clock = %lu Hz\r\n", (unsigned long)SystemCoreClock);

#if defined(TEST_CASE_PREEMPT)
    err = test_sched_preempt_run();
#else
#error "No test case selected: define TEST_CASE_xxx in tests target defineList"
#endif

    printf("[test] runner exited unexpectedly, err = %d\r\n", (int)err);
    while (1) {
    }
}

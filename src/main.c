#include <stdio.h>

#include "stm32f10x.h"
#include "system_stm32f10x.h"
#include "debug_probe.h"
#include "probe_config.h"
#include "usart_printf.h"

static void SystemConfig(void)
{
    SystemInit();

    SysTick->CTRL = 0;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
    SCB->CCR |= SCB_CCR_STKALIGN_Msk;

    NVIC_DisableIRQ(TIM2_IRQn);
    NVIC_ClearPendingIRQ(TIM2_IRQn);
    NVIC_SetPriority(TIM2_IRQn, 2);
    NVIC_EnableIRQ(TIM2_IRQn);
}

int main(void)
{
    SystemConfig();
    USART_Printf_Init(115200);
    printf("interrupt stage, clock = %lu Hz\r\n", SystemCoreClock);

    probeNormalResult = DebugProbe_RunNormalCall(0x11223344U, 0x55667788U);

    while (1) {
        __NOP();
    }
}

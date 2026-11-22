#ifndef __MINIOS_CONFIG_H
#define __MINIOS_CONFIG_H

#include "stm32f10x.h"
#include <stdint.h>

#define SYSTICK_CLOCK_HZ    (SystemCoreClock)
#define SYSTICK_RATE_HZ     1000U

/* 优先级个数 */
#define MINIOS_PRIORITY_MAX_NUM 16

/* 毫秒转换为内核 tick，向上取整 */
#define MS_TO_TICK(ms)  \
        ((uint32_t)(((uint64_t)(ms) * SYSTICK_RATE_HZ + 999U) / 1000U))

/* SysTick 重装值 */
#define SYSTICK_RELOAD \
        ((uint32_t)(SYSTICK_CLOCK_HZ / SYSTICK_RATE_HZ - 1U))
#endif

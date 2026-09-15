#include <stdint.h>

#include "stm32f10x.h"
#include "system_stm32f10x.h"

static volatile uint32_t function_result;

static uint32_t AddValues(uint32_t first, uint32_t second)
{
    return first + second;
}

int main(void)
{
    SystemInit();

    function_result = AddValues(0x11223344U, 0x55667788U);

    while (1) {
        __NOP();
    }
}

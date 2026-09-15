/*
 * ************************************************
 *  printf 重定向到 USART1 (PA9-TX / PA10-RX)
 *
 *  工具链: arm-none-eabi-gcc + newlib-nano (nano.specs/nosys.specs)
 *  GCC 下重定向 printf 的入口是 newlib 的 _write()，
 *  不是 Keil/ARMCC 用的 fputc()。
 *
 *  用法:
 *    USART_Printf_Init(115200);   // main 里先初始化
 *    printf("cntA = %d\r\n", cntA);
 *
 *  注意:
 *    - printf 缓冲: init 里已把 stdout 设为无缓冲(_IONBF)，
 *      否则不带 \n 的 printf 会一直留在缓冲区里发不出来
 *    - 浮点 %f: newlib-nano 默认不支持，需要链接选项
 *      -u _printf_float 才能用
 *    - _write 是阻塞发送（查 TXE 标志），在中断里调用要小心
 * ************************************************
 */

#include <stdio.h>
#include <unistd.h>
#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "usart_printf.h"

void USART_Printf_SendByte(uint8_t byte)
{
    /* 等待发送数据寄存器空 */
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) {
    }
    USART_SendData(USART1, byte);
}

void USART_Printf_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef gpio_init;
    USART_InitTypeDef usart_init;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

    /* PA9 = USART1_TX: 复用推挽输出 */
    gpio_init.GPIO_Pin   = GPIO_Pin_9;
    gpio_init.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio_init);

    /* PA10 = USART1_RX: 浮空输入（本模块只用发，收留好引脚配置） */
    gpio_init.GPIO_Pin  = GPIO_Pin_10;
    gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio_init);

    usart_init.USART_BaudRate            = baudrate;
    usart_init.USART_WordLength          = USART_WordLength_8b;
    usart_init.USART_StopBits            = USART_StopBits_1;
    usart_init.USART_Parity              = USART_Parity_No;
    usart_init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart_init.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &usart_init);

    USART_Cmd(USART1, ENABLE);

    /* stdout 无缓冲: printf 立即走 _write，不用等 \n 或手动 fflush */
    setvbuf(stdout, NULL, _IONBF, 0);
}

/* newlib 的底层写函数，printf/puts 都最终落到这里。
 * 定义在这里就能覆盖 nosys.specs 提供的桩函数（目标文件优先于库）。 */
int _write(int fd, char *ptr, int len)
{
    (void)fd;   /* 不区分 fd，stdout/stderr 都发到 USART1 */
    for (int i = 0; i < len; i++) {
        /* 把 \n 换成 \r\n，串口终端里才会回车换行 */
        if (ptr[i] == '\n') {
            USART_Printf_SendByte((uint8_t)'\r');
        }
        USART_Printf_SendByte((uint8_t)ptr[i]);
    }
    return len;
}

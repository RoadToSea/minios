#ifndef __USART_PRINTF_H__
#define __USART_PRINTF_H__

#include <stdint.h>

/* printf 重定向到的串口（USART1: PA9-TX / PA10-RX） */
void USART_Printf_Init(uint32_t baudrate);

/* 单字节阻塞发送，也可单独拿来用 */
void USART_Printf_SendByte(uint8_t byte);

#endif /* __USART_PRINTF_H__ */

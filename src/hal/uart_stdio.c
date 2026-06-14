/*
 * uart_stdio.c
 * ------------
 * Minimal stdio retarget: routes the C library's low-level write() to the UART
 * HAL so printf() goes out on the console UART. No CR/LF translation.
 *
 * The UART instance must already be initialized (dspic33ak_uart_init) before
 * printf() is used.
 */

#include <stdint.h>
#include <stddef.h>

#include "dspic33ak_uart.h"

/* Console UART instance used by printf(). */
#define STDIO_UART_INST   DSPIC33AK_UART_INST_1

int write(int handle, void *buffer, unsigned int len)
{
    (void)handle;
    return (int)dspic33ak_uart_write(STDIO_UART_INST, buffer, (size_t)len);
}

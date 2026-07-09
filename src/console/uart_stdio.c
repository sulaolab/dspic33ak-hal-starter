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

/* Mirror instance: console output is also copied to the PKOB4 "USB Serial
 * Device" (Phase 1 output mirror). dspic33ak_uart_write() is a no-op until the
 * instance is initialized, so a printf() before console_uart_init() is safe. */
#define MIRROR_UART_INST  DSPIC33AK_UART_INST_2

int write(int handle, void *buffer, unsigned int len)
{
    (void)handle;

    int written = (int)dspic33ak_uart_write(STDIO_UART_INST, buffer, (size_t)len);
    (void)dspic33ak_uart_write(MIRROR_UART_INST, buffer, (size_t)len);
    return written;
}

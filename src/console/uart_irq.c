/*
 * uart_irq.c
 * ----------
 * Starter-owned UART1 interrupt vector forwarding.
 */

#include <xc.h>

#include "dspic33ak_uart.h"
#include "dspic33ak_uart_rx_isr_ring.h"

void __attribute__((interrupt, context)) _U1RXInterrupt(void)
{
    dspic33ak_uart_rx_irq_handler(DSPIC33AK_UART_INST_1);
}

void __attribute__((interrupt, context)) _U1TXInterrupt(void)
{
    dspic33ak_uart_tx_irq_handler(DSPIC33AK_UART_INST_1);
}

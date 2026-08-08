/*
 * uart_irq.c
 * ----------
 * Starter-owned UART1 interrupt vector forwarding.
 */

#include <xc.h>

#include "nora_uart.h"

void __attribute__((interrupt, context)) _U1RXInterrupt(void)
{
    nora_uart_rx_irq_handler(NORA_UART_INST_1);
}

void __attribute__((interrupt, context)) _U1TXInterrupt(void)
{
    nora_uart_tx_irq_handler(NORA_UART_INST_1);
}

/*
 * dspic33ak-hal-starter  -  main.c
 * --------------------------------
 * Minimal bring-up starter for the Microchip Curiosity Platform Development
 * Board with a dsPIC33AK512MPS512 DIM, demonstrating the dspic33ak-*-hal family.
 *
 * Boot sequence (built up across phases):
 *   Phase 0: clock -> UART -> printf banner            <-- this file, now
 *   Phase 1: + SPI external-flash self-test
 *   Phase 2: + I2C bus scan
 *   Phase 3: + potentiometer ADC -> RGB LED, heartbeat
 */

#include <xc.h>
#include <stdio.h>
#include <stdint.h>

#include "dspic33ak_clock.h"
#include "dspic33ak_uart.h"
#include "board.h"

/* ---- Device configuration words ----
 * Most config bits use device defaults (the device boots on the FRC, which we
 * then feed into PLL1). Alternate-I2C2 pin mapping is selected for this board. */
#pragma config FDEVOPT_ALTI2C2 = ON   /* MikroBUS-B I2C2 on the alternate pins */

/* Rough busy-wait used only for a visible heartbeat in this minimal starter.
 * (A proper time base is introduced in a later phase.) */
static void crude_delay(void)
{
    for (volatile uint32_t i = 0u; i < 8000000u; i++) {
        /* spin */
    }
}

static void console_uart_init(void)
{
    const dspic33ak_uart_config_t cfg = {
        .uart_clk_hz = DSPIC33AK_CLOCK_SYS_HZ,   /* CLKGEN8 <- PLL1, divide-by-1 */
        .baudrate    = 230400u,
        .timeout_ms  = 0u,
        .get_ms      = NULL,
        .data_bits   = 8u,
        .stop_bits   = 1u,
        .parity      = DSPIC33AK_UART_PARITY_NONE,
        .enable_tx   = true,
        .enable_rx   = false,                    /* TX-only console for now */
        .rx_mode     = DSPIC33AK_UART_RX_MODE_POLLING,
    };

    board_uart1_pins_init();
    (void)dspic33ak_uart_init(DSPIC33AK_UART_INST_1, &cfg);
}

int main(void)
{
    (void)dspic33ak_clock_init();   /* FRC -> PLL1 200 MHz; route CLKGEN1/5/6/8/9 */
    console_uart_init();            /* UART1 pins + 230400 8N1, printf retargeted */

    printf("\n\n");
    printf("==============================================\n");
    printf(" dspic33ak-hal-starter\n");
    printf(" device : dsPIC33AK512MPS512\n");
    printf(" sysclk : %lu Hz (FRC -> PLL1)\n", (unsigned long)DSPIC33AK_CLOCK_SYS_HZ);
    printf(" uart   : UART1 @ 230400 8N1\n");
    printf("==============================================\n");

    uint32_t beat = 0u;
    while (1)
    {
        printf(" heartbeat %lu\n", (unsigned long)beat++);
        crude_delay();
    }

    return 0;
}

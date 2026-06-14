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
#include "systick.h"
#include "dspic33ak_uart.h"
#include "dspic33ak_i2c.h"
#include "sst26_min.h"
#include "i2c_scan.h"
#include "rgb_pot.h"
#include "board.h"

/* ---- Device configuration words ----
 * Most config bits use device defaults (the device boots on the FRC, which we
 * then feed into PLL1). Alternate-I2C2 pin mapping is selected for this board. */
#pragma config FDEVOPT_ALTI2C2 = ON   /* MikroBUS-B I2C2 on the alternate pins */

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
    (void)dspic33ak_clock_init();      /* FRC -> PLL1 200 MHz; route CLKGEN1/5/6/8/9 */
    board_ports_digital_default();     /* all pins digital (needed for I2C SDA/SCL) */
    systick_init();                    /* 1 ms time base (heartbeat + I2C timeout)  */
    console_uart_init();               /* UART1 pins + 230400 8N1, printf retargeted */

    printf("\n\n");
    printf("==============================================\n");
    printf(" dspic33ak-hal-starter\n");
    printf(" device : dsPIC33AK512MPS512\n");
    printf(" sysclk : %lu Hz (FRC -> PLL1)\n", (unsigned long)DSPIC33AK_CLOCK_SYS_HZ);
    printf(" uart   : UART1 @ 230400 8N1\n");
    printf("==============================================\n");

    /* ---- SPI external flash (SST26 on SPI4) ---- */
    if (sst26_min_init()) {
        uint8_t mfr = 0, typ = 0, dev = 0;
        bool id_ok = sst26_min_read_jedec(&mfr, &typ, &dev);
        printf(" SST26 JEDEC: MFR=0x%02X TYPE=0x%02X DEV=0x%02X (%s)\n",
               mfr, typ, dev, id_ok ? "good" : "unexpected");

        if (id_ok) {
            bool verify_ok = sst26_min_selftest(0x000000u);
            printf(" SST26 sector verify @0x000000: %s\n", verify_ok ? "OK" : "FAILED");
        }
    } else {
        printf(" SST26: SPI init failed\n");
    }
    printf("==============================================\n");

    /* ---- I2C bus scan (MikroBUS-A = I2C2, alternate pins) ---- */
    {
        const dspic33ak_i2c_config_t i2c_cfg = {
            .fcy_hz             = DSPIC33AK_CLOCK_SYS_HZ / 2u,   /* fcy = sysclk/2  */
            .bus_hz             = 400000u,                       /* 400 kHz         */
            .timeout_ms         = 5u,                            /* never hang      */
            .get_ms             = systick_ms,
            .pending_timeout_ms = 50u,
        };
        if (dspic33ak_i2c_init(DSPIC33AK_I2C_INST_2, &i2c_cfg) == DSPIC33AK_I2C_OK) {
            i2c_scan_run(DSPIC33AK_I2C_INST_2);
        } else {
            printf(" I2C: init failed\n");
        }
    }
    printf("==============================================\n");

    /* ---- Potentiometer (ADC5) -> RGB LED (PWM1/2/3) ---- */
    rgb_pot_init();
    printf(" RGB LED follows the potentiometer; heartbeat below.\n");
    printf("==============================================\n");

    /* Main loop: update the LED color from the pot continuously, and print a
     * 1 Hz heartbeat using the systick time base (non-blocking). */
    uint32_t beat      = 0u;
    uint32_t last_beat = systick_ms();
    while (1)
    {
        rgb_pot_update();

        uint32_t now = systick_ms();
        if ((uint32_t)(now - last_beat) >= 1000u) {
            last_beat = now;
            printf(" heartbeat %lu\n", (unsigned long)beat++);
        }
    }

    return 0;
}

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
#include "dspic33ak_i2c_master.h"
#include "sst26_min.h"
#include "i2c_scan.h"
#include "i2c_loopback.h"
#include "can_loopback.h"
#include "can_bus_test.h"
#include "rgb_pot.h"
#include "led_sw.h"
#include "board.h"

/* The optional two-board CAN FD bus test is controlled by CAN_BUS_TEST /
 * CAN_BUS_TEST_ECHO, both defined (default 0) in can_bus_test.h. With
 * CAN_BUS_TEST=1 the firmware enters the bus test after the boot self-test and
 * never returns; build one board with CAN_BUS_TEST_ECHO=0 (originator, id 0x0A0)
 * and the other with =1 (echo, id 0x0B0), J21<->J21 + 120 ohm termination. */

/* ---- Device configuration words ----
 * Most config bits use device defaults (the device boots on the FRC, which we
 * then feed into PLL1). Alternate-I2C2 pin mapping is selected for this board. */
#pragma config FDEVOPT_ALTI2C2 = ON   /* I2C2 on its alternate (board) pins */

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

static void term_init_safe(void)
{
    /*
     * Recover the terminal from a possible VT100/ISO-2022 shifted state.
     * If noise, binary data, or an interrupted ESC sequence reaches the UART,
     * some terminal emulators may switch to an alternate character set or
     * non-default text attributes. The copied text can still be correct while
     * the on-screen glyphs look corrupted.
     */
    printf("\x1b(B\x0F");  /* Select US-ASCII as G0 and shift back to G0 */
    printf("\x1b[0m");     /* Reset text attributes */
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
    printf(" build  : " __DATE__ " " __TIME__ "\n");
    printf(" device : dsPIC33AK512MPS512\n");
    printf(" sysclk : %lu Hz (FRC -> PLL1)\n", (unsigned long)DSPIC33AK_CLOCK_SYS_HZ);
    printf(" uart   : UART1 @ 230400 8N1\n");
    printf("==============================================\n");

    /* ---- User LEDs + switches (GPIO) ----
     * Power-on indicator: light all 8 LEDs for 1 s, then clear. Afterwards the
     * main loop polls SW1/SW2 for LED7/LED6 and mirrors the SW3 CN event state
     * on LED5. */
    led_sw_init();
    led_sw_boot_test(1000u);
    printf(" LEDs: SW1/SW2 poll LED7/LED6; SW3 CN event drives LED5.\n");
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

    /* ---- I2C bus scan ----
     * We drive I2C2 (MikroBUS A, alternate pins). On this board MikroBUS A and
     * B share one I2C bus through a shorting resistor, so a scan finds devices
     * plugged into *either* the A or B socket; it cannot tell which side.
     * Plug an I2C device into the MikroBUS A or B I2C header and it shows up. */
    {
        const dspic33ak_i2c_config_t i2c_cfg = {
            .fcy_hz             = DSPIC33AK_CLOCK_SYS_HZ / 2u,   /* fcy = sysclk/2  */
            .bus_hz             = 400000u,                       /* 400 kHz         */
            .timeout_ms         = 5u,                            /* never hang      */
            .get_ms             = systick_ms,
            .pending_timeout_ms = 50u,
        };
        if (dspic33ak_i2c_init(DSPIC33AK_I2C_INST_2, &i2c_cfg) == DSPIC33AK_I2C_OK) {
            i2c_scan_run(DSPIC33AK_I2C_INST_2, "MikroBUS A/B");
        } else {
            printf(" I2C: init failed\n");
        }
    }
    printf("==============================================\n");

    /* ---- I2C master<->slave loopback (I2C2 master <-> I2C3 slave @0x55) ----
     * Bring up the I2C3 slave once; the main loop then runs one Write+Read
     * round trip per heartbeat over the shared MikroBUS A/B bus. */
    bool loopback_ok = i2c_loopback_init();
    printf(" I2C loopback: I2C2 master <-> I2C3 slave @0x55 (%s); per beat below.\n",
           loopback_ok ? "ready" : "slave init FAILED");
    printf("==============================================\n");

    /* ---- CAN FD (CAN1) ----
     * Route the 20 MHz CAN clock (CLKGEN10) and configure the CAN1 pins (PPS +
     * module enable + transceiver out of standby). can_loopback_selftest() runs a
     * quick INTERNAL-loopback HAL check, then arms CAN1 in NORMAL_FD so the live
     * demo (can_loopback_tick) transmits on the REAL bus each beat. A lone node
     * (no ACK partner) goes error-passive and retransmits -> a visible burst on
     * CANH/CANL (the TX queue then fills, so sends report "queue full / timeout");
     * connect a CAN node / analyzer (or a 2nd board in echo config) for ACKed
     * traffic. With CAN_BUS_TEST=1 the firmware instead enters the dedicated
     * two-board bus test and does not return. */
    dspic33ak_clock_can_init();
    board_can1_pins_init();
    bool can_ok = can_loopback_selftest();
    printf(" CAN1 FD @500k/2M live on the bus (HAL self-check: %s).\n",
           can_ok ? "PASS" : "FAILED");
    printf("   No ACK partner -> error-passive + retransmit burst; add a CAN node\n");
    printf("   (analyzer / 2nd board echo) to ACK it and see steady CAN H/L.\n");
    printf("==============================================\n");
#if CAN_BUS_TEST
    can_bus_test_run(CAN_BUS_TEST_ECHO);   /* never returns */
#endif

    /* ---- Potentiometer (ADC5) -> RGB LED (PWM1/2/3) ---- */
    rgb_pot_init();
    printf(" RGB LED follows the potentiometer; LED0 blinks with the heartbeat.\n");
    printf("==============================================\n");

    /* Main loop: update the LED color from the pot continuously, toggle LED0 once
     * per second (visible liveness without a serial port), and on each 1 s beat
     * run ONE peripheral demo, alternating between them: even beats run the I2C
     * master<->slave round trip, odd beats run a CAN1 internal-loopback round
     * trip. Each demo therefore fires every 2 s, offset 1 s from the other. */
    uint32_t beat      = 0u;
    uint32_t last_beat = systick_ms();
    uint32_t last_term_reset = systick_ms();
    while (1)
    {
        rgb_pot_update();
        led_sw_update();      /* SW1/2 polled; SW3 event state mirrored on LED5 */

        uint32_t now = systick_ms();
        if ((uint32_t)(now - last_term_reset) >= 3000u) {
            last_term_reset = now;
            term_init_safe();
        }

        if ((uint32_t)(now - last_beat) >= 1000u) {
            last_beat = now;
            led_sw_toggle(0u);      /* LED0 = heartbeat indicator */
            if ((beat & 1u) == 0u) {
                if (loopback_ok) {
                    i2c_loopback_tick(DSPIC33AK_I2C_INST_2, beat);  /* even beat: I2C */
                }
            } else {
                can_loopback_tick(beat);                            /* odd beat: CAN */
            }
            beat++;
        }
    }

    return 0;
}

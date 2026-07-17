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

#include "starter_clock.h"
#include "dspic33ak_tick_timer.h"
#include "dspic33ak_high_res_timer.h"
#include "dspic33ak_uart.h"
#include "dspic33ak_udid.h"
#include "dspic33ak_i2c_master.h"
#include "sst26_min.h"
#include "i2c_scan.h"
#include "i2c_loopback.h"
#include "can_loopback.h"
#include "can_bus_test.h"
#include "can_rx_isr_selftest.h"
#include "rgb_pot.h"
#include "led_sw.h"
#include "board.h"
#include "app_config.h"
#include "tdm_smoke.h"
#include "tdm_neg_test.h"
#if HAL_STARTER_ENABLE_UART_ASYNC_SELFTEST
#include "uart_async_selftest.h"
#endif

/* The optional two-board CAN FD bus test is controlled by CAN_BUS_TEST /
 * CAN_BUS_TEST_ECHO, both defined (default 0) in can_bus_test.h. With
 * CAN_BUS_TEST=1 the firmware enters the bus test after the boot self-test and
 * never returns; build one board with CAN_BUS_TEST_ECHO=0 (originator, id 0x0A0)
 * and the other with =1 (echo, id 0x0B0), J21<->J21 + 120 ohm termination. */

/* ---- Device configuration words ----
 * Most config bits use device defaults (the device boots on the FRC, which we
 * then feed into PLL1). Alternate-I2C2 pin mapping is selected for this board. */
#pragma config FDEVOPT_ALTI2C2 = ON   /* I2C2 on its alternate (board) pins */

static uint8_t s_console_uart_rx_ring[256u];

/*
 * UART init status, captured so a failed bring-up is observable from the
 * debugger. Deliberately NOT reported via printf(): stdio retargets to UART1,
 * so if UART1 init failed there is no console to print to (the AK128 IFS2/IFS3
 * "no output" root cause). Inspect these instead of trusting the port.
 */
volatile dspic33ak_uart_status_t g_uart1_init_status = DSPIC33AK_UART_OK;
volatile dspic33ak_uart_status_t g_uart2_init_status = DSPIC33AK_UART_OK;

static void console_uart_init(void)
{
    const dspic33ak_uart_config_t cfg = {
        .uart_clk_hz = STARTER_CLOCK_SYS_HZ,   /* CLKGEN8 <- PLL1, divide-by-1 */
        .baudrate    = 230400u,
        .timeout_ms  = 0u,
        .get_ms      = NULL,
        .data_bits   = 8u,
        .stop_bits   = 1u,
        .parity      = DSPIC33AK_UART_PARITY_NONE,
        .enable_tx   = true,
        .enable_rx   = true,
        .rx_mode     = DSPIC33AK_UART_RX_MODE_ISR_RING,
        .rx_ring_buffer = s_console_uart_rx_ring,
        .rx_ring_buffer_size = sizeof(s_console_uart_rx_ring),
        .rx_irq_priority = 5u,
        .tx_irq_priority = 5u,
    };

    /* UART2: PKOB4 "USB Serial Device". Output mirror (stdio write() retarget
     * copies console output here) + Phase 2 input: RX enabled so keystrokes on
     * this port are teed like UART1 (see console_input_tee_poll). RX is polling
     * (drained in the main loop) -- no ring buffer / RX IRQ. */
    const dspic33ak_uart_config_t cfg2 = {
        .uart_clk_hz = STARTER_CLOCK_SYS_HZ,   /* CLKGEN8 <- PLL1, divide-by-1 */
        .baudrate    = 230400u,
        .timeout_ms  = 0u,
        .get_ms      = NULL,
        .data_bits   = 8u,
        .stop_bits   = 1u,
        .parity      = DSPIC33AK_UART_PARITY_NONE,
        .enable_tx   = true,
        .enable_rx   = true,
        .rx_mode     = DSPIC33AK_UART_RX_MODE_POLLING,
        .rx_ring_buffer = NULL,
        .rx_ring_buffer_size = 0u,
        .rx_irq_priority = 0u,
        .tx_irq_priority = 5u,
    };

    /* If pin init fails the UART is not yet up -- can't printf. Proceed anyway;
     * a wrong PPS config will be visible as garbled / no output. */
    (void)board_uart1_pins_init();
    g_uart1_init_status = dspic33ak_uart_init(DSPIC33AK_UART_INST_1, &cfg);

    (void)board_uart2_pins_init();
    g_uart2_init_status = dspic33ak_uart_init(DSPIC33AK_UART_INST_2, &cfg2);
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

void __attribute__((interrupt, context)) _T1Interrupt(void)
{
    dspic33ak_tick_timer_irq_handler();
}

static void wait_ms_from_tick(uint32_t wait_ms)
{
    uint32_t start = dspic33ak_tick_timer_get_ms();

    while ((uint32_t)(dspic33ak_tick_timer_get_ms() - start) < wait_ms) {
        ;
    }
}

static void high_res_timer_boot_test(dspic33ak_high_res_timer_status_t init_status)
{
    const uint32_t count_to_us_100 = dspic33ak_high_res_timer_count_to_us(100u);
    const uint32_t count_to_us_x10_10 = dspic33ak_high_res_timer_count_to_us_x10(10u);
    const uint32_t count_to_us_x10_100 = dspic33ak_high_res_timer_count_to_us_x10(100u);
    uint32_t count0;
    uint32_t count1;
    uint32_t count2;
    uint32_t delta1;
    uint32_t delta10;
    bool conversion_ok;
    bool counter_ok;
    bool status_ok;

    count0 = dspic33ak_high_res_timer_get_count();
    wait_ms_from_tick(1u);
    count1 = dspic33ak_high_res_timer_get_count();
    wait_ms_from_tick(10u);
    count2 = dspic33ak_high_res_timer_get_count();

    delta1 = count1 - count0;
    delta10 = count2 - count1;
    conversion_ok = (count_to_us_100 == 1u) &&
                    (count_to_us_x10_10 == 1u) &&
                    (count_to_us_x10_100 == 10u);
    counter_ok = (count1 != count0) && (count2 != count1);
    status_ok = (init_status == DSPIC33AK_HIGH_RES_TIMER_OK) &&
                dspic33ak_high_res_timer_is_present() &&
                dspic33ak_high_res_timer_is_initialized();

    printf(" HRT: init=%d present=%d initialized=%d clk=%lu Hz\n",
           (int)init_status,
           (int)dspic33ak_high_res_timer_is_present(),
           (int)dspic33ak_high_res_timer_is_initialized(),
           (unsigned long)STARTER_CLOCK_FCY_HZ);
    printf(" HRT: count0=%lu count1=%lu count2=%lu d1=%lu d10=%lu\n",
           (unsigned long)count0,
           (unsigned long)count1,
           (unsigned long)count2,
           (unsigned long)delta1,
           (unsigned long)delta10);
    printf(" HRT: conv 100cnt=%lu us, 10cnt=%lu x0.1us, 100cnt=%lu x0.1us\n",
           (unsigned long)count_to_us_100,
           (unsigned long)count_to_us_x10_10,
           (unsigned long)count_to_us_x10_100);
    printf(" HRT self-check: %s\n",
           (status_ok && counter_ok && conversion_ok) ? "PASS" : "FAIL");
    printf("==============================================\n");
}

/* Write one byte to BOTH console ports (echo to the sender + mirror to the
 * other -- for a byte that is equivalent to writing both). */
static void console_tee_write(uint8_t c)
{
    (void)dspic33ak_uart_write_byte(DSPIC33AK_UART_INST_1, c);
    (void)dspic33ak_uart_write_byte(DSPIC33AK_UART_INST_2, c);
}

/* Input port index for per-port tee state. */
enum { CONSOLE_SRC_UART1 = 0u, CONSOLE_SRC_UART2 = 1u, CONSOLE_SRC_COUNT = 2u };

/* Filter + tee one received byte from port `src`. Accept ASCII printable
 * (0x20-0x7E) and ENTER (CR or LF); drop everything else (ESC, other control,
 * non-ASCII). Escape sequences are NOT interpreted -- this is a per-byte filter.
 * A CR+LF pair is collapsed to a single line break so one Enter key press yields
 * one newline. The CRLF coalescing state is PER PORT (indexed by src): a CR on
 * one port must not swallow a later LF that is a genuine Enter on the other. */
static void console_tee_feed(uint8_t src, uint8_t c)
{
    static bool s_last_was_cr[CONSOLE_SRC_COUNT] = { false, false };

    if ((c == (uint8_t)'\r') || (c == (uint8_t)'\n')) {
        if ((c == (uint8_t)'\n') && s_last_was_cr[src]) {
            s_last_was_cr[src] = false;   /* second half of this port's CRLF */
            return;
        }
        s_last_was_cr[src] = (c == (uint8_t)'\r');
        console_tee_write((uint8_t)'\r');
        console_tee_write((uint8_t)'\n');
        return;
    }

    s_last_was_cr[src] = false;

    if ((c >= 0x20u) && (c <= 0x7Eu)) {
        console_tee_write(c);   /* printable ASCII -> both ports */
    }
    /* else: control / ESC / non-ASCII -> dropped */
}

/*
 * Console input tee (Phase 2): drain BOTH command ports and tee each accepted
 * byte to both, so "USB Serial Port" (UART1) and "USB Serial Device" (UART2)
 * show one shared session (output + both ports' echoed input). No source lock:
 * the tee is per-byte and stateless (aside from CRLF coalescing), so there is no
 * shared line buffer to corrupt. Intended use is one port at a time.
 */
static void console_input_tee_poll(void)
{
    enum { MAX_BYTES_PER_LOOP = 32 };
    uint8_t data;
    uint8_t n;

    for (n = 0u; n < MAX_BYTES_PER_LOOP; n++) {
        if (!dspic33ak_uart_rx_ready(DSPIC33AK_UART_INST_1)) {
            break;
        }
        if (dspic33ak_uart_read_byte(DSPIC33AK_UART_INST_1, &data) != DSPIC33AK_UART_OK) {
            break;
        }
        console_tee_feed(CONSOLE_SRC_UART1, data);
    }

    for (n = 0u; n < MAX_BYTES_PER_LOOP; n++) {
        if (!dspic33ak_uart_rx_ready(DSPIC33AK_UART_INST_2)) {
            break;
        }
        if (dspic33ak_uart_read_byte(DSPIC33AK_UART_INST_2, &data) != DSPIC33AK_UART_OK) {
            break;
        }
        console_tee_feed(CONSOLE_SRC_UART2, data);
    }
}

int main(void)
{
    const dspic33ak_tick_timer_config_t tick_cfg = {
        .timer_clk_hz = STARTER_CLOCK_FCY_HZ,
        .irq_priority = DSPIC33AK_TICK_TIMER_DEFAULT_IRQ_PRIORITY,
        .run_in_idle = false,
    };
    const dspic33ak_high_res_timer_config_t high_res_cfg = {
        .timer_clk_hz = STARTER_CLOCK_FCY_HZ,
        .run_in_idle = false,
    };
    dspic33ak_high_res_timer_status_t high_res_status;

    if (!starter_clock_init()) {       /* FRC -> PLL1 200 MHz; route CLKGEN1/5/6/8/9 */
        while (1) {
            Nop();
        }
    }
    board_ports_digital_default();     /* all pins digital (needed for I2C SDA/SCL) */
    if (dspic33ak_tick_timer_init(&tick_cfg) != DSPIC33AK_TICK_TIMER_OK) {
        while (1) {
            Nop();
        }
    }
    high_res_status = dspic33ak_high_res_timer_init(&high_res_cfg);
    console_uart_init();               /* UART1 pins + 230400 8N1, printf retargeted */
#if HAL_STARTER_ENABLE_UART_ASYNC_SELFTEST
    bool uart_async_ok = uart_async_selftest_run();
#endif

    printf("\n\n");
    printf("==============================================\n");
    printf(" dspic33ak-hal-starter\n");
    printf(" build  : " __DATE__ " " __TIME__ "\n");
    printf(" device : dsPIC33AK512MPS512\n");
    /* Per-die Unique Device ID (board-individual identity). UDID128 is the four
     * read-only words concatenated UDID4..UDID1 (high word first). */
    {
        dspic33ak_udid_t udid;
        if (DSPIC33AK_UDID_Read(&udid))
        {
            printf(" udid   : %08lX%08lX%08lX%08lX\n",
                   (unsigned long)udid.word[3], (unsigned long)udid.word[2],
                   (unsigned long)udid.word[1], (unsigned long)udid.word[0]);
        }
        else
        {
            printf(" udid   : read failed or invalid\n");
        }
    }
    printf(" sysclk : %lu Hz (FRC -> PLL1)\n", (unsigned long)STARTER_CLOCK_SYS_HZ);
    printf(" uart   : UART1 @ 230400 8N1, RX ISR-ring echo active\n");
#if HAL_STARTER_ENABLE_UART_ASYNC_SELFTEST
    printf(" uart async self-test: %s\n", uart_async_ok ? "PASS" : "FAIL");
#endif
    printf("==============================================\n");
    high_res_timer_boot_test(high_res_status);

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
            .fcy_hz             = STARTER_CLOCK_FCY_HZ,           /* fcy = sysclk/2  */
            .bus_hz             = 400000u,                       /* 400 kHz         */
            .timeout_ms         = 5u,                            /* never hang      */
            .get_ms             = dspic33ak_tick_timer_get_ms,
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
    bool can_runtime_ready = false;
    if (!starter_clock_can_init()) {
        printf(" ERROR: CAN clock init failed; CAN demos skipped.\n");
    } else {
        if (!board_can1_pins_init()) {
            printf(" WARNING: board_can1_pins_init failed\n");
        }
#if !CAN_BUS_TEST
        /* Single-board RX-interrupt self-test (INTERNAL loopback): proves the RX
         * callback fires, a frame can be received, RX overflow is detected, and the
         * TX interrupt line stays disabled - all without a bus or partner. Owns the
         * CAN1 RX/general vectors for this build; leaves CAN1 quiesced afterwards. */
        bool rx_isr_ok = can_rx_isr_selftest_run();
        printf(" CAN1 RX-ISR self-check: %s.\n", rx_isr_ok ? "PASS" : "FAILED");
        printf("==============================================\n");
#endif
        can_runtime_ready = can_loopback_selftest();
        printf(" CAN1 FD @500k/2M live on the bus (HAL self-check: %s).\n",
               can_runtime_ready ? "PASS" : "FAILED");
        printf("   No ACK partner -> error-passive + retransmit burst; add a CAN node\n");
        printf("   (analyzer / 2nd board echo) to ACK it and see steady CAN H/L.\n");
        printf("==============================================\n");
    }
#if CAN_BUS_TEST
    if (can_runtime_ready) {
        can_bus_test_run(CAN_BUS_TEST_ECHO);   /* never returns */
    }
#endif

    /* ---- Potentiometer (ADC5) -> RGB LED (PWM1/2/3) ---- */
    rgb_pot_init();
    printf(" RGB LED follows the potentiometer; LED0 blinks with the heartbeat.\n");
    printf("==============================================\n");

    /* NEG self-test result gates the smoke demo (fail-closed). When the NEG toggle is off this is
     * true, so the smoke runs normally in the shipped build. */
#if HAL_STARTER_ENABLE_TDM_NEG_TEST
    /* ---- OPT-IN SPI/I2S/TDM HAL negative-validation self-test (contract regression) ----
     * Runs BEFORE the smoke demo and leaves the HAL stopped + closed. Prints "[NEG] pass=N fail=M".
     * On failure the HAL may be in an unexpected state, so the TDM smoke is SKIPPED (fail-closed) --
     * the rest of the starter continues. Default OFF (app_config.h). */
    const bool tdm_neg_ok = tdm_neg_test_run();
    if (tdm_neg_ok) {
        printf(" [NEG] all negative-validation cases PASSED.\n");
    } else {
        printf(" [NEG] one or more cases FAILED (see [NEG] lines above); TDM smoke will be skipped.\n");
    }
    printf("==============================================\n");
#else
    const bool tdm_neg_ok = true;
#endif

#if HAL_STARTER_ENABLE_TDM_SMOKE_DEMO
    /* ---- SPI1 framed-mode TDM8 smoke demo (codec-less self-clocked master, MikroBUS-A) ----
     * Drives all 8 TDM slots with a ~800 Hz-class sine so a scope on DataOut shows a TDM8
     * frame. Jumper DataOut(RP101)->DataIn(RP106) to see the RX level near 0 dB rel. The
     * stream runs on DMA/ISR; the main loop only prints a status line every 5 s. A start
     * failure is reported but does NOT stop the rest of the starter. Disable in
     * app_config.h (HAL_STARTER_ENABLE_TDM_SMOKE_DEMO 0) to free the MikroBUS-A SPI pins. */
    if (!tdm_neg_ok) {
        printf(" [TDM1] smoke SKIPPED: negative self-test failed (HAL state not trusted).\n");
    } else if (tdm_smoke_init()) {
        printf(" [TDM1] SPI1 TDM8 master smoke demo started (MikroBUS-A;"
               " jumper DataOut->DataIn for loopback).\n");
    } else {
        printf(" [TDM1] start FAILED (%s) -- existing starter demos continue.\n",
               tdm_smoke_last_error_str());
    }
    printf("==============================================\n");
#else
    (void)tdm_neg_ok;   /* consumed only by the smoke gate; avoid unused-variable when smoke is off */
#endif

    /* Main loop: update the LED color from the pot continuously, toggle LED0 once
     * per second (visible liveness without a serial port), and on each 1 s beat
     * run ONE peripheral demo, alternating between them: even beats run the I2C
     * master<->slave round trip, odd beats transmit one CAN FD frame on the real
     * CAN bus. Each demo therefore fires every 2 s, offset 1 s from the other. */
    uint32_t beat      = 0u;
    uint32_t last_beat = dspic33ak_tick_timer_get_ms();
    uint32_t last_term_reset = dspic33ak_tick_timer_get_ms();
#if HAL_STARTER_ENABLE_TDM_SMOKE_DEMO
    uint32_t last_tdm  = dspic33ak_tick_timer_get_ms() - 5000u;
#endif
    while (1)
    {
        rgb_pot_update();
        led_sw_update();      /* SW1/2 polled; SW3 event state mirrored on LED5 */
        console_input_tee_poll();

        uint32_t now = dspic33ak_tick_timer_get_ms();
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
                if (can_runtime_ready) {
                    can_loopback_tick(beat);                        /* odd beat: CAN */
                }
            }
            beat++;
        }

#if HAL_STARTER_ENABLE_TDM_SMOKE_DEMO
        if ((uint32_t)(now - last_tdm) >= 5000u) {   /* TDM status line every ~5 s */
            last_tdm = now;
            tdm_smoke_status_print();
        }
#endif
    }

    return 0;
}

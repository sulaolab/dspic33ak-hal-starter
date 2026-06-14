/*
 * board.c
 * -------
 * Board pin wiring for the Curiosity + dsPIC33AK512MPS512 DIM starter.
 * Uses the GPIO HAL for pin attributes and writes the PPS registers directly
 * (see board_pins.h). The CLKGEN clock routing is done separately in
 * dspic33ak_clock_init().
 */

#include <xc.h>

#include "dspic33ak_gpio.h"
#include "board_pins.h"
#include "board.h"

void board_ports_digital_default(void)
{
    /* 0 = digital, 1 = analog. Make every analog-capable pin digital at boot so
     * module-owned pins (I2C SDA/SCL) work; analog inputs set their ANSEL bit
     * again when configured. */
    ANSELA = 0;
    ANSELB = 0;
    ANSELC = 0;
    ANSELD = 0;
}

void board_uart1_pins_init(void)
{
    /* U1TX: digital push-pull output, idle high (UART idle line is high).
     * The LAT high is applied before the pin becomes an output (glitch-free). */
    static const dspic33ak_gpio_config_t tx_cfg = {
        .dir          = DSPIC33AK_GPIO_DIR_OUTPUT,
        .pull         = DSPIC33AK_GPIO_PULL_NONE,
        .analog       = false,
        .open_drain   = false,
        .initial_high = true,
    };
    /* U1RX: digital input. */
    static const dspic33ak_gpio_config_t rx_cfg = {
        .dir          = DSPIC33AK_GPIO_DIR_INPUT,
        .pull         = DSPIC33AK_GPIO_PULL_NONE,
        .analog       = false,
        .open_drain   = false,
        .initial_high = false,
    };

    (void)dspic33ak_gpio_config(BOARD_UART1_PIN_TX, &tx_cfg);
    (void)dspic33ak_gpio_config(BOARD_UART1_PIN_RX, &rx_cfg);

    /* PPS: route U1RX input from its source RP, U1TX output onto its RP pin. */
    _U1RXR              = BOARD_UART1_RX_PPS_SRC;
    BOARD_UART1_TX_RPnR = BOARD_UART1_TX_PPS_FUNC;
}

void board_spi4_sst26_pins_init(void)
{
    /* Per-pin GPIO attributes (the SPI HAL drives only the SPI registers).
     *   SDO4 / SCK4 : digital output, idle low
     *   SDI4        : digital input
     *   CS / WP     : active-low controls, idle high (deasserted / WP released)
     *   RST         : active-low, asserted low here; the SST26 driver releases it
     *                 after the reset-pulse delay.
     */
    static const dspic33ak_gpio_config_t out_low  = {
        .dir = DSPIC33AK_GPIO_DIR_OUTPUT, .pull = DSPIC33AK_GPIO_PULL_NONE,
        .analog = false, .open_drain = false, .initial_high = false,
    };
    static const dspic33ak_gpio_config_t in_cfg   = {
        .dir = DSPIC33AK_GPIO_DIR_INPUT,  .pull = DSPIC33AK_GPIO_PULL_NONE,
        .analog = false, .open_drain = false, .initial_high = false,
    };
    static const dspic33ak_gpio_config_t out_high = {
        .dir = DSPIC33AK_GPIO_DIR_OUTPUT, .pull = DSPIC33AK_GPIO_PULL_NONE,
        .analog = false, .open_drain = false, .initial_high = true,
    };

    (void)dspic33ak_gpio_config(BOARD_SST26_PIN_SDO, &out_low);
    (void)dspic33ak_gpio_config(BOARD_SST26_PIN_SCK, &out_low);
    (void)dspic33ak_gpio_config(BOARD_SST26_PIN_SDI, &in_cfg);
    (void)dspic33ak_gpio_config(BOARD_SST26_PIN_CS,  &out_high);
    (void)dspic33ak_gpio_config(BOARD_SST26_PIN_WP,  &out_high);
    (void)dspic33ak_gpio_config(BOARD_SST26_PIN_RST, &out_low);

    /* PPS: SDI4 input from its source RP; SDO4 / SCK4 outputs onto their RP pins. */
    _SDI4R                = BOARD_SST26_SDI4_PPS_SRC;
    BOARD_SST26_SDO4_RPnR = BOARD_SST26_SDO4_PPS_FUNC;
    BOARD_SST26_SCK4_RPnR = BOARD_SST26_SCK4_PPS_FUNC;
}

void board_rgb_pins_init(void)
{
    /* RGB LED pins are PWM-driven outputs (the PWM module drives them via PPS). */
    static const dspic33ak_gpio_config_t out_low = {
        .dir = DSPIC33AK_GPIO_DIR_OUTPUT, .pull = DSPIC33AK_GPIO_PULL_NONE,
        .analog = false, .open_drain = false, .initial_high = false,
    };

    (void)dspic33ak_gpio_config(BOARD_RGB_PIN_BLUE,  &out_low);
    (void)dspic33ak_gpio_config(BOARD_RGB_PIN_GREEN, &out_low);
    (void)dspic33ak_gpio_config(BOARD_RGB_PIN_RED,   &out_low);

    /* PPS: route PWM1H/PWM2H/PWM3H outputs onto the LED pins. */
    BOARD_RGB_BLUE_RPnR  = _RPOUT_PWM1H;
    BOARD_RGB_GREEN_RPnR = _RPOUT_PWM2H;
    BOARD_RGB_RED_RPnR   = _RPOUT_PWM3H;
}

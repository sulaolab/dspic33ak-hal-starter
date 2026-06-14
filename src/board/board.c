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

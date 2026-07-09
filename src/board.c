/*
 * board.c
 * -------
 * Board pin wiring for the Curiosity + dsPIC33AK512MPS512 DIM starter.
 * PPS-capable pins are configured via the RP-first GPIO API and the PPS HAL,
 * using the RP number as the single identifier for both GPIO attributes and
 * signal routing. Non-PPS pins (CS, WP, RST, STBY) use the packed-pin API.
 * The CLKGEN clock routing is done separately in starter_clock_init().
 */

#include "dspic33ak_canfd.h"
#include "dspic33ak_gpio.h"
#include "dspic33ak_pps.h"
#include "board_pins.h"
#include "board.h"

static void board_port_digital_default(dspic33ak_gpio_port_t port)
{
    uint8_t bit;

    for (bit = 0u; bit < 16u; bit++) {
        (void)dspic33ak_gpio_set_analog(DSPIC33AK_GPIO_PIN(port, bit), false);
    }
}

void board_ports_digital_default(void)
{
    /* 0 = digital, 1 = analog. Make every analog-capable pin digital at boot so
     * module-owned pins (I2C SDA/SCL) work; analog inputs set their ANSEL bit
     * again when configured. */
    board_port_digital_default(DSPIC33AK_GPIO_PORT_A);
    board_port_digital_default(DSPIC33AK_GPIO_PORT_B);
    board_port_digital_default(DSPIC33AK_GPIO_PORT_C);
    board_port_digital_default(DSPIC33AK_GPIO_PORT_D);
}

bool board_uart1_pins_init(void)
{
    /* U1TX: idle-high output (UART idle line is high); GPIO before PPS. */
    if (!dspic33ak_gpio_rp_config_digital_output(BOARD_UART1_TX_RP, true))
        return false;
    if (!dspic33ak_pps_route_output(DSPIC33AK_PPS_OUTPUT_U1TX, BOARD_UART1_TX_RP))
        return false;

    /* U1RX: digital input; GPIO before PPS. */
    if (!dspic33ak_gpio_rp_config_digital_input(BOARD_UART1_RX_RP))
        return false;
    if (!dspic33ak_pps_route_input(DSPIC33AK_PPS_INPUT_U1RX, BOARD_UART1_RX_RP))
        return false;

    return true;
}

bool board_uart2_pins_init(void)
{
    /* PKOB4 "USB Serial Device": output mirror + Phase 2 tee input.
     * U2TX: idle-high output (UART idle line is high); GPIO before PPS. */
    if (!dspic33ak_gpio_rp_config_digital_output(BOARD_UART2_TX_RP, true))
        return false;
    if (!dspic33ak_pps_route_output(DSPIC33AK_PPS_OUTPUT_U2TX, BOARD_UART2_TX_RP))
        return false;

    /* U2RX: digital input; GPIO before PPS. Phase 2 -- keystrokes on this port
     * are teed alongside UART1. */
    if (!dspic33ak_gpio_rp_config_digital_input(BOARD_UART2_RX_RP))
        return false;
    if (!dspic33ak_pps_route_input(DSPIC33AK_PPS_INPUT_U2RX, BOARD_UART2_RX_RP))
        return false;

    return true;
}

bool board_can1_pins_init(void)
{
    /* Enable the CAN1 module before HAL init. */
    if (dspic33ak_canfd_module_enable(DSPIC33AK_CANFD_INST_1, true) != DSPIC33AK_CANFD_OK)
        return false;

    /* C1TX: idle-high output (CAN recessive state is high). */
    if (!dspic33ak_gpio_rp_config_digital_output(BOARD_CAN1_TX_RP, true))
        return false;
    if (!dspic33ak_pps_route_output(DSPIC33AK_PPS_OUTPUT_CAN1TX, BOARD_CAN1_TX_RP))
        return false;

    /* C1RX: digital input. */
    if (!dspic33ak_gpio_rp_config_digital_input(BOARD_CAN1_RX_RP))
        return false;
    if (!dspic33ak_pps_route_input(DSPIC33AK_PPS_INPUT_CAN1RX, BOARD_CAN1_RX_RP))
        return false;

    /* STBY: non-PPS output, Low = ATA6563 normal mode. Packed-pin API. */
    static const dspic33ak_gpio_config_t stby_cfg = {
        .dir = DSPIC33AK_GPIO_DIR_OUTPUT, .pull = DSPIC33AK_GPIO_PULL_NONE,
        .analog = false, .open_drain = false, .initial_high = false,
    };
    if (!dspic33ak_gpio_config(BOARD_CAN1_PIN_STBY, &stby_cfg))
        return false;

    return true;
}

bool board_spi4_sst26_pins_init(void)
{
    /* Non-PPS control pins: CS/WP (active-low, idle high) and RST (asserted low
     * here; the SST26 driver releases it after the reset-pulse delay). */
    static const dspic33ak_gpio_config_t out_high = {
        .dir = DSPIC33AK_GPIO_DIR_OUTPUT, .pull = DSPIC33AK_GPIO_PULL_NONE,
        .analog = false, .open_drain = false, .initial_high = true,
    };
    static const dspic33ak_gpio_config_t out_low = {
        .dir = DSPIC33AK_GPIO_DIR_OUTPUT, .pull = DSPIC33AK_GPIO_PULL_NONE,
        .analog = false, .open_drain = false, .initial_high = false,
    };
    if (!dspic33ak_gpio_config(BOARD_SST26_PIN_CS,  &out_high)) return false;
    if (!dspic33ak_gpio_config(BOARD_SST26_PIN_WP,  &out_high)) return false;
    if (!dspic33ak_gpio_config(BOARD_SST26_PIN_RST, &out_low))  return false;

    /* PPS-capable SPI4 signal pins via RP-first API; GPIO before PPS. */
    if (!dspic33ak_gpio_rp_config_digital_output(BOARD_SST26_SDO4_RP, false)) return false;
    if (!dspic33ak_pps_route_output(DSPIC33AK_PPS_OUTPUT_SDO4, BOARD_SST26_SDO4_RP)) return false;

    if (!dspic33ak_gpio_rp_config_digital_output(BOARD_SST26_SCK4_RP, false)) return false;
    if (!dspic33ak_pps_route_output(DSPIC33AK_PPS_OUTPUT_SCK4, BOARD_SST26_SCK4_RP)) return false;

    if (!dspic33ak_gpio_rp_config_digital_input(BOARD_SST26_SDI4_RP)) return false;
    if (!dspic33ak_pps_route_input(DSPIC33AK_PPS_INPUT_SDI4, BOARD_SST26_SDI4_RP)) return false;

    return true;
}

bool board_rgb_pins_init(void)
{
    /* RGB LED pins are PWM-driven. GPIO output (idle low) before PPS routing. */
    if (!dspic33ak_gpio_rp_config_digital_output(BOARD_RGB_BLUE_RP,  false)) return false;
    if (!dspic33ak_pps_route_output(DSPIC33AK_PPS_OUTPUT_PWM1H, BOARD_RGB_BLUE_RP))  return false;

    if (!dspic33ak_gpio_rp_config_digital_output(BOARD_RGB_GREEN_RP, false)) return false;
    if (!dspic33ak_pps_route_output(DSPIC33AK_PPS_OUTPUT_PWM2H, BOARD_RGB_GREEN_RP)) return false;

    if (!dspic33ak_gpio_rp_config_digital_output(BOARD_RGB_RED_RP,   false)) return false;
    if (!dspic33ak_pps_route_output(DSPIC33AK_PPS_OUTPUT_PWM3H, BOARD_RGB_RED_RP))   return false;

    return true;
}

bool board_spi1_tdm_smoke_pins_init(void)
{
    /* MikroBUS-A SPI1 as a self-clocked framed-mode (TDM8) MASTER for the smoke demo:
     * the dsPIC drives FS(SS1)/BCLK(SCK1)/SDO1 as outputs; SDI1 is the input (jumper
     * DataOut->DataIn for a loopback check). Per-pin: GPIO attribute first (idle low,
     * a known state before the SPI starts driving), then the PPS route. Fail-fast: a
     * GPIO failure skips that pin's PPS route so no partial config is left behind.
     * Pins/PPS only -- this does NOT touch the SPI/DMA module (the HAL owns that). */
    if (!dspic33ak_gpio_rp_config_digital_output(BOARD_TDM_SPI1_FS_RP,   false)) return false;
    if (!dspic33ak_pps_route_output(DSPIC33AK_PPS_OUTPUT_SS1,  BOARD_TDM_SPI1_FS_RP))   return false;

    if (!dspic33ak_gpio_rp_config_digital_output(BOARD_TDM_SPI1_BCLK_RP, false)) return false;
    if (!dspic33ak_pps_route_output(DSPIC33AK_PPS_OUTPUT_SCK1, BOARD_TDM_SPI1_BCLK_RP)) return false;

    if (!dspic33ak_gpio_rp_config_digital_output(BOARD_TDM_SPI1_SDO_RP,  false)) return false;
    if (!dspic33ak_pps_route_output(DSPIC33AK_PPS_OUTPUT_SDO1, BOARD_TDM_SPI1_SDO_RP))  return false;

    if (!dspic33ak_gpio_rp_config_digital_input(BOARD_TDM_SPI1_SDI_RP))          return false;
    if (!dspic33ak_pps_route_input(DSPIC33AK_PPS_INPUT_SDI1,  BOARD_TDM_SPI1_SDI_RP))   return false;

    return true;
}

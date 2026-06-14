#ifndef BOARD_PINS_H
#define BOARD_PINS_H

/*
 * board_pins.h
 * ------------
 * Board pin assignment for: Microchip Curiosity Platform Development Board with
 * a dsPIC33AK512MPS512 DIM, in its standard (unmodified) configuration.
 *
 * This is the ONE place that names the board's physical pins and the PPS
 * (peripheral pin select) codes. The HALs under src/hal know nothing about pins
 * or PPS; the board layer (board.c) wires peripherals to these pins using the
 * GPIO HAL plus the PPS registers below.
 *
 * PPS note (dsPIC33AK): _RPnnR (output) and _xxxR (input) are bit-field aliases
 * into the RPOR / RPINR SFRs. This starter writes them directly (no RPCON.IOLOCK
 * lock/unlock) to stay minimal.
 */

#include "dspic33ak_gpio.h"

/* ---- UART1 (console / printf @ 230400) ----
 *   U1TX = RH1 (RP114)   U1RX = RD1 (RP50-source code 0x32)
 */
#define BOARD_UART1_PIN_TX        DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_H, 1)
#define BOARD_UART1_PIN_RX        DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_D, 1)
#define BOARD_UART1_TX_RPnR       _RP114R    /* U1TX output pin select register */
#define BOARD_UART1_TX_PPS_FUNC   (0x13u)    /* U1TX output function code        */
#define BOARD_UART1_RX_PPS_SRC    (0x32u)    /* U1RX input  <- RP source (RD1)   */

#endif /* BOARD_PINS_H */

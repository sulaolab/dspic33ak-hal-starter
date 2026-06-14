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

/* ---- SPI4 -> SST26 external SPI NOR flash (on the Curiosity motherboard) ----
 *   SDO4 = RA12 (RP13)   SDI4 = RA13 (RP14)   SCK4 = RE1 (RP66)
 *   CS   = RD15          WP   = RE3           RST  = RE0   (active-low controls)
 */
#define BOARD_SST26_PIN_SDO       DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_A, 12)
#define BOARD_SST26_PIN_SDI       DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_A, 13)
#define BOARD_SST26_PIN_SCK       DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_E,  1)
#define BOARD_SST26_PIN_WP        DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_E,  3)
#define BOARD_SST26_PIN_CS        DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_D, 15)
#define BOARD_SST26_PIN_RST       DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_E,  0)

#define BOARD_SST26_SDI4_PPS_SRC  (14u)    /* SDI4 input  <- RP14            */
#define BOARD_SST26_SDO4_RPnR     _RP13R   /* SDO4 output pin select (RP13)  */
#define BOARD_SST26_SDO4_PPS_FUNC (34u)    /* SDO4 output function code      */
#define BOARD_SST26_SCK4_RPnR     _RP66R   /* SCK4 output pin select (RP66)  */
#define BOARD_SST26_SCK4_PPS_FUNC (35u)    /* SCK4 output function code      */

#endif /* BOARD_PINS_H */

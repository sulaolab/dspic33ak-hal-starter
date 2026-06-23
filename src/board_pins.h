#ifndef BOARD_PINS_H
#define BOARD_PINS_H

/*
 * board_pins.h
 * ------------
 * Board pin assignment for: Microchip Curiosity Platform Development Board with
 * a dsPIC33AK512MPS512 DIM, in its standard (unmodified) configuration.
 *
 * This is the ONE place that names the board's physical pins and the PPS
 * (peripheral pin select) codes. The vendored HALs under src/hal_xxx know
 * nothing about pins or PPS; the board layer (board.c) wires peripherals to
 * these pins using the GPIO HAL plus the PPS registers below.
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

/* ---- RGB LED (PWM1/2/3 -> RP51/49/58) on the Curiosity motherboard ----
 *   Blue  = PWM1H on RP51 (RD2)
 *   Green = PWM2H on RP49 (RD0)
 *   Red   = PWM3H on RP58 (RD9)
 */
#define BOARD_RGB_PIN_BLUE        DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_D, 2)
#define BOARD_RGB_PIN_GREEN       DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_D, 0)
#define BOARD_RGB_PIN_RED         DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_D, 9)
#define BOARD_RGB_BLUE_RPnR       _RP51R   /* PWM1H output pin select (RD2) */
#define BOARD_RGB_GREEN_RPnR      _RP49R   /* PWM2H output pin select (RD0) */
#define BOARD_RGB_RED_RPnR        _RP58R   /* PWM3H output pin select (RD9) */

/* ---- User LEDs and switches on the Curiosity motherboard ----
 *   LED0..LED7 = RC8..RC15  (active-high)
 *   SW1 = RF3, SW2 = RF0, SW3 = RB2  (active-low, pulled up)
 */
#define BOARD_LED0_PIN            DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C,  8)
#define BOARD_LED1_PIN            DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C,  9)
#define BOARD_LED2_PIN            DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C, 10)
#define BOARD_LED3_PIN            DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C, 11)
#define BOARD_LED4_PIN            DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C, 12)
#define BOARD_LED5_PIN            DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C, 13)
#define BOARD_LED6_PIN            DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C, 14)
#define BOARD_LED7_PIN            DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C, 15)

#define BOARD_SW1_PIN             DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_F, 3)
#define BOARD_SW2_PIN             DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_F, 0)
#define BOARD_SW3_PIN             DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_B, 2)

/* ---- Potentiometer -> ADC5 channel 0 (AD5AN0 on RA7) ---- */
#define BOARD_POT_ANSEL_PORT      ANSELA
#define BOARD_POT_ANSEL_BIT       (7u)     /* RA7 = AD5AN0 (analog input) */

/* ---- CAN1 (CAN FD) -> on-board ATA6563 transceiver (bus on J21 CANH/CANL) ----
 *   C1TX = RD13 (RP62)   C1RX = RD11 (RP60)   STBY = RD14 (GPIO, low = normal)
 * The CAN RX input PPS MUST be assigned even for internal loopback, or the
 * module never integrates and init times out.
 */
#define BOARD_CAN1_PIN_TX         DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_D, 13)
#define BOARD_CAN1_PIN_RX         DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_D, 11)
#define BOARD_CAN1_PIN_STBY       DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_D, 14)
#define BOARD_CAN1_TX_RPnR        _RP62R          /* C1TX output pin select (RP62/RD13) */
#define BOARD_CAN1_TX_PPS_FUNC    _RPOUT_CAN1TX   /* C1TX output function code           */
#define BOARD_CAN1_RX_PPS_SRC     (60u)           /* C1RX input <- RP60 (RD11)           */

#endif /* BOARD_PINS_H */

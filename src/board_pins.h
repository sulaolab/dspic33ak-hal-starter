#ifndef BOARD_PINS_H
#define BOARD_PINS_H

/*
 * board_pins.h
 * ------------
 * Board pin assignment for: Microchip Curiosity Platform Development Board with
 * a dsPIC33AK512MPS512 DIM, in its standard (unmodified) configuration.
 *
 * This is the ONE place that names the board's physical pins and the RP
 * (remappable-pin) numbers used for PPS. The vendored HALs under src/hal_xxx
 * know nothing about pins or PPS; the board layer (board.c) wires peripherals to
 * these pins using the GPIO HAL plus the PPS HAL (dspic33ak_pps_route_*).
 *
 * Pin-naming rule: PPS-capable pins are identified by their RP number alone
 * (the same number used by both the GPIO HAL's RP-first API and the PPS HAL).
 * Non-PPS pins (CS, WP, RST, STBY, LEDs, switches) are named as packed pins
 * via DSPIC33AK_GPIO_PIN(port, bit).
 *
 * PPS note (dsPIC33AK): routing goes through the GPIO HAL's companion PPS layer
 * (dspic33ak_pps_route_output/input), which self-brackets RPCON.IOLOCK. The
 * board only names the signal + the RP pin number; the raw RPORx/RPINRx
 * registers and peripheral function codes stay inside the PPS HAL.
 */

#include "dspic33ak_gpio.h"
#include "dspic33ak_pps.h"

/* ---- UART1 (console / printf @ 230400) ----
 *   U1TX = RH1 (RP114)   U1RX = RD1 (RP50)
 * PPS-capable: RP number is the single source of truth for both GPIO and PPS.
 */
#define BOARD_UART1_TX_RP         (114u)   /* U1TX output  -> RP114 (RH1), idle high */
#define BOARD_UART1_RX_RP         (50u)    /* U1RX input   <- RP50  (RD1) */

/* ---- SPI4 -> SST26 external SPI NOR flash (on the Curiosity motherboard) ----
 *   SDO4 = RA12 (RP13)   SDI4 = RA13 (RP14)   SCK4 = RE1 (RP66)
 *   CS   = RD15          WP   = RE3           RST  = RE0   (active-low controls)
 * SDO/SDI/SCK are PPS-capable (RP number only).
 * CS / WP / RST are non-PPS GPIO outputs (packed pin).
 */
#define BOARD_SST26_SDI4_RP       (14u)    /* SDI4 input  <- RP14 (RA13) */
#define BOARD_SST26_SDO4_RP       (13u)    /* SDO4 output -> RP13 (RA12) */
#define BOARD_SST26_SCK4_RP       (66u)    /* SCK4 output -> RP66 (RE1)  */

#define BOARD_SST26_PIN_WP        DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_E,  3)
#define BOARD_SST26_PIN_CS        DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_D, 15)
#define BOARD_SST26_PIN_RST       DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_E,  0)

/* ---- RGB LED (PWM1/2/3 -> RP51/49/58) on the Curiosity motherboard ----
 *   Blue  = PWM1H on RP51 (RD2)
 *   Green = PWM2H on RP49 (RD0)
 *   Red   = PWM3H on RP58 (RD9)
 * PPS-capable: RP number only.
 */
#define BOARD_RGB_BLUE_RP         (51u)    /* PWM1H output -> RP51 (RD2) */
#define BOARD_RGB_GREEN_RP        (49u)    /* PWM2H output -> RP49 (RD0) */
#define BOARD_RGB_RED_RP          (58u)    /* PWM3H output -> RP58 (RD9) */

/* ---- User LEDs and switches on the Curiosity motherboard ----
 *   LED0..LED7 = RC8..RC15  (active-high)
 *   SW1 = RF3, SW2 = RF0, SW3 = RB2  (active-low, pulled up)
 * Non-PPS: packed pin.
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
 * TX/RX are PPS-capable (RP number only). STBY is non-PPS GPIO (packed pin).
 * The CAN RX input PPS MUST be assigned even for internal loopback, or the
 * module never integrates and init times out.
 */
#define BOARD_CAN1_TX_RP          (62u)    /* C1TX output -> RP62 (RD13) */
#define BOARD_CAN1_RX_RP          (60u)    /* C1RX input  <- RP60 (RD11) */
#define BOARD_CAN1_PIN_STBY       DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_D, 14)

#endif /* BOARD_PINS_H */

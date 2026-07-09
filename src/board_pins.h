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

/* ---- UART2 (PKOB4 "USB Serial Device" -- console output mirror) ----
 *   U2TX = RH0 (RP113)   U2RX = RD10 (RP59)
 * Same Curiosity motherboard + dsPIC33AK512 DIM as the perseus firmware; this is
 * the PKOB4 back-channel UART. Phase 1 uses TX only (mirror console output);
 * U2RX (RP59) is reserved for the Phase 2 echo/mirror input path.
 */
#define BOARD_UART2_TX_RP         (113u)   /* U2TX output  -> RP113 (RH0), idle high */
#define BOARD_UART2_RX_RP         (59u)    /* U2RX input   <- RP59  (RD10), Phase 2 */

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
 * LEDs use packed pins here. Switches are RP-capable, so name them by RP number
 * and use the RP-first GPIO/CN APIs in board component code.
 */
#define BOARD_LED0_PIN            DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C,  8)
#define BOARD_LED1_PIN            DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C,  9)
#define BOARD_LED2_PIN            DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C, 10)
#define BOARD_LED3_PIN            DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C, 11)
#define BOARD_LED4_PIN            DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C, 12)
#define BOARD_LED5_PIN            DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C, 13)
#define BOARD_LED6_PIN            DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C, 14)
#define BOARD_LED7_PIN            DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C, 15)

#define BOARD_SW1_RP              ((dspic33ak_gpio_rp_t)84u)  /* RF3 */
#define BOARD_SW2_RP              ((dspic33ak_gpio_rp_t)81u)  /* RF0 */
#define BOARD_SW3_RP              ((dspic33ak_gpio_rp_t)19u)  /* RB2 */

/* ---- Potentiometer -> ADC5 channel 0 (AD5AN0 on RA7) ---- */
#define BOARD_POT_PIN             DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_A, 7)

/* ---- CAN1 (CAN FD) -> on-board ATA6563 transceiver (bus on J21 CANH/CANL) ----
 *   C1TX = RD13 (RP62)   C1RX = RD11 (RP60)   STBY = RD14 (GPIO, low = normal)
 * TX/RX are PPS-capable (RP number only). STBY is non-PPS GPIO (packed pin).
 * The CAN RX input PPS MUST be assigned even for internal loopback, or the
 * module never integrates and init times out.
 */
#define BOARD_CAN1_TX_RP          (62u)    /* C1TX output -> RP62 (RD13) */
#define BOARD_CAN1_RX_RP          (60u)    /* C1RX input  <- RP60 (RD11) */
#define BOARD_CAN1_PIN_STBY       DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_D, 14)

/* ---- MikroBUS-A SPI1 framed-mode (TDM8) smoke demo: self-clocked MASTER ----
 *   FS/CS   = RE5  (RP70)    BCLK/SCK = RE10 (RP75)
 *   DataOut = RG4  (RP101)   DataIn   = RG9  (RP106)
 * All PPS-capable (RP number only). As a master the dsPIC drives FS(SS1) / BCLK(SCK1) /
 * SDO1 as outputs; SDI1 is the input. Jumper DataOut->DataIn for a loopback check.
 * NOTE: these MikroBUS-A SPI pins are held by the TDM smoke demo when it is enabled
 * (app_config.h: HAL_STARTER_ENABLE_TDM_SMOKE_DEMO) -- disable it to use a real SPI
 * Click in MikroBUS-A. (MikroBUS-A I2C SDA/SCL are different pins, unaffected.)
 */
#define BOARD_TDM_SPI1_FS_RP      (70u)    /* SS1  output -> RP70  (RE5)  FS/CS    */
#define BOARD_TDM_SPI1_BCLK_RP    (75u)    /* SCK1 output -> RP75  (RE10) BCLK/SCK */
#define BOARD_TDM_SPI1_SDO_RP     (101u)   /* SDO1 output -> RP101 (RG4)  DataOut  */
#define BOARD_TDM_SPI1_SDI_RP     (106u)   /* SDI1 input  <- RP106 (RG9)  DataIn   */

#endif /* BOARD_PINS_H */

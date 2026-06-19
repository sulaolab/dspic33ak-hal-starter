#ifndef DSPIC33AK_CLOCK_H
#define DSPIC33AK_CLOCK_H

/*
 * dspic33ak_clock.h
 * -----------------
 * Minimal, readable clock bring-up for dsPIC33AK (dsPIC33AK512MPS512 here).
 *
 * dsPIC33AK clock tree (the part this starter uses)
 * -------------------------------------------------
 *
 *   FRC (8 MHz internal)  --> PLL1 --> 200 MHz
 *                                   |
 *                                   +--> CLKGEN1  --> System clock + peripheral
 *                                   |                 clock (I2C, ...)
 *                                   +--> CLKGEN5  --> PWM      (RGB LED)
 *                                   +--> CLKGEN6  --> ADC      (potentiometer)
 *                                   +--> CLKGEN8  --> UART     (printf @230400)
 *                                   +--> CLKGEN9  --> SPI      (SST26 flash)
 *
 * Key idea: each "CLKGEN" is a small clock generator block bound to a peripheral
 * domain. You pick its source (here: PLL1), and it feeds that peripheral. A
 * CLKGEN is configured by: select source (NOSC), enable, set divider, then
 * request the source/divider switch and wait for it to complete.
 *
 * This starter routes everything from a single 200 MHz PLL1 with divide-by-1, so
 * System / PWM / ADC / UART / SPI all run from the same well-defined clock. That
 * keeps the example easy to reason about; a real design may use different
 * sources/dividers per CLKGEN.
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CLKGEN/PLL input-source codes (the value written to xCONbits.NOSC).
 * Only the sources this starter needs are listed. */
typedef enum
{
    DSPIC33AK_CLKSRC_FRC  = 1,   /* 8 MHz internal fast RC          */
    DSPIC33AK_CLKSRC_POSC = 3,   /* primary (external) oscillator   */
    DSPIC33AK_CLKSRC_PLL1 = 5,   /* PLL1 output                     */
} dspic33ak_clock_src_t;

/* System clock produced here (PLL1 output), in Hz. The UART/SPI/etc. peripheral
 * clock equals this value because every CLKGEN below uses divide-by-1. */
#define DSPIC33AK_CLOCK_SYS_HZ   (200000000UL)

/*
 * Bring up the clock tree for this starter:
 *   - configure PLL1 from the 8 MHz FRC up to DSPIC33AK_CLOCK_SYS_HZ, then
 *   - route CLKGEN1/5/6/8/9 to PLL1 (divide-by-1).
 * Call once, early in main(), before any peripheral init.
 * Returns false if the PLL divider solution could not be found (clock left as-is).
 */
bool dspic33ak_clock_init(void);

/*
 * Route CLKGEN10 to PLL1 at divide-by-10 => 20 MHz FCAN for the CAN FD module.
 * Call after dspic33ak_clock_init() and before bringing up the CAN HAL. The CAN
 * domain is separate from CLKGEN1/5/6/8/9, so this does not affect other clocks.
 */
void dspic33ak_clock_can_init(void);

#ifdef __cplusplus
}
#endif

#endif /* DSPIC33AK_CLOCK_H */

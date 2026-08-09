#ifndef NORA_CLOCK_DSPIC33AK_H
#define NORA_CLOCK_DSPIC33AK_H

#include <stdint.h>
#include <stdbool.h>

#include "nora_clock.h"

/*
 * dsPIC33AK-only part of the Clock HAL: the CLKGEN blocks.
 *
 * WHY THIS HEADER EXISTS
 *   CLKGEN is not a clock concept, it is a dsPIC33AK BLOCK. The AK clock tree is a
 *   fan-out: two PLLs feed a set of numbered CLKGEN generators, each with its own
 *   source select and divider, and each feeding a different consumer (CLKGEN1 is
 *   the CPU's, CLKGEN13 is the CCP/SCCP time base, ...). dsPIC33C/CK has no such
 *   block at all -- it has one system PLL switched by OSCCON.OSWEN, and peripheral
 *   clocks are per-peripheral selects owned by those peripherals' own HALs
 *   (e.g. UART BCLKSEL).
 *
 *   So there is no CK implementation of these calls to write, and there never will
 *   be. Leaving them on nora_clock.h made that header look portable while its only
 *   entry point was AK-shaped. Naming them nora_clock_dspic33ak_* puts the fact at
 *   every call site instead: board code that programs a CLKGEN is AK-specific and
 *   should read that way, and board code that only needs "what is Fcy" or "switch
 *   the system clock" uses the portable face in nora_clock.h and ports as-is.
 *
 * WHO MAY INCLUDE THIS
 *   Board / platform bring-up that genuinely owns the AK clock tree
 *   (board/clock/*, the resident engine's boot platform, UART board wiring).
 *   Application and DSP code must not: if application code needs a frequency, it
 *   wants nora_clock_get_fcy_hz().
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NORA_CLOCK_DSPIC33AK_CLKGEN_1  = 1,
    NORA_CLOCK_DSPIC33AK_CLKGEN_5  = 5,
    NORA_CLOCK_DSPIC33AK_CLKGEN_6  = 6,
    NORA_CLOCK_DSPIC33AK_CLKGEN_8  = 8,
    NORA_CLOCK_DSPIC33AK_CLKGEN_9  = 9,
    NORA_CLOCK_DSPIC33AK_CLKGEN_10 = 10,
    NORA_CLOCK_DSPIC33AK_CLKGEN_12 = 12,
    /* CLKGEN13 is the CCP/SCCP time-base generator: it is what CCPxCON1.CLKSEL=1
     * selects, so it is the one generator that moves a capture time base without
     * touching any other peripheral. */
    NORA_CLOCK_DSPIC33AK_CLKGEN_13 = 13
} nora_clock_dspic33ak_clkgen_t;

typedef struct {
    nora_clock_source_t source;
    uint16_t divide_by;
} nora_clock_dspic33ak_clkgen_config_t;

/*
 * Configure a supported CLKGEN instance from a logical source and divider.
 *
 * Configuring CLKGEN1 also updates what nora_clock_get_fosc_hz() reports, because
 * on this family CLKGEN1's output IS the system clock. That works whenever the
 * source frequency is derivable inside the HAL (a PLL this HAL programmed, or the
 * on-chip FRC); for an externally-supplied source such as REFI1 the HAL cannot
 * know the frequency, so Fosc is reported as 0 (unknown) rather than guessed.
 * Declare it with nora_clock_switch_source() if a caller ever needs that case.
 */
nora_clock_status_t
nora_clock_dspic33ak_clkgen_configure(
    nora_clock_dspic33ak_clkgen_t clkgen,
    const nora_clock_dspic33ak_clkgen_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* NORA_CLOCK_DSPIC33AK_H */

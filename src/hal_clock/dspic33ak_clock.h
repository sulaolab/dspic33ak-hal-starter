#ifndef DSPIC33AK_CLOCK_H
#define DSPIC33AK_CLOCK_H

#include <stdint.h>
#include <stdbool.h>

/*
 * dsPIC33AK Clock HAL public interface.
 *
 * This HAL exposes logical PLL / CLKGEN programming requests and hides device
 * NOSC encodings, XC-DSC bitfields, and SFR names below the public API.  The
 * generic core validates PLL input, VCO, divider, and output constraints from
 * the dsPIC33AK512MPS512 DFP 1.3.185 device facts, then drives the internal
 * register layer that owns switch, ready, and timeout sequencing.
 *
 * Board policy stays above this HAL: FRC boot policy, PPS routing, REFI pin
 * selection, UART / PWM / audio clock requirements, and application frequency
 * choices belong to the board or peripheral integration layers.  No public type
 * in this header is an XC-DSC / DFP register type.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* Status types                                                               */
/* -------------------------------------------------------------------------- */

typedef enum {
    DSPIC33AK_CLOCK_OK = 0,

    /* Caller supplied a null pointer, zero divider, unknown enum, or a source
     * that the selected block cannot consume. */
    DSPIC33AK_CLOCK_ERR_INVALID_ARG,

    /* Reserved for future device-family tables where an otherwise valid logical
     * PLL / CLKGEN instance is absent on a smaller dsPIC33AK device. */
    DSPIC33AK_CLOCK_ERR_NOT_PRESENT,

    /* Reserved for future family capability checks where an instance exists but
     * the requested feature is not implemented by that device variant. */
    DSPIC33AK_CLOCK_ERR_NOT_SUPPORTED,

    /* The exact PLL output request cannot be represented within the published
     * device limits and integer divider ranges. */
    DSPIC33AK_CLOCK_ERR_UNREPRESENTABLE,

    /* A hardware switch / ready bit did not complete within the register-layer
     * polling budget. */
    DSPIC33AK_CLOCK_ERR_TIMEOUT
} dspic33ak_clock_status_t;

/* -------------------------------------------------------------------------- */
/* PLL / CLKGEN / source types                                                */
/* -------------------------------------------------------------------------- */

typedef enum {
    DSPIC33AK_CLOCK_PLL_1 = 1,
    DSPIC33AK_CLOCK_PLL_2 = 2
} dspic33ak_clock_pll_t;

typedef enum {
    DSPIC33AK_CLOCK_SOURCE_FRC,
    DSPIC33AK_CLOCK_SOURCE_BFRC,
    /* Logical primary oscillator source; device-specific NOSC encoding is not
     * exposed through this API. */
    DSPIC33AK_CLOCK_SOURCE_PRIMARY,
    DSPIC33AK_CLOCK_SOURCE_LPRC,
    DSPIC33AK_CLOCK_SOURCE_PLL1,
    DSPIC33AK_CLOCK_SOURCE_PLL2,
    DSPIC33AK_CLOCK_SOURCE_PLL1_VCO_FRACDIV,
    DSPIC33AK_CLOCK_SOURCE_PLL2_VCO_FRACDIV,
    DSPIC33AK_CLOCK_SOURCE_REFI1,
    DSPIC33AK_CLOCK_SOURCE_REFI2
} dspic33ak_clock_source_t;

typedef enum {
    DSPIC33AK_CLOCK_CLKGEN_1  = 1,
    DSPIC33AK_CLOCK_CLKGEN_5  = 5,
    DSPIC33AK_CLOCK_CLKGEN_6  = 6,
    DSPIC33AK_CLOCK_CLKGEN_8  = 8,
    DSPIC33AK_CLOCK_CLKGEN_9  = 9,
    DSPIC33AK_CLOCK_CLKGEN_10 = 10,
    DSPIC33AK_CLOCK_CLKGEN_12 = 12
} dspic33ak_clock_clkgen_t;

/* -------------------------------------------------------------------------- */
/* Configuration types                                                        */
/* -------------------------------------------------------------------------- */

typedef struct {
    dspic33ak_clock_source_t source;
    uint32_t input_hz;
    uint32_t target_hz;
} dspic33ak_clock_pll_config_t;

typedef struct {
    dspic33ak_clock_source_t source;
    uint16_t divide_by;
} dspic33ak_clock_clkgen_config_t;

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

/*
 * Configure PLL1 or PLL2 for an exact target frequency.
 *
 * actual_hz is optional and is written only after the hardware programming
 * sequence completes successfully.
 */
dspic33ak_clock_status_t
dspic33ak_clock_pll_configure(
    dspic33ak_clock_pll_t pll,
    const dspic33ak_clock_pll_config_t *config,
    uint32_t *actual_hz);

/* Configure a supported CLKGEN instance from a logical source and divider. */
dspic33ak_clock_status_t
dspic33ak_clock_clkgen_configure(
    dspic33ak_clock_clkgen_t clkgen,
    const dspic33ak_clock_clkgen_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* DSPIC33AK_CLOCK_H */

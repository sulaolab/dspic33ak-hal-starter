#ifndef NORA_CLOCK_H
#define NORA_CLOCK_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Clock HAL public interface -- the AK/CK common face.
 *
 * This HAL exposes logical PLL programming requests, a system-clock source
 * switch, and the authoritative current frequencies, and hides device NOSC
 * encodings, XC-DSC bitfields, and SFR names below the public API.  The generic
 * core validates PLL input, VCO, divider, and output constraints from the
 * dsPIC33AK512MPS512 DFP 1.3.185 device facts, then drives the internal register
 * layer that owns switch, ready, and timeout sequencing.
 *
 * Board policy stays above this HAL: FRC boot policy, PPS routing, REFI pin
 * selection, UART / PWM / audio clock requirements, and application frequency
 * choices belong to the board or peripheral integration layers.  No public type
 * in this header is an XC-DSC / DFP register type.
 *
 * WHAT IS **NOT** HERE, AND WHY
 *   The CLKGEN blocks moved to nora_clock_dspic33ak.h.  CLKGEN is a dsPIC33AK
 *   block, not a clock concept: CK has no such thing (one system PLL switched by
 *   OSCCON.OSWEN, plus per-peripheral selects), so there is no CK implementation
 *   of those calls to write.  Keeping them here made this header read as portable
 *   while its only entry point was AK-shaped.  Code that programs a CLKGEN is
 *   board bring-up and now says so at the call site; code that just needs a
 *   frequency uses nora_clock_get_fcy_hz() and ports unchanged.
 *
 * PORTABILITY OF nora_clock_source_t
 *   FRC / BFRC / PRIMARY / LPRC are oscillators every family has, and they are
 *   the only values nora_clock_switch_source() accepts.  The remaining values
 *   name nodes of the AK fan-out tree (PLL outputs, the fractional-divider taps,
 *   the REFI input pins) and exist for nora_clock_pll_configure() and the AK
 *   CLKGEN calls.  A backend that lacks one returns NORA_CLOCK_ERR_NOT_SUPPORTED
 *   rather than silently accepting it.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* Status types                                                               */
/* -------------------------------------------------------------------------- */

typedef enum {
    NORA_CLOCK_OK = 0,

    /* Caller supplied a null pointer, zero divider, unknown enum, or a source
     * that the selected block cannot consume. */
    NORA_CLOCK_ERR_INVALID_ARG,

    /* Reserved for future device-family tables where an otherwise valid logical
     * PLL / CLKGEN instance is absent on a smaller dsPIC33AK device. */
    NORA_CLOCK_ERR_NOT_PRESENT,

    /* Reserved for future family capability checks where an instance exists but
     * the requested feature is not implemented by that device variant. */
    NORA_CLOCK_ERR_NOT_SUPPORTED,

    /* The exact PLL output request cannot be represented within the published
     * device limits and integer divider ranges. */
    NORA_CLOCK_ERR_UNREPRESENTABLE,

    /* A hardware switch / ready bit did not complete within the register-layer
     * polling budget. Kept as the generic timeout value for compatibility;
     * new register-layer code reports the precise phase below. */
    NORA_CLOCK_ERR_TIMEOUT,

    NORA_CLOCK_ERR_DIV_SWITCH_TIMEOUT,
    NORA_CLOCK_ERR_PLL_SWITCH_TIMEOUT,
    NORA_CLOCK_ERR_FOUT_SWITCH_TIMEOUT,
    NORA_CLOCK_ERR_SOURCE_SWITCH_TIMEOUT,
    NORA_CLOCK_ERR_READY_TIMEOUT,
    NORA_CLOCK_ERR_POWERDOWN_TIMEOUT,
    NORA_CLOCK_ERR_CONTROL_ON_READBACK,
    NORA_CLOCK_ERR_CONTROL_OE_READBACK,
    NORA_CLOCK_ERR_CONTROL_SOURCE_READBACK,
    NORA_CLOCK_ERR_DIVIDER_READBACK
} nora_clock_status_t;

/* -------------------------------------------------------------------------- */
/* PLL / source types                                                         */
/* -------------------------------------------------------------------------- */

typedef enum {
    NORA_CLOCK_PLL_1 = 1,
    NORA_CLOCK_PLL_2 = 2
} nora_clock_pll_t;

typedef enum {
    NORA_CLOCK_SOURCE_FRC,
    NORA_CLOCK_SOURCE_BFRC,
    /* Logical primary oscillator source; device-specific NOSC encoding is not
     * exposed through this API. */
    NORA_CLOCK_SOURCE_PRIMARY,
    NORA_CLOCK_SOURCE_LPRC,
    NORA_CLOCK_SOURCE_PLL1,
    NORA_CLOCK_SOURCE_PLL2,
    NORA_CLOCK_SOURCE_PLL1_VCO_FRACDIV,
    NORA_CLOCK_SOURCE_PLL2_VCO_FRACDIV,
    NORA_CLOCK_SOURCE_REFI1,
    NORA_CLOCK_SOURCE_REFI2
} nora_clock_source_t;

/* -------------------------------------------------------------------------- */
/* Device facts                                                               */
/* -------------------------------------------------------------------------- */

/*
 * Internal Fast RC oscillator nominal frequency (device fact, 8 MHz on the
 * dsPIC33AK parts this backend builds for).  FRC is fixed regardless of the
 * system clock selection, so anything clocked directly from FRC references this
 * constant rather than the system Fcy.  Single definition point for the project.
 */
#define NORA_CLOCK_FRC_HZ (8000000UL)

/* -------------------------------------------------------------------------- */
/* Configuration types                                                        */
/* -------------------------------------------------------------------------- */

typedef struct {
    nora_clock_source_t source;
    uint32_t input_hz;
    uint32_t target_hz;
} nora_clock_pll_config_t;

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

/*
 * Configure PLL1 or PLL2 for an exact target frequency.
 *
 * actual_hz is optional and is written only after the hardware programming
 * sequence completes successfully.
 */
nora_clock_status_t
nora_clock_pll_configure(
    nora_clock_pll_t pll,
    const nora_clock_pll_config_t *config,
    uint32_t *actual_hz);

/*
 * Switch the system clock to a plain (non-PLL) oscillator source: FRC, BFRC,
 * PRIMARY, or LPRC.  input_hz is that source's frequency in Hz and is recorded as
 * the new Fosc, so get_fosc_hz()/get_fcy_hz() stay authoritative afterwards --
 * the HAL cannot measure an external oscillator, so the caller declares it.
 *
 * Returns NORA_CLOCK_ERR_NOT_SUPPORTED for any other source value: a PLL output
 * is reached through nora_clock_pll_configure(), not through here.
 */
nora_clock_status_t
nora_clock_switch_source(
    nora_clock_source_t source,
    uint32_t input_hz);

/*
 * Authoritative current system oscillator / instruction clock, in Hz.
 *
 * "Authoritative" means: what this HAL last programmed or was told, not a
 * compile-time constant that a runtime clock change could silently invalidate.
 * Returns 0 when the system clock is running from a source whose frequency the
 * HAL has not been told (see nora_clock_switch_source) -- 0 is the honest answer
 * and callers must treat it as unknown rather than dividing by it.
 */
uint32_t nora_clock_get_fosc_hz(void);
uint32_t nora_clock_get_fcy_hz(void);

#ifdef __cplusplus
}
#endif

#endif /* NORA_CLOCK_H */

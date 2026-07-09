#include "dspic33ak_clock_device.h"

/*
 * Device adaptation layer.
 *
 * Public Clock HAL APIs use logical source names.  This file is the internal
 * translation point for dsPIC33AK NOSC field encodings so the generic Clock HAL
 * core does not carry raw DFP register values.  PLL and CLKGEN blocks accept
 * different source subsets, so they intentionally have separate encoders.
 */

/* -------------------------------------------------------------------------- */
/* Encode PLL input source                                                    */
/* -------------------------------------------------------------------------- */
bool dspic33ak_clock_device_encode_pll_source(
    dspic33ak_clock_source_t source,
    uint16_t *value)
{
    if (value == 0) {
        return false;
    }

    switch (source) {
    case DSPIC33AK_CLOCK_SOURCE_FRC:
        *value = 1u;
        return true;
    case DSPIC33AK_CLOCK_SOURCE_BFRC:
        *value = 2u;
        return true;
    case DSPIC33AK_CLOCK_SOURCE_PRIMARY:
        *value = 3u;
        return true;
    case DSPIC33AK_CLOCK_SOURCE_REFI1:
        *value = 9u;
        return true;
    case DSPIC33AK_CLOCK_SOURCE_REFI2:
        *value = 10u;
        return true;
    default:
        return false;
    }
}

/* -------------------------------------------------------------------------- */
/* Encode CLKGEN input source                                                 */
/* -------------------------------------------------------------------------- */
bool dspic33ak_clock_device_encode_clkgen_source(
    dspic33ak_clock_source_t source,
    uint16_t *value)
{
    if (value == 0) {
        return false;
    }

    switch (source) {
    case DSPIC33AK_CLOCK_SOURCE_FRC:
        *value = 1u;
        return true;
    case DSPIC33AK_CLOCK_SOURCE_BFRC:
        *value = 2u;
        return true;
    case DSPIC33AK_CLOCK_SOURCE_PRIMARY:
        *value = 3u;
        return true;
    case DSPIC33AK_CLOCK_SOURCE_LPRC:
        *value = 4u;
        return true;
    case DSPIC33AK_CLOCK_SOURCE_PLL1:
        *value = 5u;
        return true;
    case DSPIC33AK_CLOCK_SOURCE_PLL2:
        *value = 6u;
        return true;
    case DSPIC33AK_CLOCK_SOURCE_PLL1_VCO_FRACDIV:
        *value = 7u;
        return true;
    case DSPIC33AK_CLOCK_SOURCE_PLL2_VCO_FRACDIV:
        *value = 8u;
        return true;
    case DSPIC33AK_CLOCK_SOURCE_REFI1:
        *value = 9u;
        return true;
    case DSPIC33AK_CLOCK_SOURCE_REFI2:
        *value = 10u;
        return true;
    default:
        return false;
    }
}

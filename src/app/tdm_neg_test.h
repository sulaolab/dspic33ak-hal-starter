#ifndef TDM_NEG_TEST_H
#define TDM_NEG_TEST_H

//===========================================================
// tdm_neg_test.h
//
// Opt-in NEGATIVE-validation self-test for the SPI/I2S/TDM HAL public contract
// (HAL_STARTER_ENABLE_TDM_NEG_TEST, default 0 in app_config.h). It drives the HAL
// through its misuse paths and asserts each returns the documented bool + the most
// specific dspic33ak_spi_i2s_tdm_error_t. It is a pure API-contract test: it configures/
// opens/starts/stops but needs no external signals (a status line's data is not checked),
// and it leaves the HAL fully stopped + closed + port-cleared so the normal TDM smoke demo
// starts cleanly afterwards. Prints one line per case and a final "[NEG] pass=N fail=M".
//
// Runs at boot BEFORE tdm_smoke_init() (main.c), under the same opt-in style as the smoke.
// Single-leg cases run in the default build; 2-leg cases (two masters / framing mismatch /
// the non-destructive double-start) are runtime-gated on instance_count()>=2 and are only
// exercised in a DSPIC33AK_TDM_USE_SPI2=1 build (logged as skipped otherwise).
//===========================================================

#include <stdbool.h>
#include <stdint.h>

// Run the negative-validation matrix. Returns true if every executed case passed
// (fail count == 0). Safe to call once at boot while the transport is stopped/closed.
bool tdm_neg_test_run( void );

#endif // TDM_NEG_TEST_H

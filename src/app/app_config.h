#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/*
 * app_config.h
 * ------------
 * Application-level demo behavior toggles for the dspic33ak-hal-starter firmware.
 *
 * These are APPLICATION choices (what the starter firmware does), NOT board facts.
 * Board pin/peripheral facts live in board_pins.h / board.h. Keep build-time demo
 * switches here so they are separate from the board description.
 */

/*
 * SPI1 framed-mode (TDM8) smoke demo -- default ON.
 *
 * When 1 (default): after the existing starter demos come up, the firmware also starts a
 * codec-less SPI1 TDM8 self-clocked master on MikroBUS-A (FS=RP70, BCLK=RP75,
 * DataOut=RP101, DataIn=RP106) and prints a one-line TDM status every ~5 s. The stream
 * runs autonomously on DMA/ISR; a start failure is reported but does NOT stop the rest of
 * the starter.
 *
 * Set to 0 to restore the classic starter firmware and FREE the MikroBUS-A SPI pins
 * (e.g. to plug in a real SPI Click board). The MikroBUS-A I2C SDA/SCL pins are different
 * and are unaffected either way.
 */
#ifndef HAL_STARTER_ENABLE_TDM_SMOKE_DEMO
#define HAL_STARTER_ENABLE_TDM_SMOKE_DEMO 1
#endif

/*
 * TDM8 smoke demo FS waveform: select the demo's frame-sync shape.
 *
 * The 50%-duty FS is a FEATURE of the SPI/I2S/TDM HAL (config field `fs_shape`); the HAL
 * emits a half-frame marker and engages CLC10 internally, the app just asks for the shape.
 * This switch only chooses what the smoke demo requests:
 *   1 (DEFAULT) : cfg.fs_shape = FS_50PCT -> ~50%-duty FS on the FS pin (RP70), 256 BCLK,
 *                 BCLK/FS ~256 (HAL CLC10 generated).
 *   0           : cfg.fs_shape = FS_PULSE -> short 1-BCLK frame sync (one pulse/frame).
 * Either way BCLK/DATA/DMA are identical; only the FS waveform differs.
 *
 * MAIN DECISION: the default is 1 (FS_50PCT). The starter demo showcases the CLC10-generated
 * 50%-duty FS (an I2S-LRCLK-style frame sync) as the headline of this build. Set to 0 for the
 * conventional short 1-BCLK frame sync. Requires HAL_STARTER_ENABLE_TDM_SMOKE_DEMO.
 */
#ifndef APP_TDM_MASTER_FS50_BY_CLC10
#define APP_TDM_MASTER_FS50_BY_CLC10 1   /* 1 = FS_50PCT (HAL CLC10, default) ; 0 = FS_PULSE */
#endif

/*
 * OPT-IN self-test (default off): exercise the runtime FS-pin restore. Requires
 * APP_TDM_MASTER_FS50_BY_CLC10 = 1. After the demo starts in FS_50PCT, it reads back the
 * external FS pin's PPS code across start -> stop -> start and prints them, proving CLC10
 * release restores the pin from CLC10OUT(78) to SS1(27). For bring-up verification only.
 */
#ifndef APP_TDM_FS_RUNTIME_SWITCH_TEST
#define APP_TDM_FS_RUNTIME_SWITCH_TEST 0
#endif

/*
 * OPT-IN UART async TX/RX self-test -- default OFF.
 *
 * When 1: after UART1 console init and before the normal boot banner, exercise
 * interrupt-driven UART TX completion, active RX abort, and interactive async
 * RX completion ("RXOK"), then continue the normal starter application.
 */
#ifndef HAL_STARTER_ENABLE_UART_ASYNC_SELFTEST
#define HAL_STARTER_ENABLE_UART_ASYNC_SELFTEST 0
#endif

/*
 * OPT-IN SPI/I2S/TDM HAL negative-validation self-test -- default OFF.
 *
 * When 1: at boot, BEFORE the TDM smoke demo, drive the SPI/I2S/TDM HAL through its misuse
 * paths and assert each returns the documented bool + dspic33ak_spi_i2s_tdm_error_t
 * (wrong-config-mode, not-open, bad-instance, already-open, already-running, topology,
 * clock-not-ready, invalid-domain), plus the non-destructive double-start on a 2-leg build.
 * Prints one line per case and a "[NEG] pass=N fail=M" summary, then leaves the HAL stopped +
 * closed so the smoke demo starts normally. Pure API-contract test -- needs no external signals.
 * The 2-leg cases are exercised only in a DSPIC33AK_TDM_USE_SPI2=1 build (logged skipped otherwise).
 * For contract-regression verification only; keep 0 for the shipped starter.
 */
#ifndef HAL_STARTER_ENABLE_TDM_NEG_TEST
#define HAL_STARTER_ENABLE_TDM_NEG_TEST 0
#endif

#endif /* APP_CONFIG_H */

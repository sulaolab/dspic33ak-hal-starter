#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/*
 * app_config.h
 * ------------
 * Application-level demo behavior toggles for the dsPIC33AK256MPS306 HAL
 * starter firmware.
 *
 * These are APPLICATION choices (what the starter firmware does), NOT board facts.
 * Board pin/peripheral facts live in board_pins.h / board.h. Keep build-time demo
 * switches here so they are separate from the board description.
 */

/*
 * SPI1 framed-mode (TDM8) smoke demo -- default ON.
 *
 * When 1 (default): after the existing starter demos come up, the firmware also starts a
 * codec-less SPI1 TDM8 self-clocked master on MikroBUS-A. For AK256MPS306 this
 * is FS=RP33, BCLK=RP42, DataOut=RP12, DataIn=RP39. It prints a one-line TDM
 * status every ~5 s. The stream runs autonomously on DMA/ISR; a start failure
 * is reported but does NOT stop the rest of the starter.
 *
 * Set to 0 to restore the classic starter firmware and FREE the MikroBUS-A SPI pins
 * (e.g. to plug in a real SPI Click board). The MikroBUS-A I2C SDA/SCL pins are different
 * and are unaffected either way.
 */
#ifndef HAL_STARTER_ENABLE_TDM_SMOKE_DEMO
#define HAL_STARTER_ENABLE_TDM_SMOKE_DEMO 1
#endif

/*
 * AK256MPS306 migration switches.
 *
 * These demos were validated on the original AK512 DIM, but their board wiring
 * is not equivalent on the AK256MPS306 DIM. Keep them build-visible, but do not
 * run them by default until each has an AK256-specific board path.
 */
#ifndef HAL_STARTER_ENABLE_SST26_DEMO
#define HAL_STARTER_ENABLE_SST26_DEMO 0
#endif

#ifndef HAL_STARTER_ENABLE_I2C_SCAN_DEMO
#define HAL_STARTER_ENABLE_I2C_SCAN_DEMO 0
#endif

#ifndef HAL_STARTER_ENABLE_I2C_LOOPBACK_DEMO
#define HAL_STARTER_ENABLE_I2C_LOOPBACK_DEMO 0
#endif

#ifndef HAL_STARTER_ENABLE_CAN_DEMO
#define HAL_STARTER_ENABLE_CAN_DEMO 0
#endif

#ifndef HAL_STARTER_ENABLE_RGB_POT_DEMO
#define HAL_STARTER_ENABLE_RGB_POT_DEMO 0
#endif

/*
 * TDM8 smoke demo FS waveform: select the demo's frame-sync shape.
 *
 * The 50%-duty FS is a FEATURE of the SPI/I2S/TDM HAL (config field `fs_shape`); the HAL
 * emits a half-frame marker and engages CLC10 internally, the app just asks for the shape.
 * This switch only chooses what the smoke demo requests:
 *   1           : cfg.fs_shape = FS_50PCT -> ~50%-duty FS on the FS pin,
 *                 BCLK/FS ~256 (HAL CLC10 generated, AK512 path only today).
 *   0           : cfg.fs_shape = FS_PULSE -> short 1-BCLK frame sync (one pulse/frame).
 * Either way BCLK/DATA/DMA are identical; only the FS waveform differs.
 *
 * MAIN DECISION: AK256MPS306 defaults to 0 because this part has CLC1..4 but
 * not the AK512 CLC10 helper used by the 50%-FS TDM path.
 * Requires HAL_STARTER_ENABLE_TDM_SMOKE_DEMO.
 */
#ifndef APP_TDM_MASTER_FS50_BY_CLC10
#define APP_TDM_MASTER_FS50_BY_CLC10 0   /* AK256MPS306 has no CLC10; use FS_PULSE by default. */
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

#endif /* APP_CONFIG_H */

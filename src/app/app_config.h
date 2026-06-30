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
 * The 50%-duty FS is now a FEATURE of the SPI/I2S/TDM HAL (config field `fs_shape`), not an
 * app-level experiment -- the HAL emits a half-frame marker and engages CLC10 internally;
 * the app just asks for the shape. This switch only chooses what the smoke demo requests:
 *   1 (default on this branch): cfg.fs_shape = FS_50PCT
 *        -> ~50%-duty FS on the FS pin (RP70), period 256 BCLK, BCLK/FS ~256.
 *   0 : cfg.fs_shape = FS_PULSE
 *        -> short 1-BCLK frame sync (one pulse per 8-word frame).
 * Either way BCLK/DATA/DMA are identical; only the FS waveform differs. Requires
 * HAL_STARTER_ENABLE_TDM_SMOKE_DEMO.
 */
#ifndef APP_TDM_MASTER_FS50_BY_CLC10
#define APP_TDM_MASTER_FS50_BY_CLC10 1   /* 1 = FS_50PCT (HAL CLC10) ; 0 = FS_PULSE */
#endif

#endif /* APP_CONFIG_H */

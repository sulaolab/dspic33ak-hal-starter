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

#endif /* APP_CONFIG_H */

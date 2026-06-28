#ifndef BOARD_H
#define BOARD_H

/*
 * board.h
 * -------
 * Board bring-up entry points. Each function wires one peripheral's pins
 * (GPIO attributes via the RP-first or packed-pin GPIO HAL + PPS routing)
 * for this board. The peripheral HALs themselves do not touch pins/PPS.
 * All pinmux functions return bool: false if any GPIO or PPS step failed.
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Set all GPIO pins to digital (clear ANSELA..D) as a clean power-on default.
 * Pins owned directly by a peripheral module (e.g. the I2C SDA/SCL pins) are not
 * configured through the GPIO HAL, so their ANSEL must be cleared here or the
 * module cannot sense the line. Per-pin analog inputs (e.g. the pot ADC) re-enable
 * ANSEL where needed. Call once at the start of main(), before peripheral init. */
void board_ports_digital_default(void);

/* Configure the UART1 console pins: U1TX digital output (idle high), U1RX
 * digital input, plus PPS routing. Returns false if any step failed. */
bool board_uart1_pins_init(void);

/* Configure the SST26 SPI4 pins: SDO/SCK digital outputs (idle low), SDI digital
 * input, CS/WP idle high (deasserted), RST asserted low; plus PPS routing.
 * RST is left asserted; the SST26 driver completes the reset pulse.
 * Returns false if any step failed. */
bool board_spi4_sst26_pins_init(void);

/* Configure the RGB LED pins (digital outputs) and route the PWM generator
 * outputs onto them via PPS. Returns false if any step failed. */
bool board_rgb_pins_init(void);

/* Configure the CAN1 (CAN FD) pins: C1TX/RX via PPS, STBY driven low; also
 * enables CAN1 module (PMD3.C1MD = 0). Returns false if any step failed. */
bool board_can1_pins_init(void);

/* Configure the MikroBUS-A SPI1 framed-mode (TDM8) smoke-demo pins for a self-clocked
 * MASTER: FS(SS1)/BCLK(SCK1)/SDO1 digital outputs (idle low) + SDI1 digital input, plus
 * PPS routing. Pins/PPS only -- it does NOT touch the SPI/DMA module. Returns false if
 * any step failed. (Used by the TDM smoke demo; see app_config.h.) */
bool board_spi1_tdm_smoke_pins_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */

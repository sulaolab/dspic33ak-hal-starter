#ifndef BOARD_H
#define BOARD_H

/*
 * board.h
 * -------
 * Board bring-up entry points. Each function wires one peripheral's pins
 * (GPIO attributes via the GPIO HAL + PPS routing) for this board. The
 * peripheral HALs themselves do not touch pins/PPS.
 */

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
 * digital input, plus PPS routing (U1TX -> RP pin, U1RX <- RP source). */
void board_uart1_pins_init(void);

/* Configure the SST26 SPI4 pins: SDO/SCK digital outputs (idle low), SDI digital
 * input, CS/WP idle high (deasserted), RST asserted low; plus PPS routing
 * (SDI4 input, SDO4/SCK4 outputs). RST is left asserted; the SST26 driver
 * completes the reset pulse. */
void board_spi4_sst26_pins_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */

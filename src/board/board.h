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

/* Configure the RGB LED pins (digital outputs) and route the PWM generator
 * outputs onto them via PPS:
 *   Blue  = PWM1H on RP51 (RD2)
 *   Green = PWM2H on RP49 (RD0)
 *   Red   = PWM3H on RP58 (RD9)
 * The PWM module itself is set up by the rgb_pot sample. */
void board_rgb_pins_init(void);

/* Configure the CAN1 (CAN FD) pins: C1TX -> RP62 (RD13), C1RX <- RP60 (RD11),
 * STBY (RD14) driven low = ATA6563 normal mode; also enables the CAN1 module
 * (PMD3.C1MD = 0). Call before the CAN HAL init; pair with
 * dspic33ak_clock_can_init() for the 20 MHz CAN clock. */
void board_can1_pins_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */

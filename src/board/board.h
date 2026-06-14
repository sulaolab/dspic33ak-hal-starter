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

/* Configure the UART1 console pins: U1TX digital output (idle high), U1RX
 * digital input, plus PPS routing (U1TX -> RP pin, U1RX <- RP source). */
void board_uart1_pins_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */

/*
 * can_rx_isr_selftest.h
 * ---------------------
 * Single-board self-test for the CAN FD RX interrupt path (no bus, transceiver
 * or partner needed). It brings CAN1 up in INTERNAL loopback, arms the RX ISR
 * with dspic33ak_canfd_isr_enable(), and proves four things on one board:
 *
 *   1) the RX callback is invoked when a frame arrives,
 *   2) the frame can be read (in the callback / after it) via receive(),
 *   3) an RX-FIFO overflow is reported (RX_OVERFLOW event),
 *   4) the transmit interrupt line (_C1TXIE) is NOT enabled by isr_enable().
 *
 * Reaching the printed summary at all also demonstrates that no unhandled-
 * interrupt trap was taken. The test leaves CAN1 quiesced (ISR disabled,
 * de-initialized) so the live can_loopback demo can bring it up afterwards.
 *
 * This file owns the CAN1 RX + general interrupt vector forwarders for the
 * single-board build; the two-board can_bus_test.c owns them when CAN_BUS_TEST=1
 * (the two are mutually exclusive, so exactly one translation unit defines them).
 */
#ifndef CAN_RX_ISR_SELFTEST_H
#define CAN_RX_ISR_SELFTEST_H

#include <stdbool.h>

/*
 * Run the RX-ISR self-test. Prints its own result lines and returns true on a
 * full pass. The caller must have already routed the CAN clock and CAN1 pins
 * (starter_clock_can_init() + board_can1_pins_init()), exactly as for the
 * can_loopback self-test.
 */
bool can_rx_isr_selftest_run(void);

#endif /* CAN_RX_ISR_SELFTEST_H */

#ifndef CAN_BUS_TEST_H
#define CAN_BUS_TEST_H

/*
 * can_bus_test.h
 * --------------
 * Two-board CAN FD bus test (example code, not a HAL). Uses CAN1 in NORMAL_FD
 * with the interrupt/event layer: the RX FIFO is drained in the ISR into a
 * software ring (so the RX interrupt does not re-storm), and the main loop
 * prints / re-transmits.
 *
 *   is_echo = false : ORIGINATOR  - sends id 0x0A0 once per second, prints RX.
 *   is_echo = true  : ECHO        - re-sends each received frame under id 0x0B0.
 *
 * Two boards, J21<->J21 (CANH/CANL/GND), 120 ohm termination (J22/J23) on BOTH.
 * Never returns. Call after starter_clock_can_init() + board_can1_pins_init();
 * forward the CAN vectors (defined in can_bus_test.c) to the HAL handler.
 */

#include <stdbool.h>

/* ---- Build switches (single source of truth, shared by main.c) ----
 * CAN_BUS_TEST:      0 = boot runs only the single-board internal-loopback
 *                        self-test, then the normal heartbeat loop (default).
 *                    1 = after that self-test, enter the two-board bus test.
 *                        Only this build defines the CAN1 interrupt vectors.
 * CAN_BUS_TEST_ECHO: 0 = ORIGINATOR (id 0x0A0), 1 = ECHO (id 0x0B0).
 * Override either on the compiler command line (e.g. -DCAN_BUS_TEST=1). */
#ifndef CAN_BUS_TEST
#define CAN_BUS_TEST       0
#endif
#ifndef CAN_BUS_TEST_ECHO
#define CAN_BUS_TEST_ECHO  0
#endif

#ifdef __cplusplus
extern "C" {
#endif

void can_bus_test_run(bool is_echo);

#ifdef __cplusplus
}
#endif

#endif /* CAN_BUS_TEST_H */

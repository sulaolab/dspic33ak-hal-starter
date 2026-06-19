#ifndef CAN_LOOPBACK_H
#define CAN_LOOPBACK_H

/*
 * can_loopback.h
 * --------------
 * CAN FD loopback self-test (example code, not a HAL).
 *
 * Brings up CAN1 in INTERNAL loopback (no transceiver, no external wiring, no
 * bus termination needed), transmits one CAN FD frame, receives it back through
 * the controller's internal loopback path, and verifies id/len/payload. This
 * exercises the CAN FD protocol engine end to end on a single bare board.
 *
 * Call once after dspic33ak_clock_can_init() (20 MHz CAN clock) and
 * board_can1_pins_init() (PPS + module enable). To instead drive the on-board
 * ATA6563 and loop back over the real CANH/CANL bus, switch the mode in
 * can_loopback.c to DSPIC33AK_CANFD_MODE_EXTERNAL_LOOPBACK (needs the J22/J23
 * termination shunts installed).
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Run the CAN1 internal-loopback self-test. Returns true if the frame round-trips
 * with matching id, length and payload. */
bool can_loopback_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* CAN_LOOPBACK_H */

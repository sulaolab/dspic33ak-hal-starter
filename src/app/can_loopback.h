#ifndef CAN_LOOPBACK_H
#define CAN_LOOPBACK_H

/*
 * can_loopback.h
 * --------------
 * CAN1 demo (example code, not a HAL). Two parts:
 *
 *  - can_loopback_selftest(): one INTERNAL-loopback round trip. Internal
 *    loopback self-ACKs and self-receives, so it verifies the CAN FD engine on
 *    a bare board with no transceiver, wiring, termination or partner. It then
 *    re-arms CAN1 in NORMAL_FD for the live demo.
 *
 *  - can_loopback_tick(): transmits one frame on the REAL bus (NORMAL_FD) each
 *    call. With an ACK partner the frame goes out once; with NO partner the
 *    controller retransmits (a burst on CANH/CANL) and climbs to bus-off, which
 *    the tick logs and recovers from. So a single board still emits a real CAN
 *    signal and the "no partner" condition is observable on a scope and in the
 *    log (state=bus-off). For a clean two-board exchange use the separate bus
 *    test (CAN_BUS_TEST=1, originator/echo with distinct ids).
 *
 * Call after dspic33ak_clock_can_init() (20 MHz CAN clock) and
 * board_can1_pins_init() (PPS + module enable + transceiver out of standby).
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Run the CAN1 internal-loopback self-test (also brings CAN1 up and leaves it
 * initialized for can_loopback_tick()). Returns true if the frame round-trips
 * with matching id, length and payload. */
bool can_loopback_selftest(void);

/* One CAN1 internal-loopback round-trip for the periodic demo: transmit an
 * 8-byte CAN FD frame (payload seeded by `beat`), receive it back and print the
 * result. No-op until can_loopback_selftest() has brought CAN1 up. */
void can_loopback_tick(uint32_t beat);

#ifdef __cplusplus
}
#endif

#endif /* CAN_LOOPBACK_H */

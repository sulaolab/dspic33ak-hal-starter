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
 *  - can_loopback_tick(): transmits one frame on the REAL CAN bus (NORMAL_FD)
 *    each call. With an ACK partner the frame is transmitted normally; with no
 *    partner the controller becomes error-passive and retransmits, so the TX
 *    queue eventually fills and later sends report timeout. A single board still
 *    emits observable CANH/CANL activity, and "no partner" shows in the log
 *    (state=error-passive, queue timeouts). For a clean two-board exchange use
 *    the separate bus test (CAN_BUS_TEST=1, originator/echo with distinct ids).
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

/* One CAN1 real-bus transmit for the periodic demo. Sends a CAN FD frame by
 * default (64 bytes, FDF+BRS; payload seeded by `beat`). If CAN_LOOPBACK_FD=0,
 * sends a classic 8-byte CAN frame instead. No-op until can_loopback_selftest()
 * has brought CAN1 up. */
void can_loopback_tick(uint32_t beat);

#ifdef __cplusplus
}
#endif

#endif /* CAN_LOOPBACK_H */

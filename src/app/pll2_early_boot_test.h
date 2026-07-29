#ifndef PLL2_EARLY_BOOT_TEST_H
#define PLL2_EARLY_BOOT_TEST_H

#include <stdbool.h>
#include <stdint.h>

/*
 * pll2_early_boot_test.h
 * -----------------------
 * Opt-in experiment for APP_BUILD_PLL2_EARLY_BOOT_TEST. Asks one question:
 *
 *   Does the known early-boot FRC 8 MHz -> PLL2 520 MHz handshake stall
 *   reproduce on this board, and if so, does a different ordering avoid it?
 *
 * The stall being hunted was recorded on a different application on the same
 * device: HAL status=5 (ERR_TIMEOUT), actual_hz=0, PLLSWEN=1 AND DIVSWEN=1 both
 * stuck, with PLL2FAIL/PLL2SCS/STOPPLL2 all clean. It stalls at the PLLSWEN
 * wait-clear inside the pinned configure_pll2(), i.e. BEFORE NOSC is written.
 * DIVSWEN is never written by that path at all, so DIVSWEN=1 is residual
 * hardware-side state. The same request succeeds later in boot.
 *
 * Two axes, one cell per boot:
 *
 *   position  P0..P4  how early in main() the attempt runs
 *   method    M0..M2  which ordering is used
 *
 * A console command writes (position, method) into persistent RAM and issues a
 * software reset. On the next boot exactly one hook fires, attempts the PLL2
 * bring-up, and stores the outcome -- it prints NOTHING, because at P0/P1/P2
 * UART1 does not exist yet. pll2_early_boot_test_report() prints the stored
 * outcome once UART1 is up.
 *
 * A hang is turned into evidence rather than a boot loop: the state is moved to
 * ATTEMPT_STARTED immediately before the PLL is touched, so a boot that finds
 * that state knows the previous boot died inside the attempt, and it disarms
 * instead of retrying. Escaping a hang needs a human MCLR or power cycle (there
 * is deliberately no watchdog -- FWDT lives in config words shared by every
 * build variation).
 *
 * See docs/pll2_early_boot_position_experiment.md for the protocol, the evidence
 * matrix, and what a given result does and does not prove.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Boot positions. P0 is not merely "earlier than P1": with PLL1 still off the
 * CPU is running on the raw FRC, which is a different condition, not just an
 * earlier one. The application whose failure this chases had already brought its
 * clock tree up, so P1 is the closest analogue and P0 is an extra data point.
 */
typedef enum {
    P2E_POS_P0_PRE_CLOCK = 0,   /* before starter_clock_init(); PLL1 off, HRT dead */
    P2E_POS_P1_POST_CLOCK,      /* right after starter_clock_init() succeeded */
    P2E_POS_P2_POST_TICK,       /* after the tick timer; still no UART */
    P2E_POS_P3_POST_UART,       /* after console_uart_init(); printf available */
    P2E_POS_P4_LATE,            /* after the boot banner: the known-PASS control */
    P2E_POS_COUNT
} p2e_position_t;

/*
 * Orderings. M1 differs from M0 in three ways at once (forced stop; dividers
 * programmed while off; NOSC written while off). Since the stall being hunted
 * happens before NOSC is ever written, an M1 pass on its own cannot attribute
 * the fix to the forced stop -- which is why M2 exists.
 */
typedef enum {
    P2E_METHOD_M0_PINNED = 0,   /* dspic33ak_clock_pll_configure() as-is */
    P2E_METHOD_M1_RESTART,      /* dspic33ak_clock_pll_restart() (forced stop + reprogram while off) */
    P2E_METHOD_M2_STOP_PINNED,  /* forced stop only, THEN the pinned configure */
    P2E_METHOD_COUNT
} p2e_method_t;

/*
 * FIRST statement in main(), before starter_clock_init(). Latches the reset
 * cause and PLL2 state, validates the persistent record, runs the tripwire
 * checks, and clears RCON's sticky bits. Read-only with respect to the clock
 * tree.
 */
void pll2_early_boot_test_early_capture(void);

/*
 * Called at all five boot positions. Returns immediately unless this exact
 * position is the armed one. Never prints, never touches the tick timer, and
 * never issues a reset -- that is what makes it legal at P0/P1/P2.
 */
void pll2_early_boot_test_hook(p2e_position_t position);

/*
 * Called after the boot banner, with UART1 and the tick timer both up. Prints
 * the stored record, and is the ONLY place that issues the software reset for a
 * repeat run (the reset path needs a running tick timer).
 */
void pll2_early_boot_test_report(void);

/* Console command bodies; see fw_command.c for the verb spellings. */
void pll2_early_boot_test_cmd_status(void);
bool pll2_early_boot_test_cmd_arm(uint8_t position, uint8_t method, uint16_t repeats);
void pll2_early_boot_test_cmd_bench(uint8_t position);
void pll2_early_boot_test_cmd_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* PLL2_EARLY_BOOT_TEST_H */

#ifndef PLL2_RESTART_TEST_H
#define PLL2_RESTART_TEST_H

#include <stdint.h>
#include <stdbool.h>

/*
 * pll2_restart_test.h
 * --------------------
 * APP_BUILD_PLL2_RESTART_TEST experiment orchestration (see
 * docs/pll2_soft_reset_restart_experiment.md): force-stop/reprogram/relock PLL2
 * from any prior state -- off, locked, or left mid-handshake by a software
 * reset -- and repeat that across a software-reset campaign of N iterations,
 * using persistent RAM to survive each reset.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Must be the very first statement in main(), before starter_clock_init().
 * Captures RCON/OSCCTRL/PLL2 state exactly as the prior reset left it, and
 * decides (but does not yet act on) whether a persisted campaign may
 * continue. Pure read: touches no register other than RCON's clear at the
 * end of this call (see dspic33ak_clock_diag_clear_rcon()). */
void pll2_restart_test_early_capture(void);

/* Print the snapshot captured by pll2_restart_test_early_capture(), the
 * decoded reset cause, and whether a campaign is being continued or was
 * cleared. Call once UART1 is up. */
void pll2_restart_test_print_early_capture(void);

/* Takes over main()'s boot sequence for APP_BUILD_PLL2_RESTART_TEST: if a
 * campaign is being continued, performs exactly one more restart iteration,
 * updates counters, and either issues the next software reset (remaining > 0
 * and PASS) or stops the campaign (remaining == 0, or a FAIL). Otherwise
 * prints the console help and returns to main()'s normal command loop --
 * this function DOES return in that case, unlike can_bus_test_run(). */
void pll2_restart_test_run(void);

/* Console command entry points, called from fw_command.c under
 * HAL_STARTER_ENABLE_PLL2_RESTART_TEST. Each prints its own result. */
void pll2_restart_test_cmd_status(void);
void pll2_restart_test_cmd_single(void);
void pll2_restart_test_cmd_double(void);
void pll2_restart_test_cmd_cancel(void);

/* count must be in [1, 9999]; returns false (prints an error) otherwise. */
bool pll2_restart_test_cmd_campaign_start(uint16_t count);

#ifdef __cplusplus
}
#endif

#endif /* PLL2_RESTART_TEST_H */

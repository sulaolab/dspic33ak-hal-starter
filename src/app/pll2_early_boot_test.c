#include "pll2_early_boot_test.h"

#include <stdio.h>

#include "dspic33ak_clock.h"
#include "dspic33ak_clock_restart.h"
#include "dspic33ak_clock_diag.h"
#include "dspic33ak_tick_timer.h"
#include "dspic33ak_uart.h"

#define P2E_FRC_HZ            (8000000UL)
#define P2E_TARGET_HZ         (520000000UL)
#define P2E_UART              DSPIC33AK_UART_INST_1
#define P2E_MAGIC             (0x50324542UL)   /* ASCII "P2EB" */
#define P2E_RESET_DELAY_MS    (75u)
#define P2E_MAX_REPEATS       (99u)

/*
 * The one request under test, shared by all three methods so the arms differ
 * ONLY in the order of operations, never in what is being asked for.
 */
static const dspic33ak_clock_pll_config_t s_request = {
    DSPIC33AK_CLOCK_SOURCE_FRC,
    P2E_FRC_HZ,
    P2E_TARGET_HZ,
};

typedef enum {
    P2E_STATE_IDLE = 0,
    P2E_STATE_ARMED,
    P2E_STATE_ATTEMPT_STARTED,
    P2E_STATE_ATTEMPT_DONE,
    P2E_STATE_REPORTED,
    P2E_STATE_TRIPPED
} p2e_state_t;

typedef enum {
    P2E_TRIP_NONE = 0,
    P2E_TRIP_IN_ATTEMPT,     /* previous boot died inside the PLL attempt */
    P2E_TRIP_AFTER_ATTEMPT   /* attempt returned, but that boot never reported */
} p2e_trip_kind_t;

typedef struct {
    uint32_t magic;
    uint32_t magic_inverse;

    uint8_t  state;
    uint8_t  armed_position;
    uint8_t  armed_method;
    uint8_t  repeats_remaining;

    uint8_t  trip_kind;
    uint8_t  trip_position;
    uint8_t  trip_method;
    uint8_t  hang_count;

    uint8_t  attempts_done;
    uint8_t  pass_count;
    uint8_t  fail_count;
    uint8_t  have_result;

    uint8_t  last_position;
    uint8_t  last_method;
    uint8_t  last_stage;      /* pll_restart_stage_t; 0 for the pinned-only arm */
    uint8_t  post_stop_bits;  /* pllsw | foutsw<<1 | osw<<2 | divsw<<3 */

    int16_t  last_status;     /* dspic33ak_clock_status_t */
    uint32_t last_actual_hz;

    uint32_t t_stop_us;
    uint32_t t_pllsw_us;
    uint32_t t_fout_us;
    uint32_t t_osw_us;
    uint32_t t_lock_us;

    uint32_t post_oscctrl;
    uint32_t post_pll2con;
    uint32_t post_pll2div;
    uint32_t post_clkfail;
    uint32_t post_scsfail;
    uint32_t post_clkdiag;
} p2e_record_t;

/*
 * Survives a software reset: crt0 does not zero-initialize a `persistent`
 * variable. Self-verified via the magic pair below.
 *
 * Deliberate divergence from pll2_restart_test.c's s_campaign: this record is
 * trusted on the magic pair ALONE, with no "was the reset a clean SWR" gate.
 * The whole point of the tripwire is to survive a boot that HUNG, and a hang is
 * escaped by MCLR or a power cycle -- which show EXTR or POR, not SWR. An
 * SWR-gated record would be wiped in exactly the case it exists for.
 */
static p2e_record_t __attribute__((persistent)) s_rec;

static dspic33ak_clock_diag_snapshot_t s_early;
static bool s_fired_this_boot;

/* ---------------------------------------------------------------------- */
/* Naming helpers                                                          */
/* ---------------------------------------------------------------------- */

static const char *position_name(uint8_t pos)
{
    switch (pos) {
    case P2E_POS_P0_PRE_CLOCK: return "P0";
    case P2E_POS_P1_POST_CLOCK: return "P1";
    case P2E_POS_P2_POST_TICK: return "P2";
    case P2E_POS_P3_POST_UART: return "P3";
    case P2E_POS_P4_LATE: return "P4";
    default: return "P?";
    }
}

static const char *method_name(uint8_t method)
{
    switch (method) {
    case P2E_METHOD_M0_PINNED: return "M0";
    case P2E_METHOD_M1_RESTART: return "M1";
    case P2E_METHOD_M2_STOP_PINNED: return "M2";
    default: return "M?";
    }
}

static const char *state_name(uint8_t state)
{
    switch (state) {
    case P2E_STATE_IDLE: return "IDLE";
    case P2E_STATE_ARMED: return "ARMED";
    case P2E_STATE_ATTEMPT_STARTED: return "ATTEMPT_STARTED";
    case P2E_STATE_ATTEMPT_DONE: return "ATTEMPT_DONE";
    case P2E_STATE_REPORTED: return "REPORTED";
    case P2E_STATE_TRIPPED: return "TRIPPED";
    default: return "?";
    }
}

static const char *reset_cause_name(const dspic33ak_clock_diag_snapshot_t *s)
{
    if (s->rcon_por)  { return "POR"; }
    if (s->rcon_bor)  { return "BOR"; }
    if (s->rcon_wdto) { return "WDTO"; }
    if (s->rcon_extr) { return "EXTR"; }
    if (s->rcon_cm)   { return "CM"; }
    if (s->rcon_swr)  { return "SWR"; }
    return "NONE";
}

/* ---------------------------------------------------------------------- */
/* Record helpers                                                          */
/* ---------------------------------------------------------------------- */

static bool record_intact(void)
{
    return (s_rec.magic == P2E_MAGIC) &&
           (s_rec.magic_inverse == (uint32_t)~P2E_MAGIC);
}

static void record_clear(void)
{
    uint8_t *p = (uint8_t *)&s_rec;
    uint16_t i;

    for (i = 0u; i < (uint16_t)sizeof(s_rec); i++) {
        p[i] = 0u;
    }
    s_rec.magic = P2E_MAGIC;
    s_rec.magic_inverse = (uint32_t)~P2E_MAGIC;
    s_rec.state = (uint8_t)P2E_STATE_IDLE;
}

static void record_disarm(void)
{
    s_rec.armed_position = 0u;
    s_rec.armed_method = 0u;
    s_rec.repeats_remaining = 0u;
}

/* ---------------------------------------------------------------------- */
/* Boot-time hooks                                                         */
/* ---------------------------------------------------------------------- */

void pll2_early_boot_test_early_capture(void)
{
    dspic33ak_clock_diag_capture(&s_early);
    s_fired_this_boot = false;

    if (!record_intact()) {
        /* Fresh silicon, a power-on with garbage RAM, or a corrupted record. */
        record_clear();
    } else if (s_rec.state == (uint8_t)P2E_STATE_ATTEMPT_STARTED) {
        /* The previous boot set this immediately before touching PLL2 and never
         * cleared it: it died INSIDE the attempt. Disarm so this does not become
         * a boot loop -- the whole point is that the position is now known. */
        s_rec.trip_kind = (uint8_t)P2E_TRIP_IN_ATTEMPT;
        s_rec.trip_position = s_rec.armed_position;
        s_rec.trip_method = s_rec.armed_method;
        if (s_rec.hang_count < 255u) { s_rec.hang_count++; }
        s_rec.state = (uint8_t)P2E_STATE_TRIPPED;
        record_disarm();
    } else if (s_rec.state == (uint8_t)P2E_STATE_ATTEMPT_DONE) {
        /* The attempt returned and its result was stored, but that boot never
         * reached the report point. Keep the result -- it is still valid and
         * still gets printed. Ambiguous with "power was cut between the attempt
         * and the print"; both land here. */
        s_rec.trip_kind = (uint8_t)P2E_TRIP_AFTER_ATTEMPT;
        s_rec.trip_position = s_rec.last_position;
        s_rec.trip_method = s_rec.last_method;
        if (s_rec.hang_count < 255u) { s_rec.hang_count++; }
        s_rec.state = (uint8_t)P2E_STATE_TRIPPED;
        record_disarm();
    }
    /* ARMED -> leave it; the matching hook fires. REPORTED / TRIPPED / IDLE ->
     * nothing armed, boot normally. */

    /* Latch done: clear RCON's sticky bits so a later software reset in this
     * same power session is not misread as "POR still set". */
    dspic33ak_clock_diag_clear_rcon();
}

static void store_post_registers(void)
{
    dspic33ak_clock_diag_snapshot_t s;

    dspic33ak_clock_diag_capture(&s);
    s_rec.post_oscctrl = s.oscctrl;
    s_rec.post_pll2con = s.pll2con;
    s_rec.post_pll2div = s.pll2div;
    s_rec.post_clkfail = s.clkfail;
    s_rec.post_scsfail = s.scsfail;
    s_rec.post_clkdiag = s.clkdiag;
}

static void store_stop_bits(const dspic33ak_clock_pll_stop_report_t *stop)
{
    s_rec.post_stop_bits = (uint8_t)((stop->post_stop_pllswen ? 1u : 0u) |
                                     (stop->post_stop_foutswen ? 2u : 0u) |
                                     (stop->post_stop_oswen ? 4u : 0u) |
                                     (stop->post_stop_divswen ? 8u : 0u));
}

/*
 * The three orderings. Nothing here prints, waits on the tick timer, or resets:
 * that is exactly what makes these callable at P0/P1/P2 where UART1 and Timer1
 * do not exist yet.
 */
static dspic33ak_clock_status_t run_attempt(uint8_t method, uint32_t *actual_hz)
{
    dspic33ak_clock_status_t status;

    s_rec.last_stage = 0u;
    s_rec.post_stop_bits = 0u;
    s_rec.t_stop_us = 0u;
    s_rec.t_pllsw_us = 0u;
    s_rec.t_fout_us = 0u;
    s_rec.t_osw_us = 0u;
    s_rec.t_lock_us = 0u;

    switch (method) {
    case P2E_METHOD_M0_PINNED:
        /* Baseline: the pinned HAL path, byte-for-byte what the reported
         * early-boot stall was observed in. */
        status = dspic33ak_clock_pll_configure(DSPIC33AK_CLOCK_PLL_2, &s_request, actual_hz);
        break;

    case P2E_METHOD_M1_RESTART: {
        const dspic33ak_clock_pll_restart_report_t *r;

        status = dspic33ak_clock_pll_restart(DSPIC33AK_CLOCK_PLL_2, &s_request, actual_hz);
        r = dspic33ak_clock_pll_restart_last_report();
        s_rec.last_stage = (uint8_t)r->last_stage;
        s_rec.post_stop_bits = (uint8_t)((r->post_stop_pllswen ? 1u : 0u) |
                                         (r->post_stop_foutswen ? 2u : 0u) |
                                         (r->post_stop_oswen ? 4u : 0u) |
                                         (r->post_stop_divswen ? 8u : 0u));
        s_rec.t_stop_us = r->stop_time_us;
        s_rec.t_pllsw_us = r->pllswen_time_us;
        s_rec.t_fout_us = r->foutswen_time_us;
        s_rec.t_osw_us = r->oswen_time_us;
        s_rec.t_lock_us = r->lock_time_us;
        break;
    }

    case P2E_METHOD_M2_STOP_PINNED: {
        /* Forced stop ONLY, then the pinned configure. This is the arm that
         * separates "the forced stop is what helps" from "programming the
         * dividers and NOSC while off is what helps" -- M1 changes both at
         * once, and the reported stall happens before NOSC is even written. */
        dspic33ak_clock_pll_stop_report_t stop;

        status = dspic33ak_clock_pll_force_stop(DSPIC33AK_CLOCK_PLL_2, &stop);
        s_rec.t_stop_us = stop.stop_time_us;
        store_stop_bits(&stop);
        if (status != DSPIC33AK_CLOCK_OK) {
            break;   /* the PLL never reported itself off; do not pile on */
        }
        /* A stale request bit is recorded but NOT treated as fatal here: the
         * question this arm asks is whether the pinned configure succeeds after
         * a forced stop, including when the stop left something behind. */
        status = dspic33ak_clock_pll_configure(DSPIC33AK_CLOCK_PLL_2, &s_request, actual_hz);
        break;
    }

    default:
        status = DSPIC33AK_CLOCK_ERR_INVALID_ARG;
        break;
    }

    return status;
}

void pll2_early_boot_test_hook(p2e_position_t position)
{
    uint32_t actual_hz = 0u;
    dspic33ak_clock_status_t status;

    if (s_fired_this_boot) {
        return;
    }
    if (s_rec.state != (uint8_t)P2E_STATE_ARMED) {
        return;
    }
    if (s_rec.armed_position != (uint8_t)position) {
        return;
    }

    s_fired_this_boot = true;
    s_rec.last_position = (uint8_t)position;
    s_rec.last_method = s_rec.armed_method;

    /* Tripwire BEFORE touching PLL2: if this attempt never returns, the next
     * boot finds ATTEMPT_STARTED and knows exactly where it died. */
    s_rec.state = (uint8_t)P2E_STATE_ATTEMPT_STARTED;

    status = run_attempt(s_rec.armed_method, &actual_hz);

    s_rec.last_status = (int16_t)status;
    s_rec.last_actual_hz = actual_hz;
    s_rec.have_result = 1u;
    store_post_registers();
    if (s_rec.attempts_done < 255u) { s_rec.attempts_done++; }
    if (status == DSPIC33AK_CLOCK_OK) {
        if (s_rec.pass_count < 255u) { s_rec.pass_count++; }
    } else {
        if (s_rec.fail_count < 255u) { s_rec.fail_count++; }
    }

    s_rec.state = (uint8_t)P2E_STATE_ATTEMPT_DONE;
    /* Deliberately no print and no reset here. */
}

/* ---------------------------------------------------------------------- */
/* Software reset (only ever called from the report path)                  */
/* ---------------------------------------------------------------------- */

static void drain_uart1(void)
{
    uint32_t guard = 0u;

    while (!dspic33ak_uart_tx_done(P2E_UART) && (guard < 2000000u)) {
        guard++;
    }
}

static void wait_ms(uint32_t wait_ms_value)
{
    uint32_t start = dspic33ak_tick_timer_get_ms();

    while ((uint32_t)(dspic33ak_tick_timer_get_ms() - start) < wait_ms_value) {
        ;
    }
}

/* Requires a running Timer1 (wait_ms) and an initialized UART1 (drain), which
 * is why only pll2_early_boot_test_report() may call it -- never the hook. */
static void issue_software_reset(void)
{
    drain_uart1();
    wait_ms(P2E_RESET_DELAY_MS);
    __asm__ volatile ("reset");
    for (;;) { }   /* unreachable */
}

/* ---------------------------------------------------------------------- */
/* Reporting                                                               */
/* ---------------------------------------------------------------------- */

static void print_help(void)
{
    printf(" [P2E] commands: ?p2e  *p2ea<p><m>  *p2er<p><m><NN>  *p2eb<p>  *p2ex\r\n");
    printf(" [P2E] p=0..4 boot position, m=0 pinned / 1 restart / 2 stop+pinned\r\n");
}

static void print_record(void)
{
    printf(" [P2E] rec state=%s armed=%s/%s rep=%u done=%u pass=%u fail=%u hang=%u\r\n",
           state_name(s_rec.state),
           position_name(s_rec.armed_position), method_name(s_rec.armed_method),
           (unsigned)s_rec.repeats_remaining, (unsigned)s_rec.attempts_done,
           (unsigned)s_rec.pass_count, (unsigned)s_rec.fail_count,
           (unsigned)s_rec.hang_count);

    if (s_rec.trip_kind == (uint8_t)P2E_TRIP_IN_ATTEMPT) {
        printf(" [P2E] trip HANG-IN-ATTEMPT at %s/%s - auto-disarmed (hang #%u)\r\n",
               position_name(s_rec.trip_position), method_name(s_rec.trip_method),
               (unsigned)s_rec.hang_count);
    } else if (s_rec.trip_kind == (uint8_t)P2E_TRIP_AFTER_ATTEMPT) {
        printf(" [P2E] trip HANG-AFTER-ATTEMPT at %s/%s - result below is valid,"
               " that boot did not finish (hang #%u)\r\n",
               position_name(s_rec.trip_position), method_name(s_rec.trip_method),
               (unsigned)s_rec.hang_count);
    } else {
        printf(" [P2E] trip none\r\n");
    }

    if (s_rec.have_result == 0u) {
        printf(" [P2E] last (no attempt recorded yet)\r\n");
        return;
    }

    printf(" [P2E] last %s/%s status=%d stage=%u hz=%lu\r\n",
           position_name(s_rec.last_position), method_name(s_rec.last_method),
           (int)s_rec.last_status, (unsigned)s_rec.last_stage,
           (unsigned long)s_rec.last_actual_hz);
    printf(" [P2E] time off=%luus pllsw=%luus fout=%luus osw=%luus lock=%luus poststop=0x%X\r\n",
           (unsigned long)s_rec.t_stop_us, (unsigned long)s_rec.t_pllsw_us,
           (unsigned long)s_rec.t_fout_us, (unsigned long)s_rec.t_osw_us,
           (unsigned long)s_rec.t_lock_us, (unsigned)s_rec.post_stop_bits);
    printf(" [P2E] post OSCCTRL=%08lX PLL2CON=%08lX PLL2DIV=%08lX\r\n",
           (unsigned long)s_rec.post_oscctrl, (unsigned long)s_rec.post_pll2con,
           (unsigned long)s_rec.post_pll2div);
    printf(" [P2E] post CLKFAIL=%08lX SCSFAIL=%08lX CLKDIAG=%08lX\r\n",
           (unsigned long)s_rec.post_clkfail, (unsigned long)s_rec.post_scsfail,
           (unsigned long)s_rec.post_clkdiag);
}

static void print_early(void)
{
    const dspic33ak_clock_diag_snapshot_t *s = &s_early;

    printf(" [P2E] early reset=%s en=%u rdy=%u on=%u clk=%u nosc=%u cosc=%u\r\n",
           reset_cause_name(s),
           (unsigned)s->pll2en, (unsigned)s->pll2rdy, (unsigned)s->pll2_on,
           (unsigned)s->pll2_clkrdy, (unsigned)s->pll2_nosc, (unsigned)s->pll2_cosc);
    printf(" [P2E] early pllsw=%u foutsw=%u osw=%u divsw=%u fail=%u scs=%u stop=%u\r\n",
           (unsigned)s->pll2_pllswen, (unsigned)s->pll2_foutswen,
           (unsigned)s->pll2_oswen, (unsigned)s->pll2_divswen,
           (unsigned)s->pll2fail, (unsigned)s->pll2scs, (unsigned)s->stoppll2);
}

void pll2_early_boot_test_report(void)
{
    /* P4 is the late control: it is armed like any other position, but its hook
     * site is here rather than in main()'s bring-up sequence. */
    pll2_early_boot_test_hook(P2E_POS_P4_LATE);

    print_early();
    print_record();

    if (s_rec.state != (uint8_t)P2E_STATE_ATTEMPT_DONE) {
        print_help();
        return;
    }

    s_rec.state = (uint8_t)P2E_STATE_REPORTED;

    if (s_rec.repeats_remaining > 0u) {
        s_rec.repeats_remaining--;
        s_rec.state = (uint8_t)P2E_STATE_ARMED;
        printf(" [P2E] repeat: %u left, resetting...\r\n",
               (unsigned)s_rec.repeats_remaining + 1u);
        issue_software_reset();   /* never returns */
    }

    print_help();
}

/* ---------------------------------------------------------------------- */
/* Console commands                                                        */
/* ---------------------------------------------------------------------- */

void pll2_early_boot_test_cmd_status(void)
{
    dspic33ak_clock_diag_snapshot_t s;

    dspic33ak_clock_diag_capture(&s);
    printf("\r\n\"?p2e\" now  en=%u rdy=%u on=%u clk=%u nosc=%u cosc=%u"
           " pllsw=%u foutsw=%u osw=%u divsw=%u\r\n",
           (unsigned)s.pll2en, (unsigned)s.pll2rdy, (unsigned)s.pll2_on,
           (unsigned)s.pll2_clkrdy, (unsigned)s.pll2_nosc, (unsigned)s.pll2_cosc,
           (unsigned)s.pll2_pllswen, (unsigned)s.pll2_foutswen,
           (unsigned)s.pll2_oswen, (unsigned)s.pll2_divswen);
    print_early();
    print_record();
    print_help();
}

bool pll2_early_boot_test_cmd_arm(uint8_t position, uint8_t method, uint16_t repeats)
{
    if (position >= (uint8_t)P2E_POS_COUNT) {
        printf("\r\n\"*p2ea\" position must be 0..%u.\r\n",
               (unsigned)P2E_POS_COUNT - 1u);
        return false;
    }
    if (method >= (uint8_t)P2E_METHOD_COUNT) {
        printf("\r\n\"*p2ea\" method must be 0..%u.\r\n",
               (unsigned)P2E_METHOD_COUNT - 1u);
        return false;
    }
    if (repeats > P2E_MAX_REPEATS) {
        printf("\r\n\"*p2er\" repeat count must be 01..%u.\r\n",
               (unsigned)P2E_MAX_REPEATS);
        return false;
    }

    record_clear();
    s_rec.armed_position = position;
    s_rec.armed_method = method;
    /* One attempt happens on the next boot; `repeats` is the TOTAL, so the
     * remainder queued after that first one is repeats-1. */
    s_rec.repeats_remaining = (uint8_t)((repeats > 1u) ? (repeats - 1u) : 0u);
    s_rec.state = (uint8_t)P2E_STATE_ARMED;

    printf("\r\n\"*p2e\" armed %s/%s x%u. Resetting...\r\n",
           position_name(position), method_name(method),
           (unsigned)((repeats > 0u) ? repeats : 1u));
    issue_software_reset();
    return true;   /* unreachable */
}

void pll2_early_boot_test_cmd_bench(uint8_t position)
{
    uint8_t m;

    /* Same-session A/B/C at the CURRENT (late) point in the boot -- no reset, so
     * `position` is only recorded as a label. Useful as a sanity check that all
     * three orderings agree once the machine is fully up; it is NOT a substitute
     * for arming a real early position. */
    printf("\r\n\"*p2eb\" running M0, M1, M2 here and now (label=%s):\r\n",
           position_name(position));

    for (m = 0u; m < (uint8_t)P2E_METHOD_COUNT; m++) {
        uint32_t hz = 0u;
        dspic33ak_clock_status_t status;

        s_rec.last_position = position;
        s_rec.last_method = m;
        status = run_attempt(m, &hz);
        s_rec.last_status = (int16_t)status;
        s_rec.last_actual_hz = hz;
        s_rec.have_result = 1u;
        store_post_registers();

        printf(" [P2E] bench %s status=%d stage=%u hz=%lu"
               " off=%luus pllsw=%luus fout=%luus osw=%luus lock=%luus poststop=0x%X\r\n",
               method_name(m), (int)status, (unsigned)s_rec.last_stage,
               (unsigned long)hz,
               (unsigned long)s_rec.t_stop_us, (unsigned long)s_rec.t_pllsw_us,
               (unsigned long)s_rec.t_fout_us, (unsigned long)s_rec.t_osw_us,
               (unsigned long)s_rec.t_lock_us, (unsigned)s_rec.post_stop_bits);
    }
}

void pll2_early_boot_test_cmd_clear(void)
{
    record_clear();
    printf("\r\n\"*p2ex\" record cleared and disarmed.\r\n");
}

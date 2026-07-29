#include "pll2_restart_test.h"

#include <stdio.h>

#include "dspic33ak_clock.h"
#include "dspic33ak_clock_restart.h"
#include "dspic33ak_clock_diag.h"
#include "dspic33ak_tick_timer.h"
#include "dspic33ak_uart.h"

#define PLL2_TEST_FRC_HZ         (8000000UL)
#define PLL2_TEST_TARGET_HZ      (520000000UL)
#define PLL2_TEST_UART           DSPIC33AK_UART_INST_1
#define PLL2_TEST_MAGIC          (0x50324352UL)  /* ASCII "P2CR" */
#define PLL2_TEST_RESET_DELAY_MS (75u)
#define PLL2_TEST_MAX_COUNT      (9999u)

typedef struct {
    uint32_t magic;
    uint32_t magic_inverse;
    uint32_t requested_count;
    uint32_t completed_count;
    uint32_t pass_count;
    uint32_t fail_count;
    uint32_t remaining_count;
    uint32_t last_failed_stage;
} pll2_campaign_state_t;

/* Survives a software reset: crt0 does not zero-initialize a `persistent`
 * variable, so its content from the previous run is whatever it was left as.
 * self-verified via magic/magic_inverse below; see
 * docs/pll2_soft_reset_restart_experiment.md section 10. */
static pll2_campaign_state_t __attribute__((persistent)) s_campaign;

static dspic33ak_clock_diag_snapshot_t s_early_snapshot;
static bool s_campaign_continuing;

static const dspic33ak_clock_pll_config_t s_test_config = {
    DSPIC33AK_CLOCK_SOURCE_FRC,
    PLL2_TEST_FRC_HZ,
    PLL2_TEST_TARGET_HZ,
};

/* ---------------------------------------------------------------------- */
/* Campaign state helpers                                                  */
/* ---------------------------------------------------------------------- */

static void campaign_clear(void)
{
    s_campaign.magic = PLL2_TEST_MAGIC;
    s_campaign.magic_inverse = (uint32_t)~PLL2_TEST_MAGIC;
    s_campaign.requested_count = 0u;
    s_campaign.completed_count = 0u;
    s_campaign.pass_count = 0u;
    s_campaign.fail_count = 0u;
    s_campaign.remaining_count = 0u;
    s_campaign.last_failed_stage = 0u;
}

static bool campaign_struct_intact(void)
{
    return (s_campaign.magic == PLL2_TEST_MAGIC) &&
           (s_campaign.magic_inverse == (uint32_t)~PLL2_TEST_MAGIC);
}

static bool campaign_in_progress(void)
{
    return campaign_struct_intact() &&
           (s_campaign.remaining_count > 0u) &&
           (s_campaign.requested_count > 0u);
}

/* Only a clean software reset (RCON.SWR set, and no POR/BOR/WDTO/EXTR/CM bit
 * also set) is trusted to continue a campaign. RCON bits accumulate across
 * resets within one power session unless cleared, so this check is only
 * meaningful directly after pll2_restart_test_early_capture() clears them. */
static bool reset_cause_is_clean_swr(const dspic33ak_clock_diag_snapshot_t *s)
{
    return (s->rcon_swr != 0u) &&
           (s->rcon_por == 0u) && (s->rcon_bor == 0u) &&
           (s->rcon_wdto == 0u) && (s->rcon_extr == 0u) && (s->rcon_cm == 0u);
}

/* ---------------------------------------------------------------------- */
/* Boot-time hooks                                                         */
/* ---------------------------------------------------------------------- */

void pll2_restart_test_early_capture(void)
{
    dspic33ak_clock_diag_capture(&s_early_snapshot);

    if (campaign_struct_intact() && reset_cause_is_clean_swr(&s_early_snapshot)) {
        s_campaign_continuing = campaign_in_progress();
    } else {
        campaign_clear();
        s_campaign_continuing = false;
    }

    /* Clear RCON's sticky bits now that this boot's cause has been latched,
     * so a later software reset in this same power session is not seen as
     * "POR still set" (see dspic33ak_clock_diag_clear_rcon()'s header note). */
    dspic33ak_clock_diag_clear_rcon();
}

static const char *reset_cause_str(const dspic33ak_clock_diag_snapshot_t *s)
{
    if (s->rcon_por)  { return "POR"; }
    if (s->rcon_bor)  { return "BOR"; }
    if (s->rcon_wdto) { return "WDTO"; }
    if (s->rcon_extr) { return "EXTR"; }
    if (s->rcon_cm)   { return "CM"; }
    if (s->rcon_swr)  { return "SWR"; }
    return "NONE";
}

void pll2_restart_test_print_early_capture(void)
{
    const dspic33ak_clock_diag_snapshot_t *s = &s_early_snapshot;

    printf(" [P2] early reset=%s en=%u rdy=%u on=%u clk=%u nosc=%u cosc=%u\r\n",
           reset_cause_str(s),
           (unsigned)s->pll2en, (unsigned)s->pll2rdy, (unsigned)s->pll2_on,
           (unsigned)s->pll2_clkrdy, (unsigned)s->pll2_nosc, (unsigned)s->pll2_cosc);
    printf(" [P2] early pllsw=%u foutsw=%u osw=%u divsw=%u fail=%u scs=%u stop=%u\r\n",
           (unsigned)s->pll2_pllswen, (unsigned)s->pll2_foutswen,
           (unsigned)s->pll2_oswen, (unsigned)s->pll2_divswen,
           (unsigned)s->pll2fail, (unsigned)s->pll2scs, (unsigned)s->stoppll2);
    if (s_campaign_continuing) {
        printf(" [P2] campaign continuing: %lu/%lu done, remaining=%lu\r\n",
               (unsigned long)s_campaign.completed_count,
               (unsigned long)s_campaign.requested_count,
               (unsigned long)s_campaign.remaining_count);
    } else if (campaign_struct_intact() && (s_campaign.requested_count == 0u)) {
        printf(" [P2] no campaign in progress.\r\n");
    } else {
        printf(" [P2] campaign state cleared this boot (reset cause or persisted state not trusted).\r\n");
    }
}

/* ---------------------------------------------------------------------- */
/* Single restart + report printing                                        */
/* ---------------------------------------------------------------------- */

static void print_report(const dspic33ak_clock_pll_restart_report_t *r)
{
    if (r->status == DSPIC33AK_CLOCK_OK) {
        printf(" [P2] PASS off=%luus pllsw=%luus fout=%luus osw=%luus lock=%luus hz=%lu\r\n",
               (unsigned long)r->stop_time_us, (unsigned long)r->pllswen_time_us,
               (unsigned long)r->foutswen_time_us, (unsigned long)r->oswen_time_us,
               (unsigned long)r->lock_time_us, (unsigned long)r->actual_hz);
        return;
    }

    printf(" [P2] FAIL stage=%d status=%d\r\n", (int)r->last_stage, (int)r->status);
    printf(" [P2] post-stop pllsw=%u foutsw=%u osw=%u divsw=%u\r\n",
           (unsigned)r->post_stop_pllswen, (unsigned)r->post_stop_foutswen,
           (unsigned)r->post_stop_oswen, (unsigned)r->post_stop_divswen);
    {
        dspic33ak_clock_diag_snapshot_t s;
        dspic33ak_clock_diag_capture(&s);
        printf(" [P2] OSCCTRL=%08lX PLL2CON=%08lX PLL2DIV=%08lX\r\n",
               (unsigned long)s.oscctrl, (unsigned long)s.pll2con, (unsigned long)s.pll2div);
        printf(" [P2] CLKFAIL=%08lX SCSFAIL=%08lX CLKDIAG=%08lX\r\n",
               (unsigned long)s.clkfail, (unsigned long)s.scsfail, (unsigned long)s.clkdiag);
    }
}

static bool run_one_restart(void)
{
    uint32_t actual_hz = 0u;
    dspic33ak_clock_status_t status =
        dspic33ak_clock_pll_restart(DSPIC33AK_CLOCK_PLL_2, &s_test_config, &actual_hz);
    print_report(dspic33ak_clock_pll_restart_last_report());
    return status == DSPIC33AK_CLOCK_OK;
}

/* ---------------------------------------------------------------------- */
/* Software reset                                                          */
/* ---------------------------------------------------------------------- */

static void drain_uart1(void)
{
    uint32_t guard = 0u;
    while (!dspic33ak_uart_tx_done(PLL2_TEST_UART) && (guard < 2000000u)) {
        guard++;
    }
}

/* The tick timer HAL intentionally has no delay function (see
 * src/hal_timer/README.md, "Out of scope"), so this is the same tick-diff
 * busy-wait idiom main.c already uses (wait_ms_from_tick()). */
static void wait_ms(uint32_t wait_ms_value)
{
    uint32_t start = dspic33ak_tick_timer_get_ms();

    while ((uint32_t)(dspic33ak_tick_timer_get_ms() - start) < wait_ms_value) {
        ;
    }
}

/* XC-DSC exposes no reset builtin for this core; `reset` is the established
 * mechanism already used by src/fw_update/fw_btseq.c (fw_sys_reset()). */
static void issue_software_reset(void)
{
    drain_uart1();
    wait_ms(PLL2_TEST_RESET_DELAY_MS);
    __asm__ volatile ("reset");
    for (;;) { }   /* unreachable */
}

/* ---------------------------------------------------------------------- */
/* Console commands                                                        */
/* ---------------------------------------------------------------------- */

void pll2_restart_test_cmd_status(void)
{
    dspic33ak_clock_diag_snapshot_t s;

    dspic33ak_clock_diag_capture(&s);
    printf("\r\n\"?p2\" now   en=%u rdy=%u on=%u clk=%u nosc=%u cosc=%u pllsw=%u foutsw=%u osw=%u divsw=%u\r\n",
           (unsigned)s.pll2en, (unsigned)s.pll2rdy, (unsigned)s.pll2_on, (unsigned)s.pll2_clkrdy,
           (unsigned)s.pll2_nosc, (unsigned)s.pll2_cosc, (unsigned)s.pll2_pllswen,
           (unsigned)s.pll2_foutswen, (unsigned)s.pll2_oswen, (unsigned)s.pll2_divswen);
    pll2_restart_test_print_early_capture();
    printf("\"?p2\" campaign requested=%lu completed=%lu pass=%lu fail=%lu remaining=%lu last_fail_stage=%lu\r\n",
           (unsigned long)s_campaign.requested_count, (unsigned long)s_campaign.completed_count,
           (unsigned long)s_campaign.pass_count, (unsigned long)s_campaign.fail_count,
           (unsigned long)s_campaign.remaining_count, (unsigned long)s_campaign.last_failed_stage);
}

void pll2_restart_test_cmd_single(void)
{
    printf("\r\n\"*p2\" single restart:\r\n");
    (void)run_one_restart();
}

void pll2_restart_test_cmd_double(void)
{
    printf("\r\n\"*p2d\" restart #1 (whatever state PLL2 is in now):\r\n");
    (void)run_one_restart();
    printf("\"*p2d\" restart #2 (force-stop the now-locked PLL2, restart again):\r\n");
    (void)run_one_restart();
}

void pll2_restart_test_cmd_cancel(void)
{
    campaign_clear();
    printf("\r\n\"*p2x\" campaign cleared.\r\n");
}

bool pll2_restart_test_cmd_campaign_start(uint16_t count)
{
    if ((count == 0u) || (count > PLL2_TEST_MAX_COUNT)) {
        printf("\r\n\"*p2r\" count must be 0001..%u.\r\n", (unsigned)PLL2_TEST_MAX_COUNT);
        return false;
    }

    campaign_clear();
    s_campaign.requested_count = count;
    s_campaign.remaining_count = count;

    printf("\r\n\"*p2r\" campaign armed: %u iterations. Resetting...\r\n", (unsigned)count);
    issue_software_reset();
    return true;   /* unreachable */
}

/* ---------------------------------------------------------------------- */
/* Boot-time campaign continuation                                         */
/* ---------------------------------------------------------------------- */

static void print_help(void)
{
    printf(" [P2] commands: ?p2  *p2  *p2d  *p2r<0001-9999>  *p2x\r\n");
}

void pll2_restart_test_run(void)
{
    bool ok;

    if (!s_campaign_continuing) {
        pll2_restart_test_print_early_capture();
        print_help();
        return;
    }

    printf(" [P2R] %04lu/%04lu resuming...\r\n",
           (unsigned long)(s_campaign.completed_count + 1u),
           (unsigned long)s_campaign.requested_count);
    ok = run_one_restart();

    s_campaign.completed_count++;
    s_campaign.remaining_count--;
    if (ok) {
        s_campaign.pass_count++;
    } else {
        s_campaign.fail_count++;
        s_campaign.last_failed_stage =
            (uint32_t)dspic33ak_clock_pll_restart_last_report()->last_stage;
    }

    printf(" [P2R] %04lu/%04lu %s\r\n",
           (unsigned long)s_campaign.completed_count,
           (unsigned long)s_campaign.requested_count,
           ok ? "PASS" : "FAIL");

    if (!ok) {
        printf(" [P2R] campaign stopped; no further software reset.\r\n");
        s_campaign.remaining_count = 0u;
        print_help();
        return;
    }
    if (s_campaign.remaining_count == 0u) {
        printf(" [P2R] campaign complete: %lu/%lu pass.\r\n",
               (unsigned long)s_campaign.pass_count, (unsigned long)s_campaign.requested_count);
        print_help();
        return;
    }

    issue_software_reset();   /* never returns */
}

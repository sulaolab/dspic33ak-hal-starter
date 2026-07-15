/*
 * tdm_neg_test.c  --  SPI/I2S/TDM HAL negative-validation self-test (see tdm_neg_test.h)
 *
 * Opt-in (HAL_STARTER_ENABLE_TDM_NEG_TEST). Asserts the HAL rejects misuse with the correct
 * bool + dspic33ak_spi_i2s_tdm_error_t. Pure API-contract test: no external signals needed;
 * the few cases that must observe a running stream start a self-clocked master briefly (no pins
 * are routed -- no port hook does pin routing here) and tear it down. Leaves the HAL stopped +
 * closed + port-cleared so the subsequent tdm_smoke_init() starts from a clean state.
 */

#include "app_config.h"

#if HAL_STARTER_ENABLE_TDM_NEG_TEST

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "tdm_neg_test.h"
#include "dspic33ak_dma.h"                 // dspic33ak_dma_global_init()
#include "dspic33ak_spi_i2s_tdm.h"         // transport HAL public API
#include "dspic33ak_spi_i2s_tdm_conf.h"    // DSPIC33AK_TDM_SLOTS_PER_FS / _BLOCK_FRAMES

//===========================================================
// Pass/fail accounting + a stateful test port for the clock-readiness case
//===========================================================
static uint32_t s_pass;
static uint32_t s_fail;

// clock_source_ready() returns this flag, so a test can make the source "drop" between
// open() (flag=true) and start (flag=false) to exercise the arm-time readiness re-check.
static volatile bool s_test_ready = true;

static bool neg_ready_hook( dspic33ak_spi_i2s_tdm_clock_role_t role )
{
    (void)role;
    return s_test_ready;
}

// NULL pin/CLC/clock-init hooks => open() does no routing; only readiness is driven.
static const dspic33ak_spi_i2s_tdm_port_t s_neg_test_port =
{
    .configure_pins      = NULL,
    .clc_passthrough     = NULL,
    .clock_source_init   = NULL,
    .clock_source_ready  = neg_ready_hook,
    .consume_clock_event = NULL,
};

//===========================================================
// Check helpers
//===========================================================
// Assert a bool-returning API returned want_ret AND left the given last-error.
static void neg_expect( const char *label, bool got,
                        bool want_ret, dspic33ak_spi_i2s_tdm_error_t want_err )
{
    const dspic33ak_spi_i2s_tdm_error_t err = dspic33ak_spi_i2s_tdm_get_last_error();
    if( ( got == want_ret ) && ( err == want_err ) )
    {
        s_pass++;
        printf(" [NEG] ok   %s\n", label );
    }
    else
    {
        s_fail++;
        printf(" [NEG] FAIL %s: ret=%d(want %d) err=%d(want %d)\n",
               label, (int)got, (int)want_ret, (int)err, (int)want_err );
    }
}

// Assert a plain boolean condition (no last-error component).
static void neg_expect_cond( const char *label, bool cond )
{
    if( cond ) { s_pass++; printf(" [NEG] ok   %s\n", label ); }
    else       { s_fail++; printf(" [NEG] FAIL %s\n", label ); }
}

//===========================================================
// Config builders (all within the supported envelope: TDM, 32-bit, compile-time geometry)
//===========================================================
static dspic33ak_spi_i2s_tdm_config_t neg_cfg( dspic33ak_spi_i2s_tdm_clock_role_t role, bool ckp )
{
    dspic33ak_spi_i2s_tdm_config_t c;
    c.format                        = DSPIC33AK_SPI_I2S_TDM_FORMAT_TDM;
    c.clock_role                    = role;
    c.slots_per_fs                  = (uint8_t)DSPIC33AK_TDM_SLOTS_PER_FS;
    c.word_bits                     = 32u;
    c.fs_shape                      = DSPIC33AK_SPI_I2S_TDM_FS_PULSE;   // no CLC10 (self-test)
    c.block_frames                  = (uint16_t)DSPIC33AK_TDM_BLOCK_FRAMES;
    c.brg                           = 7u;
    c.mclk_enable                   = true;
    c.fs_coincides_first_bclk       = true;
    c.bclk_idle_high                = ckp;
    c.bclk_change_on_active_to_idle = false;
    c.ignore_overflow               = true;
    c.ignore_underrun               = true;
    return c;
}

//===========================================================
// Test blocks
//===========================================================
// NULL/invalid handle is ERR_BAD_INSTANCE regardless of mode (validity is checked first).
static void neg_null_handle( void )
{
    neg_expect( "inst_start(NULL) -> BAD_INSTANCE",
                dspic33ak_spi_i2s_tdm_inst_start( NULL ),
                false, DSPIC33AK_SPI_I2S_TDM_ERR_BAD_INSTANCE );
    neg_expect( "inst_stop(NULL) -> BAD_INSTANCE",
                dspic33ak_spi_i2s_tdm_inst_stop( NULL ),
                false, DSPIC33AK_SPI_I2S_TDM_ERR_BAD_INSTANCE );
}

// sync_domain >= 32 is rejected by configure_system()'s preflight (no side effects, no mode change).
static void neg_sync_domain_range( void )
{
    const uint8_t n = dspic33ak_spi_i2s_tdm_instance_count();
    dspic33ak_spi_i2s_tdm_leg_setup_t setups[2];
    for( uint8_t i = 0u; i < n; i++ )
    {
        setups[i].stream      = neg_cfg( (i == 0u) ? DSPIC33AK_SPI_I2S_TDM_CLOCK_MASTER
                                                   : DSPIC33AK_SPI_I2S_TDM_CLOCK_SLAVE, true );
        setups[i].sync_domain = 0u;
    }
    setups[n - 1u].sync_domain = 32u;   // out of the 0..31 tracked range
    neg_expect( "configure_system(sync_domain=32) -> TOPOLOGY",
                dspic33ak_spi_i2s_tdm_configure_system( setups, n ),
                false, DSPIC33AK_SPI_I2S_TDM_ERR_TOPOLOGY );
}

// SINGLE-mode + primary-only per-leg API, and the open()/mode gates around it.
static void neg_single_mode( dspic33ak_spi_i2s_tdm_inst_t *spi1 )
{
    const dspic33ak_spi_i2s_tdm_config_t master = neg_cfg( DSPIC33AK_SPI_I2S_TDM_CLOCK_MASTER, true );

    // inst_configure -> SINGLE mode.
    neg_expect( "inst_configure(primary) -> ok",
                dspic33ak_spi_i2s_tdm_inst_configure( spi1, &master ),
                true, DSPIC33AK_SPI_I2S_TDM_ERR_NONE );

    // Start before open() -> NOT_OPEN (opened is checked before the readiness re-check).
    neg_expect( "inst_start before open() -> NOT_OPEN",
                dspic33ak_spi_i2s_tdm_inst_start( spi1 ),
                false, DSPIC33AK_SPI_I2S_TDM_ERR_NOT_OPEN );

    // SYSTEM stop API in SINGLE mode -> CONFIG_MODE.
    neg_expect( "stop_domain(0) in SINGLE -> CONFIG_MODE",
                dspic33ak_spi_i2s_tdm_stop_domain( 0u ),
                false, DSPIC33AK_SPI_I2S_TDM_ERR_CONFIG_MODE );
    neg_expect( "stop_all_domains in SINGLE -> CONFIG_MODE",
                dspic33ak_spi_i2s_tdm_stop_all_domains(),
                false, DSPIC33AK_SPI_I2S_TDM_ERR_CONFIG_MODE );

    // Configure-while-open is rejected (open() consumes the committed config).
    neg_expect( "open() -> ok",
                dspic33ak_spi_i2s_tdm_open(),
                true, DSPIC33AK_SPI_I2S_TDM_ERR_NONE );
    neg_expect( "inst_configure while open -> ALREADY_OPEN",
                dspic33ak_spi_i2s_tdm_inst_configure( spi1, &master ),
                false, DSPIC33AK_SPI_I2S_TDM_ERR_ALREADY_OPEN );
    {
        dspic33ak_spi_i2s_tdm_leg_setup_t s0;
        s0.stream = master; s0.sync_domain = 0u;
        neg_expect( "configure_system while open -> ALREADY_OPEN",
                    dspic33ak_spi_i2s_tdm_configure_system( &s0, dspic33ak_spi_i2s_tdm_instance_count() ),
                    false, DSPIC33AK_SPI_I2S_TDM_ERR_ALREADY_OPEN );
    }

    // close() while running -> false + still running (mute-safe teardown contract).
    neg_expect( "inst_start(primary) -> running",
                dspic33ak_spi_i2s_tdm_inst_start( spi1 ),
                true, DSPIC33AK_SPI_I2S_TDM_ERR_NONE );
    neg_expect_cond( "is_running after start", dspic33ak_spi_i2s_tdm_is_running() );
    neg_expect( "close() while running -> ALREADY_RUNNING",
                dspic33ak_spi_i2s_tdm_close(),
                false, DSPIC33AK_SPI_I2S_TDM_ERR_ALREADY_RUNNING );
    neg_expect_cond( "still running after refused close", dspic33ak_spi_i2s_tdm_is_running() );

    // Proper teardown: stop then close.
    neg_expect( "inst_stop(primary) -> ok",
                dspic33ak_spi_i2s_tdm_inst_stop( spi1 ),
                true, DSPIC33AK_SPI_I2S_TDM_ERR_NONE );
    neg_expect( "close() when stopped -> ok",
                dspic33ak_spi_i2s_tdm_close(),
                true, DSPIC33AK_SPI_I2S_TDM_ERR_NONE );
}

// Arm-time clock-readiness re-check + fail-closed recovery (start-fail -> close -> re-init).
static void neg_readiness( dspic33ak_spi_i2s_tdm_inst_t *spi1 )
{
    const dspic33ak_spi_i2s_tdm_config_t master = neg_cfg( DSPIC33AK_SPI_I2S_TDM_CLOCK_MASTER, true );

    neg_expect( "set_port(test) -> ok",
                dspic33ak_spi_i2s_tdm_set_port( &s_neg_test_port ),
                true, DSPIC33AK_SPI_I2S_TDM_ERR_NONE );
    (void)dspic33ak_spi_i2s_tdm_inst_configure( spi1, &master );   // SINGLE (already checked above)

    s_test_ready = true;
    neg_expect( "open() with ready=true -> ok",
                dspic33ak_spi_i2s_tdm_open(),
                true, DSPIC33AK_SPI_I2S_TDM_ERR_NONE );

    // Source drops between open() and start -> arm-time re-check fails closed.
    s_test_ready = false;
    neg_expect( "inst_start with ready=false -> CLOCK_NOT_READY",
                dspic33ak_spi_i2s_tdm_inst_start( spi1 ),
                false, DSPIC33AK_SPI_I2S_TDM_ERR_CLOCK_NOT_READY );

    // Recovery: close, restore readiness, re-open + start -- must NOT be blocked by a leaked open.
    neg_expect( "close() after failed start -> ok",
                dspic33ak_spi_i2s_tdm_close(),
                true, DSPIC33AK_SPI_I2S_TDM_ERR_NONE );
    s_test_ready = true;
    neg_expect( "re-open() -> ok",
                dspic33ak_spi_i2s_tdm_open(),
                true, DSPIC33AK_SPI_I2S_TDM_ERR_NONE );
    neg_expect( "re-inst_start -> running (recovered)",
                dspic33ak_spi_i2s_tdm_inst_start( spi1 ),
                true, DSPIC33AK_SPI_I2S_TDM_ERR_NONE );

    (void)dspic33ak_spi_i2s_tdm_inst_stop( spi1 );
    (void)dspic33ak_spi_i2s_tdm_close();
    neg_expect( "set_port(NULL) -> ok",
                dspic33ak_spi_i2s_tdm_set_port( NULL ),
                true, DSPIC33AK_SPI_I2S_TDM_ERR_NONE );
}

// SYSTEM-mode API + the domain-stop invalid-domain contract. Leaves mode = SYSTEM.
static void neg_system_mode( dspic33ak_spi_i2s_tdm_inst_t *spi1 )
{
    const uint8_t n = dspic33ak_spi_i2s_tdm_instance_count();
    dspic33ak_spi_i2s_tdm_leg_setup_t setups[2];
    for( uint8_t i = 0u; i < n; i++ )
    {
        // All members in domain 0 with matching framing; leg 0 is the single MASTER.
        setups[i].stream      = neg_cfg( (i == 0u) ? DSPIC33AK_SPI_I2S_TDM_CLOCK_MASTER
                                                   : DSPIC33AK_SPI_I2S_TDM_CLOCK_SLAVE, true );
        setups[i].sync_domain = 0u;
    }

    neg_expect( "configure_system(valid) -> SYSTEM ok",
                dspic33ak_spi_i2s_tdm_configure_system( setups, n ),
                true, DSPIC33AK_SPI_I2S_TDM_ERR_NONE );

    // Per-leg configure in SYSTEM mode -> CONFIG_MODE (transactional owner only).
    {
        const dspic33ak_spi_i2s_tdm_config_t m = neg_cfg( DSPIC33AK_SPI_I2S_TDM_CLOCK_MASTER, true );
        neg_expect( "inst_configure in SYSTEM -> CONFIG_MODE",
                    dspic33ak_spi_i2s_tdm_inst_configure( spi1, &m ),
                    false, DSPIC33AK_SPI_I2S_TDM_ERR_CONFIG_MODE );
    }

    // SYSTEM stop of a configured, stopped domain -> idempotent true.
    neg_expect( "stop_domain(0) SYSTEM stopped -> true",
                dspic33ak_spi_i2s_tdm_stop_domain( 0u ),
                true, DSPIC33AK_SPI_I2S_TDM_ERR_NONE );
    neg_expect( "stop_all_domains SYSTEM -> true",
                dspic33ak_spi_i2s_tdm_stop_all_domains(),
                true, DSPIC33AK_SPI_I2S_TDM_ERR_NONE );

    // Invalid domain ids -> BAD_INSTANCE (symmetry with start_domain).
    neg_expect( "stop_domain(31) no member -> BAD_INSTANCE",
                dspic33ak_spi_i2s_tdm_stop_domain( 31u ),
                false, DSPIC33AK_SPI_I2S_TDM_ERR_BAD_INSTANCE );
    neg_expect( "stop_domain(255) >=32 -> BAD_INSTANCE",
                dspic33ak_spi_i2s_tdm_stop_domain( 255u ),
                false, DSPIC33AK_SPI_I2S_TDM_ERR_BAD_INSTANCE );

    // Per-leg start in SYSTEM mode -> CONFIG_MODE.
    neg_expect( "open() (SYSTEM) -> ok",
                dspic33ak_spi_i2s_tdm_open(),
                true, DSPIC33AK_SPI_I2S_TDM_ERR_NONE );
    neg_expect( "inst_start in SYSTEM -> CONFIG_MODE",
                dspic33ak_spi_i2s_tdm_inst_start( spi1 ),
                false, DSPIC33AK_SPI_I2S_TDM_ERR_CONFIG_MODE );
    (void)dspic33ak_spi_i2s_tdm_close();
}

// 2-leg-only: two-masters-per-domain / same-domain framing mismatch / non-destructive double-start.
// Runtime-gated on instance_count()>=2 (a DSPIC33AK_TDM_USE_SPI2=1 build); logged skipped otherwise.
static void neg_two_leg( dspic33ak_spi_i2s_tdm_inst_t *spi1, dspic33ak_spi_i2s_tdm_inst_t *spi2 )
{
    dspic33ak_spi_i2s_tdm_leg_setup_t s[2];

    // (g) two clock MASTERs in one sync domain -> TOPOLOGY (preflight).
    s[0].stream = neg_cfg( DSPIC33AK_SPI_I2S_TDM_CLOCK_MASTER, true ); s[0].sync_domain = 0u;
    s[1].stream = neg_cfg( DSPIC33AK_SPI_I2S_TDM_CLOCK_MASTER, true ); s[1].sync_domain = 0u;
    neg_expect( "configure_system(2 masters/domain) -> TOPOLOGY",
                dspic33ak_spi_i2s_tdm_configure_system( s, 2u ),
                false, DSPIC33AK_SPI_I2S_TDM_ERR_TOPOLOGY );

    // (h) same-domain framing mismatch (CKP differs) -> TOPOLOGY (preflight). One master, so it
    // fails the framing check, not the >1-master check.
    s[0].stream = neg_cfg( DSPIC33AK_SPI_I2S_TDM_CLOCK_MASTER, true  ); s[0].sync_domain = 0u;
    s[1].stream = neg_cfg( DSPIC33AK_SPI_I2S_TDM_CLOCK_SLAVE,  false ); s[1].sync_domain = 0u;
    neg_expect( "configure_system(framing mismatch) -> TOPOLOGY",
                dspic33ak_spi_i2s_tdm_configure_system( s, 2u ),
                false, DSPIC33AK_SPI_I2S_TDM_ERR_TOPOLOGY );

    // R2-1 non-destructiveness (realizable form): two independent domains, each a master. A second
    // start_all_domains() and a repeat start_domain() are idempotent and DO NOT tear a running
    // domain down. (The literal "running-dom0 + failing-dom1 rollback preserves dom0" needs
    // fault-injection the public API deliberately prevents -- covered by the R2-1 two-pass code
    // review; see plan.)
    s[0].stream = neg_cfg( DSPIC33AK_SPI_I2S_TDM_CLOCK_MASTER, true ); s[0].sync_domain = 0u;
    s[1].stream = neg_cfg( DSPIC33AK_SPI_I2S_TDM_CLOCK_MASTER, true ); s[1].sync_domain = 1u;
    neg_expect( "configure_system(2 domains) -> ok",
                dspic33ak_spi_i2s_tdm_configure_system( s, 2u ),
                true, DSPIC33AK_SPI_I2S_TDM_ERR_NONE );
    neg_expect( "open() (2 domains) -> ok",
                dspic33ak_spi_i2s_tdm_open(),
                true, DSPIC33AK_SPI_I2S_TDM_ERR_NONE );
    neg_expect( "start_all_domains -> ok",
                dspic33ak_spi_i2s_tdm_start_all_domains(),
                true, DSPIC33AK_SPI_I2S_TDM_ERR_NONE );
    {
        dspic33ak_spi_i2s_tdm_status_t st1, st2;
        bool r1 = dspic33ak_spi_i2s_tdm_inst_get_status( spi1, &st1, false );
        bool r2 = dspic33ak_spi_i2s_tdm_inst_get_status( spi2, &st2, false );
        neg_expect_cond( "both legs running after start_all",
                         r1 && r2 && st1.running && st2.running );
    }
    neg_expect( "start_all_domains AGAIN -> idempotent true",
                dspic33ak_spi_i2s_tdm_start_all_domains(),
                true, DSPIC33AK_SPI_I2S_TDM_ERR_NONE );
    neg_expect( "start_domain(0) on running -> idempotent true",
                dspic33ak_spi_i2s_tdm_start_domain( 0u ),
                true, DSPIC33AK_SPI_I2S_TDM_ERR_NONE );
    {
        dspic33ak_spi_i2s_tdm_status_t st1, st2;
        (void)dspic33ak_spi_i2s_tdm_inst_get_status( spi1, &st1, false );
        (void)dspic33ak_spi_i2s_tdm_inst_get_status( spi2, &st2, false );
        neg_expect_cond( "both legs STILL running after repeat starts (non-destructive)",
                         st1.running && st2.running );
    }

    (void)dspic33ak_spi_i2s_tdm_stop_all_domains();
    (void)dspic33ak_spi_i2s_tdm_close();
}

//===========================================================
// Entry
//===========================================================
bool tdm_neg_test_run( void )
{
    dspic33ak_spi_i2s_tdm_inst_t *spi1 = dspic33ak_spi_i2s_tdm_spi1();
    dspic33ak_spi_i2s_tdm_inst_t *spi2 = dspic33ak_spi_i2s_tdm_spi2();
    const uint8_t n = dspic33ak_spi_i2s_tdm_instance_count();

    s_pass = 0u;
    s_fail = 0u;

    printf("---------------------------------------------\n");
    printf(" [NEG] SPI/I2S/TDM HAL negative-validation self-test (legs=%u)\n", (unsigned)n );
    printf("---------------------------------------------\n");

    // The start cases arm the SPI RX/TX DMA, so the DMA HAL must be globally initialized first
    // (same prerequisite as the smoke demo). Harmless if already initialized.
    dspic33ak_dma_global_init();

    if( spi1 == NULL )
    {
        printf(" [NEG] FAIL spi1() == NULL -- cannot run\n" );
        return false;
    }

    neg_null_handle();
    neg_sync_domain_range();
    neg_single_mode( spi1 );
    neg_readiness( spi1 );

    if( ( n >= 2u ) && ( spi2 != NULL ) )
    {
        neg_two_leg( spi1, spi2 );
    }
    else
    {
        printf(" [NEG] skip 2-leg cases (g/h/non-destructive double-start): needs DSPIC33AK_TDM_USE_SPI2=1\n" );
    }

    // The literal (a)/(b) "running-domain preserved when another domain's start fails" is covered by
    // the R2-1 two-pass rollback code + review -- a genuine partial/failing domain is not
    // constructible through the public API (see tdm_neg_test.h / plan).
    printf(" [NEG] note a/b: start_all rollback non-destructiveness is review-covered (see plan)\n" );

    // Leave the HAL clean for the subsequent smoke demo: stopped, closed, no test port.
    // (mode may be SYSTEM here; the smoke's configure_system() re-commits from any stopped+closed mode.)
    (void)dspic33ak_spi_i2s_tdm_set_port( NULL );

    printf(" [NEG] pass=%lu fail=%lu\n", (unsigned long)s_pass, (unsigned long)s_fail );
    return ( s_fail == 0u );
}

#endif // HAL_STARTER_ENABLE_TDM_NEG_TEST

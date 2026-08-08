#ifndef NORA_SPI_I2S_TDM_DSPIC33A_DIAG_FAST_H
#define NORA_SPI_I2S_TDM_DSPIC33A_DIAG_FAST_H

// dsPIC33A-private TDMsum profiler state and ISR fast path. This header is intentionally
// separate from nora_spi_i2s_tdm_diag.h: portable consumers see diagnostics snapshots only,
// never this mutable implementation state.

#include "nora_spi_i2s_tdm_diag.h"

/*
 * ISR fast path for the deadline check -- see the naming rule in
 * nora_dma_dspic33a_fast.h: <portable name>_hot, supplied by the backend header.
 *
 * Not a static inline like the rest of this header, and deliberately so. The
 * portable nora_spi_i2s_tdm_diag_check_deadline() takes nora_dma_channel_t, and
 * XC-DSC treats that enum as wider than the uint8_t the ISR already holds;
 * calling the portable form from tdm_rx_block() pushed that function past the
 * compiler's inline-cost threshold. This keeps the ISR ABI narrow while the
 * portable declaration stays type-safe.
 *
 * It used to live in a separate nora_spi_i2s_tdm_diag_internal.h under the name
 * _isr_raw. One convention, one header: an ISR-only entry point belongs with the
 * other ISR-only entry points of the same module.
 */
void nora_spi_i2s_tdm_diag_check_deadline_hot(
    nora_spi_i2s_tdm_diag_t* d,
    uint8_t                  channel,
    nora_dma_status_t        status );

#if NORA_TDM_SUMPROF

// Bounds the per-call window-advance loop. Steady state advances 0-1 windows per ISR edge;
// after a long stopped gap the grid is re-based rather than walking every empty window.
#define NORA_SPI_I2S_TDM_DSPIC33A_SUMPROF_MAX_CATCHUP  (4u)

typedef struct {
    volatile uint32_t window_period_ticks;
    volatile uint32_t window_end_ticks;
    volatile uint32_t busy_start_ticks;
    volatile uint32_t busy_ticks;
    volatile uint32_t max_busy_ticks;
    volatile uint32_t saturated_count;
    volatile uint8_t  busy_depth;
    volatile bool     initialized;
} nora_spi_i2s_tdm_dspic33a_sumprof_state_t;

// Defined by nora_spi_i2s_tdm_dspic33a_diag.c. External linkage is needed only because the
// inline ISR hooks and foreground snapshot code live in separate backend translation units.
extern nora_spi_i2s_tdm_dspic33a_sumprof_state_t
    g_nora_spi_i2s_tdm_dspic33a_sumprof_state;

static inline bool nora_spi_i2s_tdm_dspic33a_sumprof_reached( uint32_t now,
                                                                uint32_t end )
{
    return (int32_t)( now - end ) >= 0;
}

static inline void nora_spi_i2s_tdm_dspic33a_sumprof_close_window( void )
{
    uint32_t busy = g_nora_spi_i2s_tdm_dspic33a_sumprof_state.busy_ticks;

    if( busy > g_nora_spi_i2s_tdm_dspic33a_sumprof_state.window_period_ticks )
    {
        busy = g_nora_spi_i2s_tdm_dspic33a_sumprof_state.window_period_ticks;
    }
    if( busy > g_nora_spi_i2s_tdm_dspic33a_sumprof_state.max_busy_ticks )
    {
        g_nora_spi_i2s_tdm_dspic33a_sumprof_state.max_busy_ticks = busy;
    }
    if( busy >= g_nora_spi_i2s_tdm_dspic33a_sumprof_state.window_period_ticks )
    {
        g_nora_spi_i2s_tdm_dspic33a_sumprof_state.saturated_count++;
    }
}

static inline void nora_spi_i2s_tdm_dspic33a_sumprof_advance( uint32_t now )
{
    uint8_t guard = 0u;

    if( !g_nora_spi_i2s_tdm_dspic33a_sumprof_state.initialized ||
        ( g_nora_spi_i2s_tdm_dspic33a_sumprof_state.window_period_ticks == 0u ) )
    {
        return;
    }

    while( nora_spi_i2s_tdm_dspic33a_sumprof_reached(
               now, g_nora_spi_i2s_tdm_dspic33a_sumprof_state.window_end_ticks ) )
    {
        if( g_nora_spi_i2s_tdm_dspic33a_sumprof_state.busy_depth != 0u )
        {
            g_nora_spi_i2s_tdm_dspic33a_sumprof_state.busy_ticks +=
                (uint32_t)( g_nora_spi_i2s_tdm_dspic33a_sumprof_state.window_end_ticks -
                            g_nora_spi_i2s_tdm_dspic33a_sumprof_state.busy_start_ticks );
            g_nora_spi_i2s_tdm_dspic33a_sumprof_state.busy_start_ticks =
                g_nora_spi_i2s_tdm_dspic33a_sumprof_state.window_end_ticks;
        }

        nora_spi_i2s_tdm_dspic33a_sumprof_close_window();

        g_nora_spi_i2s_tdm_dspic33a_sumprof_state.busy_ticks = 0u;
        g_nora_spi_i2s_tdm_dspic33a_sumprof_state.window_end_ticks +=
            g_nora_spi_i2s_tdm_dspic33a_sumprof_state.window_period_ticks;

        if( ++guard >= NORA_SPI_I2S_TDM_DSPIC33A_SUMPROF_MAX_CATCHUP )
        {
            g_nora_spi_i2s_tdm_dspic33a_sumprof_state.window_end_ticks =
                now + g_nora_spi_i2s_tdm_dspic33a_sumprof_state.window_period_ticks;
            g_nora_spi_i2s_tdm_dspic33a_sumprof_state.busy_ticks = 0u;
            if( g_nora_spi_i2s_tdm_dspic33a_sumprof_state.busy_depth != 0u )
            {
                g_nora_spi_i2s_tdm_dspic33a_sumprof_state.busy_start_ticks = now;
            }
            break;
        }
    }
}

static inline void nora_spi_i2s_tdm_dspic33a_sumprof_enter( uint32_t now )
{
    nora_spi_i2s_tdm_dspic33a_sumprof_advance( now );

    if( g_nora_spi_i2s_tdm_dspic33a_sumprof_state.busy_depth == 0u )
    {
        g_nora_spi_i2s_tdm_dspic33a_sumprof_state.busy_start_ticks = now;
    }
    g_nora_spi_i2s_tdm_dspic33a_sumprof_state.busy_depth++;
}

static inline void nora_spi_i2s_tdm_dspic33a_sumprof_exit( uint32_t now )
{
    nora_spi_i2s_tdm_dspic33a_sumprof_advance( now );

    if( g_nora_spi_i2s_tdm_dspic33a_sumprof_state.busy_depth == 0u )
    {
        return;
    }

    g_nora_spi_i2s_tdm_dspic33a_sumprof_state.busy_depth--;

    if( g_nora_spi_i2s_tdm_dspic33a_sumprof_state.busy_depth == 0u )
    {
        g_nora_spi_i2s_tdm_dspic33a_sumprof_state.busy_ticks +=
            (uint32_t)( now -
                        g_nora_spi_i2s_tdm_dspic33a_sumprof_state.busy_start_ticks );
    }
}

void nora_spi_i2s_tdm_dspic33a_sumprof_configure( uint32_t now,
                                                     uint32_t window_period_ticks );
void nora_spi_i2s_tdm_dspic33a_sumprof_reset( uint32_t now );
void nora_spi_i2s_tdm_dspic33a_sumprof_snapshot( nora_spi_i2s_tdm_tdmsum_t* out,
                                                    bool clear_peak );

#endif // NORA_TDM_SUMPROF

#endif // NORA_SPI_I2S_TDM_DSPIC33A_DIAG_FAST_H

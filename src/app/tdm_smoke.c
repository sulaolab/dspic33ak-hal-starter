/*
 * tdm_smoke.c  --  codec-less SPI1 TDM8 master smoke demo (see tdm_smoke.h)
 *
 * Design notes:
 *  - The block callback runs in DMA-ISR context. It does ONLY the TX block fill and a
 *    light RX energy accumulator. No printf/sqrtf/log10f/formatting here.
 *  - RX level: each RX sample is right-shifted before squaring and summed into a 64-bit
 *    per-block accumulator (no long accumulation of raw Q31^2 -> no overflow). The dB
 *    conversion (log10f) happens in tdm_smoke_status_print(), i.e. main-loop context.
 *  - TX sine amplitude is near-full-scale but never INT32_MIN (0x70000000).
 *  - fs/bclk shown in the status line are EXPECTED (design) values, not measurements.
 */

#include "tdm_smoke.h"
#include "app_config.h"

#if HAL_STARTER_ENABLE_TDM_SMOKE_DEMO
/* When the demo is disabled (app_config.h) this whole translation unit compiles to
 * nothing -- so the sinf/log10f and HAL dependencies are dropped from the OFF build.
 * main.c also guards its calls on the same macro, so there is nothing to link. */

#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "dspic33ak_clock.h"               // DSPIC33AK_CLOCK_SYS_HZ (SPI1 baud clock src)
#include "dspic33ak_dma.h"                 // dspic33ak_dma_global_init()
#include "dspic33ak_spi_i2s_tdm.h"         // transport HAL public API
#include "dspic33ak_spi_i2s_tdm_conf.h"    // DSPIC33AK_TDM_SLOTS_PER_FS / _BLOCK_FRAMES
#include "board.h"                         // board_spi1_tdm_smoke_pins_init()

//===========================================================
// Demo constants
//===========================================================
// Target TDM frame rate (Hz). Exact value is not important -- the demo only needs a
// visible TDM8 waveform. BCLK = SYS / (2*(BRG+1)); target BCLK = slots * 32 * fs.
//   BRG = SYS / (2 * slots * 32 * fs) - 1.
//   200 MHz / (2*8*32*48000) - 1 = 7  -> BCLK 12.5 MHz, fs ~48.8 kHz (expected, not exact).
#define TDM_SMOKE_TARGET_FS_HZ   (48000u)
#define TDM_SMOKE_SPI_BRG \
    ((uint32_t)((DSPIC33AK_CLOCK_SYS_HZ / (2u * (uint32_t)DSPIC33AK_TDM_SLOTS_PER_FS * 32u * TDM_SMOKE_TARGET_FS_HZ)) - 1u))

// Sine lookup table: power-of-two length so the per-frame index step is a cheap mask.
// 64 samples/cycle at fs ~48.8 kHz -> ~763 Hz (an "~800 Hz class" tone).
#define TDM_SMOKE_LUT_N          (64u)
#define TDM_SMOKE_LUT_MASK       (TDM_SMOKE_LUT_N - 1u)
#define TDM_SMOKE_AMPLITUDE      (0x70000000)   // near-full-scale Q31, never INT32_MIN
#define TDM_SMOKE_RX_FLOOR_DB    (-140.0f)

//===========================================================
// State
//===========================================================
static int32_t  s_sine_lut[TDM_SMOKE_LUT_N];
static float    s_tx_ref_meansq;            // mean-square of (lut>>16), the 0 dB reference
static bool     s_started;
static dspic33ak_spi_i2s_tdm_error_t s_init_error = DSPIC33AK_SPI_I2S_TDM_ERR_NONE;

// Published by the block callback (debug aid; non-atomic read in the main loop is fine).
static volatile uint32_t s_phase;           // frame phase into the sine LUT
static volatile uint64_t s_rx_sumsq;        // last block: sum of (rx>>16)^2
static volatile uint32_t s_rx_count;        // last block: sample count

//===========================================================
// Block callback (DMA-ISR context) -- TX fill + light RX accumulator ONLY
//===========================================================
static void tdm_smoke_block_cb(const int32_t *src, int32_t *dst, void *user)
{
    (void)user;
    const uint32_t slots  = (uint32_t)DSPIC33AK_TDM_SLOTS_PER_FS;
    const uint32_t frames = (uint32_t)DSPIC33AK_TDM_BLOCK_FRAMES;
    uint32_t       phase  = s_phase;
    uint64_t       sumsq  = 0u;
    uint32_t       w      = 0u;

    // Frame-major buffer: [f0 s0..s7][f1 s0..s7]... All 8 slots of a frame get the same
    // sine sample; the phase advances once per frame for a continuous tone.
    for (uint32_t f = 0u; f < frames; ++f)
    {
        const int32_t sample = s_sine_lut[(phase + f) & TDM_SMOKE_LUT_MASK];
        for (uint32_t k = 0u; k < slots; ++k)
        {
            dst[w] = sample;                       // TX: drive the slot
            const int32_t r = src[w] >> 16;        // RX: shift BEFORE squaring (overflow-safe)
            sumsq += (uint64_t)((int64_t)r * (int64_t)r);
            ++w;
        }
    }

    s_phase    = (phase + frames) & TDM_SMOKE_LUT_MASK;
    s_rx_sumsq = sumsq;       // per-block (not accumulated across blocks)
    s_rx_count = w;
}

//===========================================================
// Board/clock port: pins only (master-only demo). No external clock / CLC.
//===========================================================
static bool tdm_smoke_configure_pins(dspic33ak_spi_i2s_tdm_role_t role)
{
    // This demo is master-only; reject any other role rather than silently mis-routing.
    if (role != DSPIC33AK_SPI_I2S_TDM_ROLE_MASTER)
    {
        return false;
    }
    return board_spi1_tdm_smoke_pins_init();
}

static const dspic33ak_spi_i2s_tdm_port_t s_tdm_smoke_port =
{
    .configure_pins      = tdm_smoke_configure_pins,
    .clc_passthrough     = NULL,    // no codec MCLK fan-out
    .clock_source_init   = NULL,    // self-clocked master: no external clock to bring up
    .clock_source_ready  = NULL,    // NULL => always ready (no clock gate)
    .consume_clock_event = NULL,
};

//===========================================================
// Init helpers
//===========================================================
static void tdm_smoke_build_sine(void)
{
    float ref_sumsq = 0.0f;
    for (uint32_t i = 0u; i < TDM_SMOKE_LUT_N; ++i)
    {
        const float ph = (2.0f * 3.14159265358979f * (float)i) / (float)TDM_SMOKE_LUT_N;
        const int32_t s = (int32_t)((float)TDM_SMOKE_AMPLITUDE * sinf(ph));
        s_sine_lut[i] = s;
        const float rs = (float)(s >> 16);         // same scaling the RX accumulator uses
        ref_sumsq += rs * rs;
    }
    s_tx_ref_meansq = ref_sumsq / (float)TDM_SMOKE_LUT_N;   // 0 dB reference (mean-square)
}

bool tdm_smoke_init(void)
{
    dspic33ak_spi_i2s_tdm_inst_t *inst;

    s_started    = false;
    s_init_error = DSPIC33AK_SPI_I2S_TDM_ERR_NONE;
    s_phase      = 0u;
    s_rx_sumsq   = 0u;
    s_rx_count   = 0u;

    tdm_smoke_build_sine();

    // Board/clock port (pin routing reached via the hook -- the HAL core stays board-free).
    dspic33ak_spi_i2s_tdm_set_port(&s_tdm_smoke_port);

    inst = dspic33ak_spi_i2s_tdm_spi1();
    if (inst == NULL)
    {
        s_init_error = dspic33ak_spi_i2s_tdm_get_last_error();
        return false;
    }

    // Callback must be registered before start().
    if (!dspic33ak_spi_i2s_tdm_set_block_callback(inst, tdm_smoke_block_cb, NULL))
    {
        s_init_error = dspic33ak_spi_i2s_tdm_get_last_error();
        return false;
    }

    // TDM8 self-clocked master config. Geometry comes from the same conf.h constants that
    // sized the HAL's static DMA buffers; BRG sets the master BCLK from the system clock.
    const dspic33ak_spi_i2s_tdm_config_t cfg =
    {
        .format                       = DSPIC33AK_SPI_I2S_TDM_FORMAT_TDM,
        .role                         = DSPIC33AK_SPI_I2S_TDM_ROLE_MASTER,
        .slots_per_fs                 = (uint8_t)DSPIC33AK_TDM_SLOTS_PER_FS,
        .word_bits                    = 32u,
        .block_frames                 = (uint16_t)DSPIC33AK_TDM_BLOCK_FRAMES,
        .brg                          = TDM_SMOKE_SPI_BRG,
        .mclk_enable                  = true,    // MCLKEN=1 (CLKGEN9 reference)
        .fs_one_word_wide             = true,    // FRMSYPW=1
        .fs_coincides_first_bclk      = true,    // SPIFE=1 : no 1-bit delay (TDM8)
        .bclk_idle_high               = true,    // CKP=1
        .bclk_change_on_active_to_idle = false,  // CKE=0
        .ignore_overflow              = true,    // IGNROV=1
        .ignore_underrun              = true,    // IGNTUR=1
    };
    if (!dspic33ak_spi_i2s_tdm_inst_configure(inst, &cfg))
    {
        s_init_error = dspic33ak_spi_i2s_tdm_get_last_error();
        return false;
    }

    // The DMA HAL must be globally initialized before any channel config (inst_start()
    // arms the SPI1 RX/TX DMA). Only the TDM demo uses DMA on this board, so bring it up
    // here, before start.
    dspic33ak_dma_global_init();

    // Open the shared port (routes MikroBUS-A SPI1 pins via the hook) for the master role.
    if (!dspic33ak_spi_i2s_tdm_open(DSPIC33AK_SPI_I2S_TDM_ROLE_MASTER))
    {
        s_init_error = dspic33ak_spi_i2s_tdm_get_last_error();
        return false;
    }

    // Arm DMA + enable SPI1: the stream now runs autonomously on DMA/ISR.
    if (!dspic33ak_spi_i2s_tdm_inst_start(inst))
    {
        s_init_error = dspic33ak_spi_i2s_tdm_get_last_error();
        return false;
    }

    s_started = true;
    return true;
}

//===========================================================
// Status line (main-loop context) -- heavy math lives here
//===========================================================
void tdm_smoke_status_print(void)
{
    dspic33ak_spi_i2s_tdm_inst_t  *inst;
    dspic33ak_spi_i2s_tdm_status_t st;
    uint64_t sumsq;
    uint32_t cnt;
    float    rx_db = TDM_SMOKE_RX_FLOOR_DB;
    int32_t  db10, whole, frac;
    const char *sign;

    if (!s_started)
    {
        return;
    }

    inst = dspic33ak_spi_i2s_tdm_spi1();
    if ((inst == NULL) ||
        !dspic33ak_spi_i2s_tdm_inst_get_status(inst, &st, false))
    {
        return;
    }

    // Snapshot the callback's last-block accumulator (non-atomic read is acceptable here).
    sumsq = s_rx_sumsq;
    cnt   = s_rx_count;
    if ((cnt > 0u) && (s_tx_ref_meansq > 0.0f))
    {
        const float rx_meansq = (float)sumsq / (float)cnt;
        if (rx_meansq > 0.0f)
        {
            rx_db = 10.0f * log10f(rx_meansq / s_tx_ref_meansq);
            if (rx_db < TDM_SMOKE_RX_FLOOR_DB)
            {
                rx_db = TDM_SMOKE_RX_FLOOR_DB;
            }
        }
    }

    // Format dB as a signed x.y without relying on %f (round to one decimal).
    db10  = (int32_t)((rx_db * 10.0f) + ((rx_db >= 0.0f) ? 0.5f : -0.5f));
    whole = db10 / 10;
    frac  = db10 % 10;
    if (frac < 0) { frac = -frac; }
    sign = ((db10 < 0) && (whole == 0)) ? "-" : "";   // preserve sign for -0.x

    printf(" [TDM1] TDM8 master exp_fs~48.8kHz exp_bclk~12.5MHz block=%lu miss=%lu rx=%s%ld.%ld dB rel\n",
           (unsigned long)st.block_count,
           (unsigned long)st.block_deadline_miss_count,
           sign, (long)whole, (long)frac);
}

const char *tdm_smoke_last_error_str(void)
{
    switch (s_init_error)
    {
        case DSPIC33AK_SPI_I2S_TDM_ERR_NONE:               return "none";
        case DSPIC33AK_SPI_I2S_TDM_ERR_BAD_INSTANCE:       return "bad-instance";
        case DSPIC33AK_SPI_I2S_TDM_ERR_BAD_ARGUMENT:       return "bad-argument";
        case DSPIC33AK_SPI_I2S_TDM_ERR_NOT_CONFIGURED:     return "not-configured";
        case DSPIC33AK_SPI_I2S_TDM_ERR_ALREADY_RUNNING:    return "already-running";
        case DSPIC33AK_SPI_I2S_TDM_ERR_UNSUPPORTED_CONFIG: return "unsupported-config";
        case DSPIC33AK_SPI_I2S_TDM_ERR_TOPOLOGY:           return "topology";
        case DSPIC33AK_SPI_I2S_TDM_ERR_CLOCK_INIT:         return "clock-init";
        case DSPIC33AK_SPI_I2S_TDM_ERR_CLOCK_NOT_READY:    return "clock-not-ready";
        case DSPIC33AK_SPI_I2S_TDM_ERR_PIN_CONFIG:         return "pin-config";
        case DSPIC33AK_SPI_I2S_TDM_ERR_CLC:                return "clc";
        case DSPIC33AK_SPI_I2S_TDM_ERR_DMA_CONFIG:         return "dma-config";
        default:                                           return "unknown";
    }
}

#endif /* HAL_STARTER_ENABLE_TDM_SMOKE_DEMO */

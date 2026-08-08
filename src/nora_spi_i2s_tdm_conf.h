#ifndef NORA_SPI_I2S_TDM_CONF_H
#define NORA_SPI_I2S_TDM_CONF_H

//===========================================================
// nora_spi_i2s_tdm_conf.h  --  dspic33ak-hal-starter PROJECT config (compiled)
//
// The starter's project-supplied SPI/I2S/TDM HAL config, copied from the HAL's
// nora_spi_i2s_tdm_conf.h_example template and kept here in src/ on the include path
// (the HAL core does #include "nora_spi_i2s_tdm_conf.h"; this src/ copy is found, the
// template in hal_spi_i2s_tdm/ never is -- different extension). It is self-contained: NO
// app-layer dependency. For the starter's codec-less SPI1 TDM8 master smoke demo the
// example defaults already fit -- a single SPI1 TDM8 stream, DMA0/1, HAL-owned RX vector.
// Edit the values below to change geometry/topology.
//
// This is the SOLE, self-contained HAL config entry: the core translation units include
// ONLY this header for config and it has NO app-layer dependency (it never includes
// app_specific_config_*). Everything the HAL needs lives here as plain literals -- so
// hal_spi_i2s_tdm/ stands alone (given a supplied conf.h) and is publishable as-is.
//
// Dependency direction: app code MAY read these macros (app -> HAL); the HAL MUST NOT
// read app config (HAL -> app is forbidden). If your project also derives these values
// in its own config, keep the two as independent owners and assert their CONSISTENCY on
// the APP side (compare your APP_* against the NORA_TDM_* here and #error on a
// mismatch). The HAL does not police the app, and vice versa.
//
// Each setting is -D-overridable (#ifndef-guarded). This project config defaults to a
// single SPI1 TDM8 stream with no additional transport legs. Override with -D or edit
// below for I2S (2 slots), a different block size, or additional transport legs.
//
// Compile-time integration settings:
//   NORA_TDM_SLOTS_PER_FS   slots per frame-sync: TDM8 = 8, I2S = 2.
//   NORA_TDM_BLOCK_FRAMES   frames per ping/pong half (DMA block size).
//   NORA_TDM_USE_SPI2      1 = second logical transport row is built.
//   NORA_TDM_USE_SPI3/4    1 = additional physical SPI3/SPI4 rows are built (AK512 only).
//   NORA_TDM_BASE_ON_SPI34 1 = remap logical rows 0/1 from SPI1/2 to SPI3/4 (AK512 only).
// AK128 has no SPI4 or DMA6/7, so paired SPI3/4 and four-leg modes are unavailable.
// (Sample rate is NOT a setting here -- the transport is rate-agnostic; the product's
// supported-rate policy lives in the app layer, not the HAL.)
// The core's static DMA ping-pong buffers are sized 2 * SLOTS_PER_FS *
// BLOCK_FRAMES, and inst_configure() / configure_system() reject a config_t whose slots_per_fs /
// block_frames do not match these compile-time values.
//===========================================================

// --- HAL geometry / topology (literals; -D wins) ---
// Frame width is the SINGLE geometry source seen by BOTH the app and the HAL core (it sizes
// the static DMA ping-pong buffers 2*SLOTS*BLOCK and the per-instance leg table), so it MUST
// live HERE (or via -D), NOT in app_config.h -- the HAL core does not include app config.
//
// TDM8 is the default smoke-demo geometry. TDM16 can be enabled as a hidden/experimental
// framing check (NORA_TDM_USE_TDM16 = 1, here or via -D): it increases BCLK and DMA
// traffic substantially and is intended for scope-level investigation, not as a public
// starter-board feature. Keep TDM8 as the normal default.
// (For I2S use 2 slots: define NORA_TDM_SLOTS_PER_FS=2 directly via -D.)
#ifndef NORA_TDM_USE_TDM16
#define NORA_TDM_USE_TDM16       0     // 0 = TDM8 (default) ; 1 = TDM16 (hidden experimental)
#endif
#ifndef NORA_TDM_SLOTS_PER_FS
#  if NORA_TDM_USE_TDM16
#    define NORA_TDM_SLOTS_PER_FS 16   // TDM16 (hidden experimental geometry)
#  else
#    define NORA_TDM_SLOTS_PER_FS 8    // TDM8 (default)
#  endif
#endif
#ifndef NORA_TDM_BLOCK_FRAMES
#define NORA_TDM_BLOCK_FRAMES    32    // frames per ping/pong half
#endif
#ifndef NORA_TDM_USE_SPI2
#define NORA_TDM_USE_SPI2        0     // single SPI Audio transport by default
#endif
#ifndef NORA_TDM_USE_SPI3
#define NORA_TDM_USE_SPI3        0     // no additional SPI3 row by default
#endif
#ifndef NORA_TDM_USE_SPI4
#define NORA_TDM_USE_SPI4        0     // no additional SPI4 row by default
#endif
#ifndef NORA_TDM_BASE_ON_SPI34
#define NORA_TDM_BASE_ON_SPI34   0     // default logical bank is physical SPI1/SPI2
#endif

//===========================================================
// DMA channel allocation (single source of truth for the SPI<->DMA binding)
//
// Each SPI Audio instance owns one RX + one TX DMA channel. These numbers are read by
// the HAL core for its s_spi_legs[] table and to bind its explicit _DMA<rx>Interrupt
// vectors (a compile-time assert ties each vector to its RX-DMA channel; change a channel
// and the build fails until the vector + assert match). Each is -D overridable. Maintain a
// chip-wide map by hand: the HAL cannot see other subsystems' DMA usage. Assignment errors are
// caught in two places: an RX-DMA channel that no longer matches its explicit vector fails the
// build on the _Static_assert; a duplicate channel (RX==TX on a leg, or shared across legs) is
// rejected at runtime by the topology validation (ERR_TOPOLOGY) before open()/start().
//===========================================================
#ifndef NORA_TDM_SPI1_RX_DMA
#define NORA_TDM_SPI1_RX_DMA   0
#endif
#ifndef NORA_TDM_SPI1_TX_DMA
#define NORA_TDM_SPI1_TX_DMA   1
#endif
#ifndef NORA_TDM_SPI2_RX_DMA
#define NORA_TDM_SPI2_RX_DMA   2
#endif
#ifndef NORA_TDM_SPI2_TX_DMA
#define NORA_TDM_SPI2_TX_DMA   3
#endif
#ifndef NORA_TDM_SPI3_RX_DMA
#define NORA_TDM_SPI3_RX_DMA   4
#endif
#ifndef NORA_TDM_SPI3_TX_DMA
#define NORA_TDM_SPI3_TX_DMA   5
#endif
#ifndef NORA_TDM_SPI4_RX_DMA
#define NORA_TDM_SPI4_RX_DMA   6
#endif
#ifndef NORA_TDM_SPI4_TX_DMA
#define NORA_TDM_SPI4_TX_DMA   7
#endif


//===========================================================
// DMA interrupt-vector ownership.
//   1 (default) : TURNKEY -- the HAL DEFINES the _DMA<rx>Interrupt vectors itself.
//   0           : the HAL defines NO vectors. The integrator owns the IVT and calls
//                 nora_spi_i2s_tdm_inst_rx_isr(spiN()) from their own
//                 _DMA<rx>Interrupt for each instance's RX channel.
//===========================================================
#ifndef NORA_TDM_DEFINE_DMA_VECTORS
#define NORA_TDM_DEFINE_DMA_VECTORS   1
#endif


//===========================================================
// Engine-wide TDMsum occupancy profiler (nora_spi_i2s_tdm_tdmsum_*).
//   1 : the RX-block ISR brackets itself with the profiler's enter/exit hooks.
//   0 : profiler, hooks and the three _tdmsum_* entry points are not compiled.
// 0 here: the starter runs ONE TDM leg, and the per-leg load monitor
// (nora_spi_i2s_tdm_inst_get_load(), still active) already reports its occupancy --
// the engine-wide sum only adds information when several legs overlap. The hooks are
// not free: they run in the RX-block ISR on every block whenever the high-res timer
// is up, which in this project it is.
//===========================================================
#ifndef NORA_TDM_SUMPROF
#define NORA_TDM_SUMPROF   0
#endif


//===========================================================
// Instance count + physical assignment.
//
// The transport core defines its leg enum, per-instance ping-pong buffers, the
// s_spi_legs[] table, and the explicit _DMA<rx>Interrupt vectors directly in C, keyed off
// the per-instance channel #defines above (NORA_TDM_SPIn_RX/TX_DMA) and the geometry
// macros (NORA_TDM_SLOTS_PER_FS / _BLOCK_FRAMES). By default, logical rows 0/1 map
// to physical SPI1/SPI2. NORA_TDM_BASE_ON_SPI34 explicitly remaps those same two rows
// to SPI3/SPI4; NORA_TDM_USE_SPI3/4 instead add physical SPI3/SPI4 rows after SPI1/SPI2.
// The per-leg clock role and (rate-agnostic) stream shape are set at runtime by the
// integrator's config (nora_spi_i2s_tdm_configure_system() / _inst_configure()),
// not here.
//===========================================================

// Per-leg SYNC DOMAIN id: the s_spi_legs[] default (a caller using configure_system() may
// override it per leg at runtime). Legs sharing a domain are co-clocked and started phase-locked
// as a group; legs in different domains are started/rolled-back separately and need not share
// BCLK/FS -- but this is NOT full independence (source-readiness is engine-wide/primary-gated;
// CLC10 + the clock port are shared). NOT the clock role.
// Starter smoke is SPI1-only by default; additional values are used only when those rows are built.
#ifndef NORA_TDM_SPI1_SYNC_DOMAIN
#define NORA_TDM_SPI1_SYNC_DOMAIN   (0)
#endif
#ifndef NORA_TDM_SPI2_SYNC_DOMAIN
#define NORA_TDM_SPI2_SYNC_DOMAIN   (0)
#endif
#ifndef NORA_TDM_SPI3_SYNC_DOMAIN
#define NORA_TDM_SPI3_SYNC_DOMAIN   (2)
#endif
#ifndef NORA_TDM_SPI4_SYNC_DOMAIN
#define NORA_TDM_SPI4_SYNC_DOMAIN   (3)
#endif

// sync_domain must be 0..31 (start_all_domains()'s dedup/rollback mask range). Reject negatives
// too: a negative literal would cast to a large uint8_t at runtime.
#if ((NORA_TDM_SPI1_SYNC_DOMAIN) < 0) || ((NORA_TDM_SPI1_SYNC_DOMAIN) >= 32)
#error "NORA_TDM_SPI1_SYNC_DOMAIN must be in 0..31."
#endif
#if NORA_TDM_USE_SPI2 && (((NORA_TDM_SPI2_SYNC_DOMAIN) < 0) || ((NORA_TDM_SPI2_SYNC_DOMAIN) >= 32))
#error "NORA_TDM_SPI2_SYNC_DOMAIN must be in 0..31."
#endif
#if NORA_TDM_USE_SPI3 && (((NORA_TDM_SPI3_SYNC_DOMAIN) < 0) || ((NORA_TDM_SPI3_SYNC_DOMAIN) >= 32))
#error "NORA_TDM_SPI3_SYNC_DOMAIN must be in 0..31."
#endif
#if NORA_TDM_USE_SPI4 && (((NORA_TDM_SPI4_SYNC_DOMAIN) < 0) || ((NORA_TDM_SPI4_SYNC_DOMAIN) >= 32))
#error "NORA_TDM_SPI4_SYNC_DOMAIN must be in 0..31."
#endif


#if (NORA_TDM_SLOTS_PER_FS <= 0)
#error "NORA_TDM_SLOTS_PER_FS must be positive."
#endif

#if (NORA_TDM_SLOTS_PER_FS > 255)
#error "NORA_TDM_SLOTS_PER_FS must fit in uint8_t."
#endif

#if (NORA_TDM_BLOCK_FRAMES <= 0)
#error "NORA_TDM_BLOCK_FRAMES must be positive."
#endif

#if (NORA_TDM_BLOCK_FRAMES > 65535)
#error "NORA_TDM_BLOCK_FRAMES must fit in uint16_t."
#endif

#if ((NORA_TDM_USE_SPI2 != 0) && (NORA_TDM_USE_SPI2 != 1))
#error "NORA_TDM_USE_SPI2 must be 0 or 1."
#endif
#if ((NORA_TDM_USE_SPI3 != 0) && (NORA_TDM_USE_SPI3 != 1))
#error "NORA_TDM_USE_SPI3 must be 0 or 1."
#endif
#if ((NORA_TDM_USE_SPI4 != 0) && (NORA_TDM_USE_SPI4 != 1))
#error "NORA_TDM_USE_SPI4 must be 0 or 1."
#endif
#if ((NORA_TDM_BASE_ON_SPI34 != 0) && (NORA_TDM_BASE_ON_SPI34 != 1))
#error "NORA_TDM_BASE_ON_SPI34 must be 0 or 1."
#endif
#if ((NORA_TDM_SUMPROF != 0) && (NORA_TDM_SUMPROF != 1))
#error "NORA_TDM_SUMPROF must be 0 or 1."
#endif
#if (NORA_TDM_USE_SPI3 != NORA_TDM_USE_SPI4)
#error "The SPI3/SPI4 expansion requires SPI3 and SPI4 together."
#endif
#if NORA_TDM_BASE_ON_SPI34 && !NORA_TDM_USE_SPI2
#error "The SPI34 test bank requires two logical rows (NORA_TDM_USE_SPI2=1)."
#endif
#if NORA_TDM_BASE_ON_SPI34 && (NORA_TDM_USE_SPI3 || NORA_TDM_USE_SPI4)
#error "SPI34 test-bank mode and simultaneous four-leg mode are mutually exclusive."
#endif
#if NORA_TDM_USE_SPI3 && !NORA_TDM_USE_SPI2
#error "SPI3/SPI4 expansion currently requires the existing SPI1/SPI2 pair."
#endif

#if (NORA_TDM_SLOTS_PER_FS > (2147483647 / (2 * NORA_TDM_BLOCK_FRAMES)))
#error "SPI/I2S/TDM DMA buffer geometry overflows the static buffer element count."
#endif

#endif // NORA_SPI_I2S_TDM_CONF_H

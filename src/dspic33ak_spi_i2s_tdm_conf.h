#ifndef DSPIC33AK_SPI_I2S_TDM_CONF_H
#define DSPIC33AK_SPI_I2S_TDM_CONF_H

//===========================================================
// dspic33ak_spi_i2s_tdm_conf.h  --  dspic33ak-hal-starter PROJECT config (compiled)
//
// The starter's project-supplied SPI/I2S/TDM HAL config, copied from the HAL's
// dspic33ak_spi_i2s_tdm_conf.h_example template and kept here in src/ on the include path
// (the HAL core does #include "dspic33ak_spi_i2s_tdm_conf.h"; this src/ copy is found, the
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
// the APP side (compare your APP_* against the DSPIC33AK_TDM_* here and #error on a
// mismatch). The HAL does not police the app, and vice versa.
//
// Each setting is -D-overridable (#ifndef-guarded). This template defaults to the
// SAFEST generic config: a single SPI1 TDM8 stream, no second codec. Override with -D
// or by editing below for I2S (2 slots), a different block size, a second SPI, etc.
//
// Compile-time integration settings:
//   DSPIC33AK_TDM_SLOTS_PER_FS   slots per frame-sync: TDM8 = 8, I2S = 2.
//   DSPIC33AK_TDM_BLOCK_FRAMES   frames per ping/pong half (DMA block size).
//   DSPIC33AK_TDM_USE_SPI2      1 = SPI2 Audio transport is part of this build.
// (Sample rate is NOT a setting here -- the transport is rate-agnostic; the product's
// supported-rate policy lives in the app layer, not the HAL.)
// The core's static DMA ping-pong buffers are sized 2 * SLOTS_PER_FS *
// BLOCK_FRAMES, and configure() rejects a config_t whose slots_per_fs /
// block_frames do not match these compile-time values.
//===========================================================

// --- HAL geometry / topology (literals; -D wins) ---
// Frame width is the SINGLE geometry source seen by BOTH the app and the HAL core (it sizes
// the static DMA ping-pong buffers 2*SLOTS*BLOCK and the per-instance leg table), so it MUST
// live HERE (or via -D), NOT in app_config.h -- the HAL core does not include app config.
//
// TDM8 is the default smoke-demo geometry. TDM16 can be enabled as a hidden/experimental
// framing check (DSPIC33AK_TDM_USE_TDM16 = 1, here or via -D): it increases BCLK and DMA
// traffic substantially and is intended for scope-level investigation, not as a public
// starter-board feature. Keep TDM8 as the normal default.
// (For I2S use 2 slots: define DSPIC33AK_TDM_SLOTS_PER_FS=2 directly via -D.)
#ifndef DSPIC33AK_TDM_USE_TDM16
#define DSPIC33AK_TDM_USE_TDM16       0     // 0 = TDM8 (default) ; 1 = TDM16 (hidden experimental)
#endif
#ifndef DSPIC33AK_TDM_SLOTS_PER_FS
#  if DSPIC33AK_TDM_USE_TDM16
#    define DSPIC33AK_TDM_SLOTS_PER_FS 16   // TDM16 (hidden experimental geometry)
#  else
#    define DSPIC33AK_TDM_SLOTS_PER_FS 8    // TDM8 (default)
#  endif
#endif
#ifndef DSPIC33AK_TDM_BLOCK_FRAMES
#define DSPIC33AK_TDM_BLOCK_FRAMES    32    // frames per ping/pong half
#endif
#ifndef DSPIC33AK_TDM_USE_SPI2
#define DSPIC33AK_TDM_USE_SPI2        0     // single SPI Audio transport by default
#endif

//===========================================================
// DMA channel allocation (single source of truth for the SPI<->DMA binding)
//
// Each SPI Audio instance owns one RX + one TX DMA channel. These numbers are read by
// the HAL core for its s_spi_legs[] table and to bind its explicit _DMA<rx>Interrupt
// vectors (a compile-time assert ties each vector to its RX-DMA channel; change a channel
// and the build fails until the vector + assert match). Each is -D overridable. Maintain a
// chip-wide map by hand: the HAL cannot see other subsystems' DMA usage. Duplicate channels
// fail the build (redefined vector).
//===========================================================
#ifndef DSPIC33AK_TDM_SPI1_RX_DMA
#define DSPIC33AK_TDM_SPI1_RX_DMA   0
#endif
#ifndef DSPIC33AK_TDM_SPI1_TX_DMA
#define DSPIC33AK_TDM_SPI1_TX_DMA   1
#endif
#ifndef DSPIC33AK_TDM_SPI2_RX_DMA
#define DSPIC33AK_TDM_SPI2_RX_DMA   2
#endif
#ifndef DSPIC33AK_TDM_SPI2_TX_DMA
#define DSPIC33AK_TDM_SPI2_TX_DMA   3
#endif


//===========================================================
// DMA interrupt-vector ownership.
//   1 (default) : TURNKEY -- the HAL DEFINES the _DMA<rx>Interrupt vectors itself.
//   0           : the HAL defines NO vectors. The integrator owns the IVT and calls
//                 dspic33ak_spi_i2s_tdm_inst_rx_isr(spiN()) from their own
//                 _DMA<rx>Interrupt for each instance's RX channel.
//===========================================================
#ifndef DSPIC33AK_TDM_DEFINE_DMA_VECTORS
#define DSPIC33AK_TDM_DEFINE_DMA_VECTORS   1
#endif


//===========================================================
// Instance count + physical assignment.
//
// The transport core defines its leg enum, per-instance ping-pong buffers, the
// s_spi_legs[] table, and the explicit _DMA<rx>Interrupt vectors directly in C, keyed off
// the per-instance channel #defines above (DSPIC33AK_TDM_SPIn_RX/TX_DMA) and the geometry
// macros (DSPIC33AK_TDM_SLOTS_PER_FS / _BLOCK_FRAMES). The number of legs is chosen by
// DSPIC33AK_TDM_USE_SPI2 (1 or 2 SPI audio transports) -- there is no macro row list.
// The per-leg clock role and (rate-agnostic) stream shape are set at runtime by the
// integrator's config (dspic33ak_spi_i2s_tdm_configure_system() / _inst_configure()),
// not here.
//===========================================================

// Per-leg SYNC DOMAIN id: the s_spi_legs[] default (a caller using configure_system() may
// override it per leg at runtime). Legs sharing a domain are co-clocked and started
// phase-locked as a group; different domains are independent/async. NOT the clock role.
// Starter smoke is SPI1-only, so SPI2's value is used only when DSPIC33AK_TDM_USE_SPI2=1.
#ifndef DSPIC33AK_TDM_SPI1_SYNC_DOMAIN
#define DSPIC33AK_TDM_SPI1_SYNC_DOMAIN   (0)
#endif
#ifndef DSPIC33AK_TDM_SPI2_SYNC_DOMAIN
#define DSPIC33AK_TDM_SPI2_SYNC_DOMAIN   (0)
#endif

// sync_domain must be 0..31 (start_all_domains()'s dedup/rollback mask range).
#if (DSPIC33AK_TDM_SPI1_SYNC_DOMAIN >= 32)
#error "DSPIC33AK_TDM_SPI1_SYNC_DOMAIN must be in 0..31."
#endif
#if DSPIC33AK_TDM_USE_SPI2 && (DSPIC33AK_TDM_SPI2_SYNC_DOMAIN >= 32)
#error "DSPIC33AK_TDM_SPI2_SYNC_DOMAIN must be in 0..31."
#endif


#if (DSPIC33AK_TDM_SLOTS_PER_FS <= 0)
#error "DSPIC33AK_TDM_SLOTS_PER_FS must be positive."
#endif

#if (DSPIC33AK_TDM_SLOTS_PER_FS > 255)
#error "DSPIC33AK_TDM_SLOTS_PER_FS must fit in uint8_t."
#endif

#if (DSPIC33AK_TDM_BLOCK_FRAMES <= 0)
#error "DSPIC33AK_TDM_BLOCK_FRAMES must be positive."
#endif

#if (DSPIC33AK_TDM_BLOCK_FRAMES > 65535)
#error "DSPIC33AK_TDM_BLOCK_FRAMES must fit in uint16_t."
#endif

#if ((DSPIC33AK_TDM_USE_SPI2 != 0) && (DSPIC33AK_TDM_USE_SPI2 != 1))
#error "DSPIC33AK_TDM_USE_SPI2 must be 0 or 1."
#endif

#if (DSPIC33AK_TDM_SLOTS_PER_FS > (2147483647 / (2 * DSPIC33AK_TDM_BLOCK_FRAMES)))
#error "SPI/I2S/TDM DMA buffer geometry overflows the static buffer element count."
#endif

#endif // DSPIC33AK_SPI_I2S_TDM_CONF_H

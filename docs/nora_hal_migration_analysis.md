# NORA-HAL migration — analysis (hal-starter ← sonora)

Clone: `dspic33ak-hal-starter-nora`, branch `refactor/nora-hal`, base `7d12e42`
(= `origin/main`, 0/0 vs remote at clone time).
Donor: `dsp-sonora-mothership` @ `2d02359` (`main`).

## 1. What NORA-HAL is

`docs/nora_hal_public_api.md` in sonora: NORA = Native On-chip Resource
Assistant, the public HAL brand for the dsPIC33A family (AK + CK).

- public headers `nora_<periph>.h`; functions/types `nora_*`; macros `NORA_*`
- target backends keep a silicon suffix in the **file/implementation** name
  (`nora_gpio_dspic33a.c`) — never in a public header/type/function/macro
- ISR fast paths: `static inline` in `<module>_<backend>_fast.h`, named
  `<portable name>_hot`; the out-of-line portable version calls the inline
- backend-private helpers with no portable twin keep the chip in their name
  (`nora_uart_dspic33a_reg_set`, `nora_ccp_dspic33a_hot_regs`)
- migration rule: **no compatibility aliases** — the old `dspic33ak_*` /
  `DSPIC33AK_*` public namespace is replaced, not shadowed

## 2. Module inventory

hal-starter `src/hal_*` (11): can, clock, dma, gpio, i2c, nvm, spi,
spi_i2s_tdm, timer, uart, udid. **Every one has a sonora NORA counterpart** —
no module has to be written from scratch.

Sonora additionally offers (not currently in the starter): `hal_adc`,
`hal_reset`, `hal_noinit_ram`, `hal_ccp_input_capture`. Out of scope for a
replacement; candidates for a later additive step.

## 3. API delta per module (headers, prefix-normalised)

Compared exported `dspic33ak_*` vs `nora_*` names with the prefix normalised.

| module | identical | sonora-only | starter-only (needs a decision) |
|---|---|---|---|
| hal_spi | ✅ 18/18 | – | – |
| hal_timer | ✅ 16/16 | – | – |
| hal_nvm | ✅ | – | – |
| hal_dma | ✅ | +10 (`*_hot`, `status_has_overrun/completed_half`) | – |
| hal_gpio / pps | ✅ | +2 (`nora_pinmux_route_input/output`) | – |
| hal_spi_i2s_tdm | ✅ 56 | +12 (diag deadline, `sumprof_*`, `tdmsum_*`) | – |
| hal_udid | ✅ | (header-declared in sonora) | – |
| hal_can | ✅ 31 | – | `canfd_clear_rx_overflow` |
| hal_clock | mostly | +`get_fcy_hz`, `get_fosc_hz`, `switch_source` | `clkgen_configure` → renamed `nora_clock_dspic33a_clkgen_configure` |
| hal_uart | 32 shared | – | 24 names moved to backend-private `nora_uart_dspic33a_*` (`reg_*`, `rx_isr_*`, `device_*`, `get_device`, `instance_is_present`, `async_*`) |
| hal_i2c | 53 shared | +7 `nora_i2c_device_*_irq_*` | 4 `nora_i2c_reg_irq_*` → replaced by the `device_*_irq_*` set |

So the substance of the work is **three real deltas**, not eleven:

1. **uart** — the starter's `*_reg_*` / `*_rx_isr_*` / `*_device_*` helpers are
   *portable-looking* names in the starter but *backend-private* in NORA.
   Consumers (`src/console`, `src/app`) that call them must move to
   `nora_uart_dspic33a_*` or stop calling them.
2. **i2c** — starter's `reg_irq_*` quartet is superseded by sonora's
   per-direction `device_*_irq_*` set. A call-site translation, not a
   feature loss.
3. **can** — `clear_rx_overflow` has no sonora counterpart. Either port it
   into the sonora CAN HAL (preferred, it is reusable) or drop the caller.
   Plus `clock`: one renamed entry point, three new getters.

`hal_spi_i2s_tdm` is the module I expected to be the hard one and it is not:
the starter already carries the SYSTEM/sync-domain contract, and the
`*_conf.h_example` macro schema is **byte-identical modulo the prefix**
(`DSPIC33AK_` → `NORA_`, diff empty). Both sides descend from the same
vendored upstream (`src/hal_*/UPSTREAM.md`).

## 4. Blast radius outside `src/hal_*`

`dspic33ak_*` references in consumer code: `src/app` 271, root
(`main.c`, `board.*`, `board_pins.h`, `nora_spi_i2s_tdm_conf.h`) 95,
`src/board_components` 35, `src/console` 18, `src/fw_update` 16,
`src/clock` 9 — ≈444 lowercase refs, plus the `DSPIC33AK_*` macro set
(`..._I2C_OK` 99, `..._UART_OK` 63, `..._CANFD_OK` 56, TDM geometry, …).

Almost all of it is mechanical (`dspic33ak_`→`nora_`, `DSPIC33AK_`→`NORA_`)
because the two APIs agree. The non-mechanical part is only the three deltas
in §3.

Also to touch: `firmware.X/nbproject/configurations.xml` — 32 path/name hits;
the file list is the build truth, so renamed files must be renamed there in the
same commit.

## 5. Sonora HAL self-containedness

Grep of `#include "..."` across sonora `src/hal_*`: exactly **one** non-`nora_`
include, `board/board_dbg_pins.h`. So the donor HALs drop in without dragging
app/config headers — the only inbound dependency to resolve is that debug-pin
header (or compiling it out).

## 6. Proposed migration order (one buildable commit per step)

0. (done) clone + branch.
1. **Leaf, zero-delta modules first**: spi, timer, nvm, udid, dma, gpio/pps —
   copy sonora files, delete the old ones, update `configurations.xml`,
   rename call sites. Each step builds.
2. **spi_i2s_tdm** — big but prefix-only; includes renaming
   `src/dspic33ak_spi_i2s_tdm_conf.h` → `src/nora_spi_i2s_tdm_conf.h`.
3. **clock** — rename `clkgen_configure`, adopt the new getters,
   re-check `src/clock` (9 refs) against sonora's PLL model.
4. **i2c** — translate `reg_irq_*` → `device_*_irq_*` at the call sites.
5. **uart** — the largest semantic step (console + async + rx ISR ring).
6. **can** — decide `clear_rx_overflow`: port upstream into the sonora CAN HAL,
   or drop.
7. Full clean build of every configuration, then HW bring-up: the starter's
   own boot sequence is the acceptance test (clock, HRT self-check, SST26
   verify, I2C scan, I2C loopback, CAN FD, TDM8 smoke on MikroBUS-A).

## 7. Decisions (owner, 2026-08-08)

- **Direction of truth**: the standalone HAL repos will be NORA-ised in turn,
  so a temporary divergence between this starter and the `UPSTREAM.md`
  pointers is **accepted**. Do not re-base the upstreams first.
- **`hal_can` `clear_rx_overflow`**: **port it into NORA-HAL (sonora)**, then
  vendor the result here. Sonora-side work goes on its own branch in the
  mothership; this starter takes the NORA CAN HAL only after that lands.
- **Extra HALs** (`adc`, `reset`, `noinit_ram`, `ccp_input_capture`): **not
  adopted** — this migration adds no new starter functionality.
- **Base**: `origin/main` (`7d12e42`). The unmerged branches
  (`feat/hal-i3c-foundation`, `exp/pll2-*`) are out of scope; conflicts there
  are accepted rather than pre-empted.

## 8. Module ordering constraint (measured)

Cross-module `#include` among sonora `src/hal_*` is a single edge:
`hal_spi_i2s_tdm` → `nora_dma.h`, `nora_dma_dspic33a_fast.h`,
`nora_high_res_timer.h`, `nora_tick_timer.h`. Every other module includes only
its own headers. So the §6 order is sound as long as **dma and timer precede
spi_i2s_tdm**; all other modules are independent and may be reordered freely.

## 9. What the header-symbol comparison missed (measured during steps 1-3)

The §3 table was built from exported *identifier* names. Three classes of delta
are invisible to that method and only showed up at compile time:

1. **Struct fields.** `nora_dma_channel_cfg_t` renamed `trigger_sel`
   (raw `uint8_t` DMAxSELbits.CHSEL id) to `trigger` (logical
   `nora_dma_trigger_t`). Same field count, different meaning — a semantic
   change, not a rename. One call site (the TDM backend), fixed by vendoring
   Sonora's TDM in the same step.
2. **Consumers reaching into a *register* header.** The starter's TDM diag read
   `DSPIC33AK_DMA_STAT_OVERRUN/HALF/DONE` out of the DMA reg header. NORA
   deletes those macros and answers the same question through
   `nora_dma_status_has_overrun()` / `_has_completed_half()` — which is exactly
   what the §3 "sonora-only +10" additions are for. So a module's *additions*
   can be the mandatory replacement for a consumer habit, not optional extras.
3. **Function-like UPPER-CASE APIs.** `DSPIC33AK_UDID_Read`,
   `DSPIC33AK_NVM_PageErase`, … A regex for `dspic33ak_*(` finds none of them.

Consequence for the remaining modules (clock, i2c, uart, can): expect the
compiler to surface deltas the header diff does not, especially struct fields
and reg-macro reach-through.

## 10. Build-flow gotcha

`buildtools/build.ps1` regenerates `firmware.X/nbproject/Makefile-*.mk` only
with `-Full` (or when the makefile is missing). Those generated makefiles are
tracked, and they carry the *source file list*. So after every
`configurations.xml` edit the build must be `build.ps1 -Full`; a bare
`build.ps1` fails with `No rule to make target '../src/.../<old name>.c'`.

## 11. Program-size effect

| after step | program-region bytes | delta |
|---|---|---|
| base (`7d12e42`) | 84,116 | — |
| step 1 (spi/timer/nvm/udid) | 84,116 | 0 |
| step 3 (dma/gpio + spi_i2s_tdm) | 91,364 | **+7,248** |

The +7,248 B is Sonora's richer TDM module: `sumprof_*` / `tdmsum_*` ISR-load
profiling and the extra diag paths are part of its public API and are compiled
unconditionally (only `ENA_TDM_DBG` blocks are gated). Nothing in the starter
calls them. This is HAL capability, not starter functionality, so it does not
contradict the "no new features" decision — but it is a real 7 kB. Options if
that matters: accept it; enable `--gc-sections` *with* per-function sections in
the project (they only work together); or gate the profiling behind a conf
macro upstream in Sonora.

## 12. Sonora-side residues noticed while vendoring

Reported here, deliberately **not** fixed in this starter (donor files are kept
byte-identical to Sonora):

- `nora_nvm.h` keeps `NORA_NVM_PageErase` / `NORA_NVM_ReadWord` / … —
  callable functions spelled `NORA_*`, which `nora_hal_public_api.md` reserves
  for compile-time identifiers only.
- `nora_spi_dspic33a.c` still uses file-local `DSPIC33AK_SPI_REG_ROW` /
  `DSPIC33AK_SPI_ARRAY_LEN` macros; `nora_dma_dspic33a_reg.h` likewise keeps
  `DSPIC33AK_DMA_*` bit macros. Private, but leftovers of the rename.

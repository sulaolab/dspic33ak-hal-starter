# NORA-HAL migration — analysis (hal-starter ← sonora)

Clone: `dspic33ak-hal-starter-nora`, branch `refactor/nora-hal`, base `7d12e42`
(= `origin/main`, 0/0 vs remote at clone time).
Donor: `dsp-sonora-mothership` @ `2d02359` (`main`).

## 1. What NORA-HAL is

`docs/nora_hal_public_api.md` in sonora: NORA = Native On-chip Resource
Assistant, the public HAL brand for the on-chip resource layer shared by
dsPIC33AK (dsPIC33**A** family) and dsPIC33CK (dsPIC33**C** family) projects.
The brand spans the two families; it is not a synonym for either one, and NORA
is not "the dsPIC33A HAL". (This line used to read "the public HAL brand for the
dsPIC33A family (AK + CK)", which is where the baseless `_dspic33a` backend tag
came from — see §1a.)

- public headers `nora_<periph>.h`; functions/types `nora_*`; macros `NORA_*`
- target backends keep a silicon suffix in the **file/implementation** name
  (`nora_gpio_dspic33ak.c`) — never in a public header/type/function/macro
- ISR fast paths: `static inline` in `<module>_<backend>_fast.h`, named
  `<portable name>_hot`; the out-of-line portable version calls the inline
- backend-private helpers with no portable twin keep the chip in their name
  (`nora_uart_dspic33ak_reg_set`, `nora_ccp_dspic33ak_hot_regs`)
- migration rule: **no compatibility aliases** — the old `dspic33ak_*` /
  `DSPIC33AK_*` public namespace is replaced, not shadowed

### 1a. The backend tag is `_dspic33ak`, not `_dspic33a` (2026-08-09)

This clone was migrated with a `_dspic33a` backend tag, taken from the broken
sentence quoted above. It has been normalised to **`_dspic33ak`**, naming the
part series the backend is actually written and validated against: nothing
validates a family-wide dsPIC33A backend, and the code is per-part where it has
to be (`NORA_SPI_I2S_TDM_DSPIC33AK_DEV_AK512` / `..._DEV_AK128`). There is no
`_dspic33c` counterpart either — the CK project keeps `dspic33ck_*` as a
**declared divergence** (audited: zero identifiers, macros or file names there
carry a bare `33c`), so we never write the short form `dspic33c` in our own
names. `dsPIC33A` / `dsPIC33C` still appear in this repo where they are facts
about the silicon ("CLKGEN is a dsPIC33A block") or real Microchip paths
(`xc16/support/dsPIC33A/h/…`); those are left alone.

sonora normalised first (it is the superset), and this clone follows as a
**re-sync of the same bytes** rather than an independent rename — after the
rename, 69 of the 71 shared `src/hal_*` files are byte-identical to sonora's,
the two exceptions being `hal_spi_i2s_tdm/README.md` and `hal_timer/README.md`,
which already differed before it. The plan and sonora's verification numbers are
in sonora `docs/nora_chiptag_normalisation_and_standalone_publish.md`.

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
| hal_clock | mostly | +`get_fcy_hz`, `get_fosc_hz`, `switch_source` | `clkgen_configure` → renamed `nora_clock_dspic33ak_clkgen_configure` |
| hal_uart | 32 shared | – | 24 names moved to backend-private `nora_uart_dspic33ak_*` (`reg_*`, `rx_isr_*`, `device_*`, `get_device`, `instance_is_present`, `async_*`) |
| hal_i2c | 53 shared | +7 `nora_i2c_device_*_irq_*` | 4 `nora_i2c_reg_irq_*` → replaced by the `device_*_irq_*` set |

So the substance of the work is **three real deltas**, not eleven:

1. **uart** — the starter's `*_reg_*` / `*_rx_isr_*` / `*_device_*` helpers are
   *portable-looking* names in the starter but *backend-private* in NORA.
   Consumers (`src/console`, `src/app`) that call them must move to
   `nora_uart_dspic33ak_*` or stop calling them.
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
`hal_spi_i2s_tdm` → `nora_dma.h`, `nora_dma_dspic33ak_fast.h`,
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
with `-Full` (or when the makefile is missing). Those makefiles are gitignored
(`.gitignore:13`), so a clone never has them and the first build generates
them — but once generated they persist across builds and they carry the
*source file list*. So after every
`configurations.xml` edit the build must be `build.ps1 -Full`; a bare
`build.ps1` fails with `No rule to make target '../src/.../<old name>.c'`.

## 11. Program-size effect

| after step | program-region bytes | delta |
|---|---|---|
| base (`7d12e42`) | 84,116 | — |
| step 1 (spi/timer/nvm/udid) | 84,116 | 0 |
| step 3 (dma/gpio + spi_i2s_tdm) | 91,364 | **+7,248** |
| step 4 (clock) | 91,920 | +556 |
| step 5 (i2c) | 92,316 | +396 |
| step 6 (uart) | 92,108 | -208 |
| step 7 (can) | 92,596 | +488 |
| step 8 (TDMsum gate, `NORA_TDM_SUMPROF=0`) | **87,692** | **-4,904** |

The +7,248 B is Sonora's richer TDM module: `sumprof_*` / `tdmsum_*` ISR-load
profiling and the extra diag paths are part of its public API and were compiled
unconditionally (only `ENA_TDM_DBG` blocks were gated). Options considered:
accept it; enable `--gc-sections` *with* per-function sections; or gate the
profiling behind a conf macro upstream in Sonora. The last one was chosen — see
§11f, which also corrects what this paragraph originally claimed.

## 11b. Clock: CLKGEN left the portable header (step 4, measured)

The §3 row "one renamed entry point" understated it. NORA **moved the whole
CLKGEN surface out of `nora_clock.h` into `nora_clock_dspic33ak.h`** — the enum,
the config struct and the call — on the grounds that CLKGEN is a dsPIC33A block
with no CK counterpart, so a portable header could not honour it. Concretely:

| starter | NORA |
|---|---|
| `dspic33ak_clock_clkgen_t` | `nora_clock_dspic33ak_clkgen_t` (+ `CLKGEN_13`, the CCP time base) |
| `dspic33ak_clock_clkgen_config_t` | `nora_clock_dspic33ak_clkgen_config_t` (fields unchanged: `source`, `divide_by`) |
| `dspic33ak_clock_clkgen_configure()` | `nora_clock_dspic33ak_clkgen_configure()` |
| — | `nora_clock_switch_source()`, `nora_clock_get_fosc_hz()`, `nora_clock_get_fcy_hz()` |
| `DSPIC33AK_CLOCK_ERR_TIMEOUT` (one value) | + 10 specific `..._ERR_*_TIMEOUT` / `..._READBACK` values |

`nora_clock_pll_config_t` and the source/PLL enums are unchanged, so
`starter_clock_init()`'s PLL1 call is a pure prefix swap. The only structural
edit in the consumer is the second include: `src/clock/starter_clock.c` now
includes `nora_clock_dspic33ak.h` as well, which is correct by NORA's own rule —
it is board bring-up code, and it now says so at the call site.

File-name mapping (starter → NORA): `dspic33ak_clock.c` → `nora_clock_dspic33ak.c`,
`dspic33ak_clock_device.c` → `nora_clock_device_dspic33ak.c`,
`dspic33ak_clock_reg.*` → `nora_clock_dspic33ak_reg.*`, plus the new
`nora_clock_dspic33ak.h`. Seven files where the starter had six.

`docs/clock_hal_integration.md` was left untouched on purpose: it is a
historical import record (blob SHA-1s, "removed legacy files"), so renaming
identifiers inside it would falsify the record rather than update a document.

## 11c. §3's "three real deltas" was wrong about i2c and uart (steps 5-6)

Two of the three predicted semantic deltas did not exist at the consumer
boundary, and the reason is the same in both cases: **the names that moved
were HAL-internal, so replacing the module carried them along.** The §3 method
compared *all* exported identifiers of the old module against the new one,
which conflates "this name changed" with "a caller has to change".

- **i2c** — the `reg_irq_*` → `device_*_irq_*` change is entirely inside the
  register/device layer. All three public headers (`nora_i2c.h`,
  `nora_i2c_master.h`, `nora_i2c_slave.h`) are byte-identical to the starter's
  modulo the prefix, slave ISR entry points included. Zero call-site edits.
- **uart** — of the 32 identifiers the starter's consumers actually name, 31
  map by prefix swap and all 31 live in `nora_uart.h`; the 32nd is the
  `..._rx_isr_ring.h` **filename**. The 24 names that picked up the
  `dspic33ak` tag are the ring/reg/device/async internals, which no consumer
  called. One real edit: `nora_uart_rx_irq_handler()` moved *out* of the ring
  header *into* `nora_uart.h`, which left `src/console/uart_irq.c`'s include of
  the backend ring header dead, so it was dropped.

The check that would have predicted this correctly is not a header diff at all:
intersect **the identifiers consumers actually reference** with the new
module's public header. That is the set in §4, not the set in §3. Both
measurements are cheap; only the second one predicts work.

So of the three predicted deltas only **can**'s `clear_rx_overflow` is real,
and the genuine surprises came from elsewhere entirely (§9's struct field and
reg-macro reach-through, §11b's CLKGEN relocation) — none of which a symbol
diff surfaces.

Sizes also do not move monotonically: uart came in 208 bytes **smaller**.

## 11d. can was the opposite mistake: one predicted delta, three real ones (step 7)

§3 predicted exactly one CAN delta (`clear_rx_overflow`) and §11c concluded that
`can` was "the only real one" of the three. Both statements were about the wrong
thing. The identifier-level check passes perfectly — all **28** identifiers the
starter's consumers name map by plain prefix swap, zero unmapped — and yet there
were three deltas, because two of them are **behavioural, behind an unchanged
name**:

1. **`clear_rx_overflow` missing** (predicted). Ported upstream per the owner's
   decision: sonora `feat/canfd-isr-fixes-and-tdm-sumprof-gate` `c26ecb0` adds
   `nora_canfd_clear_rx_overflow()` *and* an `rx_overflow` field on
   `nora_canfd_bus_status_t` — the function alone would have been useless, since
   a polling caller could not see the sticky flag either.
2. **`isr_enable()` arity.** Starter: `isr_enable(inst, cb, ud, prio)`, a
   deliberate one-call RX setup that registers the FIFO-draining callback
   itself. NORA: `isr_set_callback(inst, cb, ud)` + `isr_enable(inst, prio)`.
   NORA is the direction of truth, so the four call sites were split rather
   than the API restored. Mechanical, but invisible to a symbol diff: **the
   name and the spelling are identical, only the signature moved.**
3. **`isr_enable()` enabled the transmit CPU line.** The starter deliberately
   arms only the RX and general lines and does not define `_C1TXInterrupt`;
   `can_rx_isr_selftest.c` *asserts* `_C1TXIE == 0` after `isr_enable()` as one
   of the four things it proves on hardware. Sonora's `irq_line_enable()` set
   `_CxTXIP`/`_CxTXIE` unconditionally, which would have failed that assertion —
   and sonora's own header documents that its TX path is unvalidated and traps.
   Fixed upstream (`427e406`) rather than by relaxing the test: the TX CPU line
   now follows its module source, armed by `tx_start()` and disarmed by
   `tx_abort()` and the TX_COMPLETE one-shot. The same commit makes
   `priority == 0` select a new `NORA_CANFD_ISR_DEFAULT_PRIORITY` (4) instead of
   returning `ERR_INVALID_ARG`, which is what the starter documented and what
   its self-test passes. No sonora consumer references `nora_canfd_*` today, so
   neither change alters behaviour in the mothership.

The lesson generalises past this migration: §11c's cheap check (consumer
identifiers ∩ new public header) predicts **call-site edits**, and it is right
about those. It cannot see a changed signature behind an unchanged name, and it
cannot see a changed register write behind an unchanged function. On `can` the
identifier check was 28/28 clean and the module still could not be dropped in.
The only thing that catches deltas of class 2 is the compiler; the only thing
that catches class 3 is reading the backend, or a hardware test that happens to
assert on it — which this starter, unusually, has.

Two sonora commits are therefore prerequisites of this step and are **not yet
pushed**: `c26ecb0` and `427e406` on `feat/canfd-isr-fixes-and-tdm-sumprof-gate`, in the
worktree `_wt_nora_canfd_ovf` (a separate worktree because the mothership's main
tree carries another session's uncommitted edits). Until they land in the
mothership, this starter's `src/hal_can/` is *not* reproducible from sonora
`main`.

## 11e. Hardware acceptance (2026-08-09) — PASS

All 11 modules NORA-ised, `build.ps1 -Full` + `flashauto.ps1` on PKOB4
`020085204RYN000057` (dsPIC33AK512MPS512, Device ID `0xa77c`, UDID
`FFFFFFFF010B00DBB8D0000D00D76A9D` — the same board as the clock-HAL closeout in
`clock_hal_integration.md`). Console read through the `sonora_monitor` HTTP
bridge (COM12 @230400); the monitor was already running and was left running.

| check | result |
|---|---|
| clock | `sysclk : 200000000 Hz (FRC -> PLL1)` |
| HRT self-check | PASS (`d1=79856 d10=999998`) |
| SST26 | JEDEC `BF 26 12` good; sector verify @0x000000 OK |
| I2C scan | 1 device, ACK at 0x1A |
| I2C loopback | runs; reads back zero — **not wirable on this board**, see below |
| CAN1 RX-ISR self-test | **PASS**, all four criteria |
| CAN1 FD 500k/2M | live on the bus, HAL self-check PASS |
| TDM8 smoke (MikroBUS-A) | `exp_fs~48kHz exp_bclk~12500kHz miss=0` over 22,917 blocks |
| bank / config | P1 active, BTSEQ=0xFFF, active UCA OK |

The two lines that specifically validate step 7's upstream work:

```
   RX overflow        : detected (status=yes, callback=yes; 4/24 frames held)
   TX interrupt line  : disabled (as required)
```

The first exercises **both** paths added by `c26ecb0` — the sticky-flag read via
`get_status()` with interrupts off, and the RX_OVERFLOW callback after re-arming
— which is why the port needed the `rx_overflow` field and not just the clear
function. The second is the assertion that sonora's original
`irq_line_enable()` would have failed, now passing because `427e406` moved the
TX CPU line to `tx_start()`.

`<[CAN1 Tx] transmit queue full / timeout` with `state=error-passive TEC=128` is
the documented no-ACK-partner behaviour on a single node, not a fault; the
firmware prints the explanation itself.

**I2C loopback cannot be confirmed on this board at all** (owner, 2026-08-09).
README's expected output is `>[I2C3 Rd] size=8 1122334455667788`; this setup
reports `size=0` and an all-zero master read. That is the *expected* result here:
this motherboard is modified so that I2C1/I2C2 are split off to drive CODEC-A and
CODEC-B, so the I2C2-master ↔ I2C3-slave loop the demo wants is not wired and
cannot be. `clock_hal_integration.md` §"Caveats" records the identical result on
this same board (matching PKOB4 serial and UDID), so it also predates the NORA
work and is not a regression from this migration.

So this is not a pending fixture-based check to schedule on *this* board — it is
out of scope for it. Positive confirmation of the I2C slave data path needs a
board whose I2C2/I2C3 pins are free.

## 11f. The TDMsum profiler was not dead code — it ran in the ISR (correction + fix)

§11 originally said of `sumprof_*` / `tdmsum_*`: *"Nothing in the starter calls
them."* **That was wrong**, and wrong in the direction that matters. The four
call sites are inside `tdm_rx_block()` in `nora_spi_i2s_tdm_dspic33ak.c` — the
TDM RX-block ISR — bracketing every block:

```c
const bool sum_meas = nora_high_res_timer_is_initialized();
if( sum_meas ) { nora_spi_i2s_tdm_dspic33ak_sumprof_enter( nora_high_res_timer_get_count() ); }
```

The only gate was high-res-timer availability, which the starter satisfies. So
the starter was paying **ROM for the profiler and ISR cycles on every block**
for a measurement no code in the repo ever read. The mistake came from checking
for *consumer* references (the app layer) and concluding "unused", when the
caller was the HAL's own ISR — the same class of error as §11d: an
identifier-level check answers a question about call sites, not about behaviour.

Fixed **upstream in Sonora**, per the owner's ruling, as a new compile-time
config macro `NORA_TDM_SUMPROF` (`nora_spi_i2s_tdm_conf.h`, 0/1, `#ifndef`-
guarded, **default 1** so the mothership is unaffected). At 0 the following are
not compiled at all: the profiler state + inline hooks in
`nora_spi_i2s_tdm_dspic33ak_diag_fast.h`, its three out-of-line bodies in
`..._diag.c`, the four ISR call sites, the three public `nora_spi_i2s_tdm_tdmsum_*`
entry points (declaration and definition), and their all-leg RX-DMA-IE masking
helpers. Sonora's own `audio_transport.c` TDMsum telemetry line is gated to
match, so the mothership still builds with 0.

Two details worth keeping:

- **The declaration is gated too**, not just the body. A reference then fails at
  compile time instead of silently returning a zero snapshot that never updates.
- **A missing macro cannot be allowed to mean 0.** `NORA_TDM_SUMPROF` gates code
  *out*, so an older project `conf.h` that predates the macro would silently
  lose the profiler. `nora_spi_i2s_tdm.h` therefore `#error`s if it is undefined
  after including the conf header.

Measured: the starter sets 0 and drops **92,596 → 87,692 B (−4,904)**. Since
`--gc-sections` leaves merged `.const` strings behind even when it discards a
function, physical exclusion is the only way to measure this honestly — the gate
*is* the measurement. The per-leg monitor
(`nora_spi_i2s_tdm_inst_get_load()`) is untouched and still reports the
starter's single-leg occupancy, so no visible capability was lost. The ISR-cycle
saving was not measured on hardware.

## 11g. Review round 1 (AK side): the TX line had a second way back on

The reviewer approved `c26ecb0` and the `NORA_TDM_SUMPROF` gate, and found one
real defect in `427e406` — the fix for "`isr_enable()` enables the TX CPU line"
closed only the *enable* route:

```c
nora_canfd_isr_disable(inst);      /* all three CPU lines + module sources off */
nora_canfd_tx_start(inst, &f);     /* ... re-arms the TX CPU line anyway */
```

`tx_start()` calls `irq_tx_line_enable()` unconditionally, and the priority it
uses is `g_irq_priority[inst]`, which `isr_disable()` never forgot. So a caller
that deliberately disabled interrupts got one back, together with the
`_CxTXInterrupt`-vector requirement it had avoided.

Fixed upstream (sonora `bf232f4`): the ISR layer now tracks `g_isr_enabled[]`
(set by `isr_enable()`, cleared by `isr_disable()`), and `tx_start()` returns
`NORA_CANFD_ERR_SEQUENCE` while it is false. That is the honest contract —
async TX reports completion *only* through the event callback, so it is
meaningless with the event layer off, and the blocking
`nora_canfd_transmit()` remains the interrupt-free path, unchanged.

`can_rx_isr_selftest.c` gained the matching regression, printed as a fifth
criterion and included in the PASS condition:

```
   tx_start when off  : refused, line still off
```

It runs where the test already has interrupts off (the phase-2 overflow setup)
and checks both the return code and `_C1TXIE` itself — a return code alone would
not prove the register was left alone. A refused `tx_start()` queues no frame, so
the overflow phase is unaffected. Cost: 87,692 → 88,324 B (+632, guard + test).

**Hardware: PASS (2026-08-09, `2d7f2d1`, `APP_BUILD_STARTER_DEFAULT`, 88,324 B,
B-XTAL jumper, PKOB4 `020085204RYN000057`).** Clean `-Full` build, provisioned
bundle `PASS`, flashed and reset; console read through the `sonora_monitor` HTTP
bridge (COM12 @230400 — no COM port opened directly):

```
 CAN1 RX-ISR self-test:
   callback fired     : yes
   frame content      : match
   RX overflow        : detected (status=yes, callback=yes; 4/24 frames held)
   TX interrupt line  : disabled (as required)
   tx_start when off  : refused, line still off
 CAN1 RX-ISR self-check: PASS.
```

The rest of the banner matches §11e (HRT self-check PASS, SST26 JEDEC good +
sector verify OK, I2C scan finds 0x1A, SW3 CN event line present, TDM1 smoke
running with `miss=0`). The I2C loopback still reports `I2C3 Rd size=0` and a
zero read on the I2C2 side, which is the expected result on this board — its
I2C1/I2C2 are wired to CODEC-A/CODEC-B, so the loop is absent by construction
(see §11e). Positive confirmation of the slave data path needs a different board.

The reviewer's other two points: the `NORA_TDM_SUMPROF=0` path is now built and
linked clean on the sonora side as well (the mothership's own conf stays at 1, and
`audio_transport.c`'s TDMsum line compiles out with it) — only the pre-existing
`touch.c` pre-release `#warning` appears. Splitting the TDM gate onto its own
branch was *not* done: it would mean force-pushing a branch already under review,
which is the owner's call, not a cleanup to take unilaterally.

## 12. Sonora-side residues noticed while vendoring

Reported here, deliberately **not** fixed in this starter (donor files are kept
byte-identical to Sonora):

- `nora_nvm.h` keeps `NORA_NVM_PageErase` / `NORA_NVM_ReadWord` / … —
  callable functions spelled `NORA_*`, which `nora_hal_public_api.md` reserves
  for compile-time identifiers only.
- `nora_clock_dspic33ak.h:30` writes a path as `board/clock/*` inside a block
  comment, so the compiler emits `warning: "/*" within comment [-Wcomment]`
  three times per build. Cosmetic, but it is the only warning this project
  produces; fixing it belongs upstream.
- `nora_dma_dspic33ak_reg.h:11` refers to a file called `dspic33ak_i2c_reg.h`,
  which no longer exists on either side (it is `nora_i2c_dspic33ak_reg.h` now).
- `nora_spi_dspic33ak.c` still uses file-local `DSPIC33AK_SPI_REG_ROW` /
  `DSPIC33AK_SPI_ARRAY_LEN` macros; `nora_dma_dspic33ak_reg.h` likewise keeps
  `DSPIC33AK_DMA_*` bit macros. Private, but leftovers of the rename.

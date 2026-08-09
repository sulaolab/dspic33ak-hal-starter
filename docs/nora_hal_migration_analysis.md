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

**Update (2026-08-09, §13):** they have landed. `c26ecb0`, `427e406` and
`bf232f4` (§11g) are all on sonora `main`, and a blob comparison of sonora
`main` against this starter's `b70982d` now reports `src/hal_can/` **10/10
identical** — so the reproducibility caveat above is discharged. The same
comparison over `src/hal_spi_i2s_tdm/` matches on every source file, with
`README.md` the one deliberate divergence (de-codenamed for publication) and
`UPSTREAM.md` starter-only.

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

## 13. Task B: the standalone HAL repositories become published snapshots

Ten `sulaolab/dspic33ak-hal-*` repositories are being NORA-ised **from this
starter's bytes** and renamed `nora-hal-dspic33ak-*`. The same move inverts the
vendoring direction: a repository stops being the place its HAL is edited and
becomes a published snapshot of `src/hal_<m>/` here, which is itself a snapshot of
the Sonora tree that runs on silicon. A fix made only in a standalone repository
would now be a fork of validated code — `a2ce22a` rewrote the two `UPSTREAM.md`
files that still claimed the old direction.

Seven steps per repository (gpio was the approved template):

1. **rename-only commit** — every file R100, 0 insertions / 0 deletions, so the
   rename reviews on its own and the next diff is readable;
2. **content refresh** — `src/` overwritten with this starter's bytes;
3. **mechanical proof** — reverse-normalise each new file (strip the
   `_dspic33ak` tag, `nora_` → `dspic33ak_`, `NORA_` → `DSPIC33AK_`) and diff
   against its pre-rename blob; whatever survives that is, by construction, not
   naming;
4. **blob-hash identity** against this starter, recorded file by file;
5. **docs** — identifier substitution in prose plus `docs/nora_migration.md`,
   touching no file under `src/`, so step 4's claim stays true;
6. `gh repo rename` and a PR;
7. **`main` lands last**, together with this starter, so the fleet flips
   namespace in one step.

State, 2026-08-09. Every repository sits on `refactor/nora-hal` with `main`
untouched. "ref" is the starter commit its `src/` is verified identical to.

| repo | rename | content | docs | PR | GH renamed | ref |
|---|---|---|---|---|---|---|
| gpio (template) | `b5fbcec` | `f812d11` | `bfcd662` | #9 | yes | `b70982d` |
| spi | `b9632f2` | `5ec9ced` | `d3efabd` | #3 | yes | `b70982d` |
| timer | `e218a12` | `280168c` | `ead23e8`, `d7c5c06` | #6 | yes | `a2ce22a` |
| dma | `76f8fe1` | `0f778e3` | `39b65e5` | #5 | yes | `b70982d` |
| clock | `f9c3a53` | `c2518d2` | `3728e28` | #4 | yes | `b70982d` |
| i2c | `b148137` | `9e60f69` | `97223c6` | #9 | yes | `b70982d` |
| uart | `510e4ef` | `092f676` | `86dc04b` | #11 | yes | `b70982d` |
| can | `f995c99` | `34d06e7` | `edbc659` | #3 | yes | `a2ce22a` |
| spi-i2s-tdm | `9e2c54d` | `a8dfac2` | `31a4b79` | #7 | yes | `a2ce22a` |
| ccp-input-capture | `21cb76a` | `8de6f33` | `00d6c51` | #1 | yes | n/a (sonora `main`) |

`a2ce22a` differs from `b70982d` in documentation only, so the seven repositories
already published against `b70982d` keep an accurate claim; only timer's
`src/README.md` needed the re-sync (`d7c5c06`).

Measured for the can + spi-i2s-tdm wave — both **100 % blob-identical** to this
starter (10/10 and 12/12):

- **can** — 7 of 10 files reverse-normalise byte for byte, and the residue is the
  ISR layer alone (`nora_canfd_isr.h` non-comment +3/−0,
  `nora_canfd_isr_dspic33ak.c` +61/−7, `nora_canfd_node.h` 0/0). API 25 → 26
  functions and 13 → 14 public `#define`s across the five headers (the "33 → 34"
  figure first recorded here counted a wider macro set; the delta is +1 either
  way), nothing removed — so what the diff really carries is
  §11d and §11g **behind unchanged names**: the sticky `rx_overflow` field with
  `clear_rx_overflow()`, the TX CPU line following its module source rather than
  `isr_enable()`, `priority == 0` selecting `NORA_CANFD_ISR_DEFAULT_PRIORITY`, and
  `tx_start()` returning `ERR_SEQUENCE` while the ISR layer is off. §11d's
  four-argument `isr_enable()` arity delta does **not** apply to that repository:
  it already had the `isr_set_callback` + `isr_enable(inst, prio)` shape.
  Its `docs/` therefore cites sonora `main` (`c26ecb0` / `427e406` / `bf232f4`)
  rather than the `91adb63` the other seven name: that commit predates this work,
  and sonora `main`'s `src/hal_can/` was verified 10/10 blob-identical to this
  starter before the claim was written. Its two prose documents were **not** merely
  substituted — the refresh made two statements factually wrong ("forward all three
  vectors", and the `IEC`/`IFS` mask list), and both were corrected in `edbc659`.
- **spi-i2s-tdm** — 3 of 11 pure, 1 new file, from five causes: (a) the DMA HAL's
  `nora_dma_trigger_t` / `_channel_t` / `_status_t` adopted in place of raw CHSEL
  bytes and `uint8_t`/`uint32_t`, so `nora_spi_i2s_tdm_diag.h` now includes
  `nora_dma.h` and the cross-repo dependency is explicit in a header; (b) the CPU
  interrupt bits off the pointer table onto DFP bit aliases, as in i2c and uart,
  which also removes the AK128 `IEC1`/`IEC2` bank straddle; (c) `DEV_AK512` /
  `DEV_AK128` / `DEVICE`, `STAT_SPIROV` and `hw_sample_ack_errflags()` moved from
  the public headers into `_dspic33ak_hw.h` / `_dspic33ak_reg.h` — they measure as
  "removed" only because the measurement reads untagged headers; (d)
  `NORA_TDM_SUMPROF` made mandatory by `#error`, **the one breaking change for an
  existing consumer's conf header**; (e) §11f's TDMsum profiler, three entry
  points plus the new `_dspic33ak_diag_fast.h`. Its `src/README.md` residue is
  exactly three pre-rename sibling-repo URLs; the fourth changed line, the
  register-mask helper's name, reverse-normalises onto the old text, which is the
  mechanical proof that `a2ce22a`'s fix there was naming and nothing else.

can and spi-i2s-tdm are done through step 6: docs committed, both repositories
renamed on GitHub, local `origin` re-pointed, and PRs open (can #3,
spi-i2s-tdm #7). Their PR bodies were hand-written rather than generated from the
`nora_pr.py` template, because the template hard-codes the `91adb63` → `b70982d`
chain and the plain tag-stripping normalisation, and neither holds for these two.

**ccp-input-capture** is now done through step 6 as well, and it is the one
repository in the ten whose "ref" column cannot name a starter commit: there is no
`src/hal_ccp_input_capture/` here, so its upstream anchor is sonora `main`'s
`src/hal_ccp_input_capture/` directly. Its `docs/nora_migration.md` says so
explicitly rather than citing this starter, which would be a claim about a file
that does not exist. What that wave measured:

- **Blob identity: `src/` 6 of 6, `tests/` 8 of 9.** The single difference is
  `tests/run_host_tests.ps1`, and it is unavoidable: upstream the module is at
  `<repo>/src/hal_ccp_input_capture` and the runner resolves `$repoRoot` two levels
  up, while the snapshot publishes the module flat into `src/`. Two assignment
  lines changed, the SPDX header was restored, and the file states the change in a
  comment. Every test `.c` file was checked path-free and published verbatim.
- **The `include/` flattening** is new to this repository — the other nine already
  kept their public header inside `src/`. The user's decision was to flatten, so
  the layout matches both the siblings and the upstream module folder; a consumer
  changes only its include path, since headers are included by bare name.
- **Residue: the device whitelist left the public header.** `nora_ccp_input_capture.h`
  is +1/−15 non-comment, and the 15 are the whole `DEV_AK512` / `DEV_UNSUPPORTED`
  / `__dsPIC33AK512MPS512__` / `#error` block plus the opt-in escape hatch
  `DSPIC33AK_CCP_ICAP_ALLOW_UNSUPPORTED_STUBS`. Public `#define`s in the header go
  **6 → 1**. The guarantee moved into the backend as a DFP capability test
  (`CCP9CON1` + `_IFS4_CCP9IF_MASK` + `_IEC4_CCP9IE_MASK` + `_IPC16_CCP9IP_MASK`
  → `NORA_CCP_DSPIC33AK_HAS_FULL_CCP_MAP`), with `tests/test_unsupported.c`
  compiling the backend against a CCP9-less `xc.h` and calling every entry point so
  that a function confined to the full-inventory branch surfaces as a link error.
  This is the cleanest instance in the migration of §1's rule paying off: device
  selection is not public API. API 8 → 9 functions, the +1 being
  `nora_ccp_icap_irq_clear()` for an application-owned ISR that drains the FIFO
  itself and until now had to write `IFSx` by hand — which is why such files ended
  up including `<xc.h>` and the backend register header. `_reg.h` residue is 1
  dropped SPDX line; the backend is +85/−44 non-comment (capability test, the move
  onto `_CCPnIF`/`_CCPnIE` bit aliases as in i2c/uart/dma/spi-i2s-tdm, and the hot
  path relocating into the new `_fast.h`).
- **`LICENSE` was a blob-level EOL inversion**, not a working-tree artefact: CRLF
  baked into the blob against the repository's own `.gitattributes`, so it read as
  permanently modified and `git checkout --` could not clear it.
  `git add --renormalize` (`a4161ef`, landed straight on `main` since it is
  namespace-independent) produced a pure-LF blob with `git diff --ignore-all-space`
  empty.
- **`prepare/v1.0.0` deleted** (user decision). Its only difference from `main` was
  a weaker `.gitattributes` / `.gitignore` — a superseded release-prep branch.
- **The host suite does not currently pass, and that is upstream's**, recorded in
  the repository's README, CHANGELOG, `docs/nora_migration.md` and PR rather than
  patched. It stops at `test_validation`, and the examples build fails identically,
  because `tests/fake_xc/xc.h` defines the `_IFSx_CCPnIF_MASK` capability macros
  but not the bare `_CCPnIF` / `_CCPnIE` bit-alias lvalues the code now writes.
  Verified not to be the adapted path: upstream's own copy of the runner, against
  the upstream tree, produces the identical `C2065` errors. The two header stages
  pass. Target builds with XC-DSC against the real DFP are unaffected — this is the
  fake header lagging the DFP-bit-alias change, and it is the first place where
  §11-class refactoring outran the host test scaffolding.

Its PR body was hand-written for the same reason can's and spi-i2s-tdm's were,
plus one more: the template's chain has no valid value for this repository at all.

Remaining:

- The five CMSIS driver repositories (`dspic33ak-{can,gpio,i2c,usart,sai}-cmsis-driver`)
  **break** on the renames. **Filed 2026-08-09**, one tailored issue each: can #3,
  gpio #2, i2c #9, usart #4, sai #7. Measuring it first changed the story in three
  ways worth keeping here:
  - `UPSTREAM_REPO` does **not** break. GitHub redirects a renamed repository's
    clone URL indefinitely, verified by cloning `dspic33ak-hal-gpio.git` after the
    rename. So the sync keeps succeeding while `UPSTREAM.md` records a name that no
    longer exists — a silent accuracy bug, not an outage.
  - `HAL_FILES` breaks loudly (the script raises on the first missing file), but the
    dangerous case is the opposite one: it is a literal list, so a file **added** by
    the refresh is silently not copied and surfaces later as a missing include. Three
    places have that shape — `nora_dma_dspic33ak_fast.h` (needed by the DMA backend
    *and* by both spi-i2s-tdm `.c` files, so it hits sai twice),
    `nora_spi_i2s_tdm_dspic33ak_diag_fast.h`, and i2c where `dspic33ak_i2c_common.h`
    is **gone** and `nora_i2c_dspic33ak_internal.h` took its place — the only entry
    in the five that is a restructure rather than a rename. gpio gained a PPS pair
    it does not currently vendor.
  - Fixing the sync script is not enough: the wrappers call the renamed API
    directly — 65 / 37 / 57 / 49 / 57 HAL identifier occurrences in
    can / gpio / i2c / usart / sai `cmsis_driver/*.{c,h}`. In gpio and sai the
    *wrapper's own public header* includes a HAL header, so the rename reaches the
    CMSIS driver's consumers too. sai additionally has to define
    `NORA_TDM_SUMPROF` in its preserved local `..._conf.h`, or §11d's `#error`
    stops its build, and its `RTE_Device_SAI_..._example.h` is the only RTE example
    in the five that names a HAL macro (`DSPIC33AK_TDM_SLOTS_PER_FS`).

  None of them is broken *today*, because all ten HAL repositories still have an
  untouched `main`. They break at the moment step 7 lands, which is the argument for
  landing that batch deliberately rather than incidentally.
- Eight of the ten `src/hal_*` folders here carry no `UPSTREAM.md` at all; only
  the two that had one were inverted.
- §11's "zero-delta modules" line still counts gpio/pps as zero while §9's table
  records `+2` for it; one clarifying line would settle which is meant.
- Upstream (Sonora, then re-sync). **Superseded on 2026-08-09 for the comment items:**
  the naming residues below were fixed standalone-first and reflected into this
  starter, so `starter == standalone` holds and *both* now differ from Sonora until
  Sonora is patched. See §14. The items were: "Internal dsPIC33A helpers" in the i2c NORA
  headers, "dsPIC33A DMA hot-path helpers" / "Only the dsPIC33A backend" in
  `nora_dma_dspic33ak_fast.h`, `nora_spi_i2s_tdm_dspic33ak_diag_fast.h`'s
  "dsPIC33A-private" opening, and a garbled leftover comment fragment in
  `nora_i2c_dspic33ak_reg.h`. Added by the can wave: `nora_canfd_node.h`'s opening
  comment still says "Phase 1: ... Interrupt-driven operation is added in a later
  phase", which is **staler than the text it replaced** in the standalone repository
  — the only place in this migration where the refresh moved a comment backwards.
  Recorded in that repository's `docs/nora_migration.md` and left unpatched so the
  snapshot stays byte-identical.
  Added by the ccp-input-capture wave, and both live in Sonora's
  `src/hal_ccp_input_capture/` (or its `tests/`):
  - `tests/hal_ccp_input_capture/fake_xc/xc.h` defines `_IFSx_CCPnIF_MASK` but not
    the bare `_CCPnIF` / `_CCPnIE` bit-alias lvalues the backend and
    `..._dspic33ak_fast.h` now write, so upstream's own host suite stops at
    `test_validation` with `C2065`. This one is a **real gap in the test
    scaffolding**, not a cosmetic leftover: it means the validation, callback, overflow
    and timebase stages have not run since the DFP-bit-alias change. Fixing the
    fake header is the highest-value item on this list.
  - SPDX regression: `SPDX-FileCopyrightText: 2026 SulaoLab` was dropped from the
    files that carried it, and `nora_ccp_input_capture_dspic33ak_fast.h` carries no
    SPDX line at all.

## 14. The naming sweep the verification methods could not see (2026-08-09)

A documentation review after step 6 found a class of error that **neither** of this
migration's two verification methods detects. Both are structurally blind to it, which is
the part worth keeping:

* **§9's header-symbol comparison** compares exported *identifiers*. Prose — comments,
  folder READMEs, `#error` strings — is not an identifier, so none of it is in scope.
* **Task B step 3's reverse-normalisation** rewrites `nora_*` back to `dspic33ak_*` and
  diffs against the pre-rename blob. A naming error in prose reverse-normalises to the
  *correct* pre-rename spelling, so the diff is empty. `nora_<mod>_hw.{c,h}` becomes
  `dspic33ak_<mod>_hw.{c,h}` — which is what the file really was called — while the file
  today is `nora_<mod>_dspic33ak_hw.{c,h}`. Both sides of that diff are naming, so
  **naming is exactly what it cannot check**. The same cancellation hides `Nora` vs
  `NORA` and `dsPIC33A` vs `dsPIC33AK`.

A third shape needs neither method to be wrong, only incomplete: a Files table that
**omits** a file the refresh added produces no diff line at all (ccp-input-capture's
`nora_ccp_input_capture_dspic33ak_fast.h`).

What actually detects them, and is now the check to run after any namespace sweep:

1. Resolve every `nora_*.{c,h}` **mentioned in prose** against the real contents of the
   folder; a mention with no matching file is a dead reference.
2. Compare each folder README against the module's root README — divergence means one of
   them is a generation behind. `hal_spi_i2s_tdm/README.md` still told a reader to extend
   a per-device `IEC`/`IFS` mask table that the DFP-bit-alias change had deleted.
3. Grep `dsPIC33A(?![K/])` and `Nora`, then read every hit. Most hits are correct:
   `dsPIC33A` is right whenever the sentence is about the **core** — unified/linear
   address space, no PSV/table-read, no NVMKEY (the PAC replaces it), the 128-bit Flash
   word. It is wrong only when the sentence is about **this backend**. The `dsPIC33A/h/`
   DFP include paths are literal directory names and never change.

Applied here, that gave 37 `dsPIC33A` and 14 `Nora` hits across 27 files under `src/`.
**Nine were left alone as correct**: all of `hal_nvm` (5) and `hal_udid` (3) are
core-architecture statements, plus `nora_gpio_dspic33ak_reg.h`, which already says "the
dsPIC33AK (dsPIC33A core) GPIO SFRs" and states the distinction better than the rule
does. `nora_pps_dspic33ak.c`'s "dsPIC33A guards RPCON via the PAC" and
`nora_spi_dspic33ak.c`'s "the Microchip dsPIC33A headers" are also correct as written.

`hal_nvm` and `hal_udid` are the two NORA-ised modules with **no standalone repository**,
so the standalone-side review never looked at them. They were checked here for the first
time — and needed nothing. That is the argument for running the greps in this repository
too rather than assuming the snapshot review covered the fleet.

### Direction of this wave, and what it costs

Every other fix in this migration flowed Sonora → starter → standalone. This one ran
**backwards** by explicit decision: standalone first (ten `refactor/nora-hal` branches
pushed 2026-08-09), then reflected here, with Sonora last. Two consequences to hold on to
until Sonora is patched:

* The 21 files changed here were **copied from the standalone tree**, not re-edited, after
  verifying each starter file still matched the standalone *pre-fix* blob. So
  `starter == standalone` is byte-exact by construction, not by re-derivation.
* `Sonora == starter` is therefore **broken on purpose** for those 21 files, and the
  snapshot READMEs carry an explicit "one exception" note saying so. When Sonora is
  patched and re-vendored, that note and each `docs/nora_migration.md`
  "ahead of upstream" section become history and need rewriting to "resolved".

Since ccp-input-capture has no `src/hal_ccp_input_capture/` here, its two corrected files
have no starter counterpart and go straight from the snapshot to Sonora.


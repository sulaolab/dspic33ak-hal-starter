# Timer HAL — NORA public / AK backend boundary review (pilot)

Date: 2026-08-09. Scope: `src/hal_timer/` on `refactor/nora-hal`.
Phase 1 (investigation) and Phase 2 (judgement) only — **no source file was changed by
this review.** Phase 3 is a single go/no-go question, recorded in §8.

The question this review answers is *not* "how many `DSPIC33AK_*` are left". It is:
**can we state where the NORA consumer-visible contract ends and the AK backend
implementation begins, and does the Timer HAL sit on that line?**

---

## 1. Method

The Timer HAL was chosen as pilot because it is the smallest module and — unlike
`hal_dma`, `hal_clock`, `hal_gpio`, `hal_i2c`, `hal_can`, `hal_spi`, `hal_uart` — it has
**no `*_reg.h` register layer at all**. Five files, three trees:

```
src/hal_timer/README.md
src/hal_timer/nora_tick_timer.h                 <- public
src/hal_timer/nora_tick_timer_dspic33ak.c       <- backend
src/hal_timer/nora_high_res_timer.h             <- public
src/hal_timer/nora_high_res_timer_dspic33ak.c   <- backend
```

Measured, not assumed:

1. every `DSPIC33AK_*` / `dspic33ak_*` occurrence in the module, with its scope;
2. every reference to those names from **outside** the module (12 consumers in the
   starter tree);
3. whether any public header reaches a timer header transitively through `#include`;
4. the AK public API surface vs the CK public API surface, as a diff;
5. blob identity of all five files across sonora / starter / standalone.

---

## 2. Deliverable 1 — consumer-visible processor-specific identifiers and concepts

**Identifiers: zero.**

- `nora_tick_timer.h` (63 lines): no `DSPIC33AK_*`, no `dspic33ak_*`, no SFR name, no
  register bit, no chip-specific type.
- `nora_high_res_timer.h` (70 lines): same — zero.
- 12 consumers in the starter tree: **zero** references to any `DSPIC33AK_*TIMER*` name.
- No public header `#include`s a timer header, so there is no transitive propagation.
  `nora_spi_i2s_tdm.h` is the only public header that mentions the high-res timer at all,
  and only in prose and in a unit (`window_period_ticks`, documented as "raw
  high-res-timer counts") — a NORA-level unit, not a silicon name.

**Concepts: three.** These are what a consumer actually learns about the silicon:

| # | Concept | Where it is visible | Consumer must know it? |
|---|---|---|---|
| C1 | The tick timer is backed by **Timer1**; the high-res timer is backed by **Timer2** | prose in both public headers (`Timer1` ×6, `Timer2` ×7) and the two `README.md` section titles | **Yes** for tick, see §4 |
| C2 | The interrupt vector is named **`_T1Interrupt`** | `README.md` example; real instance at `src/main.c:138` | **Yes** — the HAL deliberately does not own vectors |
| C3 | "At 100 MHz, one cycle is about 42.95 seconds" | `nora_high_res_timer.h:47` | No — illustrative, and stated conditionally |

---

## 3. Deliverable 2 — classification

| Item | Bucket | Reason |
|---|---|---|
| `DSPIC33AK_TICK_TIMER_PRESENT`, `DSPIC33AK_HIGH_RES_TIMER_PRESENT` | **A KEEP** | Result of a DFP presence test (`defined(T1CON) && defined(TMR1) && …`). This *is* an AK silicon fact, and the `DSPIC33AK_` prefix makes that explicit. Translation-unit-local `#define` in the backend `.c`. Never reaches a caller — see §4 for why this is the load-bearing design decision of the whole module. |
| `DSPIC33AK_TICK_TIMER_MAX_PRIORITY` (`7u`) | **A KEEP** | The AK CPU interrupt-priority model's upper bound. Hardware limit, TU-local, used once at `nora_tick_timer_dspic33ak.c:152` to range-check `config->irq_priority`. |
| `DSPIC33AK_TICK_TIMER_HZ` (`1000u`) | **A KEEP, with a note** | Honestly: `1000 Hz` is *not* a silicon fact — it is the HAL's logical 1 ms tick contract, so the `DSPIC33AK_` prefix is arguably misleading. But it is TU-local in the backend `.c`, so **no consumer is misled by it**, and it is explicitly out of scope by the review's own rule. Recorded here rather than renamed. |
| C1 — `Timer1`/`Timer2` named in public-header prose | **B/C boundary — the one real finding** | See §4. |
| C2 — `_T1Interrupt` | **A KEEP** | Consequence of a deliberate design decision (HAL owns no vectors). The *shape* of the contract — "you write the vector, you call `nora_tick_timer_irq_handler()`" — is portable; only the vector's spelling is silicon, and that spelling is the compiler/DFP's, not ours. `README.md` states the ownership split. |
| C3 — the 42.95 s example | **A KEEP** | Conditional ("At 100 MHz…"), illustrative, not a promise. |
| CK's `nora_tick_timer_clock_source_t` / `NORA_TICK_TIMER_CLOCK_{INTERNAL,FRC}` / `NORA_TICK_TIMER_ERR_INEXACT_PERIOD` | **C capability — NORA-wide contract question, not an AK defect** | CK added a field and an enum value to types that AK also defines. Same type name, different content. AK hardcodes `T1CONbits.TCS = 0` (`:66`) and has no such concept. Not something to "fix" in AK — see §7. |
| Historical residue (**D**) | **none found** | No pre-NORA callable, type, or macro survives anywhere in the module, public or backend. |

---

## 4. Deliverable 3 — the one real finding, and why C1 is not simply "residue"

Both public headers state the backing hardware block in prose **without marking it as
implementation-defined**.

For the high-res timer this is pure over-specification: the public API is already fully
abstract, and we have proof — the **AK and CK high-res public API surfaces are byte-level
identical in shape** despite AK using Timer2 and CK using SCCP1 in 32-bit mode. The
abstraction works; only the prose disagrees with it.

For the tick timer the prose is **load-bearing**, and this is why the finding is a
boundary question rather than a cleanup:

- the consumer must write `_T1Interrupt` — it cannot do that without knowing it is Timer1;
- the consumer must not reconfigure Timer1 elsewhere (ownership);
- `config->timer_clk_hz` means *"the clock that actually reaches Timer1"*, so the
  consumer's clock-tree setup has to agree with it.

So C1 must not be deleted. The question is only whether it is **labelled** as a backend
fact.

**Evidence that leaving it unlabelled is a live hazard, not a theoretical one:** CK's own
public header already contradicts itself. `dspic33ck-hal-lab/src/hal_timer/nora_high_res_timer.h`
says at `:9` "Built on SCCP1 in 32-bit timer mode" and at `:13-15` "NOT Timer2/3 — dsPIC33CK
replaced Timer2-Timer5 with SCCP/MCCP, so neither CK256MP508 nor CK64MC105 has T2CON" — and
then at `:42-43` still says "Configure and start Timer2/3 (paired 32-bit) as a free-running
counter. On success this HAL owns Timer2 and Timer3 until deinit()". A silicon fact carried
in a portable header's prose survived into a port where it is false. (Per the brief, CK is
comparison material only; nothing in the CK tree was touched.)

## 5. Deliverable 3 (cont.) — representative `DSPIC33AK_*` judged **not** a problem

All four macros in §3 are in this category. The clearest example is presence detection:

```c
/* nora_tick_timer_dspic33ak.c:18-23 */
#if defined(T1CON) && defined(TMR1) && defined(PR1) && \
    defined(_T1IF) && defined(_T1IE) && defined(_T1IP)
#define DSPIC33AK_TICK_TIMER_PRESENT         1
#else
#define DSPIC33AK_TICK_TIMER_PRESENT         0
#endif
```

This is exactly the shape the review's brief blesses: the name being `DSPIC33AK_*` is what
makes it obvious that this is an AK hardware fact. It is TU-local, and the capability it
computes is published as a **function**, `nora_tick_timer_is_present()` — not as a macro.
That single choice is why a consumer can be compiled against AK or CK without a single
`#ifdef`, and it is the most reusable result of this pilot.

---

## 6. Deliverable 4 — starter ⇄ standalone relationship and diff status

Confirmed **from the repositories' own records**, not inferred:
`docs/nora_migration.md` in the standalone snapshot states the direction explicitly
("Direction matters: it used to run the other way", "A fix made here and not upstream would
be a fork"). The chain is:

```
sonora audio-board project   (the tree that runs on hardware — source of truth)
        v
sulaolab/dspic33ak-hal-starter   (MPLAB X project, 11 HAL modules)
        v
nora-hal-dspic33ak-timer         (published snapshot, read-only role)
```

**Diff status: none.** All five module files are byte-identical across the three trees.
Measured with `git rev-parse HEAD:<path>`, sonora and starter at `src/hal_timer/<file>`,
snapshot with the module flattened into `src/` (and the module README at `src/README.md`):

| File | blob |
|---|---|
| `README.md` (snapshot: `src/README.md`) | `d5cf2a24` |
| `nora_tick_timer.h` | `5188de7a` |
| `nora_tick_timer_dspic33ak.c` | `963f739e` |
| `nora_high_res_timer.h` | `10f865f4` |
| `nora_high_res_timer_dspic33ak.c` | `07057be8` |

The same five blobs are present at starter `4af7049` (before this review's doc commit),
which is the mechanical confirmation that the review changed no HAL content.

The snapshot additionally carries a **top-level** `README.md` (`13571b44`, snapshot
packaging — not the module README above), plus `docs/`, `LICENSE`, `.gitattributes`,
`.gitignore`.

Consequence for Phase 3: **any change must be made sonora → starter → standalone**, in that
order, or the byte-identity invariant breaks and the snapshot becomes a fork.

---

## 7. Deliverable 6 — Timer-specific vs NORA-wide

**NORA-wide principles (earned here, applicable beyond Timer):**

- **P1 — publish capability as a function, not a macro.** `nora_*_is_present()` is what let
  AK/Timer2 and CK/SCCP1 share an identical public surface. A `#define
  NORA_X_PRESENT` in a public header would have forced `#ifdef` into every consumer.
- **P2 — compute presence from DFP `defined()` tests inside the backend.** The test is the
  chip's, so it belongs in the chip's translation unit, named for the chip.
- **P3 — if a public header names the backing hardware block, mark it
  implementation-defined.** Not deleting it — *labelling* it. CK's self-contradicting
  header is the evidence that unlabelled silicon prose does not survive a second
  implementation.
- **P4 — an implementation must not silently widen a shared config type.** CK added
  `clock_source` to `nora_tick_timer_config_t` and `NORA_TICK_TIMER_ERR_INEXACT_PERIOD` to
  the shared status enum. Same type name, different contract, so CK→AK is not
  source-compatible in that direction. Who may extend a shared type, and how, is a NORA
  contract rule that does not exist yet. **Out of scope here** (this is not a CK fix task,
  and not an AK defect).

**Timer-specific:**

- Vector-name exposure applies to the tick timer only; the high-res timer needs no
  `_T2Interrupt`. Other HALs will have different vector-ownership stories.
- Timer had no `*_reg.h`, which is why it was a clean pilot. It says nothing yet about
  modules that do.

---

## 8. Deliverable 5 — Phase 3

**Nothing has been changed.** The AK Timer consumer-visible boundary is clean:
zero silicon identifiers in the public headers, zero silicon references from 12 consumers,
capability abstracted behind functions, and all four `DSPIC33AK_*` macros correctly closed
inside backend translation units. **"Timer は現在のままでよい" is a defensible outcome.**

One option was considered and **declined by the owner on 2026-08-09**: adding a short note
to the two AK public headers marking the backing hardware block as backend-specific
(doc-only, no API or identifier change). The boundary is already clean, so the note would
document a property the code already has. **P3 therefore stands as a NORA-wide principle in
§7 and not as a Timer change.** The three trees keep their blob identity (§6), and
`src/hal_timer/` is unmodified by this review.

The CK header contradiction in §4 is CK's to resolve; it is cited here only as evidence for
P3, and nothing in the CK tree was touched.

---

## 9. Deliverable 7 — what to check next in ADC / Clock (no changes now)

1. **`*_reg.h` modules** (`dma`, `clock`, `gpio`, `i2c`, `can`, `spi`, `uart`): repeat only
   steps 1–3 of §1 — does any register-layer macro reach a public header? Timer cannot
   answer this because it has no register layer.
2. **Clock** is the hardest case and should be read before any rule is proposed: PLL /
   oscillator / `CLKGEN` / `REFI` *are* the silicon structure, so "what belongs in the
   public contract" is a genuine design question, not a naming one.
3. **ADC**: check whether P1 holds — channel count, resolution, and trigger sources differ
   between AK and CK. If those are macros in a public header, that is the same hazard C1
   describes, in a form that breaks compilation rather than documentation.
4. **P4 needs an owner.** The shared-type-widening question surfaced from CK's tick timer
   and will recur in every module CK ports. It is a NORA contract decision, not a per-module
   cleanup.
5. Explicitly **not** here: DMA processor-neutralization and the UART async port. Separate
   tracks, per the brief.

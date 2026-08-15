# NORA HAL sync gap — starter `refactor/nora-hal` vs sonora `main`

Gap measured 2026-08-10. Revised 2026-08-11 after the sonora-internal merge described in
§0, which discharged part of what the first revision listed and corrected two claims in it.

| tree | ref | head |
|---|---|---|
| `dspic33ak-hal-starter` | `refactor/nora-hal`, when the gap was measured | `ac42fb3` (last source commit `4af7049`, 2026-08-09 23:35) |
| `dspic33ak-hal-starter` | `refactor/nora-hal`, now | `ee31e1e` — §1 fully discharged, see §4 |
| `dspic33ak-audio-dsp-sonora-dev` | `main`, before the merge | `17a57b3` |
| `dspic33ak-audio-dsp-sonora-dev` | `fix/nora-naming-convergence` | `e64b3fe` |
| `dspic33ak-audio-dsp-sonora-dev` | `main`, after the merge | `9f9d380` |

**The starter re-sync targets `refactor/nora-hal`, never the starter's `main`.** Where §4
step 1 says to take `origin/main`, that is `origin/main` → the branch, as the source of one
commit (`7773395`); `cherry-pick` is an equally good way to get it.

Comparison is blob-level over `src/hal_*` (git object ids, not text diff).

---

## 0. Sonora had two NORA HAL lineages, and they had never met

`e64b3fe` is **not an ancestor of `main`** — `merge-base` is `3843976`, and the only branch
containing it is `fix/nora-naming-convergence`. That is the actual shape of the
"three trees drifting apart" problem, and it is why reviewing any one tree kept reporting
the other two as behind:

| lineage | had | lacked |
|---|---|---|
| `main` | the 2026-08-10 contract wave — clock r4, `NORA_DMA_TRIGGER_NONE`, `nora_tdm_slot_t`, `nora_i2c_status_str()`, UART RX recovery counters, `nora_pps_find_output_rp()` | the naming wave, `.namegate-ignore`, the ccp/noinit host-test fixes |
| `fix/nora-naming-convergence` | the naming wave (7 commits) | the entire contract wave |
| starter `refactor/nora-hal` | — | the contract wave |

The measurement that decided the plan: **starter `refactor/nora-hal` and `e64b3fe` differ in
exactly one file out of 71 shared `src/hal_*` paths** — `hal_spi_i2s_tdm/README.md`, the
divergence already declared in `nora_hal_migration_analysis.md` §17/§18. So the starter held
no source change of its own, and one sonora-internal merge was enough to make `main` a
superset of both trees. Nothing had to be ported from the starter.

Merged as `9f9d380` (parents `17a57b3` + `e64b3fe`). Five conflicts, none of them code:

| file | resolution | why |
|---|---|---|
| `hal_clock/nora_clock_dspic33ak.c` | **main** | main's 20-line design comment for `nora_clock_source_hz()`; conv side empty there |
| `hal_clock/nora_clock_dspic33ak.h` | **main** | main's summary line is the superset (`+ the register capture, and the backend's diagnostic codes`) |
| `hal_dma/nora_dma.h` | **main** | main's `NORA_DMA_TRIGGER_NONE` paragraph; conv side empty there |
| `resident_de/app/resident_de_app_console.c` | **conv** | continuation-line indent only |
| `resident_de/boot/resident_de_bootloader.c` (×3) | **conv** | same — both branches did the nvm snake_case rename, only conv re-aligned the arguments after `NORA_NVM_program` shortened to `nora_nvm_program` |

`git checkout --ours` / `--theirs` was **not usable**, and `nora_dma.h` is the counter-example:
line 58 already carried conv's `NORA port` fix from a clean auto-merge, so `--ours` would have
reverted six naming fixes while resolving one comment conflict. Markers were removed by hand.

The gate is the evidence that taking main's side lost no naming fix:

```
ai-namegate.ps1 -Repo dsp-sonora-mothership   →  FAIL, 30 used hits
ai-namegate.ps1 -Repo <merged worktree>       →  PASS,  0 used hits
```

29 closed by the merge itself: `src/touch/*` (Microchip's own `qtm_acq_dspic33a_*` spelling,
now excluded via `.namegate-ignore`), `Nora` → `NORA` in five module READMEs, and one
`dspic33ak_spi_*` reference in `SST26_drv.c`. The 30th needed a **new** fix:
`docs_internal/shared/nora_dma_contract_declaration_2026-08-10.md:284` named `Nora` and
`dsPIC33A` in *double* quotes, which the gate reads as *used* rather than *referred to*.
That file was written on 08-10, after the naming wave had already passed through, so no
earlier sweep could have caught it.

## 1. Starter is behind main — 18 shared files at the time of measurement, 0 now

Unchanged by the merge (the merge touched none of these on the sonora side). Line counts
are `only_sonora / only_starter` = diff lines present on one side only, measured against
pre-merge `main`.

**Every row in this table has landed** — see §4 *Done* for the commits. The table is kept
because the reasoning is the record of what was ported and why. Measured after the last
one, the two trees differ on **no** shared `src/hal_*` path except
`hal_spi_i2s_tdm/README.md`, which is the declared divergence at the bottom of the table
and not a gap. What remains is §2, and it is a scope decision rather than a sync.

| # | file | −/+ | sonora commit(s) | what starter is missing |
|---|---|---|---|---|
| 1 | `hal_clock/nora_clock.h` | 484 / 71 | `de6c3d3`, `a7de05b`, `76eb351` | portable clock contract **r4** — `nora_clock_get_state()`, backend-named `nora_clock_<family>_raw_capture()`, source-enum split into "every family has it" vs family-fanout range, CK-safe wording |
| 2 | `hal_clock/nora_clock_dspic33ak.c` | 835 / 88 | `de6c3d3`, `a7de05b`, `4d82462`, `bcd61b4` | r4 backend implementation + resident-bootloader clock bypass returned to the HAL + bring-up console blocks removed |
| 3 | `hal_clock/nora_clock_dspic33ak.h` | 135 / 8 | same | backend-facing r4 declarations |
| 4 | `hal_clock/nora_clock_dspic33ak_reg.c` | 272 / 18 | same | register layer: switch/ready/timeout sequencing rework |
| 5 | `hal_clock/nora_clock_dspic33ak_reg.h` | 78 / 1 | same | ditto |
| 6 | `hal_clock/nora_clock_device_dspic33ak.c` | 88 / 3 | same | device-fact table now needed **in both directions** (encode a request, decode an observation) |
| 7 | `hal_dma/nora_dma.h` | 52 / 2 | `93a94ff`, `20bb2ec`, `a84b59c` | **`NORA_DMA_TRIGGER_NONE`** (software-only channel), per-family trigger-enum note, ping-pong vs single-shot predicate rules (`has_completed_half()` ≠ `has_completed()`) |
| 8 | `hal_dma/nora_dma_dspic33ak.c` | 53 / 0 | same | TRIGGER_NONE handling + contract declarations |
| 9 | `hal_dma/nora_dma_dspic33ak_fast.h` | 8 / 0 | same | |
| 10 | `hal_dma/nora_dma_dspic33ak_reg.h` | 4 / 0 | same | |
| 11 | `hal_gpio/nora_pps.h` | 9 / 0 | `6a16f1d` | `nora_pps_find_output_rp()` declaration |
| 12 | `hal_gpio/nora_pps_dspic33ak.c` | 44 / 0 | `6a16f1d` | its implementation (physical RP only, RPV not searched, read-only w.r.t. IOLOCK) |
| 13 | `hal_i2c/nora_i2c.h` | 6 / 0 | `e70a785` | `nora_i2c_status_str()` declaration |
| 14 | `hal_i2c/nora_i2c_dspic33ak_common.c` | 21 / 0 | `e70a785` | its table; never NULL, unknown → `"?"` |
| 15 | `hal_spi_i2s_tdm/nora_spi_i2s_tdm.h` | 120 / 14 | `93a94ff`, `20bb2ec` | **`nora_tdm_slot_t`** — one name, per-family representation (AK `int32_t`, CK `struct {uint16_t wire[2];}`) plus the three portability rules |
| 16 | `hal_spi_i2s_tdm/nora_spi_i2s_tdm_dspic33ak.c` | 4 / 4 | same | `tx_fill_ptr()` / block-callback signatures retyped `int32_t*` → `nora_tdm_slot_t*` |
| 17 | `hal_uart/nora_uart.h` | 9 / 0 | `30d5aae` | three RX-recovery counters (`rx_stall_recovery_count`, `rx_ie_lost_count`, `rx_overrun_recovered_count`); AK reports 0, CK populates them |
| — | `hal_spi_i2s_tdm/README.md` | 72 / 23 | — | **not a gap.** The declared divergence: sonora keeps the upstream version (265 lines), the starter and the published snapshots carry the de-codenamed one (213 lines). Verified intact after the merge. |

### `nora_pps_find_output_rp` is a special case

It is already on starter **`origin/main`** as `7773395 fix(hal_gpio): add PPS output RP lookup`,
which `refactor/nora-hal` does not contain (the branch is 1 behind `origin/main`).
So items 11–12 landed for free by merging `origin/main` into the branch (`6ec819d`).

## 2. Modules sonora has and starter does not (18 files)

Not "behind" in the same sense — these were never ported. Each needs an explicit
in-scope / out-of-scope decision before the fleet-main landing, because the starter's
README HAL inventory is what a consumer reads.

| module | files | note |
|---|---|---|
| `hal_ccp_input_capture` | 7 (`LICENSE`, `README.md`, `nora_ccp_input_capture{,_dspic33ak,_dspic33ak_fast,_dspic33ak_reg}.h/.c`, `..._conf_dspic33ak.h_example`) | **already published** as `nora-hal-dspic33ak-ccp-input-capture` — published from sonora, not via the starter. The only missing module with a public snapshot, so the starter gap is visible from outside. |
| `hal_adc` | 4 (+ `ADC_HAL_RESTART_NOTES.md`) | ADC HAL is on hold per its own notes |
| `hal_reset` | 2 | `nora_reset.h/.c` |
| `hal_noinit_ram` | 2 | `nora_noinit_ram.h/.c` |
| `hal_clock/nora_clock_device_dspic33ak.h` | 1 | see below — a **rename**, not a new file |
| `hal_dma/README.md`, `hal_gpio/README.md` | 2 | starter has no README for these two modules |

Starter-only and correct as-is: `hal_dma/UPSTREAM.md`, `hal_spi_i2s_tdm/UPSTREAM.md`
(packaging metadata, deliberately unpublished and deliberately absent from sonora).

### The clock device header is a rename, not an addition

sonora `hal_clock/nora_clock_device_dspic33ak.h` ⟷ starter `hal_clock/nora_clock_device.h`.
Sonora's own header states the reason: *in this tree an untagged name reads as portable, and
this content is not.* Sonora also renamed the guard (`NORA_CLOCK_DEVICE_DSPIC33AK_H`) and
fixed `PLL1` → `PLL_1` in the prose. Adopt the sonora name — do not treat the starter file
as a separate module.

## 3. What the first revision of this document got wrong

Both corrections are recorded rather than deleted, because both are recurring shapes.

### "sonora is behind on three READMEs" — the direction was right, the mechanism was not

The first revision listed `hal_timer/README.md`, `hal_uart/README.md` and
`hal_i2c/README.md` as *sonora being behind*, on the strength of reading the content and
judging the starter's wording correct. That conclusion held, but it was a judgement
presented as a measurement. Measured properly, the three items are not the same kind of
thing:

* **`hal_uart` / `hal_i2c` — ancestry, measurable.** The starter's fix commit `d210af8`
  carries the proof in its own diff header: `index 990e7cc..3a2ebc9` and
  `index 0d0a690..820e330`. The left-hand (pre-fix) blobs `990e7cc` and `0d0a690` were
  **exactly the blobs pre-merge sonora `main` was sitting on**. So `main` held the
  starter's pre-fix state.
* **`hal_timer` — independent divergence.** sonora `a4b72a4` matches neither the starter's
  pre-fix blob (`847eb25`) nor its post-fix one (`d5cf2a2`). The stale
  `dspic33ak_tick_timer_*()` was real, but there is no ancestry line to appeal to.
* **The mechanism in all three cases** was not that sonora lacked the fix. `e64b3fe` had
  all three, byte-identical to the starter. `e64b3fe` had simply never been merged into
  `main`. All three converged in `9f9d380`, which is why the shared-path difference count
  fell from 21 to 18.

The generalisation worth keeping: *"tree X is behind tree Y"* is not a measurement until
the branch containing the fix has been checked for reachability from the ref being judged.

### "sonora's HAL has not changed since the §18 seal" — a silently empty pathspec

That claim came from

```
git diff --name-only e64b3fe origin/main -- src/hal_      → 0 files
```

`src/hal_` is not a directory, so the pathspec **matches nothing** and the empty result
reads as "no changes". With `'src/hal_*'` the same command returns **32 files**. The
module-scoped commands in the same session (`-- src/hal_clock`, `-- src/hal_dma`) named real
directories and were correct throughout, which is why the per-module history in §1 was
unaffected — the only casualty was that one sentence.

This is the third member of a family already recorded in `nora_hal_migration_analysis.md`
§14: a check whose *shape* is narrower than what it claims to cover, returning a strict
subset and reading as clean. `ai-namegate.ps1` was built for exactly this failure mode.

## 4. Remaining work — §1 is closed, §2 is not

Everything below happens on **`refactor/nora-hal`**. The starter's `main` is not a
destination in this work at all.

### Done

* **2026-08-11, sonora:** `main` is a superset of both other trees for every `src/hal_*`
  source file, `.namegate-ignore` included, namegate PASS, `.text` byte-identical (§0).
* **2026-08-10, starter:** `hal_i2c` (`fae5b17`) and `hal_uart` (`96229bc`) — §1 rows 13, 14,
  17. Both verified blob-identical to sonora `main`, so nothing to redo.
* **2026-08-11, starter — the remaining three groups, in this order:**

  | # | commit | what | §1 rows |
  |---|---|---|---|
  | 1 | `6ec819d` | `git merge origin/main` — `nora_pps_find_output_rp()` for free | 11–12 |
  | 2 | `c9d2fcd` | `hal_dma` + `hal_spi_i2s_tdm`, one commit | 7–10, 15, 16 |
  | 3 | `ee31e1e` | `hal_clock` r4, incl. the device-header rename | 1–6 |

  Group 1 was taken as a merge rather than a cherry-pick: the branch was 3 ahead / 1
  behind, the one commit touched exactly the two pps files, and a merge keeps the branch's
  eventual return to `main` free of a duplicated commit.

  Each group's files were verified **blob-identical** to sonora `9f9d380` before the commit
  (`git rev-parse <sonora>:<path>` vs `git rev-parse :<path>`, not a text diff), each was
  built with `buildtools/build.ps1 -Full` (0 warnings, provision PASS, dual-partition
  artifacts PASS), and namegate was re-run after each. `hal_spi_i2s_tdm/README.md` was not
  synced, and is now the only shared `src/hal_*` path where the two trees differ.

  Two things this plan did not predict, both worth carrying forward as shapes:

  * **A "prose" rename was an identifier rename.** §2 recorded `PLL1` → `PLL_1` as a fix
    *in the prose* of the device header. It is also the enumerator: r4 spells them
    `NORA_CLOCK_SOURCE_PLL_1` / `_PLL_2`. `src/clock/starter_clock.c` used the old spelling
    in two places, so group 3 carried the one consumer source change of the whole re-sync.
    Reading a rename wave's summary is not the same as grepping the tree for the old names.
  * **The MPLAB project file is part of a rename.** `firmware.X/nbproject/configurations.xml`
    lists headers by path, so `nora_clock_device.h` → `nora_clock_device_dspic33ak.h` had to
    be re-registered there. The build does not fail without it — the header is found by
    include path — so nothing would have caught this but looking.

  Cost of the r4 surface: program region 88,936 → 92,192 bytes (+3,256).

### Left

1. **Hardware check for `hal_clock`.** Group 3 changes the clock bring-up path
   (`nora_clock_switch_source()` no longer normalizes the AK system divider; CLKGEN1 is
   re-sourced through the system-clock switch sequence). Built and blob-verified only; not
   yet run on a board.
2. **Decide §2** — port or explicitly declare out-of-scope, per module.
   `hal_ccp_input_capture` first, since it already has a public snapshot and so the starter's
   omission is visible from outside. 17 files across 6 items; `hal_adc` is on hold under its
   own notes, and `hal_dma/README.md` + `hal_gpio/README.md` are the cheap two.

Now that groups 1–3 have landed, the §18 blob-identity seal can be re-run. Its sonora
coordinate (`e64b3fe`) is a *merged* commit rather than a branch tip, so use `9f9d380`;
see the note added to §18.

## 5. §2 closed — 2026-08-12

The "cheap two" of §4 and the rest of §2 are done. The starter was synced to sonora
`main` `ab5f355` in one commit (`8abf10c`, 13 files, every one blob-verified identical
to sonora):

- `hal_gpio/`: `nora_gpio.h`, `nora_pps.h`, `nora_pps_dspic33ak.c` refreshed;
  `nora_gpio_table.h`, `nora_gpio_table_dspic33ak.c`, `README.md` added.
- `hal_i2c/`: all 5 sources + `README.md` refreshed.
- `hal_dma/README.md` added.
- `firmware.X/nbproject/configurations.xml`: `nora_gpio_table_dspic33ak.c` registered —
  the MPLAB project file is part of the port, exactly as §2 warned it is part of a
  rename.

`nora_gpio_table.{h,c}` were taken in rather than treated as integration-only: the
module gives its own reason (API parity with the dsPIC33CK NORA HAL), and a public HAL
implements what other family members implement even with no caller here.

`hal_spi_i2s_tdm/README.md` stays divergent on purpose — the difference is §10 plus the
de-codenaming, with no new content since the seal. `src/hal_*/UPSTREAM.md` remains
starter-local and unpublished.

Verified: `buildtools/build.ps1 -Full` on this tree — 0 errors, links,
94,172 B program (35%) / 11,194 B data (17%), with `nora_gpio_table_dspic33ak.o` in the
build. The port was then re-published to the 7 snapshots and on into the 5 CMSIS-Driver
repos; the full record is §6 of the audio project's
`docs/cmsis_verification_prep_2026-08-12.md`.

**§4 "Left" now reads:** only the `hal_clock` hardware check remains. §2 is closed.

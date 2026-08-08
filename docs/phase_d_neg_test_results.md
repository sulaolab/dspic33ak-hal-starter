# Phase D — SPI/I2S/TDM HAL negative-validation self-test: verification results

Contract-regression self-test (`src/app/tdm_neg_test.c`, opt-in
`HAL_STARTER_ENABLE_TDM_NEG_TEST`, shipping default **0**). This note records the
build verification and the two on-board runs that exercise the full misuse matrix.

Verified commit: `ada2adb` on `feat/tdm-system-topology-v2` (the GPT-review fixes on
top of the Phase D initial `58dc7f1`). Result: **both runs pass, fail=0.**

## 1. Build verification (host)

`buildtools/build.ps1 -Full` on `firmware.X` / `dsPIC33AK512`, both toggle states:

| Toggle | Result |
|--------|--------|
| `HAL_STARTER_ENABLE_TDM_NEG_TEST 1` (test) | exit 0, `firmware.X.production.hex` produced, `tdm_neg_test.o` + `main.o` compiled, 0 errors |
| `HAL_STARTER_ENABLE_TDM_NEG_TEST 0` (shipping) | exit 0, clean shipping form |

The 2-leg code paths are compiled (though runtime-gated) even in a single-leg build,
so type-checking is covered by the toggle-ON build.

## 2. Committed change (shipping form)

`ada2adb` touches exactly three files — `src/app/tdm_neg_test.c`, `src/main.c`,
`src/app/app_config.h` — and keeps `HAL_STARTER_ENABLE_TDM_NEG_TEST` at 0. It applies
the GPT review fixes:

- **P1** — `neg_system_mode(spi1)` is now called from `run()`, so the SYSTEM-family
  contract cases actually execute (the initial version implemented but never called it).
- **P3** — open-time `inst_configure` rejection is exercised across all instances
  (`setups[2]`, `count = instance_count()`).
- **P2 framing** — 2-leg framing mismatch is built from `SPIFE` / `CKP` / `CKE` /
  `fs_shape` (one field each → `TOPOLOGY`), since geometry (`slots`) cannot express it.
- **P2 transactional** — commit valid system A → snapshot via `inst_get_setup` → reject
  invalid system B (two masters) → assert A is preserved.
- **P2 cleanup** — return values are checked (`neg_expect`) on the trailing
  `inst_stop` / `close` / `stop_all_domains` and on `run()`'s final `set_port(NULL)`.
- **main.c** — fail-closed smoke skip: `tdm_neg_ok` gates `tdm_smoke_init()`; when the
  NEG toggle is off `tdm_neg_ok = true`, and it is `(void)`-cast when the smoke is off.

## 3. Hardware

- Target: dsPIC33AK512MPS512, programmed via on-board **PKOB4** serial
  `020085205AUR000183` (auto-detected by `buildtools/flashauto.ps1`).
- Console: **UART1 @ 230400 8N1** (primary) with a mirror on **UART2** → the PKOB4
  "USB Serial Device" (**COM34** on this host). `[NEG]` output appears on both.
- The self-test is board-independent (pure API-contract), needs no external signals.

## 4. Run A — single-leg (`NEG=1`, `NORA_TDM_USE_SPI2=0`, `SMOKE=1`)

`[NEG] ... self-test (legs=1)` → **`[NEG] pass=35 fail=0`**, `all ... PASSED`.

Coverage confirmed on device:

- bad-instance (`inst_start`/`inst_stop(NULL) → BAD_INSTANCE`), topology
  (`configure_system(sync_domain=32) → TOPOLOGY`);
- lifecycle: `NOT_OPEN`, `ALREADY_OPEN` (both `inst_configure` and `configure_system`),
  `ALREADY_RUNNING` (close refused while running, still running afterward),
  `CLOCK_NOT_READY` + recovery, readiness cleanup;
- SINGLE-mode family guard: `stop_domain(0)` / `stop_all_domains` in SINGLE →
  `CONFIG_MODE`;
- **SYSTEM-mode family (the P1 fix), all present:** `configure_system(valid) → SYSTEM ok`,
  `inst_configure in SYSTEM → CONFIG_MODE`, `stop_domain(0) SYSTEM stopped → true`,
  `stop_all_domains SYSTEM → true`, `stop_domain(31) no member → BAD_INSTANCE`,
  `stop_domain(255) ≥32 → BAD_INSTANCE`, `inst_start in SYSTEM → CONFIG_MODE`;
- 2-leg cases correctly **logged skipped** (`needs NORA_TDM_USE_SPI2=1`).

After the summary the smoke demo resumed —
`[TDM1] SPI1 TDM8 master smoke demo started` — proving the harness left the HAL
stopped + closed (clean).

## 5. Run B — 2-leg (`NEG=1`, `NORA_TDM_USE_SPI2=1`, `SMOKE=0`)

`[NEG] ... self-test (legs=2)` → **`[NEG] pass=53 fail=0`**, `all ... PASSED`.
(SMOKE must be 0 here: the smoke's fixed 1-leg system table would fail
`configure_system()`'s count check against a 2-leg build.)

The 35 single-leg cases plus the 2-leg matrix, all present on device:

- `configure_system(2 masters/domain) → TOPOLOGY` (case g);
- `framing mismatch SPIFE / CKP / CKE / fs_shape → TOPOLOGY` (P2 framing, four fields);
- **transactional (P2):** `configure_system A (valid) → ok`, `B (invalid) → TOPOLOGY`,
  `system A preserved after rejected B`;
- **non-destructive double-start:** `start_all_domains → ok`, `both legs running`,
  `start_all_domains AGAIN → idempotent true`, `start_domain(0) on running → idempotent
  true`, `both legs STILL running after repeat starts`;
- 2-leg cleanup: `stop_all_domains → ok`, `close → ok`, `all legs stopped`.

The `literal a/b` rollback non-destructiveness remains review-covered (a partial-running
state cannot be built through the public API); `run()` prints the note line, as designed.

## 6. Post-verification state

- Test toggles restored to shipping defaults in the working tree:
  `HAL_STARTER_ENABLE_TDM_NEG_TEST 0`, `HAL_STARTER_ENABLE_TDM_SMOKE_DEMO 1`,
  `NORA_TDM_USE_SPI2 0`. Working tree clean at `ada2adb`.
- Board firmware is left as the Run B build (restore not required; the next user flashes
  their own image).

## 7. Remaining (out of scope here)

Round 5 review / CMSIS Phase C publish prep / oscilloscope FS-waveform capture /
`main`-merge decision. `main` is unchanged on all three repositories.

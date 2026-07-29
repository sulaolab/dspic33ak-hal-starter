# PLL2 Forced-Stop / Restart and Software-Reset Campaign

## §1 Purpose and Question

The dsPIC33AK PLL bring-up path in the pinned Clock HAL
(`dspic33ak_clock_reg.c`, `configure_pll1()` / `configure_pll2()`) programs new
dividers and *then* switches `NOSC`, on a PLL it implicitly assumes is either at
its device-reset default or already running on the same reference. That
assumption is fine at cold boot. It is exactly what is in question for a PLL
left **hot** across a reset: locked to a reference, or worse, left mid-handshake
with a `PLLSWEN` / `FOUTSWEN` / `OSWEN` / `DIVSWEN` request bit still set.

The operational problem motivating this work: changing the PLL configuration is
known to sometimes fail to lock, recoverable only by a full power cycle. Before
attacking that on a live SYSCLK path, this experiment answers a narrower,
strictly measurable question:

> Can PLL2 be forced into a known OFF state and then reliably re-locked to the
> same target (FRC 8 MHz -> 520 MHz), regardless of the state it was in before,
> and does that hold across 10 / 100 / 1000 software resets?

Answer, measured on hardware: **yes, 1152/1152 restarts passed, zero failures.**
See §11 for the full result table and §14 for what this does and does not imply.

## §2 Scope

In scope:

| Item | Detail |
|---|---|
| PLL | PLL2 only |
| Source | FRC, 8 MHz, `NOSC = FRC` |
| Target | 520 MHz |
| Mechanism | Explicit forced stop, reprogram while stopped, staged re-enable |
| Validation | Console single/double restart, plus 10/100/1000-iteration software-reset campaigns |

Explicitly **out** of scope for this round (unchanged, untouched):

PLL1; CPU clock switching; REFI1 and any external reference; CLKGEN peripherals
sourced from PLL2; glitchless live switching; audio / SPI / DMA integration;
mainline merge; and the behavior of the existing
`dspic33ak_clock_pll_configure()` API, which is not modified by this branch.

"Measure First / One Lever Only" was followed: the only lever moved is PLL2's
own stop/restart sequence.

## §3 Why PLL1 Is Refused

`dspic33ak_clock_pll_restart()` returns `DSPIC33AK_CLOCK_ERR_NOT_SUPPORTED` for
`DSPIC33AK_CLOCK_PLL_1`. PLL1 drives the live SYSCLK on this Starter (FRC ->
PLL1 200 MHz -> FCY 100 MHz), so force-stopping it would stop the CPU executing
the stop sequence. Refusing it explicitly is deliberate: the alternative --
silently "handling" PLL1 -- would put a dangerous path behind a generic-looking
API.

## §4 Environment

| Item | Value |
|---|---|
| Repository | `sulaolab/dspic33ak-hal-starter` |
| Branch | `exp/pll2-soft-reset-restart` (branched from `main` @ `7d12e42`) |
| Working folder | `C:\00_storage\_Git_Work\vscode-home\dspic33ak-hal-starter` |
| MPLAB configuration | `dsPIC33AK512` |
| `APP_BUILD` | `APP_BUILD_PLL2_RESTART_TEST` (10) |
| Target device | `dsPIC33AK512MPS512` |
| Device ID | `0xa77c` |
| Device revision | `0x1` |
| Boot UDID | `FFFFFFFF010B00DBB8D0000D00D76A9D` |
| PKOB4 serial | `020085204RYN000057` |
| MPLAB X | v6.30 |
| DFP | `dsPIC33AK-MP_DFP` 1.3.185 |
| Compiler | XC-DSC 3.31.01 |
| Firmware build stamp | `Jul 29 2026 19:19:20` |
| Boot bank | `P1 active, BTSEQ=0xFFF` |
| SYSCLK during the experiment | `200000000 Hz (FRC -> PLL1)` |
| Console | UART1 @ 230400 8N1, PKOB4 CDC, `COM12` |
| Console bridge | `sonora_monitor` HTTP API on `127.0.0.1:8080` |
| Test date | 2026-07-29 |

## §5 Build Variation

`APP_BUILD_PLL2_RESTART_TEST` forces off everything that could perturb the
measurement or write flash during a 1000-reset campaign:

| Feature | State |
|---|---|
| `HAL_STARTER_ENABLE_TDM_SMOKE_DEMO` | 0 |
| `HAL_STARTER_ENABLE_TDM_NEG_TEST` | 0 |
| `HAL_STARTER_ENABLE_UART_ASYNC_SELFTEST` | 0 |
| `CAN_BUS_TEST` / `CAN_BUS_TEST_ECHO` | 0 / 0 |
| LED / SST26 / I2C / CAN / RGB demos | compiled out of `main()` |
| SST26 erase / write | never executed |

Only clock bring-up, tick timer, high-resolution timer, UART1, the boot banner,
and the PLL2 test run at boot; control then stays in the shared console loop.
All other `APP_BUILD` variations set `HAL_STARTER_ENABLE_PLL2_RESTART_TEST 0`,
so **normal Starter build behavior is unchanged**.

## §6 The Stop / Restart Sequence

Implemented in `src/hal_clock/dspic33ak_clock_restart.c`. Added *alongside* the
pinned six-file HAL rather than inside it, so byte identity with upstream (see
`docs/clock_hal_integration.md`) is preserved.

1. **Precondition checks** (never skipped): CPU is on PLL1; no CLKGEN is sourced
   from PLL2; FRC is explicitly enabled and `OSCCTRL.FRCRDY == 1`;
   `OSCCTRL.CLKLOCK == 0`.
2. **Forced stop**: `PLL2CONbits.ON = 0`, then `OSCCTRLbits.PLL2EN = 0`; confirm
   `OSCCTRL.PLL2RDY == 0` and `PLL2CON.CLKRDY == 0` under timeout.
3. **Stale-handshake check** (`PLL_RESTART_STAGE_CHECK_STALE_BITS`): if any of
   `PLLSWEN` / `FOUTSWEN` / `OSWEN` / `DIVSWEN` is still set after the stop, that
   is recorded as a failure. Those bits are **not** silently cleared.
4. **Configure while stopped**: `PLLPRE`, `PLLFBDIV`, `POSTDIV1`, `POSTDIV2`,
   and `NOSC = FRC`. `VCO2DIV` / `DIVSWEN` are unused this round.
5. **Restart**: `ON = 1`, `PLL2EN = 1`; then `PLLSWEN` -> wait, `FOUTSWEN` ->
   wait, `OSWEN` -> wait, strictly sequential; then verify `PLL2RDY == 1` and
   `CLKRDY == 1`.
6. **Fail-fast**: on any stage timeout, stop at that stage -- do not issue the
   next handshake bit on top of a failed one, do not issue a software reset,
   keep the console alive, and print the register snapshot.

## §7 Stages and Timing

`pll_restart_stage_t` (`dspic33ak_clock_restart.h`):

```text
NONE, SOURCE_READY, FORCE_OFF, WAIT_OFF, CHECK_STALE_BITS,
PROGRAM_DIVIDERS, ENABLE, PLLSWEN, FOUTSWEN, OSWEN,
WAIT_LOCK, VERIFY, DONE
```

`CHECK_STALE_BITS` is an addition to the original stage sketch: without it, a
stale-handshake failure and an OFF-poll timeout would both report
`WAIT_OFF` and be indistinguishable in the logs.

Timing fields (`stop_time_us`, `pllswen_time_us`, `foutswen_time_us`,
`oswen_time_us`, `lock_time_us`) are measured with the Starter's
high-resolution timer (Timer2 HRT, 100 MHz, self-check PASS at boot).

## §8 Register Snapshots

`src/hal_clock/dspic33ak_clock_diag.c` is the only new file that touches raw
SFRs; it surfaces `RCON`, `OSCCTRL`, `PLL1CON`, `PLL1DIV`, `PLL2CON`, `PLL2DIV`,
`VCO2DIV`, `CLKFAIL`, `SCSFAIL`, `CLKDIAG` plus decoded fields as a plain C
struct (`dspic33ak_clock_diag_snapshot_t`). Higher layers never read SFRs
directly.

Decoded fields include: `RCON.{POR,BOR,EXTR,SWR,WDTO,CM}`;
`OSCCTRL.{FRCEN,FRCRDY,PLL1EN,PLL1RDY,PLL2EN,PLL2RDY,CLKLOCK}`;
`PLL2CON.{ON,NOSC,COSC,CLKRDY,PLLSWEN,FOUTSWEN,OSWEN,DIVSWEN}`;
`PLL2DIV.{PLLPRE,PLLFBDIV,POSTDIV1,POSTDIV2}`; `CLKFAIL.PLL2FAIL`;
`SCSFAIL.PLL2SCS`; `CLKDIAG.STOPPLL2`.

## §9 Early Capture

`pll2_restart_test_early_capture()` is the **first** call in `main()`, before
`starter_clock_init()` touches PLL1. It is read-only with respect to the clock
tree; it latches the reset cause and PLL2 state, then clears `RCON`'s sticky
bits so the *next* software reset in the same power session is not misread as
"POR still set". The snapshot is printed once UART1 is up.

## §10 Reset-Persistent Campaign State

`pll2_campaign_state_t` (magic, magic_inverse, requested/completed/pass/fail/
remaining counts, last_failed_stage) is held in a **`__attribute__((persistent))`**
variable, not a normal static -- crt0 does not zero it, so it survives a
software reset.

Placement verified in the map file, as required:

```text
.pbss              0x0000         0x20 build/dsPIC33AK512/production/_ext/.../pll2_restart_test.o
```

`0x20` = 32 bytes = the eight `uint32_t` fields. The struct self-verifies via
`magic` / `magic_inverse`, and is auto-cleared on POR/BOR, magic mismatch, or
any reset cause that is not a clean software reset. Only a clean `RCON.SWR`
(with POR/BOR/WDTO/EXTR/CM all clear) is trusted to continue a campaign, so
**only software resets count toward the pass total**.

## §11 Results

### Phase 0 -- Baseline (POR)

```text
 app    : APP_BUILD_PLL2_RESTART_TEST (PLL2 forced-stop/restart and software-reset campaign)
 sysclk : 200000000 Hz (FRC -> PLL1)
 bank   : P1 active, BTSEQ=0xFFF
 HRT self-check: PASS
 [P2] early reset=POR en=0 rdy=0 on=0 clk=0 nosc=1 cosc=1
 [P2] early pllsw=0 foutsw=0 osw=0 divsw=0 fail=0 scs=0 stop=0
 [P2] no campaign in progress.
```

PLL2 fully off at POR, no residual handshake bits, no `PLL2FAIL` / `PLL2SCS` /
`STOPPLL2`. Campaign state correctly reported empty. **PASS**

### Phase 1 -- `*p2` x10 (restart from whatever state PLL2 is in)

10/10 PASS. `actual_hz == 520000000` every time. Post-phase state:

```text
"?p2" now   en=1 rdy=1 on=1 clk=1 nosc=1 cosc=1 pllsw=0 foutsw=0 osw=0 divsw=0
```

Locked, with every request bit clear. **PASS**

### Phase 2 -- `*p2d` x10 (restart, then force-stop the now-locked PLL2 and restart again)

20/20 restarts PASS (two per command). The second restart of each pair is the
one that matters: it force-stops a PLL2 that is **locked and running**, and
relocks it. No stale-handshake failure occurred in any iteration. **PASS**

### Phase 3 -- Software-reset campaigns

Run strictly in ascending order; no scale was started before the previous one
completed cleanly.

| Campaign | Iterations | Pass | Fail | Result |
|---|---|---|---|---|
| `*p2r0010` | 10 | 10 | 0 | PASS |
| `*p2r0100` | 100 | 100 | 0 | PASS |
| `*p2r1000` | 1000 | 1000 | 0 | PASS |

Final counters after the 1000-iteration campaign:

```text
"?p2" now   en=1 rdy=1 on=1 clk=1 nosc=1 cosc=1 pllsw=0 foutsw=0 osw=0 divsw=0
 [P2] early reset=SWR en=0 rdy=0 on=0 clk=0 nosc=1 cosc=1
 [P2] early pllsw=0 foutsw=0 osw=0 divsw=0 fail=0 scs=0 stop=0
 [P2] campaign continuing: 1000/1000 done, remaining=0
"?p2" campaign requested=1000 completed=1000 pass=1000 fail=0 remaining=0 last_fail_stage=0
```

Whole-session log scan: `1110` `[P2R] .... PASS` lines (10 + 100 + 1000) and
**zero** lines matching `FAIL`, `TIMEOUT`, or `stopped`.

Iteration wall-clock was ~139 ms (75 ms drain/settle delay + reset + boot +
restart), so the 1000-iteration campaign completed in about 2 min 20 s.

### Lock-time statistics

Across **all 1152** measured restarts in the session (Phase 1 + Phase 2 +
Phase 3 + Phase 4):

| Stage | min | max | mean |
|---|---|---|---|
| `stop_time_us` | 0 | 0 | 0.00 |
| `pllswen_time_us` | 0 | 0 | 0.00 |
| `foutswen_time_us` | 177 | 178 | 178.00 |
| `oswen_time_us` | 0 | 0 | 0.00 |
| `lock_time_us` | 9 | 9 | 9.00 |

`actual_hz` was `520000000` on every one of the 1152 restarts -- a single
distinct value, no near-misses. Whole-session scan: `1120` `[P2R] .... PASS`
lines and **zero** lines matching `FAIL`, `TIMEOUT`, or `stopped`.

Reading: the forced stop, the `PLLSWEN` handshake, and the `OSWEN` handshake all
complete below the 1 us reporting granularity. The cost is concentrated in
`FOUTSWEN` (~178 us) and, after it, a further 9 us to `PLL2RDY`. `FOUTSWEN`
being ~20x the final lock wait is the notable shape here: the output-switch
handshake, not VCO lock, dominates PLL2 restart time.

### First failure stage

None. No stage ever timed out, and `last_failed_stage` remained `0`
(`PLL_RESTART_STAGE_NONE`) throughout.

## §12 Phase 4 -- Power-Cycle Recovery

Performed by physically disconnecting and reconnecting USB *immediately after*
the 1000-iteration campaign had finished with `requested=1000 completed=1000
pass=1000`, i.e. with a fully-populated persistent campaign struct in RAM.

Post-POR state:

```text
"?p2" now   en=0 rdy=0 on=0 clk=0 nosc=1 cosc=1 pllsw=0 foutsw=0 osw=0 divsw=0
 [P2] early reset=POR en=0 rdy=0 on=0 clk=0 nosc=1 cosc=1
 [P2] early pllsw=0 foutsw=0 osw=0 divsw=0 fail=0 scs=0 stop=0
 [P2] no campaign in progress.
"?p2" campaign requested=0 completed=0 pass=0 fail=0 remaining=0 last_fail_stage=0
```

Three things are confirmed here:

1. The reset cause is correctly latched as `POR`, not `SWR`.
2. PLL2 is back to fully off with every request bit clear and no
   `PLL2FAIL` / `PLL2SCS` / `STOPPLL2` -- the 1000 software resets left no
   sticky clock-fault state behind.
3. The persistent campaign struct **auto-cleared on POR** (1000/1000 -> all
   zero), exactly as §10 requires. A cold power cycle cannot resurrect a stale
   campaign.

Fresh restart after the power cycle:

```text
"*p2d" restart #1 (whatever state PLL2 is in now):
 [P2] PASS off=0us pllsw=0us fout=178us osw=0us lock=9us hz=520000000
"*p2d" restart #2 (force-stop the now-locked PLL2, restart again):
 [P2] PASS off=0us pllsw=0us fout=178us osw=0us lock=9us hz=520000000
```

Short campaign re-armed after the power cycle: `*p2r0010` -> `10/10 pass`.

Timings are identical to the pre-power-cycle numbers (178 us / 9 us), so the
power cycle produced no measurable change in restart behavior. **PASS**

## §13 Pass/Fail Checklist

| Criterion | Result |
|---|---|
| Phase 0 baseline snapshot as expected | PASS |
| Phase 1 `*p2` x10 all pass | PASS (10/10) |
| Phase 2 `*p2d` x10 all pass | PASS (20/20 restarts) |
| Phase 3 10-iteration campaign | PASS (10/10) |
| Phase 3 100-iteration campaign | PASS (100/100) |
| Phase 3 1000-iteration campaign | PASS (1000/1000) |
| Zero stage timeouts | PASS |
| Zero `CLKFAIL.PLL2FAIL` | PASS |
| Zero `SCSFAIL.PLL2SCS` | PASS |
| Zero unexpected reset causes during campaigns | PASS (campaign continuation requires clean `SWR`) |
| Zero persistent-state corruption | PASS (magic/magic_inverse intact for 1000 resets) |
| No UART loss | PASS (console responsive before, during, and after all campaigns) |
| Every iteration: `PLL2RDY == 1`, `CLKRDY == 1`, all request bits clear | PASS |
| Every iteration: `actual_hz == 520000000` | PASS (1152/1152) |
| Phase 4 power-cycle recovery | PASS (POR latched, state auto-cleared, restart + 10/10 campaign after) |

## §14 What This Does and Does Not Show

Shown:

- An explicit forced-stop-then-reprogram-then-relock sequence relocks PLL2
  deterministically, from both the off state and the locked-and-running state.
- **A software reset does return PLL2 to a fully off state on this device.**
  Every campaign boot reported `early reset=SWR en=0 rdy=0 on=0 clk=0` with all
  request bits clear. PLL2 hardware state does *not* survive `SWR` here.
- Restart timing is extremely repeatable (lock time identical to the microsecond
  across 1152 trials), which argues against a marginal/racy handshake in this
  configuration.

Not shown, and deliberately not claimed:

- Nothing about **PLL1**, whose forced stop is untestable from code executing on
  it. The lock-failure-needs-power-cycle problem in the field involves PLL1 and
  SYSCLK; this experiment does not reproduce or explain it.
- Nothing about a **different reference source**. Only `NOSC = FRC` was
  exercised. The hot-restart case that most concerns the field problem -- a PLL
  previously locked to REFI1 being reprogrammed to FRC, or the reverse -- is
  untested here.
- Nothing about restarting PLL2 **early in boot**. This experiment deliberately
  starts from a fully-booted, stable Starter, isolating it from the known
  "too-early-boot FRC -> PLL2 divider handshake stalls" problem.
- Nothing about PLL2 feeding a **peripheral** (CLKGEN). PLL2 was left
  unconnected throughout, by precondition check.

## §15 Recommendation

The forced-stop-first ordering is worth adopting as the general PLL restart
primitive: it is strictly more defensive than the pinned
`configure_pll2()` ordering (which programs dividers before switching `NOSC` on
a possibly-hot PLL), it costs ~178 us dominated by one handshake, and it failed
zero times in 1152 trials including 1000 software resets.

Recommended, in order, as separate pieces of work:

1. Repeat this harness with a **source change** across the restart
   (FRC -> REFI1 and REFI1 -> FRC), since that is the case closest to the field
   problem and is currently untested.
2. Then move the same restart primitive to an **earlier boot position**, which
   is the natural follow-up now that 1000 iterations pass from a stable state.
3. Only after both: consider whether an equivalent primitive can be built for
   PLL1, which needs a fundamentally different approach (the CPU cannot be
   running on the PLL it stops).

This branch is **not** merged to `main`. It adds files alongside the pinned
Clock HAL and gates all behavior behind `APP_BUILD_PLL2_RESTART_TEST`, so it can
sit on a branch indefinitely without affecting the shipped Starter.

## §16 Console Commands

| Command | Effect |
|---|---|
| `?p2` | Print current PLL2 state, the boot early-capture snapshot, and campaign counters |
| `*p2` | One forced-stop/restart from whatever state PLL2 is in |
| `*p2d` | Two restarts back to back: the second force-stops a locked, running PLL2 |
| `*p2r0010` / `*p2r0100` / `*p2r1000` | Arm an N-iteration software-reset campaign and reset immediately |
| `*p2x` | Clear campaign state |

All fit the existing 32-byte UART1 command-line capacity. `*p2r` takes exactly
four digits (`0001`-`9999`).

## §17 Files

| File | Role |
|---|---|
| `src/hal_clock/dspic33ak_clock_diag.{c,h}` | Register snapshot; only new file touching raw clock SFRs |
| `src/hal_clock/dspic33ak_clock_restart.{c,h}` | `dspic33ak_clock_pll_restart()`; PLL1 refused |
| `src/app/pll2_restart_test.{c,h}` | Early capture, campaign state, console command handlers |
| `src/app/app_build_config.h` | `APP_BUILD_PLL2_RESTART_TEST` (10) |
| `src/app/app_config.h` | `HAL_STARTER_ENABLE_PLL2_RESTART_TEST` default 0 |
| `src/console/fw_command.c` | `?p2` / `*p2` / `*p2d` / `*p2r` / `*p2x` dispatch |
| `src/main.c` | Early capture first; test run replaces the demo block |
| `firmware.X/nbproject/configurations.xml` | New sources registered |

The pinned six `src/hal_clock/dspic33ak_clock*.{c,h}` files are **unmodified**;
byte identity with upstream is preserved.

## §18 Build Verification

| Check | Result |
|---|---|
| `build.ps1 -Preset APP_BUILD_PLL2_RESTART_TEST -Full` | PASS, provision PASS, program region 59220 bytes |
| `build.ps1 -Preset APP_BUILD_STARTER_DEFAULT -Full` | PASS, provision PASS, program region 83980 bytes |
| `git diff --check` | clean |
| Persistent-section placement in map file | verified (`.pbss 0x20`) |

The default variant building and running unchanged is what backs the
"normal Starter build behavior is unchanged" claim in §5.

## §19 Branch State

| Item | Value |
|---|---|
| Branch | `exp/pll2-soft-reset-restart` |
| Base | `main` @ `7d12e42` |
| Mainline merge | **not performed, by design** |
| Firmware left on board | `APP_BUILD_PLL2_RESTART_TEST` image from this branch |
| Commits | diagnostics snapshot; forced-stop/restart path; reset campaign; this results document |

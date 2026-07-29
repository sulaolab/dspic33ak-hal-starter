# PLL2 Early-Boot Position Experiment — Protocol

Branch `exp/pll2-early-boot-position`, based on `exp/pll2-soft-reset-restart`.
**Not merged to main.** Results section is empty until the hardware run happens.

## §1 The Question

[`pll2_soft_reset_restart_experiment.md`](pll2_soft_reset_restart_experiment.md)
established that the forced-stop-first PLL2 restart primitive works from a
**fully-booted, stable** state: 1152/1152 restarts of FRC 8 MHz → 520 MHz passed,
including a 1000-iteration software-reset campaign, with lock time identical to
the microsecond across every trial. Its §15 named the next step, and its §14
explicitly disclaimed the thing this document tests: *nothing about restarting
PLL2 early in boot.*

The reason that matters is a separate, still-unresolved failure recorded outside
this repository, on a different application running on the same device. Requesting
**exactly the same** FRC 8 MHz → PLL2 520 MHz at early application startup fails:

```text
HAL status=5   (DSPIC33AK_CLOCK_ERR_TIMEOUT)
actual_hz=0

PLLSWEN=1      <-- stuck
DIVSWEN=1      <-- stuck
CLKRDY=0
PLL2RDY=0

CLKFAIL.PLL2FAIL=0
SCSFAIL.PLL2SCS=0
CLKDIAG.STOPPLL2=0
```

Three details make this worth chasing rather than filing as "PLL sometimes does
not lock":

1. **It is not a lock failure.** The stall is at the `PLLSWEN` wait-clear inside
   the pinned `configure_pll2()` — *before* `NOSC` is ever written. The source
   switch never happens, so nothing has been asked to lock yet.
2. **`DIVSWEN` is never written by the pinned PLL path at all.** It appears only
   in the CLKGEN macro. So `DIVSWEN=1` in that snapshot is residual
   hardware-side state, not something the code set.
3. **The identical request succeeds later in the same boot.** Only the position
   differs.

Recorded evidence matrix from that investigation:

| Condition at the early position | Result |
|---|---|
| immediate FRC → PLL2 | FAIL |
| + 5000 ms delay | FAIL |
| + PPS unlock/lock only | FAIL |
| + unrelated PPS same-value write | FAIL |
| + `REFI1R` same-value write 0 → 0 | FAIL |
| + `REFI1R` 0 → RP51 (non-clock pin) | FAIL |
| + `REFI1R` 0 → RP16 | **PASS** |
| late FRC → PLL2 | **PASS** |

No workaround was adopted; that application avoids early FRC → PLL2 entirely, and
the root cause is explicitly recorded as unresolved. Whether the RP16 effect needs
the *active* 12.288 MHz signal on that pin or is an RP16-specific register
condition was left open.

So: **does that stall reproduce on this board, and if so, does a different
ordering avoid it?**

## §2 Design — Two Axes, One Cell Per Boot

### Positions

| Pos | Site in `src/main.c` | Machine state |
|---|---|---|
| P0 | before `starter_clock_init()` | raw FRC, PLL1 **off**, HRT dead → times read 0 |
| **P1** | right after `starter_clock_init()` | PLL1 up, nothing else |
| P2 | after `dspic33ak_tick_timer_init()` | tick up, still no UART |
| P3 | after `console_uart_init()` | printf available |
| P4 | after the boot banner | the known-PASS late control |

**P1 is the head-to-head.** The application that failed had already brought its
own clock tree up, so it was running on its PLL. P0, with PLL1 off, is a
*different condition* rather than merely an earlier one — a useful extra data
point, but not the comparison.

### Orderings

All three arms share one `static const dspic33ak_clock_pll_config_t
{DSPIC33AK_CLOCK_SOURCE_FRC, 8000000, 520000000}`, so they differ **only** in the
order of operations, never in what is requested.

| Arm | Sequence | Isolates |
|---|---|---|
| **M0** | `dspic33ak_clock_pll_configure()` — the pinned path, unmodified | the baseline: does the stall reproduce at all? |
| **M1** | `dspic33ak_clock_pll_restart()` — force stop, verify off, check stale bits, program dividers + `NOSC` while off, then staged re-enable | the full composite primitive |
| **M2** | `dspic33ak_clock_pll_force_stop()`, then the **pinned** `dspic33ak_clock_pll_configure()` | whether the forced stop *alone* is what helps |

M2 is not optional decoration. M1 changes three things at once — forced stop,
dividers programmed while off, `NOSC` written while off — and the stall being
hunted happens *before `NOSC` is written*, so an M1 pass on its own cannot
attribute the fix to any one of them. M0 → M2 → M1 is what separates them:

- M0 fails, M2 passes → the forced stop is sufficient.
- M0 fails, M2 fails, M1 passes → programming while off is what matters, not the
  stop.
- M0 passes everywhere → the trap does not reproduce here, and **nothing** is
  claimed about ordering.

## §3 Observability Before UART Exists

At P0/P1/P2 there is no UART1 and (at P0/P1) no Timer1, so the attempt cannot
print and cannot reset. The primitive is already safe to call there: timing comes
only from the high-resolution timer (which returns 0 when uninitialized rather
than failing), and every wait is bounded by a self-contained decrementing counter
(`RESTART_POLL_LIMIT` = 1,000,000), so it cannot hang indefinitely on a
never-clearing bit.

Therefore the hook **stores and stays silent**. A persistent-RAM record carries
the outcome across to the point where UART1 exists, and
`pll2_early_boot_test_report()` prints it after the banner. That report function
is also the **only** place allowed to issue the software reset for a repeat run,
because the reset path needs a running tick timer.

`dspic33ak_high_res_timer_init()` was moved up to immediately after
`starter_clock_init()` so P1/P2/P3 timings are comparable with the P4 control. It
needs only a correct `timer_clk_hz` — no tick timer, no UART, no interrupt — so
this is safe for every build variation.

### The tripwire

A hang must become evidence, not a boot loop. The record's state moves to
`ATTEMPT_STARTED` **immediately before** PLL2 is touched:

| State found at boot | Meaning | Action |
|---|---|---|
| magic pair bad | fresh/garbage RAM | zero the record |
| `ATTEMPT_STARTED` | previous boot died **inside** the attempt | `trip_kind=1`, disarm, do not retry |
| `ATTEMPT_DONE` | attempt returned but that boot never reported | `trip_kind=2`, disarm, **keep** the result and print it |
| `ARMED` | armed and not yet run | leave armed; the matching hook fires |
| `REPORTED` / `TRIPPED` | nothing armed | boot normally |

Two deliberate choices:

- **No clean-SWR gate.** Unlike `s_campaign` in `pll2_restart_test.c`, this record
  is trusted on its magic pair alone. A hang is escaped by MCLR or a power cycle,
  which report EXTR/POR — an SWR-gated record would be wiped in exactly the case
  the tripwire exists for.
- **No watchdog.** Enabling FWDT would change config words shared by every build
  variation. A hung board therefore needs a human MCLR; `?p2e` then names the
  position that hung.

Known ambiguity: `trip_kind=2` cannot distinguish "the boot hung after the
attempt" from "power was cut between the attempt and the print".

Safety margin for this board: PLL2 feeds no CLKGEN in this Starter, so a botched
PLL2 cannot break SYSCLK or the console. And per the previous experiment's
finding that PLL2 state does not survive a software reset, every single-shot
attempt starts from a PLL2-off entry state; only a repeat run varies that.

## §4 Console Commands

| Command | Meaning |
|---|---|
| `?p2e` | print live PLL2 state, the boot early-capture snapshot, and the record |
| `*p2ea<p><m>` | arm ONE attempt (`p`=0..4, `m`=0..2), then software reset — e.g. `*p2ea11` = P1/M1 |
| `*p2er<p><m><NN>` | arm `NN`=01..99 attempts at that cell, one per boot |
| `*p2eb<p>` | run M0, M1, M2 immediately with no reset (late position; `p` is only a label) |
| `*p2ex` | clear the record and disarm |

`*p2eb` is a sanity check that the three orderings agree once the machine is fully
up. It is **not** a substitute for arming a real early position.

## §5 Test Phases

Board: PKOB4 `020085204RYN000057`, console `COM12` @230400 via the
`sonora_monitor` HTTP bridge on `127.0.0.1:8080`.

| Phase | Commands | Pass criteria |
|---|---|---|
| **A** harness integrity | `*p2ea40`, `*p2ea41`, `*p2ea42` | each produces a record; P4/M1 reproduces the known 520 MHz PASS; `?p2e` still readable after a manual MCLR; counters accumulate and state reaches `REPORTED` |
| **B** M0 baseline walk | `*p2ea00`, `10`, `20`, `30`, `40` | **verdict phase.** Trap reproduced iff some position gives `status=5`, `hz=0`, and `PLL2CON` shows `PLLSWEN=1` (ideally `DIVSWEN=1`) |
| **C** M2 then M1 | `*p2ea<p>2`, `*p2ea<p>1` at the positions B covered | `status=0`, `hz=520000000` |
| **D** repeat run | `*p2er<p><m>50` | `pass=50 fail=0 hang=0`, then one power cycle with `?p2e` still readable |
| **E** matrix | — | one row per (position, ordering) in §6, with verbatim `[P2E]` lines |

**Stop and report after Phase B.** If Phase B does not reproduce the stall, Phase C
still runs for completeness but the result is simply *"the trap did not reproduce
on hal-starter at P0..P4"* and no claim of any kind is made about ordering.

The tripwire is asserted by code review, not by faking a hang. If anything hangs
during Phase B it will fire on its own.

## §6 Results

Not yet run.

| Position | M0 pinned | M2 stop+pinned | M1 restart |
|---|---|---|---|
| P0 pre-clock | — | — | — |
| P1 post-clock | — | — | — |
| P2 post-tick | — | — | — |
| P3 post-UART | — | — | — |
| P4 late (control) | — | — | — |

## §7 What A Result Would and Would Not Prove

**M0 fails / M1 or M2 passes at the same position** would prove that on this board,
at that boot position, the pinned configure sequence is reproducibly blockable
while the other ordering completes — i.e. the failure is a property of the
*sequence in that state*, and the signature reproduces outside the application it
was first seen in, making it a Clock-HAL-level concern rather than an integration
artefact. M2 vs M1 then attributes it to the forced stop or to programming while
off.

It would **not** prove:

- **That it fixes the original application.** Different board, different
  PPS/`REFI1R` state, different application — and the RP16 evidence implies a
  board-level contributor this experiment does not control.
- **Any root cause** for `PLLSWEN`/`DIVSWEN` being stuck. The fault bits were
  clean there and are expected clean here; nothing in this harness explains the
  hardware handshake.
- **Robustness beyond N.** A single pass says nothing about a 1-in-500 flake;
  Phase D bounds it only to 1/50.
- **That the pinned HAL should change.** The six pinned
  `src/hal_clock/dspic33ak_clock*.{c,h}` files stay byte-identical to upstream by
  requirement. The deliverable is evidence for that discussion, not a patch.

If Phase B reproduces nothing, the leading explanation is that the trap needs the
other board's live 12.288 MHz on RP16. That is a **result**, and it points at a
genuinely useful separate one-lever follow-up: this board has no 12.288 MHz on
RP16, so writing `REFI1R` → RP16 *here* would answer the question the original
investigation left open — does the effect need the active signal, or only the
route? That belongs in its own experiment, not as a third axis in this harness.

## §8 Files

| File | Role |
|---|---|
| `src/app/pll2_early_boot_test.{c,h}` | persistent record, tripwire state machine, M0/M1/M2 arms, reporting, command bodies |
| `src/hal_clock/dspic33ak_clock_restart.{c,h}` | `dspic33ak_clock_pll_force_stop()` split out so M1 and M2 share one stop implementation |
| `src/app/app_build_config.h` | `APP_BUILD_PLL2_EARLY_BOOT_TEST` (11) |
| `src/app/app_config.h` | gate default 0, plus derived `HAL_STARTER_PLL2_EXPERIMENT_BUILD` |
| `src/main.c` | five hooks, the report call, and the HRT hoist |
| `src/console/fw_command.c` | command dispatch |

The six pinned `src/hal_clock/dspic33ak_clock*.{c,h}` files are **unmodified**.
`APP_BUILD_PLL2_RESTART_TEST` (10) is untouched, so the recorded 1152/1152 result
stays reproducible.

Map check for the persistent record, as required for `s_campaign` before it:

```text
.pbss                       0x64ec                   0            0x4c  (76)
```

76 bytes, matching the record struct exactly.

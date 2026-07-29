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

Phase D was originally gated on Phase B *reproducing* the stall. It was run
anyway, on the M0 baseline at P1, for the opposite reason: a null result from one
attempt per cell cannot distinguish "does not happen" from "happens
intermittently", so the repeat bounds the null instead of the fix.

**Stop and report after Phase B.** If Phase B does not reproduce the stall, Phase C
still runs for completeness but the result is simply *"the trap did not reproduce
on hal-starter at P0..P4"* and no claim of any kind is made about ordering.

The tripwire is asserted by code review, not by faking a hang. If anything hangs
during Phase B it will fire on its own.

## §6 Results

Run 2026-07-29 on PKOB4 `020085204RYN000057`, device `dsPIC33AK512MPS512`
(ID `0xa77c`, rev `0x1`), DFP 1.3.185, XC-DSC 3.31.01, MPLAB X 6.30, console
`COM12` @230400.

### Verdict: the stall did NOT reproduce

**Every cell passed.** 15/15 single attempts reached
`status=0`, `hz=520000000`, with `CLKFAIL`/`SCSFAIL`/`CLKDIAG` all zero and every
handshake request bit clear afterwards. No stage ever timed out, no tripwire ever
fired (`hang=0` throughout), and no boot had to be rescued.

| Position | M0 pinned | M2 stop+pinned | M1 restart |
|---|---|---|---|
| P0 pre-clock (PLL1 off) | PASS | PASS | PASS |
| P1 post-clock | PASS | PASS | PASS |
| P2 post-tick | PASS | PASS | PASS |
| P3 post-UART | PASS | PASS | PASS |
| P4 late (control) | PASS | PASS | PASS |

Because a single pass per cell cannot rule out an intermittent stall, the most
relevant cell was repeated: **P1/M0 ×50 software resets → `done=50 pass=50
fail=0 hang=0`.** So the null result is "did not reproduce in 50 consecutive
boots at the head-to-head position", not "did not reproduce once". Total
attempts this session: **65, all PASS**.

Representative records, verbatim:

```text
 [P2E] last P0/M0 status=0 stage=0 hz=520000000
 [P2E] time off=0us pllsw=0us fout=0us osw=0us lock=0us poststop=0x0
 [P2E] post OSCCTRL=0000A300 PLL2CON=80028101 PLL2DIV=01004109
 [P2E] post CLKFAIL=00000000 SCSFAIL=00000000 CLKDIAG=00000000

 [P2E] last P1/M1 status=0 stage=12 hz=520000000
 [P2E] time off=0us pllsw=0us fout=178us osw=0us lock=0us poststop=0x0
 [P2E] post OSCCTRL=0000E380 PLL2CON=80028101 PLL2DIV=01004109

 [P2E] rec state=ATTEMPT_DONE armed=P1/M0 rep=0 done=50 pass=50 fail=0 hang=0
```

`PLL2CON=80028101` and `PLL2DIV=01004109` were **identical in all 15 cells** —
the same divider geometry and the same locked state regardless of position or
ordering.

### Three incidental findings worth keeping

**1. The pinned path locks PLL2 with `OSCCTRL.PLL2EN` left at 0.**
`OSCCTRL` came back `…E300` after M0 and M2, but `…E380` after M1 — bit `0x80`
differs, and M1 is the only arm that writes `OSCCTRLbits.PLL2EN = 1`. Confirmed
live afterwards:

```text
"?p2e" now  en=0 rdy=1 on=1 clk=1 nosc=1 cosc=1 pllsw=0 foutsw=0 osw=0 divsw=0
```

PLL2 is running and ready (`rdy=1 on=1 clk=1`) with `PLL2EN=0`. So on this
device `PLL2EN` is not required for PLL2 to lock and be usable via
`PLL2CON.ON`, and the pinned `configure_pll2()` never touches it. Whether that
matters for anything downstream is not established here — it is recorded because
it is a real asymmetry between two paths that both "work".

**2. The true lock wait is under 1 us, not 9 us.**
[`pll2_soft_reset_restart_experiment.md`](pll2_soft_reset_restart_experiment.md)
reported `lock=9us`, measured with a timestamp shared with the OSWEN stage. With
the dedicated timestamp added on this branch, every M1 cell reports `lock=0us`
while `fout` stays at 178 us. The earlier 9 us was therefore OSWEN-start → lock,
and PLL2's actual lock wait after the output switch is sub-microsecond. The
earlier figure was not wrong about the total, only about the attribution.

**3. `P0` reports all-zero times, exactly as designed.**
At P0 the high-resolution timer is not yet initialized, so `fout` reads 0 for
P0/M1 while P1..P4/M1 all read 178 us. Pass/fail at P0 is still valid; only its
timing is blind.

### What this rules out

The trap is not a property of:

- **being early in boot per se** — P0, with `PLL1` off and the CPU on the raw FRC,
  is earlier than the position where the original failure was seen, and it passed
  50/50 at P1 and 1/1 at P0;
- **the pinned `configure_pll2()` sequence in isolation** — M0 is that sequence,
  unmodified, and it passed at every position;
- **the FRC → 520 MHz operating point** — the divider geometry solved and locked
  identically everywhere.

### Consequence

**No claim is made about ordering.** M1 and M2 passing means nothing here,
because M0 never failed — there was nothing for them to fix. The M2 arm, built
specifically to separate "the forced stop is what helps" from "programming while
off is what helps", has no work to do on this board and remains unused evidence
for whenever a reproduction exists.

The leading explanation stands as anticipated in §7: the discriminator in the
original evidence was **pin-specific, not temporal** — `REFI1R` 0 → RP16 flipped
FAIL to PASS while 0 → RP51 did not, and a 5000 ms delay did not help. This board
has nothing on RP16. That makes the recommended next step the `REFI1R` experiment
described in §7, not more boot positions.

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

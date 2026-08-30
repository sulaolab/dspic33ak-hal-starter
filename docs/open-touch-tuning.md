# Open capacitive touch — tuning manual

This is the procedure for making the three touch pads behave the way *your*
product needs them to, and for telling the difference between a pad that is badly
tuned and a pad that is badly built. It assumes the code as shipped
([`src/hal_touch/`](../src/hal_touch/)) and the
module `k` console commands listed in [open-touch.md](open-touch.md).

Everything below was measured on a dsPIC33A Curiosity Platform Development Board
with an AK512 DIM, at 200 MHz on the ITC clock. **The numbers are reference data,
not constants of nature** — the procedures are what transfer to another board, and
the reason each figure is quoted with the measurement that produced it is so you
can tell when it no longer applies.

**Scope, stated as a limit.** This gets a board to "the pads work, and I know why".
It does *not* cover drift compensation over temperature and humidity, wet-finger
rejection, frequency hopping, or conducted-immunity qualification. Raising this to
a product is the integrator's work, and the file headers say so rather than
implying a coverage they do not have.

---

## 0. The two facts that look like faults

Read these before you conclude anything is broken.

**A touched pad's signal alternates sign, scan to scan.** A finger produces
*activity* of either sign, not a sustained offset. Measured on a light touch,
recorded at scan rate: delta ranged −4,180…+2,346 with 9 sign flips in 17 scans,
while the mean of |delta| went from 159 idle to 1,607 touched. So detection
compares a **magnitude** — the mean of |raw − baseline| over the last
`NORA_TOUCH_MAG_SCANS` = 4 scans, about 27 ms — and never the signed delta.

Two consequences that kill the obvious fixes:

- *Lowering a signed threshold* cannot work: the idle tail reaches ~800 counts, so
  a signed press level would have to sit underneath it.
- *Averaging the signed delta* cannot work either: averaging a sign-alternating
  signal cancels it, and the noise it would be fighting is low-frequency drift, not
  white noise. Over a 64-scan trace the per-scan sd is 182–200, and a 16-scan mean
  brings 182 only to 103 — far short of the √N a white-noise assumption predicts.
  Inter-pad correlation is 0.51–0.67, so common-mode subtraction buys sd 200 → 145
  and no more.

Rectify first, then average: on the same recording, the 4-scan mean of |delta| was
1,929 at the touched pad's peak against 274 idle and 317 on the other pads — a
factor of seven, which no signed pair reaches.

**Every threshold in this document is in magnitude counts.** A number carried over
from a signed scheme is roughly 3× too large.

**The first press or two after a cold boot may not register.** Each pad learns its
own press amplitude *from being touched* (§3) and walks its threshold down. Nothing
is stored across a power cycle, deliberately (§3.4). The cost is stated plainly
rather than hidden.

---

## 1. Acquisition: the analog settings

### 1.0 The conclusion first, so you can skip this chapter

| knob | what a sweep of it looks like | a sensitivity lever? |
|---|---|---|
| `CVDCAP` (internal CVD capacitor code, 0–7) | count monotonic and saturating; **noise tail flat across the whole range**. No peak. | **No** |
| charge time `TMRA` (500–5000 ns) | count monotonic, saturating, 0.55 % total span; **the noise tail is not flat** — peak-to-trough over matched 16 s windows fell 1268 → 974 from 1,000 to 5,000 ns, and touch magnitude rose with it. | **Yes, modestly** |
| balance time `TMRB` (250–4000 ns) | tail flat within the measurement's own scatter; 250 ns marginally worst. **A floor, not an optimum.** | **No** |
| accumulation depth `ACCCNT` (2^n) | tail ÷ count 0.163 % at 2^4 → 0.054 % at 2^8 | **Yes — but only ~3×** |

So the honest procedure is: **get the three analog knobs out of their bad regions,
then buy signal-to-noise with accumulation depth, and stop.** Four doublings of
accumulation is worth a factor of three, because the absolute noise grows 5× while
the count grows 16×. Nothing in the ITC's own settings is worth an order of
magnitude. If a design needs one, it has to come from the electrode, the stack-up
or the guard.

This contradicts the intuition the family reference manual's framing invites —
that `CVDCAP` exists to be *matched* to the electrode capacitance, with a
resonant-looking sweet spot to hunt for. On this board there is not one. With very
different electrodes (much larger area, long traces, a thick overlay) re-run §1.3
before believing that. The point is not that the numbers transfer; it is that
**the sweep is cheap, so measure rather than assume.**

The shipped defaults, which are the outcome of that procedure:

```
CVDCAP 4 · charge 5000 ns · balance 1000 ns · accumulation 2^8
→ ~110 scans/s over 3 electrodes, ~9 ms per scan
```

**The charge time changed on 2026-08-30, and the "not a lever" verdict above is a
correction, not a new measurement of the same thing.** The original sweep was taken
with the touch shield held statically High and the idle electrodes floating — a
condition the data sheet rules out — so "the knobs are flat" was a statement about
that condition. Re-swept with the shield grounded and the idle electrodes tied,
5,000 ns lowers the idle tail *and* raises touch magnitude, which is why it is now
the shipped default. It costs scan rate: on the upstream board 199 → 110 scans/s,
because each of the 2 samples charges 3 us longer, 2^8 times. The 4-scan magnitude
window is then 36 ms and a 2-scan debounce 18 ms, both still well inside a tap.
5,000 ns is near the ceiling: `TMRA` is 8-bit in TAD and 5,000 ns is already 250 TAD.

### 1.1 Prove the chain before you sweep it

Do not tune a measurement you have not yet shown to be a measurement. In order:

1. **`?ki`** — expect `configured`, `hardware ready (DRDY)`, `idle`, and the
   converted timer counts. A time that rounded to 0 TAD is visible here and
   nowhere else.
2. **`?kd`** — the register dump. Every bring-up failure so far was *silent*:
   records, timers, pin selects and the ready flags all read correct while the
   result was wrong. This dump is what separates "the peripheral did not accept the
   configuration" from "the electrode is dead". Read it before you form a theory.
3. **`?kr` several times** — counts must be non-zero, of the same sign, and
   repeatable. All-zeros is a configuration bug, not a wiring bug.
4. **Touch a pad, `?kr` again** — one record must move and the others must not.
   Until this passes, everything below measures nothing.

Ordering caution: **`?kd` reads the result registers, which clears the
acquisition-done flag.** A dump taken between a scan and `?kr` changes the answer.

In the shipped build the detection layer owns the acquisition list and scans it
continuously, so `?kr` / `*ks` / `*ki` are refused — a console-started scan would
race the detection pump for the same completion flag. Read the equivalent from
`?ko` instead, or set `HAL_STARTER_ENABLE_TOUCH` to 0 to get the console's own list
back.

### 1.2 The metric, and the trap inside it

Sensitivity is **delta ÷ noise**, and only the noise half can be measured with
nobody at the board. Hence:

- **Do not tune on the absolute count.** All three analog knobs move the baseline
  by fractions of a percent while doing nothing for the delta. A sweep that shows
  the count changing smoothly looks like progress and is not.
- **Compare only at equal accumulation depth.** The count is *not* linear in depth
  (per-repeat 1,230 at 2^4 vs 3,406 at 2^8, because a record's first repeats read
  low and a deep accumulation is dominated by the settled value). Two readings at
  different depths are not comparable at all.
- **An absolute count is only meaningful within one image.** Re-baseline after any
  configuration change, and never store a threshold as a raw count.

**And six samples is not the noise figure.** The quick figure — `?kr` six times,
take max − min — is a first look only. Measured: electrodes that spread ±250 over
six samples reached +508…+776 when the extremes were tracked over ~11,000
consecutive scans (`*kz`, wait, `?ko`). Six samples underestimated the peak
excursion by about **3×**, because what threatens a threshold is the tail and a
handful of samples never sees it.

Worse, the underestimate is not a constant factor: it is larger at greater
accumulation depth, where the tail is wider — exactly the axis a sweep compares.
An accumulation-depth figure derived from six-sample spreads came out **13× too
optimistic** for that reason. If you take one habit from this chapter, take this
one: **quote the metric with the conclusion, or the conclusion cannot be checked.**

So: use `*kz` + `?ko` peak/trough over tens of thousands of scans for anything you
intend to act on, **including the sweeps themselves**.

### 1.3 The sweeps

Each command re-applies the whole list, so a value that does not fit its timer is
refused and the previous value is put back — which is why the command prints what
is now in force, and why a reading after a refusal belongs to the *old* setting.

Two facts about re-configuring, both of which cost a debugging session: a
completion left over from the previous setting makes the *next* scan complete
instantly with results belonging to no scan, and the first real scan after a change
reads low. **Allow a moment after each sweep command before believing anything** —
`?ko`'s scan counter restarts at each command, which is the thing to watch. Each
command also re-seeds the baseline and clears the peaks, so a sweep point can be
judged by the tail-tracked peak/trough figure rather than by six samples.

```
*ka08          accumulation 2^8   (first: it sets the floor everything else is measured against)
*kc04          CVDCAP code 4
*kg1388        charge  5000 ns    (16-bit ns as hex: 1388 = 5000)
*kb03E8        balance 1000 ns
```

- **`CVDCAP`** — `*kc00` … `*kc07`. Expect a saturating curve; pick a code in the
  middle of the region where the increments have gone small (4 here).
- **charge time** — `*kg01F4` (500 ns) … `*kg1388` (5000 ns). `TMRA` is 8-bit at
  20 ns/TAD, so **5100 ns is the hard ceiling** and anything above is refused.
  Expect saturation; pick just past the knee, not the largest value — every
  nanosecond here is multiplied by `2^ACCCNT × record_count` in the scan time.
- **balance time** — `*kb00FA` (250 ns) … `*kb0FA0` (4000 ns), and look at the
  spread rather than the mean. Expect the shortest setting to be the noisiest and
  the rest to be indistinguishable. Take the first value that is no longer the
  noisy one, plus margin.
- **accumulation** — `*ka04`, `*ka06`, `*ka08`. This is the one that pays. Deeper
  is better for noise and linearly worse for scan time, so the choice is a
  scan-rate budget decision, not a measurement one: **pick the deepest setting
  that still fits the response time the application needs.**

### 1.4 Recording the result

The settings mean nothing without the evidence. Record, per electrode: baseline
count, tracked peak/trough, and the finger magnitude, **all at the chosen depth**.
That table is what later tells you whether a board drifted, a panel is different,
or a change regressed something.

Reference values for this board at the shipped settings:

| electrode | baseline (raw) | idle peak / trough | touch magnitude |
|---|---|---|---|
| `CVDAN1` (pad 1) | ≈ −875,300 | +552 / −535 | 2,445 |
| `CVDAN8` (pad 2) | ≈ −875,400 | +802 / −714 | 2,653 |
| `CVDAN10` (pad 3, weakest) | ≈ −876,000 | +776 / −489 | 1,936 |

The touch column is the tracked peak of ten deliberate taps; the idle columns are
the same figure with nobody at the board. Read the margin as the ratio of the two
— roughly 2.5–3× at the shipped threshold, and note that it is the *weakest* pad
that sets the design, not the average one.

### 1.5 Self-check without an electrode

`*kt<hhll>` makes every conversion return an injected constant, exercising
accumulation, the A − B subtraction and the sign path with numbers you chose. Use
it to separate "our arithmetic is wrong" from "the electrode is wrong"; `*ku`
leaves it.

One constraint, learned the hard way: **injection stalls an acquisition sequence
that waits on end-of-conversion.** No real conversion happens, so the wait never
releases. Injection therefore tests the layers *above* the converter and cannot be
dropped into a normal scan unchanged.

---

## 2. Detection: from counts to keys

[`src/hal_touch/`](../src/hal_touch/) is the detection layer: a tracked baseline, a
magnitude threshold with hysteresis, debounce, a plausibility guard, and per-pad
threshold learning. It touches no registers.

### 2.1 Why each default is what it is

| setting | default | derived from |
|---|---|---|
| press threshold | 700 (magnitude) | ~2.5× the worst tracked idle tail (802), ~1/3 of the weakest pad's touch magnitude |
| release threshold | 350 | lower on purpose: a finger rolling off dwells near the threshold, and one number for both directions is what makes a key chatter |
| press debounce | 2 scans | one bad conversion cannot make a key; costs ~10 ms of latency |
| release debounce | 4 scans | asymmetric on purpose: the magnitude window is 4 scans long, so a release needs at least that much quiet to be real |
| magnitude window | 4 scans (~27 ms) | long enough to average the alternation out, short enough that a 60 ms tap is several windows long |
| baseline shift | 6 (≈ 64 scans, ~320 ms after the window) | absorbs hand-warming drift without eating a slow press |

Two design points that are easy to get wrong:

- **The baseline is tracked only while the key is released.** Tracking during a
  press follows the finger and releases the key on its own after a second or two.
- **The baseline is seeded from the median of 3 agreeing scans, after 8 are
  discarded.** Seeding from zero would report an 875,000-count "touch" on the first
  scan; seeding from the first scan alone loses to the burst described in §2.3.

### 2.2 Choosing a threshold, and the trap in the press log

**The press log understates the touch.** `press, mag N` prints the magnitude *at
the moment the threshold was crossed* — one debounce period into a finger's
approach, while the capacitance is still rising. It is not the size of the touch.
Any threshold reasoned about from a press line is reasoned about from the wrong end
of the signal.

The durable rule: **set the press threshold from `?ko` peak/trough, never from a
press line, and never from a six-sample spread.** `*kp`/`*kq` change both at
runtime and clear the peaks (a peak belongs to one setting), so a threshold sweep
costs a console command instead of a reflash.

Procedure:

1. `*kz`, then leave the board alone for a few minutes. `?ko` — `peak` and
   `trough` are now the idle tail. This is the number a false press has to beat.
2. `*kz` again, tap each pad ten times at the pressure you consider normal, then
   `?ko`. `peak` is now the touch magnitude.
3. Put the threshold where it is ≳ 2× the idle tail **and** ≲ 1/2 of the weakest
   pad's touch peak. If those two conditions cannot both be met, the electrode or
   the overlay is the problem and no threshold will fix it.
4. `release ≈ press/2`. Learning derives it that way for the same reason.

### 2.3 Rejecting what the electrode cannot have produced

One failure mode in this stack cannot be tuned against, so the layer defends
itself. It was found by a sweep that reconfigured the list 26 times in a row.

**The symptom.** After a reconfiguration, a record occasionally reads back **near
zero** instead of near −875,000 — in 12 of 20 sweep points, always on the second or
third record of the list and never the first, in runs of tens of consecutive
samples. In steady state it does not happen: 131,398 scans without reconfiguration
produced none.

**Why it is not a tuning problem.** That reading is a delta of ~80 % of full scale;
a finger is worth 1.5 %. No threshold separates them — one high enough to reject it
would reject every real touch by a factor of fifty. It has to be rejected as
*impossible*, not as *small*.

| defence | what it catches | why the previous one was not enough |
|---|---|---|
| discard 8 scans after a reconfigure | the systematic error: a record's first repeats read low | — |
| seed the baseline from the median of 3 *agreeing* scans | a bad sample landing on the seeding scan | the median alone loses to a run of two, which is what actually occurs |
| reject any sample more than ¼ of the count from the baseline, and **re-seed after 16 consecutive rejections** | the rest of the burst | the guard alone is *worse than the bug*: with a baseline latched near zero it rejects every correct sample and the key is dead until the next reconfigure — observed as 2,445 rejections in 2,473 scans on a perfectly good electrode |

That third row is the one to remember if you write your own detection layer. **A
plausibility guard needs an escape route**, because the reference it guards against
is itself something that can be wrong — and then the guard defends the error. The
rejection count and the re-seed are what make it self-correcting.

**How to see it.** `?ko` reports `implausible` beside `rejected`, and they mean
different things: `rejected` is a scan the ITC would not run, `implausible` is one
it ran that gave an impossible answer. Idle and unchanged, both should be 0; a
handful right after `*kc`/`*kg`/`*kb`/`*ka` is this fault being absorbed and is
expected. If `implausible` climbs while idle, stop tuning — the acquisition is
broken and no threshold will fix it.

The mechanism behind the near-zero read is **not** understood. Only its shape is,
and it is stated as such rather than explained away.

---

## 3. Per-pad learning

Pads on one board legitimately differ, and boards differ more. Rather than ask an
integrator to hand-tune three numbers per board, each pad learns its own press
amplitude from being touched and walks its own threshold **down** towards it.

### 3.1 Two thresholds, because one cannot start

Learning from presses has a bootstrap problem: a touch too light to cross the press
threshold produces no event *and*, if the same threshold gates learning, no sample.
The pad would never learn the very touches it is failing to detect — precisely the
case worth fixing. So there are two thresholds and only one is visible to the
application:

| | value | fires events | learns |
|---|---|---|---|
| candidate | `max(idle_ref × 4, 800)` | no | yes |
| press | starts at the configured 700; the floor keeps it there unless the pad's own noise pushes it higher | yes | no |

An excursion above the candidate that holds for 3 scans is recorded at its peak
magnitude when it ends. Nothing on that path consults the press threshold, which is
what makes a missed first touch useful rather than wasted.

`idle_ref` is the pad's own quiet magnitude, kept as a **decaying** maximum that
follows the quiet magnitude by 2⁻⁶ of the gap per scan in *both* directions (a few
hundred ms) — fast enough for drift, far slower than a tap.

Symmetric on purpose, and this is a bug fix worth repeating: a version that let
`idle_ref` rise *instantly* ("quiet, so this is what quiet looks like") is wrong,
because the magnitude is a 4-scan mean and the scan on which a touch first falls
below the candidate still contains touch. Measured consequence: one tap pushed
`idle_ref` to ~650 and the candidate to ~1,950, so every following tap (magnitude
780–1,400) sat *under* the candidate. Ten taps per pad produced zero samples while
the fixed press threshold kept firing events perfectly — **so the log looked
healthy and the learner was starved.** If you modify this layer, that is the
failure mode to test for.

**And "quiet" is counted in seconds, not milliseconds** (fixed 2026-08-30). A pad's
magnitude only feeds `idle_ref` after it has stayed below the candidate for
`NORA_TOUCH_IDLE_QUIET_RUN` = 400 scans — about 2 s. The previous 8 scans (40 ms)
was chosen to flush the touch out of the 4-scan averaging window, which it does, but
a hand is still millimetres from the pad 40 ms after a release and couples in a
large fraction of a real touch. Measured on a pad tapped 0.3–0.7 s apart, `idle_ref`
walked to 129–202 (resting value minutes later: 90–110) and the floor multiplied it
into thresholds of 774–900 against real presses of 870–1151 — the threshold landed
*inside* the tap distribution, so the same press worked or did not depending on what
the previous release had left behind. This is per-pad and it, not the all-pads-quiet
gate, is what protects the pad being tapped: while one pad is tapped the other two
are quiet, so that gate stays open for the whole burst. Cost: `idle_ref` follows a
genuine rise in noise up to 2 s late, and it feeds only the floor.

### 3.2 The rule, and the two limits that bound it

```
press   = ( second-smallest of the last up to 8 press magnitudes ) × 35/100
          capped at the configured press threshold,
          then raised to max( 700, idle_ref × 6 )   <- floor applied last
release = press / 2
```

Each clause answers a specific way this could go wrong:

| clause | what it prevents |
|---|---|
| a **low-order** statistic, not the median | the median ties the threshold to how hard the operator happened to press. A run of ordinary firm taps gave medians past 1,556 on all three pads, so 35–45 % of them cleared the shipped 700, the ceiling held, and learning did nothing at all. What the threshold must sit under is the **weakest** press the pad will see — the bottom of the sample set, not its middle. |
| second-smallest once there are ≥ 4 samples (smallest below that) | a single unusually light or clipped excursion dragging the pad down on its own |
| × 35/100 | a threshold that sits on top of the taps it must accept |
| ceiling = the configured value | learning ever making a pad **less** sensitive than the value already proven on this hardware — except where the floor below says that value would press itself. |
| floor = `max(700, idle_ref × 6)`, **applied after the ceiling** | a pad pressing itself. This is the only vote idle noise still gets, and it is a veto rather than the rule — so it has to outrank the ceiling. Applied in the other order the ceiling silently undoes it: with the floor's own minimum equal to the shipped default, floor-then-ceiling pins every learned pair at exactly the default and `idle_ref × 6` becomes dead code. |
| `release = press / 2`, derived | a release level learned separately landed inside the noise band. Hysteresis now scales with a learned-down pad instead of being lost by it. |

The two constants were measured, and both moved on 2026-08-16 (`FLOOR_MIN` 500 → 700,
`FLOOR_MULT` 3 → 6) on the sonora board after an unrelated noise improvement dropped
`idle_ref` far enough that the *absolute* part of the floor became the binding one and
landed inside the noise band: 30 minutes of quiet produced 21 false presses at 500,
and none at 700. The 700 pair has since held for a **7 h 52 m** unattended soak on
that board — zero `press` and zero `release` lines between the last deliberate tap
and the check, at 5,692,179 scans with 0 rejected and 0 implausible samples
(2026-08-16, three pads at press 700 / release 350, `idle_ref` 67 / 63 / 119). Thirty
minutes was enough to see the broken state; it took hours to earn the claim that the
fixed one is quiet. 700 is the smallest value measured clean, and it still sits under
every deliberate tap on that board (the lightest was 1,085; only the fourth and fifth
tap of a fast six-tap burst ever fell below it). `FLOOR_MULT` 6 rather than 3 because
with a 700 minimum, ×3 would not bind until `idle_ref` passed 233, well above anything
observed; ×6 puts the crossover at 117.

The deliberate cost: a pad can no longer learn its way below 700. That is the point —
the values it would have learned below 700 are the ones that could not be told apart
from idle noise by amplitude. A product that needs a lower pair sets it explicitly
through `nora_touch_set_key_thresholds()`, which outranks learning.
| `release = press / 2`, derived | a release level learned separately landed inside the noise band. Hysteresis now scales with a learned-down pad instead of being lost by it. |

### 3.3 Three presses is a minimum, not a quota

The pair is recomputed on **every** press from the third onwards, over up to eight
samples. A statistic of three is a usable threshold now and one of six is a better
threshold later; there is no reason to make the pad wait for the second in order to
have the first.

Three is then forced by the acceptance criterion (§4): a rule that converges only
at the fifth press cannot meet it, because taps three and four would still be
judged by the shipped 700 — the threshold light taps fall under in the first place.
Three is the largest minimum that has the pad learned before the first tap the
criterion counts.

Deciding on three samples is riskier than on five, and the answer is the limits
rather than an argument: the ceiling means an unlucky sample can only make a pad
*more* sensitive than shipped, the floor keeps it at or above `max(700, idle_ref × 6)`,
and the next press recomputes from four.

### 3.4 Nothing is stored across a power cycle

Deliberately. Persistence would save two or three taps once per boot; in exchange,
what the board does at power-up would depend on what happened before it, and every
future report of "it feels different today" would begin by asking what the flash
remembers. That trade is not worth it at this scale.

If your product needs it anyway, the pieces are all readable through `?kl` and
writable through `*kp`/`*kq` — restore a stored pair at boot from the application
layer, and keep that decision out of the HAL where the argument above still holds.

### 3.5 Reading it from the console

```text
?kl
   key 0  8/3 presses  idle 88  press 537  release 268  learned
   key 1  8/3 presses  idle 98  press 627  release 313  learned
   key 2  8/3 presses  idle 95  press 500  release 250  learned
```

That capture is from **before the 2026-08-16 floor change**, and it is kept because
the shape of the line is what this section is about. The three pairs in it can no
longer occur: every one is below the 700 floor, so the same run today reports
`press 700 / release 350` on all three unless a pad's own `idle_ref` exceeds 117.

`samples/needed` comes before the thresholds on purpose: a pad short of its minimum
is *unfinished*, not insensitive, and that is the difference between "this pad is
broken" and "tap it twice more". `idle` sits beside `press` because the ratio
between them is the margin — and it is the number that differs between two boards.

The last column is the pad's state, and a third value belongs there:
`COLD (strict until the first press)`. See §3.6.

`*kl` forgets the samples **and** restores the configured pair. Restoring matters:
the ceiling is the configured value, so relearning from an already-learned pair
would be a second descent rather than a fresh start. No hands-off window is needed;
this command measures nothing, it only forgets.

### 3.6 The cold gate: stricter until a pad's first event

A pad that has never reported a press has no evidence that a human is near it, and
the learner cannot supply any — learning consumes presses, so before the first one
there is nothing to consume. Until then each pad runs on a second, **stricter** pair:
`cold_press_threshold` (800) and `cold_debounce_scans`, and `?kl` reports it as
`COLD (strict until the first press)` rather than `learned`.

**800, lowered from 900 on 2026-08-30, and the reason is the charge time.** 900 was
set when an untouched pad reached 705 at charge 2,000 ns; at the 5,000 ns default the
idle magnitude is 81–119 and a 2 h 32 m idle soak fired nothing at press 700 /
debounce 2 — a weaker configuration than this gate — while real taps read 994–1408.
900 had ended up inside the bottom of the tap distribution, so it was costing first
presses without buying anything. The gate is still here rather than removed: what it
exists for is the minutes between power-on and the first learned press, and that
window has not been soaked (the soak above began 40 minutes after a power-on).

Why an event and not a quiet interval decides it: idle noise cannot tell you a finger
is present, so no amount of quiet is evidence. One event is trusted to say "a human
is here" — and *only* that. It is deliberately not trusted to set a threshold, since a
single light excursion would then pin the pad low.

The rules around it are all in one direction:

| | |
|---|---|
| both cold values may only make a pad **stricter** | a cold pair below the shipped one, or a cold debounce shorter than `debounce_scans`, is ignored rather than quietly raising sensitivity before anything is known |
| an explicitly pinned pair switches the gate off for that pad | `nora_touch_set_key_thresholds()` is the integrator's number, and it is meant to be the one in force |
| `nora_touch_calibrate()` and `nora_touch_set_acquisition()` re-arm it | a pad whose evidence was just discarded has not, as far as anything here can tell, been touched |
| nothing survives a power cycle | as with the rest of the learning state |

Independent of `learn_presses`: with learning off the shipped pair never moves, and a
pad nobody has touched should still be the stricter of the two.

**When reading a log, `COLD` and "insensitive" look identical** — both are a pad that
failed to report a light touch. That is why the state is reported at all.

---

## 4. Acceptance: the counted-tap run

This is the measurement that says a board is finished, and the reason the `n`
column and `*kz` exist. Without them a run's event total includes every touch since
boot, and taps made before the run started cannot be told from one tap that split
into two events.

**Criterion.** From a default-only power-up: ten taps per pad, **no miss from the
third tap onwards**, and the converged pair within ±20 % of the hand-tuned best for
that board.

1. Silence anything that prints periodically, so the event lines are readable.
2. `*kz` — clears peaks, troughs and press counts.
3. Tap each pad ten times at the pressure you consider normal.
4. `?ko` — `n` must equal the number of taps on every pad. **More** means taps are
   splitting; **fewer** means taps are being missed.
5. `?kl` — the pair each pad settled on.

Reference result on this board, ten taps per pad from a cold boot — **measured before
the 2026-08-16 floor change, and not yet re-run since**:

| | `n` | learned press / release | idle |
|---|---|---|---|
| key 0 `CVDAN1` | 10 | 537 / 268 | 88 |
| key 1 `CVDAN8` | 10 | 627 / 313 | 98 |
| key 2 `CVDAN10` | 10 | 500 / 250 | 95 |

Hand-tuned best on the same board was 500 / 250, so all three pads were inside the
±20 % band, and the run had no misses and no splits.

**The ±20 % clause needs reading with the floor in mind.** All three of those pairs
are below the 700 floor, so the same run now converges to 700 / 350 and no longer
lands within ±20 % of a hand-tuned 500. That is not a regression to chase: the clause
exists to catch a learner that converges somewhere unrelated to the pad, and the floor
is a deliberate veto placed above the hand-tuned value. Compare the value **before**
the floor is applied, and treat 700 / 350 as the expected outcome on a quiet board.
The miss and split counts (`n` = taps, from the third tap onwards) are the parts of
the criterion the floor does not touch, and they remain the pass/fail. A counted-tap
run on this board with the new floor has not been done — the change was measured on
the sonora board.

**If a pad misses taps, do not reach for the thresholds first.** Run `*kv` to arm
the scan-rate trace, tap, and `?kv` to see whether the signal was there at all —
the polled `?ko` view samples a few times a second against ~110 scans a second, so
it can show that a pad exceeded the threshold and still not show whether it did so
on the *consecutive* scans the debounce requires. A threshold moved on a guess is a
threshold that has to be moved again on the next board.

---

## 5. What is deliberately not here

- **Drift over temperature and humidity.** The tracked baseline handles
  hand-warming and slow room drift; it is not a qualified compensation scheme.
- **Wet-finger and water rejection.** Not attempted. A water film reads as a large
  slow magnitude and this layer will call it a press.
- **Frequency hopping / conducted immunity.** The acquisition runs at one fixed
  timing. Noise immunity and guard-electrode design are electrode and stack-up
  work, not threshold work, and none of it is claimed here.
- **Scrollers, sliders, gestures, multi-touch.** Three independent keys only.

Each of these is a place where a product needs work this project does not do — said
explicitly, because a tuning manual that ends without a limits section reads as a
claim of completeness.

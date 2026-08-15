# Open capacitive touch — using and tuning it

The three touch pads of the Curiosity Platform motherboard are read by
`src/hal_touch/`, an open implementation this project owns
outright. For where it came from and what it deliberately does not do, see
[touch-addon.md](touch-addon.md).

Two things are worth knowing before anything else, because both look like faults:

- **A touched pad's signal alternates sign scan to scan.** Detection therefore
  compares a *magnitude* — the mean of `|raw − baseline|` over the last four scans
  — and never the signed delta. Every threshold on this page is in magnitude
  counts. A number carried over from a signed scheme is roughly 3× too large.
- **The first press or two after a cold boot may not register.** Each pad learns
  its own press amplitude from being touched and walks its own threshold *down*
  towards it; a touch light enough to fall under the shipped threshold is counted
  as evidence but not reported as an event. Nothing is stored across a power cycle,
  deliberately: what the board does at boot never depends on what happened before
  it.

## Quick start

Build and flash as usual (`buildtools/build.ps1`, `buildtools/flashauto.ps1`).
On the console at 230400:

```text
 [TOUCH] open ITC touch started on CVDAN 1/8/10 -- press/release printed; try ?ko
```

Touch a pad and an event line appears. To see what detection is working with:

```text
?ko
```

which prints, per key: `raw`, `base`, `delta`, `mag`, `peak`, `trough`, `n`
(press events since the last `*kz`) and the state. `peak` answers "how much signal
does this electrode really give"; `trough` answers "how far does it wander with
nobody there".

Touch is on by default and can be removed with `HAL_STARTER_ENABLE_TOUCH 0` in
`src/app/app_config.h`.

## Commands (module `k`)

Same grammar as the `dspic33ak-audio-dsp-sonora` firmware this was written in:
`<kind><module><name><hex pairs>`, `?` reads and `*` writes, arguments are hex byte
pairs. That is not a coincidence — it is so one tuning document describes both.

| Command | What it does |
|---|---|
| `?ko` | per-key detection view: raw / base / delta / mag / peak / trough / n / state |
| `*kz` | clear peak, trough and the press counts on every key |
| `?kl` | what each pad has learned: idle reference, samples so far, thresholds in force |
| `*kl` | forget it and relearn from the configured pair |
| `*kp<hhll>` | set the press threshold, magnitude counts (`*kp02bc` = 700) |
| `*kq<hhll>` | set the release threshold, magnitude counts |
| `*kv[<hhll>]` | arm the scan-resolution trace (optional trigger); `?kv` dumps it |
| `*kc<hh>` | CVDCAP code, then re-apply the list |
| `*ka<hh>` | accumulation depth 2^n, then re-apply |
| `*kg<hhll>` / `*kb<hhll>` | charge / balance time in ns, then re-apply |
| `?ki` | ITC state; `*ki<cvdan>…` reprograms list 0 from the console |
| `?kr` / `*ks` | one scan with / without printing raw counts |
| `?kd` | raw register dump |
| `*kt<hhll>` / `*ku` | test injection on / off |

Commands that would reprogram the acquisition while the detection layer owns the
list re-apply it *through* that layer, so the console and detection can never end
up as two owners of one peripheral.

## The counted-tap procedure

This is the measurement to run whenever "the pads feel wrong", and the reason the
`n` column and `*kz` exist. Without them a run's event total includes every touch
since boot, and taps made before the run started cannot be told from one tap that
split into two events.

1. `*kz` — clears peaks and press counts.
2. Tap each pad a counted number of times, ten is enough, at the pressure you
   consider normal.
3. `?ko` — `n` should equal the number of taps for every pad. More than that means
   taps are splitting; fewer means taps are being missed.
4. `?kl` — shows what each pad settled on. Pads on one board legitimately differ,
   but a learned pair can never go below `max(700, idle_ref × 6)`, so on a quiet
   board all three read 700/350. (Before the 2026-08-16 floor change the same
   reference board landed around 500/250, 500/250 and 543/271.)

If a pad misses taps, do not reach for the thresholds first: run `*kv` to arm the
trace, tap, and `?kv` to see whether the signal was there at all. A threshold moved
on a guess is a threshold that has to be moved again on the next board.

## Detail

[open-touch-tuning.md](open-touch-tuning.md) is the full tuning manual: the
acquisition sweeps and what they are worth, the measured basis for every default,
the per-pad learning rule clause by clause, the acceptance run, and a limits
section saying what a product still has to do itself.

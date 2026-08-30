# Keeping the touch files in sync with sonora

`src/hal_touch/` and `src/console/touch_console.{c,h}` are maintained in
the `dspic33ak-audio-dsp-sonora` firmware and vendored here. They are kept
**byte-identical apart from the divergences listed below**, on purpose: the tuning
manual is written in these command strings, and a fix found on one board has to be
applicable to the other by copying a file rather than by re-deriving it.

Upstream source paths:

| Here | Upstream |
|---|---|
| `src/hal_touch/nora_touch.{c,h}` | `src/app/hal_touch/` |
| `src/hal_touch/nora_itc_internal.h` | `src/app/hal_touch/` |
| `src/hal_touch/nora_itc_dspic33ak.c` | `src/app/hal_touch/` |
| `src/hal_touch/nora_itc_dspic33ak_reg.h` | `src/app/hal_touch/` |
| `src/console/touch_console.{c,h}` | `src/app/uart_app/touch_console.{c,h}` |
| `src/console/app_console.h` | `src/app/uart_app/app_console.h` (types only) |

**One HAL, one directory.** There used to be a `src/hal_itc/` here as well, holding
the acquisition layer under its own public `nora_itc.h`. Both projects merged it
into `hal_touch/` on 2026-08-14, and the header it published became
`nora_itc_internal.h`: a caller that *can* include the acquisition layer will, and
then the peripheral has two owners and the baseline moves under the one that is
not looking. The ITC is now reachable only through `nora_touch_hw_*()` in
`nora_touch.h`, which is where the bring-up console gets it from.

Upstream's paths gained a `src/app/` level in the same period (its own tree grew a
`boot`/`shared`/`linker` split). That is sonora's layout, not a divergence in these
files -- this starter keeps one flat folder per HAL, and the file contents are what
is kept identical.

## Deliberate divergences

1. **`touch_console.c`'s internal-document citation is stripped here.** Upstream
   marks a citation of its non-public tuning notes as `[internal] <file>`; a reader
   of this repository cannot open that document, so the marker is dropped and the
   file name left as prose. One line, and it is the only difference *inside* a
   vendored file.

   (There used to be an include-path divergence here -- `nora_touch.h` against
   upstream's `hal_touch/nora_touch.h`. There is none: `src` is on this project's
   include path as well as `src/hal_touch`, so upstream's spelling resolves and the
   line is vendored verbatim.)

2. **`app_console_line.c` is local.** sonora parses `<kind><module><name><hex>`
   lines in `app_console.c` and dispatches by module letter; this starter has a
   line-based `strcmp` chain in `fw_command.c`. Rather than translate the command
   names — which would fork the manual — this file reproduces just that parse. Only
   `app_console.h`'s *types* are vendored; the upstream parser is not.
3. **The clock and the electrodes are stated in this project's own board layer**
   (`board_pins.h`, `main.c`), as they are in sonora's — the same rule, different
   values in principle, identical values in fact (CLKGEN6 = 200 MHz on both).

That is the whole list: divergence 1 is the only difference *inside* a vendored
file, and 2 and 3 are files this project owns. Anything else that differs is drift,
not design. `nora_touch.c`'s constants in particular are measured values with their
measurement recorded in the comment beside them; changing one here without changing
it upstream leaves two boards claiming different evidence for the same number.

## The upstream build switch, and how it is satisfied here

`touch_console.{c,h}` asks upstream's question by name:

```c
#if defined(ENA_OPEN_TOUCH_EXCLUSIVE)
```

Upstream that is a device fact — sonora also builds for a `dsPIC33AK128MC106`,
which has no ITC and no ADC 5, so every console verb would print zeros from a
peripheral that is not there. The `#else` branch in `touch_console.h` supplies an
"unknown module" stub, which is also what keeps the module out of that image: the
dispatcher's unconditional `case 'k'` is otherwise the one reference that keeps
`touch_console.c` — and through it `nora_touch.c` — linked.

Rather than delete the guard (which would fork both files), this starter answers
the question from its own switch. `src/app/app_specific_config_defs.h` is a local
adapter, ~10 lines:

```c
#include "app_config.h"     /* HAL_STARTER_ENABLE_TOUCH */

#if HAL_STARTER_ENABLE_TOUCH
#define ENA_OPEN_TOUCH_EXCLUSIVE  1
#endif
```

`ENA_TOUCH_SHIELD_PHASE` is a second upstream switch, and it is deliberately left
undefined here. It makes the ITC drive the shield electrode in phase with each CVD
sample (`CVDTX23` on the upstream board) instead of holding it statically Low. That
is a board fact -- it needs a shield electrode wired to a CVDTX pin -- and this
starter's board layer does not claim one, so the vendored code takes its
statically-shielded path. Nothing has to be defined for that; the file compiles the
`#else` branch.

So there is one switch here and not two, and `HAL_STARTER_ENABLE_TOUCH 0` now does
what `docs/touch-addon.md` has always said it does: the console body compiles out,
the header answers `APP_CONSOLE_ERR_NOT_FOUND`, and `--gc-sections` drops both
files instead of keeping program Flash that nothing can reach.

Measured on the `dsPIC33AK512` configuration, 2026-08-15: 111,128 bytes in the
program region with touch on, 94,844 with it off — **16,284 bytes**, the detection
layer, the backend and the console together. (sonora's own comment quotes 9.0 KiB;
that is its build, counting what its `case 'k'` was holding.)

## Checking

All seven vendored files at once, against a sonora clone (`origin/main`), with the
one citation-marker divergence normalised away:

```sh
python - <<'PY'
import io, subprocess
S = '../dsp-sonora-dev'          # any sonora clone; blobs are read from origin/main
pairs  = [('src/app/hal_touch/' + f, 'src/hal_touch/' + f) for f in (
          'nora_touch.h', 'nora_touch.c', 'nora_itc_internal.h',
          'nora_itc_dspic33ak.c', 'nora_itc_dspic33ak_reg.h')]
pairs += [('src/app/uart_app/' + f, 'src/console/' + f) for f in (
          'touch_console.c', 'touch_console.h')]
for up, here in pairs:
    a = subprocess.run(['git', '-C', S, 'show', 'origin/main:' + up],
                       capture_output=True, check=True).stdout.decode('utf-8')
    a = a.replace('\r\n', '\n').replace('[internal] ', '')
    b = io.open(here, encoding='utf-8', newline='').read().replace('\r\n', '\n')
    print(('identical' if a == b else 'DIVERGED '), here)
PY
```

Last run: **7 of 7 identical** against sonora `cf56f58` (2026-08-30) -- the
`touch/cold-gate-800` branch (PR #11), which is `origin/main` = `5464222` plus the
cold gate lowered from 900 to 800. Everything else in these files is on `origin/main`;
substitute it in the snippet above once that PR has landed.

Before that, sonora `main` = `2653def` (2026-08-15). The two commits that made the
published comments self-contained (`ab970a0`, `2653def`) were developed on a branch,
rebased onto `main` = `40baee2`, and landed there; they changed comments only, so the
vendored bytes here did not move.

Those comments no longer cite anything a reader of this repository cannot open:
no `docs_internal/` paths, and no section numbers from the *internal* tuning
manual. References to the tuning manual itself stay, and resolve to
[open-touch-tuning.md](open-touch-tuning.md).

The `\r\n` normalisation is not cosmetic: this starter's files are CRLF and some of
the upstream files are LF, so a byte comparison without it always reports a
difference and tells you nothing.

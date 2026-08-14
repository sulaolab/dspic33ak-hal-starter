# Keeping the touch files in sync with sonora

`src/hal_touch/` and `src/console/touch_console.c` are maintained in
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

1. **`touch_console.c` include path** — `#include "nora_touch.h"` here,
   `#include "hal_touch/nora_touch.h"` upstream. This starter puts every HAL folder
   on the include path; sonora includes through the folder name.
2. **`app_console_line.c` is local.** sonora parses `<kind><module><name><hex>`
   lines in `app_console.c` and dispatches by module letter; this starter has a
   line-based `strcmp` chain in `fw_command.c`. Rather than translate the command
   names — which would fork the manual — this file reproduces just that parse. Only
   `app_console.h`'s *types* are vendored; the upstream parser is not.
3. **The clock and the electrodes are stated in this project's own board layer**
   (`board_pins.h`, `main.c`), as they are in sonora's — the same rule, different
   values in principle, identical values in fact (CLKGEN6 = 200 MHz on both).

Anything else that differs is drift, not design. `nora_touch.c`'s constants in
particular are measured values with their measurement recorded in the comment
beside them; changing one here without changing it upstream leaves two boards
claiming different evidence for the same number.

## Checking

```sh
python - <<'PY'
a = open('../sonora/src/app/uart_app/touch_console.c','rb').read().replace(b'\r\n', b'\n')
b = open('src/console/touch_console.c','rb').read().replace(b'\r\n', b'\n')
a = a.replace(b'hal_touch/nora_touch.h', b'nora_touch.h')
print('identical' if a == b else 'DIVERGED')
PY
```

The `\r\n` normalisation is not cosmetic: this starter's files are CRLF and some of
the upstream files are LF, so a byte comparison without it always reports a
difference and tells you nothing.

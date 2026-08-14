# Keeping the touch files in sync with sonora

`src/hal_itc/`, `src/hal_touch/` and `src/console/itc_console.c` are maintained in
the `dspic33ak-audio-dsp-sonora` firmware and vendored here. They are kept
**byte-identical apart from the divergences listed below**, on purpose: the tuning
manual is written in these command strings, and a fix found on one board has to be
applicable to the other by copying a file rather than by re-deriving it.

Upstream source paths:

| Here | Upstream |
|---|---|
| `src/hal_itc/nora_itc*.{c,h}` | `src/hal_itc/` |
| `src/hal_touch/nora_touch.{c,h}` | `src/hal_touch/` |
| `src/console/itc_console.{c,h}` | `src/uart_app/itc_console.{c,h}` |
| `src/console/app_console.h` | `src/uart_app/app_console.h` (types only) |

## Deliberate divergences

1. **`itc_console.c` include path** — `#include "nora_touch.h"` here,
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
a = open('../sonora/src/uart_app/itc_console.c','rb').read().replace(b'\r\n', b'\n')
b = open('src/console/itc_console.c','rb').read().replace(b'\r\n', b'\n')
a = a.replace(b'hal_touch/nora_touch.h', b'nora_touch.h')
print('identical' if a == b else 'DIVERGED')
PY
```

The `\r\n` normalisation is not cosmetic: this starter's files are CRLF and some of
the upstream files are LF, so a byte comparison without it always reports a
difference and tells you nothing.

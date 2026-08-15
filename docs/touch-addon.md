# Capacitive touch

This starter **includes** capacitive touch, as an open implementation written from
the family reference manual and bench measurement. It is not the QTouch library and
does not depend on it.

That is a change from earlier revisions of this page, which said the starter
deliberately shipped no touch support. The reason it gave was correct at the time
and is still correct about the library it was talking about: Microchip's touch
support for this board is tool-generated code plus prebuilt QTouch Modular Library
objects (`qtm_*.X.a`), the demo's own notes call it pre-release and
not-for-production, and its terms do not fit an MIT-0 starter. None of that says
anything about *touch*; it says something about *that library*. So the touch pads
are supported here by code this project owns outright.

- **What it is:** `src/hal_touch/`, in two layers behind one header: the
  Integrated Touch Controller driver (pins, timing, accumulation, raw counts) and
  the detection above it (baseline tracking, hysteresis and debounce, and per-pad
  threshold learning).
- **Where it came from:** written for the `dspic33ak-audio-dsp-sonora` firmware
  from DS70005591 ch.18 (ITC), the DFP SFR header, and measurements on this same
  board. No vendor touch-library source, header or binary was consulted, and no
  vendor detection algorithm was inspected. The file headers state this per file.
- **What it does not do:** drift compensation over temperature and humidity,
  wet-finger rejection, frequency hopping, scrollers. Those are the integrator's,
  and the header says so rather than implying coverage it does not have.

Usage, the console commands, and the tuning procedure are in
[open-touch.md](open-touch.md); the full tuning manual is
[open-touch-tuning.md](open-touch-tuning.md).

## Official reference source (for the board, not for the code)

Microchip's **Out of Box Demo** ("OOB") for the dsPIC33A Curiosity Platform
Development Board remains the reference for what the board can do, and is worth
having open when comparing behaviour:

- EV74H48A Curiosity Platform Development Board:
  https://www.microchip.com/en-us/development-tool/EV74H48A
- Microchip dsPIC33A Curiosity OOB demo repository:
  https://github.com/microchip-pic-avr-examples/dspic33a-curiosity-oob
- dsPIC33AK512MPS512 DIM OOB demo:
  https://github.com/microchip-pic-avr-examples/dspic33a-curiosity-oob/tree/main/dspic33ak512mps512_dim

Its touch files live under `dspic33ak512mps512_dim/dspic33ak512mps512_dim.X/mcc_generated_files/touch/`
(`touch.c`, `qtm_*.h`, `lib/qtm_*.X.a`, `datastreamer/`).

**Those files were not used to write this, and should not be used to modify it.**
Comparing the two firmwares' behaviour from the console — does a light tap
register, how many taps in ten are missed — is legitimate and is how this
implementation was scored. Reading the vendor headers or disassembling the
prebuilt objects is not: it would put provenance the starter cannot license into
a repository that is MIT-0 precisely so anyone can take it.

## Removing it

Set `HAL_STARTER_ENABLE_TOUCH` to 0 in `src/app/app_config.h`. The touch code
drops out of the image and the three CVDAN inputs are free — measured on the
`dsPIC33AK512` configuration: 111,128 bytes of program region with touch on,
94,844 with it off.

That one switch reaches the bring-up console too, through
`src/app/app_specific_config_defs.h`: the console is vendored from sonora and asks
its own project's question (`ENA_OPEN_TOUCH_EXCLUSIVE`), which that file answers
from `HAL_STARTER_ENABLE_TOUCH`. Without it the dispatcher's `case 'k'` would keep
`touch_console.c` — and through it `nora_touch.c` — linked in a build that cannot
use either. See [open-touch-sync.md](open-touch-sync.md).

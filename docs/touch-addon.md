# Capacitive touch add-on

This starter intentionally does **not** include the capacitive-touch library.

The Curiosity Platform Development Board has three on-board capacitive touch pads,
but the touch implementation used by Microchip's demo is generated code plus
prebuilt QTouch Modular Library objects. Those files are not part of this MIT-0
starter.

## Official reference source

For reference, Microchip provides an official **Out of Box Demo** repository for
the dsPIC33A Curiosity Platform Development Board. In this page, **OOB** means
**Out of Box Demo**.

- EV74H48A Curiosity Platform Development Board:
  https://www.microchip.com/en-us/development-tool/EV74H48A
- Microchip dsPIC33A Curiosity OOB demo repository:
  https://github.com/microchip-pic-avr-examples/dspic33a-curiosity-oob
- dsPIC33AK512MPS512 DIM OOB demo:
  https://github.com/microchip-pic-avr-examples/dspic33a-curiosity-oob/tree/main/dspic33ak512mps512_dim

In that OOB demo, the touch-related generated files are located under:

```text
dspic33ak512mps512_dim/
  dspic33ak512mps512_dim.X/
    mcc_generated_files/
      touch/
```

The relevant files include:

```text
mcc_generated_files/touch/touch.h
mcc_generated_files/touch/src/touch.c
mcc_generated_files/touch/include/qtm_*.h
mcc_generated_files/touch/datastreamer/...
mcc_generated_files/touch/lib/qtm_*.X.a
```

## Why this starter does not include it

The Microchip OOB demo notes that the touch library in
`mcc_generated_files/touch` contains pre-release code intended solely for
demonstration purposes and is not intended for production use.

For that reason, this starter keeps capacitive touch as an optional add-on
instead of vendoring the generated touch library and prebuilt QTouch objects
directly.

Do not vendor or copy the touch library into this MIT-0 starter.

## How to experiment with touch

If you want to experiment with the three on-board touch pads:

1. Open the Microchip OOB demo for `dspic33ak512mps512_dim`.
2. Inspect the generated touch files under `mcc_generated_files/touch/`.
3. Use the OOB demo as the reference for touch pad setup, QTouch library linkage,
   and optional Data Visualizer streaming.
4. Keep the imported touch code clearly separated from this starter's MIT-0 HAL
   examples.

This starter focuses on small, readable HAL bring-up code. Capacitive touch is
left as a documented integration path because the official demo depends on
generated files and Microchip-provided QTouch library objects.

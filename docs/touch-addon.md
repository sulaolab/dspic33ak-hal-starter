# Capacitive touch add-on (optional)

The Curiosity motherboard has capacitive-touch pads. This starter does **not**
include touch, on purpose:

* Touch on dsPIC33AK is driven by Microchip's **QTM** (QTouch Modular) libraries.
  Those are **proprietary, pre-built `.a` libraries plus generated headers** —
  they are governed by Microchip's license, **not MIT-0**, and cannot be shipped
  in this repository.
* The acquisition/tuning configuration is produced by a code-generation tool
  (MPLAB Code Configurator / QTouch Configurator), which is exactly the kind of
  tooling this minimal, hand-written starter avoids depending on.

If you want touch, you generate the QTM pieces yourself and wire them into the
running starter. The rest of this page is a pointer, not a drop-in.

## What you need (generate yourself)

Using MPLAB Code Configurator / QTouch Configurator for the dsPIC33AK512MPS512 on
this board, generate and add to your own copy of the project:

* the QTM acquisition + library archives (`qtm_*.X.a`) and their API headers
  (`qtm_acq_*`, `qtm_touch_key_*`, ...),
* the generated node configuration (the per-pad `NODE_*_PARAMS`, channel count,
  acquisition settings), tuned for this board's pads.

Add the `.a` files to the project's linker library list and the generated `.c`/
`.h` to the source tree. These files remain under Microchip's license.

## Where it plugs into this starter

The clock side is already done: `dspic33ak_clock_init()` routes **CLKGEN6 -> ADC**
(PLL1), which is the acquisition clock QTM uses. So you only need to:

1. Call the generated `touch_init()` once, after `dspic33ak_clock_init()` (and
   after `board_ports_digital_default()` — note that touch pads must be left in
   the analog/CVD state QTM expects, so do not force those pins digital).
2. Drive the QTM acquisition/processing from the main loop, e.g. once per tick:

   ```c
   /* in the main while(1), alongside rgb_pot_update(): */
   touch_process(systick_ms());      /* name/signature per your generated code */
   if (touch_key_pressed(0)) {
       printf(" touch: key 0 pressed\n");
   }
   ```

3. Map a touch event to something visible — e.g. nudge the RGB color or print a
   line — so you can confirm it works.

## License note

Anything you generate from QTM / QTouch Configurator is licensed by Microchip.
Keep it separate from this MIT-0 project and do not redistribute the QTM
libraries under MIT-0.

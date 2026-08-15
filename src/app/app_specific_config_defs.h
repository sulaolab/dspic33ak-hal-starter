#ifndef APP_SPECIFIC_CONFIG_DEFS_H
#define APP_SPECIFIC_CONFIG_DEFS_H

/*
 * app_specific_config_defs.h
 * --------------------------
 * Build-specific switches that vendored code asks about by name.
 *
 * src/console/touch_console.{c,h} is vendored from the sonora audio-board
 * project byte-for-byte (the only local change is the flat include path,
 * "nora_touch.h" instead of "hal_touch/nora_touch.h"), and it asks whether this
 * build has touch at all:
 *
 *     #if defined(ENA_OPEN_TOUCH_EXCLUSIVE)
 *
 * There the answer is a device fact -- that project also builds for a part with
 * no ITC and no ADC 5 (dsPIC33AK128MC106), where every console verb would print
 * zeros from a peripheral that is not there. Here it is this starter's own
 * HAL_STARTER_ENABLE_TOUCH switch, so there is one switch and not two: the file
 * below is the adapter between the upstream name and the local one.
 *
 * It is also what makes "drop the touch code from the image" true.
 * touch_console.c is compiled in every configuration, so the unconditional
 * case 'k' in app_console_line.c is a reference that keeps touch_console.c --
 * and through it nora_touch.c -- linked even with the demo switched off.
 * Without the switch reaching the console, HAL_STARTER_ENABLE_TOUCH 0 would
 * free the three CVDAN inputs but keep ~9 KiB of program Flash. With it, the
 * header supplies the "unknown module" stub, the body compiles out, and
 * --gc-sections drops both files.
 *
 * Not to be confused with app_build_config.h, which selects one APP_BUILD demo
 * variation. This header states build facts that vendored code reads.
 */

#include "app_config.h"     /* HAL_STARTER_ENABLE_TOUCH */

#if HAL_STARTER_ENABLE_TOUCH
#define ENA_OPEN_TOUCH_EXCLUSIVE  1
#endif

#endif /* APP_SPECIFIC_CONFIG_DEFS_H */

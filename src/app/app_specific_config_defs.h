#ifndef APP_SPECIFIC_CONFIG_DEFS_H
#define APP_SPECIFIC_CONFIG_DEFS_H

/*
 * app_specific_config_defs.h
 * --------------------------
 * Build-specific switches used by the touch console.
 *
 * src/console/touch_console.{c,h} asks whether this build has touch at all:
 *
 *     #if defined(ENA_OPEN_TOUCH_EXCLUSIVE)
 *
 * HAL_STARTER_ENABLE_TOUCH is this starter's build switch. It maps to the
 * console's ENA_OPEN_TOUCH_EXCLUSIVE name so the touch console and HAL are both
 * omitted when capacitive touch is disabled.
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
 * variation. This header states build facts used by the touch console.
 */

#include "app_config.h"     /* HAL_STARTER_ENABLE_TOUCH */

#if HAL_STARTER_ENABLE_TOUCH
#define ENA_OPEN_TOUCH_EXCLUSIVE  1
#endif

#endif /* APP_SPECIFIC_CONFIG_DEFS_H */

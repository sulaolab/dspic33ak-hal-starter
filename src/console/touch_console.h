#ifndef SONORA_TOUCH_CONSOLE_H
#define SONORA_TOUCH_CONSOLE_H

/* Provenance: written from DS70005591 ch.18 (ITC) and the DFP SFR header only.
 * No vendor touch-library source, header or binary was consulted.
 * See docs_internal/shared/open_touch/provenance_rules.md.
 */

#include "app_console.h"

/* Open capacitive-touch bring-up console (module 'k'). Raw ITC counts and
 * nothing above them: no baseline, no threshold and no key decision, all of which
 * belong to nora_touch (src/hal_touch) and are read here rather than computed.
 * Every number this prints is what the hardware produced, which is what the
 * tuning manual's procedures need.
 *
 *   *k i <cvdan> [<cvdan> ...]  configure list 0, one record per CVDANx number
 *                               given as payload bytes (no pin list is baked
 *                               in: they come from the board schematic)
 *   *k s                        one software-triggered scan
 *   ?k r                        scan and print the raw signed counts
 *   ?k i                        list state: DRDY/BUSY, converted timer counts,
 *                               CVDCAP code, accumulation depth, scan count
 *   *k t <hi> <lo>              enable test injection with that 16-bit value
 *   *k u                        disable test injection
 *   ?k d                        raw register dump (ITC + the ADC 5 registers it
 *                               depends on). Note it reads ITCRESx, which clears
 *                               ACCDONE — dumping between a scan and ?kr changes
 *                               the answer.
 *   *k c <code>                 set the CVDCAP code and re-apply the list
 *   *k a <n>                    set the accumulation depth (2^n) and re-apply
 *   ?k o                        detection layer (nora_touch) per-key view:
 *                               raw / baseline / delta / peak / trough /
 *                               pressed, plus the scan and rejected-scan counts
 *                               and the thresholds in force. Only present in an
 *                               ENA_OPEN_TOUCH_EXCLUSIVE build; note that *ki
 *                               reprograms the list it is scanning.
 *   *k z                        clear peak/trough on every key. peak/trough are
 *                               tracked whatever the threshold is, which is the
 *                               only way to see a touch that failed to reach it:
 *                               a miss prints no event line at all.
 * *   ?k l                        per-pad calibration: each pad's measured noise
 *                               tail and the thresholds derived from it, or
 *                               PINNED where the integrator set the pair by hand.
 *                               Two boards' individual characteristics are this
 *                               table, side by side.
 *   *k l                        measure it again. Hands off the pads for ~1 s: a
 *                               window with a finger in it is discarded, and
 *                               enough discards in a row keep the old values.
   *k p <hi> <lo>              press threshold, in counts
 *   *k q <hi> <lo>              release threshold, in counts (must stay below
 *                               press: equal values remove the hysteresis and
 *                               the key chatters). Either command clears the
 *                               peaks, since a peak belongs to one setting.
 *   *k g <hi> <lo>              set the charge time in ns and re-apply
 *   *k b <hi> <lo>              set the balance time in ns and re-apply
 *                               (both 16-bit ns; TMRA/TMRB are 8-bit TAD, so a
 *                               value that does not fit is refused with
 *                               "a requested time does not fit its timer" and
 *                               the previous value is put back)
 */
void touch_console_onmsg( app_console_msg_t* msg );

#endif /* SONORA_TOUCH_CONSOLE_H */

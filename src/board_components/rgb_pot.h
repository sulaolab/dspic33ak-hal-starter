#ifndef RGB_POT_H
#define RGB_POT_H

/*
 * rgb_pot.h
 * ---------
 * Board component helper (NOT a HAL): original AK512 RGB/POT demo.
 *
 * On AK256MPS306 this translation unit currently builds as a no-op stub because
 * the pot moved to AD1AN2 / RA3 and needs a fresh board path.
 *
 * Pin routing (PPS + LED GPIO) is owned by board_rgb_pins_init() when the demo
 * is enabled on a supported board.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Set up the pot ADC and RGB PWM on supported boards. No-op on AK256MPS306
 * until the AD1AN2 path is implemented. */
void rgb_pot_init(void);

/* Read the pot (0..4095) and update the RGB LED color from it. */
void rgb_pot_update(void);

#ifdef __cplusplus
}
#endif

#endif /* RGB_POT_H */

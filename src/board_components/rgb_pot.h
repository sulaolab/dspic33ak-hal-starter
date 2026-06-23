#ifndef RGB_POT_H
#define RGB_POT_H

/*
 * rgb_pot.h
 * ---------
 * Board component helper (NOT a HAL): read the on-board potentiometer with ADC5
 * and drive the RGB LED color with PWM1/2/3. Minimal, hand-written register
 * setup, just enough to be a lively demo. A real project would wrap ADC/PWM in
 * proper drivers.
 *
 * Pin routing (PPS + LED GPIO) is owned by board_rgb_pins_init(); the pot analog
 * pin (RA7 / AD5AN0) ANSEL is set here.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Set up the pot ADC (ADC5 ch0) and the RGB PWM (PG1/2/3). Calls
 * board_rgb_pins_init() for the LED pins/PPS. Call once after the clock is up. */
void rgb_pot_init(void);

/* Read the pot (0..4095) and update the RGB LED color from it. */
void rgb_pot_update(void);

#ifdef __cplusplus
}
#endif

#endif /* RGB_POT_H */

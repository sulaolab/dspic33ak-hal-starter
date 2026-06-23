#ifndef LED_SW_H
#define LED_SW_H

/*
 * led_sw.h
 * --------
 * Tiny GPIO sample for the Curiosity motherboard's 8 user LEDs and 3 user
 * switches, built on dspic33ak_gpio (the GPIO HAL). This is example code, not
 * a HAL.
 *
 * Board wiring (EV74H48A + EV80L65A, dsPIC33AK512MPS512 DIM):
 *   LED0..LED7  = RC8..RC15  (active-high: drive high to light)
 *   SW1 = RF3, SW2 = RF0, SW3 = RB2  (active-low: reads 0 while pressed)
 *
 * Note the indexing trap: LEDs are 0-based (LED0..LED7, matching the board
 * silkscreen) while switches are 1-based (SW1..SW3).
 *
 * Note RF0 is also an analog input (AD3AN4), so this sample clears ANSEL on
 * every pin it owns; it does not rely on board_ports_digital_default(), which
 * only covers ANSELA..ANSELD.
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LED_SW_LED_COUNT  8u   /* LED0..LED7 */
#define LED_SW_SW_COUNT   3u   /* SW1..SW3   */

/* Configure the 8 LED pins as outputs (all off) and the 3 switch pins as
 * inputs with pull-ups. Call once after the clock is up. */
void led_sw_init(void);

/* Drive one LED. led is 0..LED_SW_LED_COUNT-1 (LED0..LED7); out-of-range
 * calls are ignored. */
void led_sw_set(uint8_t led, bool on);

/* Drive all 8 LEDs at once. */
void led_sw_all(bool on);

/* Toggle one LED. led is 0..LED_SW_LED_COUNT-1 (LED0..LED7); out-of-range
 * calls are ignored. */
void led_sw_toggle(uint8_t led);

/* True while the given switch is pressed. sw is 1..LED_SW_SW_COUNT. */
bool led_sw_pressed(uint8_t sw);

/* Power-on indicator: light all LEDs, hold for the given time, then clear.
 * Blocking; uses the Timer1 tick millisecond time base. */
void led_sw_boot_test(uint32_t hold_ms);

/* Map the switches to LEDs. SW1 and SW2 are polled and drive LED7/LED6.
 * SW3 is handled by a CN GPIO event and drives LED5. Call repeatedly from the
 * main loop so ISR state is reflected on the LEDs outside the callback. */
void led_sw_update(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_SW_H */

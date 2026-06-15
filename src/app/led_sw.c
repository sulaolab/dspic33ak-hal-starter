/*
 * led_sw.c
 * --------
 * 8 user LEDs + 3 user switches sample, built on the GPIO HAL. See led_sw.h.
 *
 * Board wiring (dsPIC33AK512MPS512 DIM on the Curiosity motherboard):
 *   LED1..LED8 = RC8..RC15  (active-high)
 *   SW1 = RF3, SW2 = RF0, SW3 = RB2  (active-low, pulled up)
 */

#include <stdio.h>

#include "led_sw.h"
#include "dspic33ak_gpio.h"
#include "systick.h"

/* LED0..LED7 -> RC8..RC15, lit when driven high. LEDs are 0-indexed to match
 * the board silkscreen (LED0..LED7); the switches below are 1-indexed (SW1..3). */
static const dspic33ak_gpio_pin_t LED_PINS[LED_SW_LED_COUNT] = {
    DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C,  8),   /* LED0 */
    DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C,  9),   /* LED1 */
    DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C, 10),   /* LED2 */
    DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C, 11),   /* LED3 */
    DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C, 12),   /* LED4 */
    DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C, 13),   /* LED5 */
    DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C, 14),   /* LED6 */
    DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_C, 15),   /* LED7 */
};

/* SW1..SW3, active-low (reads 0 while pressed). */
static const dspic33ak_gpio_pin_t SW_PINS[LED_SW_SW_COUNT] = {
    DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_F, 3),    /* SW1 */
    DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_F, 0),    /* SW2 (RF0 = AD3AN4) */
    DSPIC33AK_GPIO_PIN(DSPIC33AK_GPIO_PORT_B, 2),    /* SW3 */
};

void led_sw_init(void)
{
    uint8_t i;

    for (i = 0u; i < LED_SW_LED_COUNT; i++) {
        dspic33ak_gpio_set_analog(LED_PINS[i], false);
        dspic33ak_gpio_set_direction(LED_PINS[i], DSPIC33AK_GPIO_DIR_OUTPUT);
        dspic33ak_gpio_write(LED_PINS[i], false);    /* off */
    }

    for (i = 0u; i < LED_SW_SW_COUNT; i++) {
        dspic33ak_gpio_set_analog(SW_PINS[i], false);
        dspic33ak_gpio_set_direction(SW_PINS[i], DSPIC33AK_GPIO_DIR_INPUT);
        dspic33ak_gpio_set_pull(SW_PINS[i], DSPIC33AK_GPIO_PULL_UP);
    }
}

void led_sw_set(uint8_t led, bool on)
{
    if (led < LED_SW_LED_COUNT) {
        dspic33ak_gpio_write(LED_PINS[led], on);
    }
}

void led_sw_all(bool on)
{
    uint8_t i;
    for (i = 0u; i < LED_SW_LED_COUNT; i++) {
        dspic33ak_gpio_write(LED_PINS[i], on);
    }
}

bool led_sw_pressed(uint8_t sw)
{
    if (sw >= 1u && sw <= LED_SW_SW_COUNT) {
        /* Active-low: a pressed switch pulls the pin to 0. */
        return (dspic33ak_gpio_read(SW_PINS[sw - 1u]) == false);
    }
    return false;
}

void led_sw_boot_test(uint32_t hold_ms)
{
    uint32_t start;

    led_sw_all(true);

    start = systick_ms();
    while ((uint32_t)(systick_ms() - start) < hold_ms) {
        /* busy-wait on the systick time base */
    }

    led_sw_all(false);
}

void led_sw_update(void)
{
    /* SW1->LED7, SW2->LED6, SW3->LED5, each lit only while its switch is held. */
    static const uint8_t SW_TO_LED[LED_SW_SW_COUNT] = { 7u, 6u, 5u };
    uint8_t sw;

    for (sw = 1u; sw <= LED_SW_SW_COUNT; sw++) {
        led_sw_set(SW_TO_LED[sw - 1u], led_sw_pressed(sw));
    }
}

/*
 * led_sw.c
 * --------
 * 8 user LEDs + 3 user switches sample, built on the GPIO HAL. See led_sw.h.
 *
 * Board wiring (dsPIC33AK512MPS512 DIM on the Curiosity motherboard):
 *   LED1..LED8 = RC8..RC15  (active-high)
 *   SW1 = RF3, SW2 = RF0, SW3 = RB2  (active-low, pulled up)
 */

#include <xc.h>
#include <stdio.h>

#include "led_sw.h"
#include "dspic33ak_gpio.h"
#include "dspic33ak_gpio_event.h"
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

#define LED_SW_SW3_INDEX      2u
#define LED_SW_SW3_NUMBER     3u
#define LED_SW_SW3_EVENT_LED  5u

static volatile bool s_sw3_pressed;

static void led_sw_log_transition(uint8_t sw, bool pressed)
{
    if (sw == LED_SW_SW3_NUMBER) {
        printf(" SW3 CN ISR event: %s\n", pressed ? "falling press" : "rising release");
    } else {
        printf(" SW%u %s\n", (unsigned)sw, pressed ? "press" : "release");
    }
}

static void led_sw_sw3_event_callback(dspic33ak_gpio_pin_t pin,
                                      dspic33ak_gpio_event_edge_t edge,
                                      void *user_data)
{
    (void)pin;
    (void)user_data;

    if (edge == DSPIC33AK_GPIO_EVENT_EDGE_FALLING) {
        s_sw3_pressed = true;     /* active-low press */
    } else if (edge == DSPIC33AK_GPIO_EVENT_EDGE_RISING) {
        s_sw3_pressed = false;    /* active-low release */
    }
}

void __attribute__((__interrupt__, __no_auto_psv__)) _CNBInterrupt(void)
{
    /* The event layer clears CNFB for owned pins and the CNB interrupt flag. */
    dspic33ak_gpio_event_process_isr();
}

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

    s_sw3_pressed = led_sw_pressed(LED_SW_SW3_NUMBER);
    (void)dspic33ak_gpio_event_attach(SW_PINS[LED_SW_SW3_INDEX],
                                      DSPIC33AK_GPIO_EVENT_EDGE_EITHER,
                                      led_sw_sw3_event_callback,
                                      0);

    _CNBIP = 4u;
    _CNBIF = 0u;
    _CNBIE = 1u;
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

void led_sw_toggle(uint8_t led)
{
    if (led < LED_SW_LED_COUNT) {
        dspic33ak_gpio_toggle(LED_PINS[led]);
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
    static bool initialized;
    static bool previous_pressed[LED_SW_SW_COUNT];
    bool sw1_pressed = led_sw_pressed(1u);
    bool sw2_pressed = led_sw_pressed(2u);
    bool sw3_pressed = s_sw3_pressed;

    led_sw_set(7u, sw1_pressed);
    led_sw_set(6u, sw2_pressed);
    led_sw_set(LED_SW_SW3_EVENT_LED, sw3_pressed);

    if (!initialized) {
        previous_pressed[0] = sw1_pressed;
        previous_pressed[1] = sw2_pressed;
        previous_pressed[2] = sw3_pressed;
        initialized = true;
        return;
    }

    if (sw1_pressed != previous_pressed[0]) {
        led_sw_log_transition(1u, sw1_pressed);
        previous_pressed[0] = sw1_pressed;
    }
    if (sw2_pressed != previous_pressed[1]) {
        led_sw_log_transition(2u, sw2_pressed);
        previous_pressed[1] = sw2_pressed;
    }
    if (sw3_pressed != previous_pressed[2]) {
        led_sw_log_transition(3u, sw3_pressed);
        previous_pressed[2] = sw3_pressed;
    }
}

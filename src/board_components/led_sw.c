/*
 * led_sw.c
 * --------
 * Board component helper for the 8 user LEDs + 3 user switches, built on the
 * GPIO HAL. See led_sw.h.
 *
 * Board wiring (dsPIC33AK512MPS512 DIM on the Curiosity motherboard):
 *   LED0..LED7 = RC8..RC15  (active-high)
 *   SW1 = RF3, SW2 = RF0, SW3 = RB2  (active-low, pulled up)
 */

#include <xc.h>
#include <stdio.h>

#include "led_sw.h"
#include "board_pins.h"
#include "dspic33ak_gpio.h"
#include "dspic33ak_gpio_event.h"
#include "dspic33ak_tick_timer.h"

/* LED0..LED7 -> RC8..RC15, lit when driven high. LEDs are 0-indexed to match
 * the board silkscreen (LED0..LED7); the switches below are 1-indexed (SW1..3). */
static const dspic33ak_gpio_pin_t LED_PINS[LED_SW_LED_COUNT] = {
    BOARD_LED0_PIN,
    BOARD_LED1_PIN,
    BOARD_LED2_PIN,
    BOARD_LED3_PIN,
    BOARD_LED4_PIN,
    BOARD_LED5_PIN,
    BOARD_LED6_PIN,
    BOARD_LED7_PIN,
};

/* SW1..SW3, active-low (reads 0 while pressed). */
static const dspic33ak_gpio_rp_t SW_RPS[LED_SW_SW_COUNT] = {
    BOARD_SW1_RP,
    BOARD_SW2_RP,
    BOARD_SW3_RP,
};

#define LED_SW_SW3_INDEX      2u
#define LED_SW_SW3_NUMBER     3u
#define LED_SW_SW3_EVENT_LED  5u

#if !defined(LED_SW_DEBOUNCE_MS)
/* Mechanical-switch contact settle time: a raw level must hold this long before
 * led_sw_update() accepts it, so contact bounce on a quick/sloppy press no
 * longer produces spurious transition logs and LED flicker. Overridable. */
#define LED_SW_DEBOUNCE_MS    (15u)
#endif

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
    /* The event layer clears owned per-pin flags and the CNB interrupt flag. */
    dspic33ak_gpio_event_process_isr();
}

void led_sw_init(void)
{
    uint8_t i;

    /* LEDs: glitch-aware one-call config -- seeds LAT low (LED off) before
     * enabling the output driver, so no LED flashes during init. */
    for (i = 0u; i < LED_SW_LED_COUNT; i++) {
        (void)dspic33ak_gpio_config_digital_output(LED_PINS[i], false);
    }

    /* Switches: active-low with board pull-up. Use full config struct to
     * set all attributes in one call. */
    static const dspic33ak_gpio_config_t sw_cfg = {
        .dir = DSPIC33AK_GPIO_DIR_INPUT, .pull = DSPIC33AK_GPIO_PULL_UP,
        .analog = false, .open_drain = false, .initial_high = false,
    };
    for (i = 0u; i < LED_SW_SW_COUNT; i++) {
        (void)dspic33ak_gpio_rp_config(SW_RPS[i], &sw_cfg);
    }

    s_sw3_pressed = led_sw_pressed(LED_SW_SW3_NUMBER);
    (void)dspic33ak_gpio_event_rp_attach(SW_RPS[LED_SW_SW3_INDEX],
                                         DSPIC33AK_GPIO_EVENT_EDGE_EITHER,
                                         led_sw_sw3_event_callback,
                                         0);
    (void)dspic33ak_gpio_event_rp_irq_enable(SW_RPS[LED_SW_SW3_INDEX], 4u);
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
        /* Active-low: a pressed switch pulls the pin to 0. read() returns a
         * 3-state level; treat only a genuine LOW as pressed (ERROR -> not). */
        return (dspic33ak_gpio_rp_read(SW_RPS[sw - 1u]) == DSPIC33AK_GPIO_LEVEL_LOW);
    }
    return false;
}

void led_sw_boot_test(uint32_t hold_ms)
{
    uint32_t start;

    led_sw_all(true);

    start = dspic33ak_tick_timer_get_ms();
    while ((uint32_t)(dspic33ak_tick_timer_get_ms() - start) < hold_ms) {
        /* busy-wait on the Timer1 tick time base */
    }

    led_sw_all(false);
}

/* Per-switch stable-confirm debounce. Returns the confirmed (debounced) level
 * for switch index idx (0..LED_SW_SW_COUNT-1): the raw level is only accepted
 * once it has held steady for LED_SW_DEBOUNCE_MS, so bounce shorter than that
 * window is filtered out. Both the polled (SW1/2) and CN-event (SW3) inputs are
 * passed through this, so all three switches behave identically downstream. */
static bool led_sw_debounce(uint8_t idx, bool raw)
{
    static bool     confirmed[LED_SW_SW_COUNT];      /* debounced level         */
    static bool     candidate[LED_SW_SW_COUNT];      /* last raw level seen     */
    static uint32_t candidate_ms[LED_SW_SW_COUNT];   /* when candidate appeared */

    uint32_t now = dspic33ak_tick_timer_get_ms();

    if (raw != candidate[idx]) {
        candidate[idx]    = raw;
        candidate_ms[idx] = now;        /* raw changed -> restart settle window */
    } else if ((uint32_t)(now - candidate_ms[idx]) >= LED_SW_DEBOUNCE_MS) {
        confirmed[idx]    = raw;        /* stable long enough -> accept         */
    }
    return confirmed[idx];
}

void led_sw_update(void)
{
    static bool initialized;
    static bool previous_pressed[LED_SW_SW_COUNT];
    bool sw1_pressed = led_sw_debounce(0u, led_sw_pressed(1u));
    bool sw2_pressed = led_sw_debounce(1u, led_sw_pressed(2u));
    bool sw3_pressed = led_sw_debounce(LED_SW_SW3_INDEX, s_sw3_pressed);

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

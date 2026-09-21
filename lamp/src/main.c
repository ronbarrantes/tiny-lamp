#include <math.h>
#include <stdio.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#include "ws2812.pio.h"

#define LED_COUNT 15u
#define LED_DATA_PIN 16u
#define BUTTON_PIN 17u
#define BRIGHTNESS 51u /* 20% of 255 */
#define DEBOUNCE_MS 30u
#define HOLD_MS 1200u

enum pattern {
    PATTERN_WHITE = 0,
    PATTERN_RGB_SWEEP,
    PATTERN_RAINBOW_SWEEP,
    PATTERN_SINGLE_FILL,
    PATTERN_COUNT,
};

static PIO led_pio = pio0;
static uint led_sm;
static enum pattern active_pattern = PATTERN_WHITE;
static bool lamp_on = true;

static uint8_t scale_channel(uint8_t value) {
    return (uint8_t)(((uint16_t)value * BRIGHTNESS) / 255u);
}

static uint32_t pack_grb(uint8_t red, uint8_t green, uint8_t blue) {
    return ((uint32_t)scale_channel(green) << 24u) |
           ((uint32_t)scale_channel(red) << 16u) |
           ((uint32_t)scale_channel(blue) << 8u);
}

static void put_pixel(uint32_t pixel) {
    pio_sm_put_blocking(led_pio, led_sm, pixel);
}

static void show_black(void) {
    for (uint i = 0; i < LED_COUNT; ++i) {
        put_pixel(0);
    }
    sleep_us(80);
}

static uint32_t hsv_to_grb(uint16_t hue) {
    uint8_t region = (uint8_t)(hue / 60u);
    uint8_t remainder = (uint8_t)((hue % 60u) * 255u / 60u);
    uint8_t rising = remainder;
    uint8_t falling = (uint8_t)(255u - remainder);
    uint8_t red = 0, green = 0, blue = 0;

    switch (region % 6u) {
        case 0: red = 255; green = rising; break;
        case 1: red = falling; green = 255; break;
        case 2: green = 255; blue = rising; break;
        case 3: green = falling; blue = 255; break;
        case 4: red = rising; blue = 255; break;
        default: red = 255; blue = falling; break;
    }
    return pack_grb(red, green, blue);
}

static void render(uint32_t frame) {
    if (!lamp_on) {
        show_black();
        return;
    }

    for (uint i = 0; i < LED_COUNT; ++i) {
        uint32_t pixel;
        switch (active_pattern) {
            case PATTERN_WHITE:
                pixel = pack_grb(255, 255, 255);
                break;
            case PATTERN_RGB_SWEEP: {
                uint phase = (frame + i * 3u) % (LED_COUNT * 3u);
                pixel = phase < LED_COUNT ? pack_grb(255, 0, 0)
                       : phase < LED_COUNT * 2u ? pack_grb(0, 255, 0)
                       : pack_grb(0, 0, 255);
                break;
            }
            case PATTERN_RAINBOW_SWEEP:
                pixel = hsv_to_grb((uint16_t)((frame * 4u + i * 360u / LED_COUNT) % 360u));
                break;
            case PATTERN_SINGLE_FILL:
                pixel = i <= (frame % (LED_COUNT + 1u)) ? pack_grb(255, 255, 255) : 0;
                break;
            default:
                pixel = 0;
                break;
        }
        put_pixel(pixel);
    }
    sleep_us(80);
}

static void setup_leds(void) {
    uint offset = pio_add_program(led_pio, &ws2812_program);
    ws2812_program_init(led_pio, led_sm, offset, LED_DATA_PIN, 800000, false);
}

static void setup_button(void) {
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
}

static void handle_button(bool pressed, uint64_t now_ms, bool *was_pressed,
                          uint64_t *pressed_at, uint64_t *last_edge_ms,
                          bool *hold_fired) {
    if (pressed == *was_pressed || now_ms - *last_edge_ms < DEBOUNCE_MS) {
        return;
    }
    *last_edge_ms = now_ms;
    *was_pressed = pressed;
    if (pressed) {
        *pressed_at = now_ms;
        *hold_fired = false;
        return;
    }

    if (!*hold_fired) {
        active_pattern = (enum pattern)((active_pattern + 1) % PATTERN_COUNT);
        lamp_on = true;
        printf("pattern %d\n", active_pattern);
    }
}

int main(void) {
    stdio_init_all();
    setup_leds();
    setup_button();

    bool was_pressed = false;
    bool hold_fired = false;
    uint64_t pressed_at = 0;
    uint64_t last_edge_ms = 0;
    uint32_t frame = 0;

    while (true) {
        uint64_t now_ms = to_ms_since_boot(get_absolute_time());
        bool pressed = !gpio_get(BUTTON_PIN);
        handle_button(pressed, now_ms, &was_pressed, &pressed_at, &last_edge_ms, &hold_fired);

        if (pressed && !hold_fired && now_ms - pressed_at >= HOLD_MS) {
            lamp_on = !lamp_on;
            hold_fired = true;
            printf("lamp %s\n", lamp_on ? "on" : "off");
        }

        render(frame++);
        sleep_ms(33);
    }
}

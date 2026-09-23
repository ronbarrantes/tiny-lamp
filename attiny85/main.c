#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

#define LED_COUNT 12u
#define LED_PIN PB0
#define BUTTON_PIN PB1
#define BRIGHTNESS 102u /* 40% of 255 */
#define LOOP_MS 10u
#define DEBOUNCE_TICKS 3u
#define BLINK_TICKS 50u /* 500 ms */

enum pattern {
    PATTERN_WHITE = 0,
    PATTERN_YELLOW_ALTERNATING,
    PATTERN_RED_BLINK,
    PATTERN_COUNT,
};

struct color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

static void clock_init(void) {
    /* Remove the factory divide-by-8 prescaler and run the internal clock at 8 MHz. */
    CLKPR = _BV(CLKPCE);
    CLKPR = 0;
}

static void ws2812_send_byte(uint8_t value) {
    uint8_t bit_count = 8;
    uint8_t port_high = PORTB | _BV(LED_PIN);
    uint8_t port_low = PORTB & (uint8_t)~_BV(LED_PIN);

    /* Ten CPU cycles per bit gives the WS2812B an 800 kHz data signal at 8 MHz. */
    __asm__ volatile(
        "1: out %[port], %[high]" "\n\t"
        "nop"                    "\n\t"
        "sbrs %[value], 7"       "\n\t"
        "out %[port], %[low]"    "\n\t"
        "lsl %[value]"           "\n\t"
        "nop"                    "\n\t"
        "out %[port], %[low]"    "\n\t"
        "dec %[count]"           "\n\t"
        "brne 1b"                "\n\t"
        : [value] "+&r" (value), [count] "+&r" (bit_count)
        : [port] "I" (_SFR_IO_ADDR(PORTB)),
          [high] "r" (port_high), [low] "r" (port_low)
    );
}

static void ws2812_show(const struct color pixels[LED_COUNT]) {
    uint8_t saved_status = SREG;
    cli();

    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        ws2812_send_byte(pixels[i].green);
        ws2812_send_byte(pixels[i].red);
        ws2812_send_byte(pixels[i].blue);
    }

    SREG = saved_status;
    _delay_us(80);
}

static void fill(struct color pixels[LED_COUNT], struct color color) {
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        pixels[i] = color;
    }
}

static void render(struct color pixels[LED_COUNT], enum pattern pattern,
                   uint8_t phase_on) {
    const struct color off = {0, 0, 0};
    const struct color white = {BRIGHTNESS, BRIGHTNESS, BRIGHTNESS};
    const struct color yellow = {BRIGHTNESS, BRIGHTNESS, 0};
    const struct color red = {BRIGHTNESS, 0, 0};

    switch (pattern) {
        case PATTERN_WHITE:
            fill(pixels, white);
            break;
        case PATTERN_YELLOW_ALTERNATING:
            for (uint8_t i = 0; i < LED_COUNT; ++i) {
                pixels[i] = ((i & 1u) == phase_on) ? yellow : off;
            }
            break;
        case PATTERN_RED_BLINK:
            fill(pixels, phase_on ? red : off);
            break;
        default:
            fill(pixels, off);
            break;
    }

    ws2812_show(pixels);
}

int main(void) {
    clock_init();

    DDRB |= _BV(LED_PIN);
    PORTB &= (uint8_t)~_BV(LED_PIN);

    DDRB &= (uint8_t)~_BV(BUTTON_PIN);
    PORTB |= _BV(BUTTON_PIN);

    struct color pixels[LED_COUNT];
    enum pattern pattern = PATTERN_WHITE;
    uint8_t phase_on = 1;
    uint8_t blink_ticks = 0;
    uint8_t button_stable = 1;
    uint8_t button_candidate = 1;
    uint8_t debounce_ticks = 0;

    render(pixels, pattern, phase_on);

    while (1) {
        uint8_t button_sample = (PINB & _BV(BUTTON_PIN)) != 0;

        if (button_sample != button_candidate) {
            button_candidate = button_sample;
            debounce_ticks = 0;
        } else if (debounce_ticks < DEBOUNCE_TICKS) {
            ++debounce_ticks;
            if (debounce_ticks == DEBOUNCE_TICKS && button_stable != button_candidate) {
                button_stable = button_candidate;
                if (!button_stable) {
                    pattern = (enum pattern)((pattern + 1) % PATTERN_COUNT);
                    phase_on = 1;
                    blink_ticks = 0;
                    render(pixels, pattern, phase_on);
                }
            }
        }

        if (pattern != PATTERN_WHITE && ++blink_ticks >= BLINK_TICKS) {
            blink_ticks = 0;
            phase_on = !phase_on;
            render(pixels, pattern, phase_on);
        }

        _delay_ms(LOOP_MS);
    }
}

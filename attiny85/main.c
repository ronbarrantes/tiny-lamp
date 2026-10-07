#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/atomic.h>
#include <util/delay.h>

#define LED_COUNT 15u
#define LED_PIN PB0
#define BUTTON_PIN PB1
#define ENCODER_A_PIN PB3
#define ENCODER_B_PIN PB4

#define ENCODER_REVERSE 0   /* Set to 1 if clockwise turns the wrong way. */
#define ENCODER_HALF_STEP 0 /* Set to 1 if one detent is half a quadrature cycle. */

#define MAX_LEVEL 128u    /* 50% brightness cap */
#define DIM_VALUE 32u     /* Below this brightness, LEDs switch off one by one */
#define MIN_LIT 3u        /* LEDs still lit at the lowest brightness */
#define SWAY_DEPTH 160u   /* Deepest brightness dip, out of 255 (about 60%) */
#define HUE_SWAY 8        /* Hue drifts this far either way, out of 256 */
#define SAT_SWAY 30u      /* Deepest saturation dip, out of 255 */
#define FRAME_MS 20u
#define DEBOUNCE_POLLS 20u /* About 20 ms */
#define HOLD_MS 700u
#define FAST_TURN_MS 40u
#define CUE_STEP_MS 150u
#define SAVE_DELAY_MS 3000u
#define SETTINGS_MAGIC 0x4Cu

enum mode {
    MODE_HUE = 0,
    MODE_SATURATION,
    MODE_BRIGHTNESS,
    MODE_COUNT,
};

struct color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

struct settings {
    uint8_t magic;
    uint8_t hue;
    uint8_t saturation;
    uint8_t value;
};

static struct settings EEMEM saved_settings;

static volatile uint32_t ms_ticks;
static volatile int8_t encoder_detents;
static volatile int8_t encoder_accum;
static volatile uint8_t encoder_prev;

static const int8_t quadrature_steps[16] = {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0,
};

static void clock_init(void) {
    /* Remove the factory divide-by-8 prescaler and run the internal clock at 8 MHz. */
    CLKPR = _BV(CLKPCE);
    CLKPR = 0;
}

static void timer_init(void) {
    /* 8 MHz / 64 / 125 = 1 kHz tick. */
    TCCR0A = _BV(WGM01);
    TCCR0B = _BV(CS01) | _BV(CS00);
    OCR0A = 124;
    TIMSK = _BV(OCIE0A);
}

ISR(TIMER0_COMPA_vect) {
    ++ms_ticks;
}

static uint32_t millis(void) {
    uint32_t now;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        now = ms_ticks;
    }
    return now;
}

static uint8_t encoder_read(void) {
    uint8_t a = (PINB & _BV(ENCODER_A_PIN)) != 0;
    uint8_t b = (PINB & _BV(ENCODER_B_PIN)) != 0;
#if ENCODER_REVERSE
    return (uint8_t)((b << 1) | a);
#else
    return (uint8_t)((a << 1) | b);
#endif
}

static void encoder_reset(void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        encoder_prev = encoder_read();
        encoder_accum = 0;
        encoder_detents = 0;
    }
}

ISR(PCINT0_vect) {
    uint8_t state = encoder_read();
    encoder_accum += quadrature_steps[(encoder_prev << 2) | state];
    encoder_prev = state;

    /* Count a detent only when the encoder settles at rest, so a missed edge cannot drift. */
#if ENCODER_HALF_STEP
    uint8_t at_rest = state == 0x3u || state == 0x0u;
#else
    uint8_t at_rest = state == 0x3u;
#endif
    if (at_rest) {
        if (encoder_accum >= 2) {
            ++encoder_detents;
        } else if (encoder_accum <= -2) {
            --encoder_detents;
        }
        encoder_accum = 0;
    }
}

static int8_t encoder_take(void) {
    int8_t detents;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        detents = encoder_detents;
        encoder_detents = 0;
    }
    return detents;
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
    /* About 450 us with interrupts off; the 1 ms tick and encoder flags stay pending. */
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

static void show_off(struct color pixels[LED_COUNT]) {
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        pixels[i] = (struct color){0, 0, 0};
    }
    ws2812_show(pixels);
}

/* Full-brightness color for a hue and saturation; brightness is applied separately. */
static struct color hsv_to_rgb(uint8_t hue, uint8_t saturation) {
    uint8_t region = hue / 43u;
    uint8_t remainder = (uint8_t)((hue - region * 43u) * 6u);
    uint8_t p = 255u - saturation;
    uint8_t q = 255u - (uint8_t)(((uint16_t)saturation * remainder) >> 8);
    uint8_t t = 255u - (uint8_t)(((uint16_t)saturation * (255u - remainder)) >> 8);

    switch (region) {
        case 0: return (struct color){255, t, p};
        case 1: return (struct color){q, 255, p};
        case 2: return (struct color){p, 255, t};
        case 3: return (struct color){p, q, 255};
        case 4: return (struct color){t, p, 255};
        default: return (struct color){255, p, q};
    }
}

/* Brightness knob value to output level, with gamma so the low end steps smoothly. */
static uint8_t value_to_level(uint8_t value) {
    if (value < DIM_VALUE) {
        return 1;
    }
    uint8_t gamma = (uint8_t)(((uint16_t)value * value + value) >> 8);
    return (uint8_t)(1u + (uint16_t)gamma * (MAX_LEVEL - 1u) / 255u);
}

/* Smooth 0..255..0 wave over one phase cycle. */
static uint8_t wave(uint8_t phase) {
    uint32_t x = phase < 128u ? phase * 2u : (255u - phase) * 2u;
    return (uint8_t)(x * x * (765u - 2u * x) / 65025u);
}

/* At the lowest level, round so only the main color channel stays lit instead of washing to white. */
static uint8_t scale(uint8_t channel, uint8_t level) {
    return (uint8_t)(((uint16_t)channel * level + 128u) >> 8);
}

/* 1/255 is as dim as a WS2812B gets, so go darker by lighting fewer of them. */
static uint8_t value_to_lit(uint8_t value) {
    if (value >= DIM_VALUE) {
        return LED_COUNT;
    }
    return (uint8_t)(MIN_LIT + (uint16_t)value * (LED_COUNT - MIN_LIT) / DIM_VALUE);
}

static void render(struct color pixels[LED_COUNT], uint8_t hue, uint8_t saturation,
                   uint8_t level, uint8_t lit, uint32_t now) {
    /* Independent slow waves per LED make the sway look organic. */
    uint8_t dim_slow = (uint8_t)(now / 16u); /* 4.1 s period */
    uint8_t dim_fast = (uint8_t)(now / 11u); /* 2.8 s period */
    uint8_t hue_phase = (uint8_t)(now / 23u); /* 5.9 s period */
    uint8_t sat_phase = (uint8_t)(now / 19u); /* 4.9 s period */

    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        /* Spread the lit LEDs evenly along the strip. */
        if ((uint8_t)((i + 1u) * lit / LED_COUNT) == (uint8_t)(i * lit / LED_COUNT)) {
            pixels[i] = (struct color){0, 0, 0};
            continue;
        }

        uint8_t w = (uint8_t)(((uint16_t)wave((uint8_t)(dim_slow + i * 22u)) +
                               wave((uint8_t)(dim_fast - i * 37u))) >> 1);
        uint8_t dip = (uint8_t)(((uint16_t)w * SWAY_DEPTH) >> 8);
        uint8_t pixel_level = (uint8_t)(((uint16_t)level * (255u - dip) + 254u) / 255u);

        int16_t hue_shift = ((int16_t)wave((uint8_t)(hue_phase + i * 53u)) - 127) * HUE_SWAY / 127;
        uint8_t sat_dip = (uint8_t)(((uint16_t)wave((uint8_t)(sat_phase - i * 29u)) * SAT_SWAY) >> 8);
        uint8_t pixel_saturation = saturation > sat_dip ? saturation - sat_dip : 0;
        struct color base = hsv_to_rgb((uint8_t)(hue + hue_shift), pixel_saturation);

        pixels[i].red = scale(base.red, pixel_level);
        pixels[i].green = scale(base.green, pixel_level);
        pixels[i].blue = scale(base.blue, pixel_level);
    }

    ws2812_show(pixels);
}

static uint8_t add_clamped(uint8_t value, int16_t delta, uint8_t minimum) {
    int16_t result = (int16_t)value + delta;
    if (result < minimum) {
        return minimum;
    }
    if (result > 255) {
        return 255;
    }
    return (uint8_t)result;
}

static void load_settings(struct settings *settings) {
    eeprom_read_block(settings, &saved_settings, sizeof(*settings));
    if (settings->magic != SETTINGS_MAGIC) {
        *settings = (struct settings){SETTINGS_MAGIC, 24, 220, 160}; /* Warm amber */
    }
}

int main(void) {
    clock_init();

    DDRB |= _BV(LED_PIN);
    PORTB &= (uint8_t)~_BV(LED_PIN);

    DDRB &= (uint8_t)~(_BV(BUTTON_PIN) | _BV(ENCODER_A_PIN) | _BV(ENCODER_B_PIN));
    PORTB |= _BV(BUTTON_PIN) | _BV(ENCODER_A_PIN) | _BV(ENCODER_B_PIN);

    timer_init();
    encoder_reset();
    GIMSK |= _BV(PCIE);
    PCMSK = _BV(ENCODER_A_PIN) | _BV(ENCODER_B_PIN);
    sei();

    struct settings settings;
    load_settings(&settings);

    struct color pixels[LED_COUNT];
    enum mode mode = MODE_HUE;
    uint8_t lamp_on = 1;

    uint8_t button_down = 0;
    uint8_t debounce_polls = 0;
    uint8_t ignore_release = 0;
    uint32_t press_started = 0;
    uint32_t last_poll = 0;

    uint32_t last_frame = 0;
    uint32_t last_turn = 0;
    uint32_t changed_at = 0;
    uint8_t unsaved = 0;
    uint32_t cue_started = 0;
    uint8_t cue_steps = 0;

    while (1) {
        uint32_t now = millis();

        if (now != last_poll) {
            last_poll = now;
            uint8_t pressed = (PINB & _BV(BUTTON_PIN)) == 0;

            if (pressed == button_down) {
                debounce_polls = 0;
            } else if (++debounce_polls >= DEBOUNCE_POLLS) {
                debounce_polls = 0;
                button_down = pressed;

                if (button_down) {
                    press_started = now;
                    if (!lamp_on) {
                        lamp_on = 1;
                        ignore_release = 1;
                        encoder_reset();
                        last_frame = now - FRAME_MS;
                    }
                } else if (ignore_release) {
                    ignore_release = 0;
                } else {
                    mode = (enum mode)((mode + 1u) % MODE_COUNT);
                    cue_started = now;
                    cue_steps = (uint8_t)((mode + 1u) * 2u);
                }
            }

            if (button_down && lamp_on && !ignore_release && now - press_started >= HOLD_MS) {
                lamp_on = 0;
                ignore_release = 1;
                cue_steps = 0;
                show_off(pixels);
            }
        }

        int8_t detents = encoder_take();
        if (detents != 0 && lamp_on) {
            uint8_t quick = now - last_turn < FAST_TURN_MS;
            last_turn = now;

            switch (mode) {
                case MODE_HUE:
                    settings.hue += (uint8_t)(detents * (quick ? 16 : 4));
                    break;
                case MODE_SATURATION:
                    settings.saturation = add_clamped(settings.saturation,
                                                      detents * (quick ? 24 : 8), 0);
                    break;
                default:
                    settings.value = add_clamped(settings.value,
                                                 detents * (quick ? 24 : 8), 0);
                    break;
            }

            unsaved = 1;
            changed_at = now;
        }

        if (lamp_on && now - last_frame >= FRAME_MS) {
            last_frame = now;
            uint8_t level = value_to_level(settings.value);

            /* After a click, blink dim once for hue, twice for saturation, three times for brightness. */
            if (cue_steps != 0) {
                uint32_t step = (now - cue_started) / CUE_STEP_MS;
                if (step >= cue_steps) {
                    cue_steps = 0;
                } else if ((step & 1u) == 0) {
                    level = (uint8_t)(level / 5u);
                }
            }

            render(pixels, settings.hue, settings.saturation, level,
                   value_to_lit(settings.value), now);
        }

        if (unsaved && now - changed_at >= SAVE_DELAY_MS) {
            unsaved = 0;
            eeprom_update_block(&settings, &saved_settings, sizeof(settings));
        }
    }
}

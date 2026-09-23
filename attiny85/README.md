# ATtiny85 lamp test

This firmware drives 12 WS2812B LEDs and uses one button to cycle three patterns.

1. Steady white at 40% brightness
2. Yellow LEDs alternating between even and odd positions every 500 ms
3. All LEDs blinking red every 500 ms

## Wiring

| ATtiny85 connection | Physical pin | Connect to |
| --- | ---: | --- |
| PB0 | 5 | 330 ohm resistor, then the first LED's DIN |
| PB1 | 6 | Button, with the other button leg connected to GND |
| VCC | 8 | Battery positive, 4.5 V from three alkaline AA cells |
| GND | 4 | Battery negative and LED GND |

Connect the LED strip's positive lead to the same 4.5 V supply. Keep all grounds connected. The firmware enables PB1's internal pull-up resistor, so the button must connect PB1 to ground when pressed.

The firmware changes the factory-default ATtiny85 clock prescaler at startup and runs from the internal 8 MHz oscillator. It does not require a clock-fuse change. This assumes the chip still uses its factory clock fuse settings.

## Build

```sh
make -C attiny85
```

The output is `attiny85/build/tiny_lamp_attiny85.hex`.

## Flash

The flash target matches the `pattern-game` setup: an Arduino Uno R4 Minima running ArduinoISP at 19200 baud.

```sh
make -C attiny85 flash
```

The Makefile detects common Linux and macOS serial-port names. Set the port explicitly if more than one serial device is connected:

```sh
make -C attiny85 flash PORT=/dev/cu.usbmodem101
```

Connect ATtiny85 RESET to Arduino D10, PB0/MOSI to D11, PB1/MISO to D12, PB2/SCK to D13, VCC to 5 V, and GND to GND. Disconnect the LED data line from PB0 and the button from PB1 while flashing if they interfere with programming. Do not power the ATtiny from the battery and Arduino at the same time. Do not change the fuse bytes for this test.

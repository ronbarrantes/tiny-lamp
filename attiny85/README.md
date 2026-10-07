# ATtiny85 lamp

This firmware drives 15 WS2812B LEDs from one rotary encoder with a push switch. The lamp holds one HSV color. Each LED sways softly darker, drifts slightly in hue, and dips in saturation, like embers. Tune the amounts with `SWAY_DEPTH`, `HUE_SWAY`, and `SAT_SWAY`.

## Controls

| Action | Result |
| --- | --- |
| Turn | Adjusts the current mode |
| Click | Next mode: hue → saturation → brightness. The lamp blinks dim 1, 2, or 3 times to show which |
| Hold (0.7 s) | Lamp off |
| Click while off | Lamp on, same color and mode |

Hue wraps around. Turning quickly takes bigger steps. Brightness is capped at 50% (`MAX_LEVEL`), and the sway only dips below the set brightness. At the bottom of the range, LEDs switch off one by one down to 3 (`MIN_LIT`), since one LED cannot dim any further. The color is saved to EEPROM 3 s after the last change and restored at power-up. The lamp starts in hue mode.

## Wiring

| ATtiny85 pin | Signal | Connect to |
| --- | --- | --- |
| 1 (PB5/RESET) | — | Unconnected; Arduino D10 only while flashing |
| 2 (PB3) | Encoder A (CLK) | Encoder A |
| 3 (PB4) | Encoder B (DT) | Encoder B |
| 4 | GND | USB-C breakout GND, LED GND, encoder common, switch |
| 5 (PB0) | LED data | 330 ohm resistor, then the first LED's DIN |
| 6 (PB1) | Encoder switch | Switch; other switch leg to GND |
| 7 (PB2) | — | Unused |
| 8 | VCC | USB-C breakout VBUS (5 V) |

Power everything from the Adafruit sunken USB-C breakout (#6050) and a 5 V, 2 A or larger USB-C adapter. Put a 0.1 µF capacitor across pins 8 and 4 and a 470–1000 µF capacitor across the LED supply. The firmware enables internal pull-ups on the encoder and switch pins, so a bare encoder needs no resistors: its middle pin goes to GND. A 5-pin module (KY-040 style) also connects its `+` to 5 V.

If turning clockwise goes the wrong way, set `ENCODER_REVERSE` to 1. If each detent moves two steps, set `ENCODER_HALF_STEP` to 1.

The firmware changes the factory-default clock prescaler at startup and runs from the internal 8 MHz oscillator. It does not require a fuse change.

## Build

```sh
make -C attiny85
```

The output is `attiny85/build/tiny_lamp_attiny85.hex`.

## Flash

Use an Arduino Uno R4 Minima running ArduinoISP at 19200 baud.

```sh
make -C attiny85 flash
```

The Makefile detects common Linux and macOS serial-port names. Set the port explicitly if more than one serial device is connected:

```sh
make -C attiny85 flash PORT=/dev/cu.usbmodem101
```

Connect ATtiny85 RESET to Arduino D10, PB0/MOSI to D11, PB1/MISO to D12, PB2/SCK to D13, VCC to 5 V, and GND to GND. Unplug the USB-C supply while flashing. If programming fails, disconnect the LED data line from PB0. Do not change the fuse bytes. Flashing erases the saved color, so the lamp starts at warm amber afterward.

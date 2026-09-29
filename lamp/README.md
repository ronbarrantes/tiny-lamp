# First Pico firmware

This milestone drives 15 WS2812B LEDs and one momentary button.

- GP16, physical pin 21: LED DIN through the optional 330 ohm resistor.
- GP17, physical pin 22: button to GND.
- Physical pin 23: shared GND.

Short press cycles white, RGB sweep, rainbow sweep, and single fill. Hold for 1.2 seconds to toggle the lamp. The first frame is white at 20% brightness.

## Build

Install the Pico SDK and ARM GNU toolchain, then set `PICO_SDK_PATH`:

```sh
cmake -S lamp -B lamp/build -G Ninja
cmake --build lamp/build
```

The UF2 is `lamp/build/tiny_lamp.uf2`.

## Flash

Hold BOOTSEL while connecting the Pico 2 W over USB. Copy `lamp/build/tiny_lamp.uf2` to the mounted `RPI-RP2` drive. The board reboots into the firmware.

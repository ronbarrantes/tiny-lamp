# ESP32-C3 POV picture editor

An independent experiment for the ESP32-C3 Super Mini and a row of WS2812B LEDs. Starts with **11 LEDs × 10 columns**, ten brush colors plus off, and 5 ms per column. The browser page, picture storage, and LED playback run on the ESP32.

## Open and use

Join the same router Wi-Fi as the ESP32, then open `http://<board-ip>/editor`. The USB serial log and Wi-Fi status page at `/` show the current address. The first flashed board was at **http://192.168.4.46/editor**; DHCP may assign another address later.

If Wi-Fi is not configured, use the existing `TinyLamp-Setup-…` network with password `12345678` and open `http://192.168.4.1` to provision it. Existing credentials from the Wi-Fi experiment are reused. The editor is also available at `/editor` while connected to setup Wi-Fi.

1. Set **LEDs / height** to the number of working LEDs in your chain.
2. Set **Columns / width**, or click **+ Column**. Each column is one frame played across the physical strip.
3. Choose a brush, then click pixels or drag to paint. Black erases a pixel. LED 1, the first LED connected to DIN, is the top grid row.
4. Adjust **Column duration** to match your sweep speed. One image takes `column count × column duration` milliseconds.
5. Click **Send picture to lamp**. This saves the picture and settings, then starts playback.
6. Use the web on/off switch or press the breadboard button to toggle playback. Web controls reflect the physical button within about two seconds.

Changing settings in the editor takes effect on the lamp when you send the picture. The on/off switch plays the last picture sent. Resizing preserves overlapping pixels by row and column; shrinking asks before cropping. **Clear** also asks before removing pixels.

Pictures and settings survive power loss. Playback starts **off** after each boot. Wi-Fi reset clears only Wi-Fi credentials, leaving the picture intact.

Supported limits are 1–32 LEDs, 1–64 columns, 2–100 ms per column, and 1–40% brightness. Brightness starts at 20%. A separate playback task drives the RMT peripheral, while the main loop serves Wi-Fi and HTTP. This is manually swept POV without position sensing, so changes in motion still stretch or distort the image. The column duration is a target, not a calibrated physical sweep rate.

## Breadboard pinout

Use the **GPIO numbers printed on your board**, rather than counting header positions.

| ESP32-C3 / supply | Connect to |
| --- | --- |
| **GPIO4** | LED chain **DIN**, preferably through a 74AHCT125 level shifter and a 330 Ω series resistor |
| **GPIO5** | One side of a momentary on/off button |
| **GND** | Other side of the button, LED GND, and supply GND |
| Regulated **5 V supply** | LED **5V / VCC** |
| **USB** | ESP32 power and flashing |

GPIO5 uses the ESP32's internal pull-up. Pressing the button connects it to ground; no external pull-up is needed. For a four-leg switch, use one leg from each electrically separate pair. Debouncing is 30 ms, and a held press toggles once.

Keep GPIO9 for the board's BOOT button. Hold it for five seconds while running to erase Wi-Fi settings and reopen setup. RESET reboots and turns playback off without erasing the saved picture.

Power the LEDs from 5 V, **not the ESP32's 3V3 pin**. For the initial 11-LED breadboard test, a regulated 5 V / 1 A LED supply provides margin. With a separate LED supply and USB-powered ESP32, connect their grounds and keep the positive supplies separate. A 5 V-powered WS2812B chain should use a 3.3 V-to-5 V data level shifter for reliable operation. Put the 330 Ω resistor near the first LED's DIN after the shifter. A 500–1000 µF capacitor across LED power near the chain helps with power transients. See [Adafruit's wiring and power guidance](https://learn.adafruit.com/adafruit-neopixel-uberguide/best-practices).

Connect to DIN, following the LEDs' data arrows. Changing LED count cannot repair a broken data connection; bypass or remove the damaged LED so data reaches the remaining chain. The firmware sends black to unused positions through LED 32 so reducing the count clears previously lit trailing LEDs.

## Build and flash

Requires Arduino CLI and Espressif's Arduino ESP32 core. No separate Arduino board is used. From the repository root:

```sh
arduino-cli core install esp32:esp32@3.3.12 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
python3 esp32-pov/embed_web.py
arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc --build-path /tmp/tiny-lamp-pov-build --warnings all esp32-pov
arduino-cli upload --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc --port /dev/cu.usbmodem101 --input-dir /tmp/tiny-lamp-pov-build esp32-pov
```

Replace the USB port with the one shown by `arduino-cli board list`. `editor.html` contains the editable HTML, CSS, and JavaScript; `embed_web.py` generates `web_assets.h`. `wifi_bridge.cpp` compiles the existing Wi-Fi module into this separate sketch, rather than maintaining a copy. Wi-Fi accepts an optional route-registration callback; its standalone sketch still works without it.

`pattern.cpp` validates the uploaded dimensions and column-major RGB data before changing the active picture. `pov.cpp` owns storage, the HTTP API, the RMT LED output, and the debounced physical button. Wi-Fi and picture data use separate NVS namespaces. [Espressif RMT API reference](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/rmt.html).

## Checks

```sh
python3 esp32-wifi/tests/test_connection_timer.py
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I esp32-pov esp32-pov/pattern.cpp esp32-pov/tests/pattern_test.cpp -o /tmp/pov-pattern-test
/tmp/pov-pattern-test
```

The editor's DOM test uses jsdom. It can be installed outside the repository:

```sh
pnpm --dir /tmp/tiny-lamp-ui-test add jsdom
NODE_PATH=/tmp/tiny-lamp-ui-test/node_modules node esp32-pov/tests/editor_test.cjs
```

Verified on the connected ESP32-C3: compilation and upload, successful router reconnect, editor HTTP response, invalid-upload rejection, picture save/readback, and web on/off. Browser checks via Browse.sh covered desktop and a 390-pixel phone viewport, painting, adding a column, saving, and on/off. The DOM test also checks that resizing preserves pixel coordinates. The parser test checks bounds and invalid input with address and undefined-behavior sanitizers.

Physical LED colors, the breadboard switch, and the moving POV image still need wiring and testing. Start with a few colored pixels, verify row order, then adjust the sweep duration. Reboot to check picture persistence and off-at-boot behavior.

# Tiny Lamp

Tiny Lamp is a personal lamp with 15 WS2812B addressable LEDs and one push button. The lamp stores its presets and hosts its own web controls.

## Lamp firmware

The isolated [ESP32-C3 Wi-Fi test](esp32-wifi/README.md) provisions a router connection over a temporary setup network. It builds separately from the lamp firmware.

The [ESP32-C3 lamp and POV editor](esp32-pov/README.md) adds solid colors and animated lamp presets alongside an adjustable pixel grid, saved pictures, timed LED columns, and web/button on/off. It reuses the Wi-Fi setup and builds as its own experiment.

The firmware will be written in C for a Raspberry Pi Pico 2 W, with a later port to ESP32-C3. Keep lamp behavior, the LED driver, board-specific code, Wi-Fi provisioning, and HTTP handlers separate.

The button cycles four presets and toggles the lamp on/off. A setup mode creates a temporary Wi-Fi network and serves a page for entering the home Wi-Fi credentials. Settings and presets persist across boots, and Wi-Fi can be reset without losing presets.

## Web controls

The Pico serves plain HTML, CSS, and JavaScript. The page polls the lamp only while visible and saves patterns directly to the device. No hosted lamp application, database, or login is needed.

Ron's WireGuard server lives in the cloud. The Pico will join it as a separate peer with its own credentials and VPN address, then serve the controls only through that VPN interface. Phones and laptops on the VPN can reach the page when peer routing permits. The temporary Wi-Fi setup page remains available for configuration and recovery.

Implement and validate the embedded WireGuard client as a separate milestone. Start with working local controls for development; that intermediate build does not restrict access to VPN clients. VPN keepalive traffic continues while idle, even though browser polling stops when the page closes.

Use `lamp/` for firmware and `server/` for the HTTP modules and web assets compiled into that firmware. Both run on the Pico.

See [PLAN.md](PLAN.md) for the staged implementation handoff, pin assignments, and verification checks.

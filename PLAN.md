# Tiny Lamp implementation handoff

This is an implementation plan, not completed functionality. Build the milestones in order. Deliver a usable offline lamp at the end of milestone 1 so Ron can use it while the web features are developed. Keep that firmware artifact available.

## Scope and defaults

- Hardware: Raspberry Pi Pico 2 W, 15 WS2812B 5050 RGB LEDs in one chain, and one external momentary push button.
- Firmware: C11 with the Raspberry Pi Pico SDK and CMake. Use PIO for LED timing. Pin the SDK revision and record the toolchain version.
- Website: plain HTML, CSS, and JavaScript served by the Pico itself. Bundle every asset in the firmware. No React, TanStack, external CDN, or separate hosted server.
- This is one personal lamp. No cloud database, Vercel, Convex, Supabase, user accounts, or device enrollment. If a future hosted version needs login, Ron prefers Clerk; do not add it now.
- Ron's WireGuard server runs in the cloud. The Pico will be its own WireGuard peer, with its own credentials and VPN address. The phone or laptop joins the same VPN and accesses the Pico's website through that address. Validate the embedded client in milestone 5; do not assume a home VPN gateway exists.
- The browser polls only while the control page is visible. The lamp does not poll a cloud application or upload telemetry. WireGuard still needs tunnel maintenance traffic while idle so the lamp remains reachable.
- Custom patterns in this version are static arrangements of 15 colors. Include four built-in presets, including animated effects. Defer a custom animation timeline or scripting language.
- The physical button and saved presets work without networking. Local development controls work without home internet. Access through the cloud VPN requires internet connectivity from the lamp and the viewing device.
- ESP32-C3 is a future port. Build the boundaries now, but do not implement or choose pins for an unspecified ESP32 board.
- Implement everything described here in stages. Physical verification and router/VPN configuration may require Ron; finish independent work and report those dependencies precisely.

The preset colors, startup behavior, one-second browser polling interval, and direct-flash storage below are explicit implementation defaults. Device-hosted plain JavaScript and local persistence reflect Ron's chosen direction.

## Network access

The Pico connects outbound over ordinary Wi-Fi to Ron's cloud WireGuard endpoint. Its embedded VPN client decrypts incoming traffic and passes HTTP requests to the device-hosted server. The cloud machine forwards traffic between authorized peers; it does not host the lamp application or its settings. The browser uses `http://<lamp-vpn-ip>/` after joining the VPN.

For the final VPN configuration, bind the control server to the WireGuard interface/address and reject control traffic arriving through the ordinary Wi-Fi interface. Do not silently fall back to LAN exposure when the VPN fails. Keep the temporary setup AP separate and locally accessible when physically requested. Milestones 3 and 4 use a LAN listener as an explicit development stage before VPN integration. Never expose the lamp's HTTP port through public router port forwarding.

Ron assigns a stable VPN address when creating the Pico's peer. The cloud server must permit peer-to-peer forwarding and each peer needs appropriate routes/AllowedIPs; a successful handshake alone does not prove HTTP reachability. Route only the needed VPN addresses through the tunnel and keep the outer WireGuard endpoint reachable over ordinary Wi-Fi. Show LAN/VPN addresses over USB for diagnosis. A home DHCP reservation is optional for local development. See [WireGuard routing](https://www.wireguard.com/).

## Wiring

Use the external button for lamp control. The Pico's BOOTSEL button remains for flashing firmware. It selects USB programming mode when held at power-up. Pin names and physical locations are documented in the [Pico 2 W datasheet, figures 2 and 4](https://datasheets.raspberrypi.com/picow/pico-2-w-datasheet.pdf).

| Connection | Pico signal | Physical pin | Wiring |
| --- | --- | --- | --- |
| LED data | GP16 | 21 | For the bench prototype, use a short wire through a 330 ohm series resistor to the first LED's DIN. Add a 74AHCT125 level shifter for reliable 5 V signaling if needed. |
| Button | GP17 | 22 | Normally open button between GP17 and GND; enable the internal pull-up |
| Shared ground | GND | 23 | Connect button ground, LED supply ground, and level-shifter ground if fitted |
| External board power, if used | VSYS | 39 | Regulated 5 V through a Schottky diode when USB might also be connected |

For initial development, power the Pico through USB and the LEDs through a separate regulated 5 V supply, with grounds connected. There is no need to connect the LED supply's positive lead to the Pico in that arrangement. A later shared supply can feed the LEDs directly and VSYS through the diode. Do not connect an external supply to VBUS while USB is attached. See the [datasheet's power arrangements](https://datasheets.raspberrypi.com/picow/pico-2-w-datasheet.pdf), section 3.5.

Power the LEDs from their 5 V supply, not the Pico's 3V3 pin. Budget up to approximately 0.9 A for 15 LEDs using the conservative 60 mA per LED estimate; actual WS2812B revisions vary. A regulated 5 V, 2 A supply provides headroom for this assembly. Place the data resistor near DIN and a 500-1000 microfarad capacitor rated at least 6.3 V across the strip's power input. These choices follow [Adafruit's wiring guidance](https://learn.adafruit.com/adafruit-neopixel-uberguide/basic-connections) and [power guidance](https://learn.adafruit.com/adafruit-neopixel-uberguide/powering-neopixels).

The Pico accepts 5 V power but its GPIO data output is 3.3 V. Trying that directly into DIN is acceptable for this bench prototype; compatibility depends on the actual LED revision and wiring. Do not block milestone 1 on buying a shifter. If signaling is unreliable, fit a 74AHCT125 powered at 5 V, ground its active-low enable, and add a 100 nF bypass capacitor. Its output feeds the existing data resistor. See [Adafruit's explanation of logic-level variation](https://learn.adafruit.com/adafruit-neopixel-uberguide/logic-level).

Set GPIO numbers in one board configuration file. Start at 20% brightness. Verify the chain direction and RGB channel order with a red/green/blue sequence before running effects.

## Button and lamp behavior

Sample the button without blocking. Debounce both edges for 30 ms. Measure holds using a monotonic clock.

| Gesture | Result |
| --- | --- |
| Normal press released before 3 seconds | Advance to the next of four preset slots, wrapping after slot 4. If off, turn on at the next slot. |
| Normal press held for 3 seconds | Toggle the LEDs on/off once at the threshold. Suppress the release click and any further action until release. |
| Button already held at boot, continuously for 5 seconds | Clear saved Wi-Fi credentials and enter setup mode. Suppress normal button actions throughout this boot gesture. |
| Boot hold released before 5 seconds | Cancel the reset gesture and resume normal operation without a click or power toggle. |

The boot gesture is armed only when the button is down during initial startup sampling. Holding for 5 seconds during normal use only performs the 3-second on/off action. This prevents overlapping actions and accidental Wi-Fi resets.

Off means the LED frame is black. The controller stays powered and connected so the button and website can turn it on again.

Start with these four preset slots: warm white, amber, a slow rainbow, and a warm breathing effect. Define exact colors and effect periods as named constants. Render animations at 30 frames per second using elapsed time. A delayed frame must not slow the animation clock.

First boot defaults to on, warm white, 20% brightness. Later boots restore the last saved power state, active slot, brightness, and slot definitions. Debounce routine state persistence for two seconds after the last change; an immediate unplug may lose those last two seconds. Explicit preset saves and successful Wi-Fi setup must commit before reporting success.

Wi-Fi reset preserves presets, lamp state, setup-network password, and WireGuard configuration. It is not a factory reset. A factory-reset feature is outside this version.

## Code boundaries

Use Ron's existing `lamp/` and `server/` directories. Add the subdirectories below as their milestones need them, not as empty scaffolding. Use a small set of C modules and headers. Do not build a general plugin framework. Platform-independent modules must compile on a desktop for focused tests.

```text
lamp/
  app/                    Startup and event-loop coordination
  lamp/                   State, preset rendering, button gesture rules
  drivers/ws2812/          RGB-to-GRB packing and LED protocol configuration
  ports/                  Small contracts for clock, button, pixel transport,
                          storage, Wi-Fi, and HTTP transport
  platform/pico/          GPIO, PIO program, flash, and CYW43/lwIP adapters
  wifi/                   Connection and provisioning state machines
  vpn/                    WireGuard configuration and tunnel lifecycle
  tests/                  Host tests with fake time, storage, and transports
server/
  http/                   C routing, validation, and lamp API handlers
  provisioning/           C setup endpoints calling lamp/wifi services
  web/                    Plain HTML/CSS/JS for setup and lamp control
protocol/                 Versioned JSON examples and validation rules
docs/                     Wiring, flashing, network setup, verification log
```

The lamp renderer produces exactly 15 RGB values. The WS2812 driver packs them for transmission. The Pico transport sends them using PIO and enforces frame completion and the required reset interval. Keep PIO and SDK headers out of lamp logic and portable driver code.

The Wi-Fi state machine accepts configuration and emits status. The setup webpage calls provisioning endpoints, which call that state machine. Neither the HTML nor its HTTP handlers manipulate flash or GPIO directly. Control endpoints send validated commands to the same lamp API used by the button. The firmware build compiles the C modules under `server/` and embeds its web assets; that directory is a code boundary, not a separate deployed process.

Start with one cooperative event loop and the SDK's polling networking integration. Use asynchronous connection operations, timeouts, and bounded work per tick. Do not block lamp rendering or button processing while waiting for Wi-Fi or HTTP. Do not add an RTOS or a second core unless a measured constraint requires it.

## Persistent storage

Use small JSON documents for Wi-Fi credentials, the setup password, WireGuard configuration, and lamp configuration, stored as versioned flash records. Keep the JSON schema explicit, validate it on load, and bound each document's length. Do not serialize raw C structs with compiler-dependent padding. Four static RGB presets require only 180 bytes for their colors before names, JSON formatting, and other metadata, so a database is unnecessary for this version.

Ron is happy with simple text/file storage. JSON provides readable text with a defined format. The Pico adapter can store those bytes directly in reserved flash; a filesystem and actual `.json` files are not required. Browser assets can likewise be embedded at build time. The persistence adapter owns the record integrity and recovery details below; callers only load or save configuration.

Ron suggested SQLite if a database is needed. Prefer direct flash records here: SQLite needs an appropriate platform/storage adapter on a bare-metal target, and this application does not need SQL queries. This is a scope choice, not a claim that SQLite cannot run on embedded hardware. Keep the storage contract portable so ESP32-C3 can use its platform's persistent storage later. See [SQLite's custom-build requirements](https://www.sqlite.org/custombuild.html).

Reserve storage through the linker/build configuration so firmware cannot grow over it. Verify the board's actual flash size rather than assuming it. Use two independently erasable banks, sequence numbers, lengths, and CRCs. Write and validate a new snapshot before retiring the previous valid snapshot. Recover the latest complete snapshot after an interrupted write; handle empty or invalid storage with safe defaults.

Use the SDK's safe flash execution facilities and observe their interrupt/XIP constraints. Flash operations must occur outside network callbacks. Document brief render pauses during commits. Coalesce writes and use append space within the banks before erasing to reduce wear. Never write every animation frame, poll, or slider movement. See [Pico SDK flash documentation](https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#hardware_flash).

Never log home Wi-Fi passwords, WireGuard private keys, or preshared keys, or return them through the control API. Keep actual VPN configurations out of version control and browser storage. Credentials in ordinary flash are not protected against a person with physical access; do not claim otherwise. Reset Wi-Fi by committing a newer record without credentials. Never fall back to superseded Wi-Fi credentials after that reset. Physical secure erasure is outside this prototype's scope.

## Milestone 1: a lamp Ron can use

1. Add the firmware build, pin configuration, portable lamp modules, Pico LED transport, and external-button handling.
2. Implement the four presets, brightness scaling, on/off, and normal button gestures.
3. Implement persistent lamp configuration and the boot gesture event. Until milestone 2 provides Wi-Fi setup, that event only shows a short amber acknowledgement and resumes operation.
4. Add focused host tests for debounce/hold boundaries, one action per press, slot wrapping, brightness output, and storage recovery after interrupted writes.
5. Produce a Pico 2 W UF2, wiring instructions, exact build/flash commands, and a short physical acceptance checklist. Preserve this artifact separately from later builds.

Completion checks: all 15 LEDs display the expected colors; short clicks cycle four presets; a normal 5-second hold toggles power only once; reboot restores saved state; no networking is required. Record hardware checks as pending until somebody actually performs them. Deliver this milestone before continuing with web features.

## Milestone 2: persistent Wi-Fi and phone setup

1. On first boot without credentials, start a setup access point named `TinyLamp-<device suffix>` and serve a bundled webpage at `http://192.168.4.1`. Provide DHCP. Automatic captive-portal opening is optional; the explicit URL must work without internet or external assets.
2. Protect the AP with a unique random password generated once and saved on the device. Provide a documented USB provisioning command to retrieve it for the owner's label. Never use a shared default password or derive the secret from a public device ID.
3. Build a phone-friendly form for SSID and password. Manual SSID entry is required; network scanning is optional. Initially support 2.4 GHz WPA2-Personal networks and document that scope. Keep passwords out of URLs and responses. Bound and validate input, including SSID/password byte lengths and request size.
4. Bind provisioning endpoints only to the setup network. Allow same-origin requests and require a setup-session CSRF token on mutations. Disable provisioning routes in normal station mode; milestone 3 serves the separate lamp-control page and API there.
5. On submission, acknowledge that a connection attempt is starting before switching from AP to station mode. Tell the phone that the temporary network will disappear. Try association and DHCP for up to 30 seconds. Do not require internet access to accept valid Wi-Fi credentials.
6. If association and DHCP succeed, commit the credentials and stay in station mode. If they fail, restart the same setup network and show the failure when the owner reconnects. This AP-to-station transition avoids depending on concurrent AP and station support.
7. With saved credentials, reconnect automatically at boot. On outages, retry with backoff from 1 second to a maximum of 60 seconds. Keep the lamp usable. Never erase credentials or start an AP just because the router or internet is temporarily unavailable.
8. Connect the 5-second boot gesture to Wi-Fi reset and setup. In setup mode, show a dim blue pulse on LED 1, briefly green after success, and briefly amber after failure. Keep this temporary status separate from saved preset data.

Use the official [Pico access-point example](https://github.com/raspberrypi/pico-examples/tree/master/pico_w/wifi/access_point) as a reference, not a complete production implementation.

Completion checks: configure from a phone; reconnect after unplugging; recover from a wrong password; recover after the router returns; change routers using the boot gesture; verify presets survive reset. Check inputs with spaces, escaped characters, and maximum supported lengths. Include a simulated write interruption during credential save/reset.

## Milestone 3: device-hosted controls

Build the static page against a small mock API first, while Ron uses milestone 1 or 2. Then embed the assets and connect the actual C handlers. Use the official [Pico HTTP server example](https://github.com/raspberrypi/pico-examples/tree/master/pico_w/wifi/httpd) as a starting point. No JavaScript framework or runtime is needed on the device.

### Page behavior

1. Serve the page at `http://<lamp-ip>/`. On opening, fetch the lamp's state before enabling controls. Display connection status, power, brightness, active preset, and four preset slots.
2. Fetch state every second while the page is visible. Allow only one poll in flight. Pause when hidden or closed; abort pending work on page exit. Refresh immediately when visible again. A two-minute visit produces about 120 state polls, with no cloud requests.
3. Give each request a three-second timeout. On failure, show disconnected/stale state and retry with backoff up to five seconds while visible. Disable writes until a fresh state response arrives. Do not queue offline commands for later replay.
4. Send changes directly when the user clicks Apply or a power/preset control. Do not wait for the next poll to send a command. Refresh after a successful mutation. Never send a command merely because a poll updated a control's displayed value.
5. Keep unsaved editor drafts separate from polled device state. All assets and API requests stay on the lamp's origin. No fonts, scripts, analytics, or other internet dependencies.

### HTTP contract

Use JSON with an 8 KiB maximum API body. Bound headers, parsing buffers, connections, and timeouts as well. Reject invalid input without partial changes. Serve assets from embedded flash data rather than copying whole files into RAM.

| Route | Purpose |
| --- | --- |
| `GET /api/state` | Return schema version, boot ID, state revision, power, brightness, active slot, and whether settings are durably saved |
| `GET /api/presets` | Return the four complete named slot definitions and their revision |
| `POST /api/state` | Set absolute power, brightness, and/or active slot; return applied state |
| `PUT /api/presets/{0..3}` | Validate and save one complete preset slot; acknowledge only after its flash commit |

Every button action and accepted state mutation advances the RAM state revision. Every boot has a fresh boot ID. Writes include the boot ID and expected revision; reject stale writes with HTTP 409 and refresh the page's state. This avoids delayed/retried requests overwriting newer button actions. Do not silently retry a rejected write. Do not advance revisions or write flash for polls, unchanged values, or animation frames. Accept only one browser write in flight at a time.

Apply ordinary state changes in RAM immediately and persist through the existing two-second coalescing path. Report pending persistence accurately. A preset-save response must not claim success before its durable commit, and any failed commit must leave the previously saved slot recoverable. The UI labels drafts, applied settings, and saved settings accurately.

No user login is required. Use HTTP on the trusted LAN during development, then inside the WireGuard interface for the final configuration. Both the Pico and viewing device terminate their own tunnels to the cloud hub; the hub remains a trusted intermediary. Keep provisioning routes inaccessible in station mode, use same-origin requests without permissive CORS, validate Host/Origin, and require a per-boot page token in a custom header for writes. This token prevents unrelated websites from issuing browser writes; it is not an access restriction on clients that can already reach the device.

### Verification

Verify controls from a phone on home Wi-Fi and again with the router's internet connection unavailable. Check a button press appears on the page within the polling interval, hidden pages stop polling, returning pages refresh, dropped connections recover, stale writes are rejected, and saving a slot survives reboot. Verify malformed and oversized HTTP requests cannot corrupt settings or stall the button/LED loop.

Deliver the working LAN controls before milestone 5 integrates the embedded VPN. Do not present this development build as VPN-only.

## Milestone 4: custom patterns

1. Add a visual editor showing LEDs 1-15 in chain order. Use zero-based indices 0-14 in the wire format.
2. Allow selection of all LEDs, one LED, or any group. Applying a color changes the selected LEDs. Store groups only as editor selections for this version; save the resulting 15 colors, not a separate group-execution system.
3. Store a custom pattern as a schema version, name of at most 48 UTF-8 bytes, and exactly 15 RGB triples with integer channels 0-255. Brightness remains a separate lamp setting. Validate this contract in the browser and C handlers.
4. Save a named pattern directly into one of four device slots, replacing that slot's definition. Allow choosing a built-in preset again. The complete slot definitions live on the Pico and survive closed browsers, internet loss, and power cycles. No separate pattern database is needed.
5. Make Save to slot explicit and wait for its persisted acknowledgement. Assignment does not change the selected slot; assigning the active slot updates its displayed pattern. Optional browser localStorage can hold drafts, but it is not the source of truth for saved slots. Handle unavailable/cleared browser storage without breaking the editor. Do not store Wi-Fi credentials there.

Completion checks: edit the whole lamp, one LED, and a non-contiguous group; verify physical LED ordering; reject malformed arrays/out-of-range channels; save a pattern into a slot; unplug internet and cycle through all four slots; reboot and verify the custom slot remains.

## Milestone 5: Pico as a WireGuard peer

1. First prove a minimal WireGuard connection on the actual Pico 2 W with the pinned C SDK. Candidate references are the C [wireguard-lwip implementation](https://github.com/smartalock/wireguard-lwip) and a [Pico W adaptation](https://github.com/Mr-Pine/pi-pico-wireguard-lwip). These are third-party candidates, not verified dependencies. An [Arduino Pico/2 W port](https://github.com/jaszczurtd/arduino-wireguard-pico-w) also exists, but does not establish compatibility with this plain-C SDK build. Inspect licenses, platform hooks, entropy, handshake timestamps across reboot, and current compatibility before selection. Do not implement cryptography from scratch or silently switch frameworks.
2. Measure firmware size, RAM, handshake/rekey cost, and HTTP responsiveness while the LEDs animate. Verify that networking remains compatible with the cooperative event loop. Record failures concretely; if the client cannot fit or operate reliably, report the constraint and request a choice before changing the hardware/framework or moving the VPN to a separate gateway.
3. Add persistent configuration for the device private key, VPN address, cloud endpoint host/port, cloud peer public key, optional preshared key, and allowed VPN routes. Accept these fields through the physically activated setup AP and a documented USB command. Validate them explicitly; do not execute imported `wg-quick` shell directives. No keys are needed to build or test the offline lamp.
4. Reconnect the tunnel after Wi-Fi recovery and re-resolve the cloud endpoint when necessary. Use a 25-second persistent keepalive as the initial NAT traversal setting, following the [WireGuard guidance](https://www.wireguard.com/quickstart/). This is small background VPN traffic even when the webpage is closed; it is separate from browser state polling and passes through Ron's cloud server, not Vercel.
5. Document the public key and VPN address Ron needs to add to the cloud server, plus the required peer-forwarding/firewall and client-route settings. Do not alter that server without authorization. Bind the normal control page/API to the VPN interface only, retain the physically activated setup AP for recovery, and preserve VPN settings during Wi-Fi reset.

Completion checks: access the page from an authorized VPN phone on cellular data; read and save settings; confirm home-LAN and non-VPN control requests fail in the final build; recover after Wi-Fi/server outages and reboot; remain reachable after an idle period; preserve LED/button behavior during rekeys. Verify actual interface isolation, not just destination-address binding. Physical controls continue to work if the VPN is unavailable. Report any missing peer configuration or unperformed hardware checks as outstanding.

## Delivery and review rules

- Each milestone must include reproducible commands, its usable artifact, focused verification results, and outstanding hardware/service dependencies. Keep documentation synchronized with the code.
- Use feature branches and pull requests to `main`. Do not push directly to `main`, merge for Ron, or deploy production from unreviewed local work. Production must remain equal to or behind `main`. This version needs no hosted application deployment.
- Provide build/flash instructions and router/VPN instructions without secrets. Do not reconfigure Ron's router or WireGuard peers without the relevant configuration and authorization.
- Do not claim hardware behavior, VPN reachability, or persistent storage reliability was tested unless it was actually exercised.
- Defer hosted lamp applications/databases, login, SQLite, ESP32 firmware, OTA updates, custom animated timelines, Bluetooth, and a dedicated mobile app. The existing cloud WireGuard hub is part of the chosen network path.

Suggested prompt for the implementing model:

> Read README.md, PLAN.md, and applicable AGENTS.md instructions. Implement PLAN.md in milestone order, beginning with the usable offline Pico 2 W lamp. Use the existing lamp/ and server/ directories. Preserve each working firmware artifact. Complete and hand off milestone 1 before continuing the web work. Follow the documented module boundaries and behavior. Record actual verification and remaining dependencies without claiming simulated checks were hardware tests. Use the stated defaults unless a concrete incompatibility requires a change, and explain that incompatibility before changing scope.

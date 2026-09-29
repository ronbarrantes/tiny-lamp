# ESP32-C3 Wi-Fi test

Standalone firmware for the ESP32-C3 Super Mini. Power and flash it directly over USB. No lamp, LEDs, buttons, or card hardware are involved.

## Try it

1. Flash the firmware using the commands below.
2. Join `TinyLamp-Setup-XX:XX` from your phone or computer. The suffix comes from the board's MAC address. The setup password is `12345678`.
3. Stay connected even if your device says there is no internet. Open **http://192.168.4.1** manually. This test does not open a captive portal automatically.
4. Click **Scan for Wi-Fi**, then choose your router from **Nearby networks**. The picker fills in the Wi-Fi name, also called its SSID. Enter the password and click **Connect**. You can also type the name manually, including for hidden networks. Use a **2.4 GHz** network. This form supports home Wi-Fi and open networks, not enterprise Wi-Fi with individual usernames.
5. The page refreshes while the ESP32 tries to connect. On success, it shows the router-assigned IP address and saves the credentials. The setup network closes after one minute.
6. Rejoin your normal Wi-Fi and open the displayed `http://` address. You should see the connected status page served by the ESP32. Your router must allow devices to talk to one another; guest-network isolation can block this.

If the page drops while connecting, rejoin the setup network and reload it. The ESP32 has one radio, so joining the router can change the setup network's channel. The USB serial log also prints the assigned IP address.

Scanning runs in the background and the page refreshes until it finishes. Results show up to 40 network entries with signal strength in dBm and whether they require security. A value closer to zero means a stronger signal. Scan before entering a password, since scanning reloads the form. The ESP32-C3 cannot discover 5 GHz-only networks.

If connecting times out, the page and USB serial log show an explanation based on the last ESP32 disconnect reason, plus its numeric code when available. These are diagnostic clues, not proof of a specific cause. Authentication timeouts can come from a wrong password, router settings, or poor signal.

Credentials survive power loss. On boot, the board tries the saved network for 30 seconds. If it cannot connect, it opens setup again. A wrong password leaves setup available for another attempt and does not overwrite previously saved credentials. If the router drops later, the board retries for 30 seconds before reopening setup.

To change a working network, open the serial monitor at 115200 baud and send `setup` followed by Enter. This disconnects the router and reopens setup. Saved credentials remain until another connection succeeds.

To erase saved Wi-Fi, hold **BOOT for five seconds while the board is running**, then release it. This clears only this experiment's Wi-Fi settings and reopens the setup network. A short BOOT press does nothing. The Super Mini BOOT button uses GPIO9.

**RESET** restarts the board without erasing credentials. Holding BOOT while pressing RESET enters the chip's flashing mode, so use BOOT alone for the Wi-Fi reset.

## Build and flash

Uses the Arduino ESP32 core and its bundled WiFi, WebServer, and Preferences libraries. An Arduino board and the Arduino IDE are not required. Install `arduino-cli`, then run from the repository root:

```sh
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc --build-path /tmp/tiny-lamp-wifi-build esp32-wifi
arduino-cli board list
arduino-cli upload --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc --port /dev/cu.usbmodem101 --input-dir /tmp/tiny-lamp-wifi-build esp32-wifi
arduino-cli monitor --port /dev/cu.usbmodem101 --config baudrate=115200
```

Replace the USB port with the one shown by `board list`. If upload cannot connect, hold BOOT, press and release RESET, then release BOOT and retry. Close any serial monitor before uploading. If needed, press RESET once after the upload.

For Arduino IDE, select **ESP32C3 Dev Module**, enable **USB CDC On Boot**, select the USB port, and open `esp32-wifi.ino`.

## Code boundaries

`esp32-wifi.ino` only calls `wifiSetupBegin()` and `wifiSetupLoop()`. `wifi_setup.cpp` owns setup Wi-Fi, the HTTP form, connection handling, and credential storage. Call those same two functions when integrating this experiment later.

The HTTP form accepts credential changes only through the setup network. The router-facing page only shows status. Router credentials are stored in the board's NVS flash under `wifi-test`, never printed or included in a page. This is a local test with HTTP and unencrypted credential storage, not a production provisioning system. Change the shared setup password before broader use.

## Hardware checks

Run `python3 esp32-wifi/tests/test_connection_timer.py` from the repository root for the host-side connection timer regression check. It requires a C++ compiler and executes the firmware's actual loop with simulated Wi-Fi and time. It checks that a slow Wi-Fi start does not immediately trigger the unsigned timeout calculation, and that a failed connection gets the full 30 seconds. It does not test the radio or router.

Compiled with Arduino ESP32 core 3.3.12 and flashed over USB to the connected ESP32-C3 with 4 MB flash. Serial output confirmed `TinyLamp-Setup-FD:E4` started at `192.168.4.1`. Router connection, persistence, and the physical BOOT long press still need the checks below.

- Enter a wrong password. Setup should remain available after 30 seconds.
- Scan for Wi-Fi. Select a result and check that it fills the SSID field. If connecting fails, record the displayed explanation and reason code.
- Enter the correct password. Open the status page from your router's network.
- Unplug and reconnect USB. The ESP32 should rejoin without asking for credentials.
- Send `setup` in the serial monitor. The temporary network should return.
- Hold BOOT for five seconds, release it, then press RESET. Setup should return without reconnecting to the old network.
- Turn off the router or move out of range. Setup should reopen after the reconnect attempt times out.

API references: [Espressif Wi-Fi API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html) and [Preferences storage](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html).

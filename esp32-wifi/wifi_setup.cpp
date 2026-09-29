#include "wifi_setup.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <cctype>
#include <cstring>
#include <atomic>
#include <esp_wifi.h>

namespace {
constexpr char setupPassword[] = "12345678";
constexpr uint32_t connectTimeoutMs = 30000;
constexpr uint32_t setupGraceMs = 60000;
constexpr uint8_t bootButtonPin = 9;
constexpr uint32_t resetHoldMs = 5000;

struct Credentials {
  char ssid[33];
  char password[65];
};

enum class State { setup, pending, connecting, connected };
WebServer server(80);
Preferences preferences;
Credentials credentials{};
State state = State::setup;
bool storageReady = false;
bool setupActive = false;
bool saveOnConnect = false;
bool closeSetup = false;
uint32_t stateSince = 0;
uint32_t connectedSince = 0;
String notice;
String serialCommand;
bool buttonDown = false;
bool buttonHandled = false;
uint32_t buttonSince = 0;
bool scanning = false;
int16_t scanCount = -1;
std::atomic<uint16_t> disconnectReason{0};

String connectionFailure() {
  const uint16_t reason = disconnectReason.load();
  String message;
  switch (reason) {
    case WIFI_REASON_NO_AP_FOUND:
      message = "Router not found. Scan for networks and check that your router has 2.4 GHz Wi-Fi enabled.";
      break;
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
      message = "Wi-Fi authentication failed or timed out. Check the password and router security settings. Weak signal can also cause this.";
      break;
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
      message = "No router with compatible Wi-Fi security found. Check the router security settings.";
      break;
    case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
      message = "Router signal is too weak. Move the ESP32 closer and try again.";
      break;
    case WIFI_REASON_ASSOC_FAIL:
    case WIFI_REASON_ASSOC_TOOMANY:
      message = "Router association failed. Check router access restrictions and connected-device limits.";
      break;
    default:
      message = "Could not get a working Wi-Fi connection within 30 seconds. Check the network, signal, and router DHCP settings.";
  }
  if (reason) message += " ESP32 disconnect reason: " + String(reason) + ".";
  return message;
}

void clearScan() {
  if (scanning) esp_wifi_scan_stop();
  WiFi.scanDelete();
  scanning = false;
  scanCount = -1;
}

String escapeHtml(String value) {
  value.replace("&", "&amp;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  value.replace("\"", "&quot;");
  value.replace("'", "&#39;");
  return value;
}

bool isSetupClient() {
  return setupActive && server.client().localIP() == WiFi.softAPIP();
}

void sendPage() {
  const bool joining = state == State::pending || state == State::connecting;
  String page = F("<!doctype html><html lang='en'><meta charset='utf-8'>"
                  "<meta name='viewport' content='width=device-width,initial-scale=1'>");
  if (joining || scanning || (isSetupClient() && state == State::connected)) {
    page += F("<meta http-equiv='refresh' content='3'>");
  }
  page += F("<title>ESP32 Wi-Fi test</title><style>"
            "body{font:18px system-ui;max-width:440px;margin:48px auto;padding:0 20px;"
            "background:#f4f5f0;color:#17291c}label{display:block;margin-top:20px}"
            "input,button,select{font:inherit;box-sizing:border-box;width:100%;padding:12px;"
            "margin-top:8px}button{background:#234e35;color:white;border:0;border-radius:8px}"
            "p{line-height:1.5}</style><h1>ESP32 Wi-Fi test</h1>");
  if (state == State::connected) {
    const String address = WiFi.localIP().toString();
    page += "<p>Connected to <strong>" + escapeHtml(WiFi.SSID()) + "</strong>.</p>";
    page += "<p>Join that Wi-Fi network on your phone or computer, then open "
            "<a href='http://" + address + "'>http://" + address + "</a>.</p>";
    if (isSetupClient() && closeSetup) {
      page += F("<p>The temporary setup network closes one minute after connection.</p>");
    }
  } else if (joining) {
    page += F("<p>Connecting to your router. This can take up to 30 seconds. "
              "This page refreshes automatically.</p>");
  }
  if (notice.length()) page += "<p>" + escapeHtml(notice) + "</p>";
  if (isSetupClient() && !joining && state != State::connected) {
    page += F("<p>Choose a nearby 2.4 GHz network, or enter its name manually.</p>");
    if (scanning) {
      page += F("<p>Scanning for Wi-Fi. This page will refresh when results are ready.</p>");
    } else {
      page += F("<form method='post' action='/scan'><button>Scan for Wi-Fi</button></form>");
      if (scanCount == 0) page += F("<p>No networks found. Move closer to your router and scan again.</p>");
      if (scanCount > 0) {
        page += F("<label for='networks'>Nearby networks</label><select id='networks' "
                  "onchange=\"document.getElementById('ssid').value=this.value\">"
                  "<option value=''>Choose a network</option>");
        for (int16_t i = 0; i < scanCount && i < 40; ++i) {
          const String ssid = WiFi.SSID(i);
          if (ssid.isEmpty()) continue;
          const String safeName = escapeHtml(ssid);
          page += "<option value='" + safeName + "'>" + safeName + " (" + String(WiFi.RSSI(i)) + " dBm, ";
          page += WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "secured";
          page += F(")</option>");
        }
        page += F("</select><p>Signal closer to zero is stronger. Hidden networks need manual entry.</p>");
      }
      page += F(
              "<form method='post' action='/connect'>"
              "<label for='ssid'>Wi-Fi name (SSID)</label>"
              "<input id='ssid' name='ssid' maxlength='32' required autocapitalize='none' spellcheck='false'>"
              "<label for='password'>Wi-Fi password</label>"
              "<input id='password' name='password' type='password' maxlength='64' autocomplete='off'>"
              "<p>Leave the password blank only for an open network.</p>"
              "<button>Connect</button></form>");
    }
  }
  page += F("</html>");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html; charset=utf-8", page);
}

void startSetup() {
  const String name = "TinyLamp-Setup-" + WiFi.macAddress().substring(12);
  if (!setupActive) {
    WiFi.mode(WIFI_AP_STA);
    setupActive = WiFi.softAP(name.c_str(), setupPassword);
  }
  if (!setupActive) {
    Serial.println("Could not start setup Wi-Fi. Restart the board to retry.");
    return;
  }
  Serial.printf("Setup Wi-Fi: %s\nSetup password: %s\n", name.c_str(), setupPassword);
  Serial.print("Setup page: http://");
  Serial.println(WiFi.softAPIP());
}

void startConnection() {
  clearScan();
  disconnectReason.store(0);
  WiFi.disconnect();
  WiFi.begin(credentials.ssid, credentials.password);
  state = State::connecting;
  stateSince = millis();
  notice = "";
  Serial.println("Connecting to router...");
}

void reopenSetup() {
  clearScan();
  WiFi.disconnect();
  state = State::setup;
  saveOnConnect = false;
  closeSetup = false;
  notice = "";
  startSetup();
}

// Trigger once per long press. A short press leaves saved Wi-Fi alone.
void serviceResetButton(uint32_t now) {
  if (digitalRead(bootButtonPin) != LOW) {
    buttonDown = false;
    buttonHandled = false;
    return;
  }
  if (!buttonDown) {
    buttonDown = true;
    buttonSince = now;
  }
  if (!buttonHandled && now - buttonSince >= resetHoldMs) {
    buttonHandled = true;
    if (!storageReady || !preferences.clear()) {
      Serial.println("Could not erase Wi-Fi settings. Restart and try again.");
      return;
    }
    credentials = {};
    reopenSetup();
    notice = "Saved Wi-Fi erased. Enter a network to connect again.";
    Serial.println(notice);
  }
}

void handleConnect() {
  if (!isSetupClient()) {
    server.send(403, "text/plain", "Join the setup Wi-Fi to change credentials.");
    return;
  }
  if (state != State::setup || scanning) {
    server.send(409, "text/plain", "A connection is already in progress or complete.");
    return;
  }
  const String ssid = server.arg("ssid");
  const String password = server.arg("password");
  bool validPassword = password.isEmpty() || (password.length() >= 8 && password.length() <= 63);
  if (password.length() == 64) {
    validPassword = true;
    for (size_t i = 0; i < password.length(); ++i) {
      if (!isxdigit(static_cast<unsigned char>(password[i]))) validPassword = false;
    }
  }
  if (ssid.isEmpty() || ssid.length() > 32 || !validPassword ||
      memchr(ssid.c_str(), '\0', ssid.length()) || memchr(password.c_str(), '\0', password.length())) {
    server.send(400, "text/plain", "Use a Wi-Fi name of 1-32 bytes and a password of 8-63 characters, "
                "64 hexadecimal digits, or blank for an open network. Go back and try again.");
    return;
  }
  credentials = {};
  ssid.toCharArray(credentials.ssid, sizeof(credentials.ssid));
  password.toCharArray(credentials.password, sizeof(credentials.password));
  saveOnConnect = true;
  state = State::pending;
  stateSince = millis();
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "Connecting...");
}

void handleScan() {
  if (!isSetupClient()) {
    server.send(403, "text/plain", "Join the setup Wi-Fi to scan.");
    return;
  }
  if (state != State::setup || scanning) {
    server.send(409, "text/plain", "Wait for the current scan or connection attempt to finish.");
    return;
  }
  clearScan();
  notice = "";
  const int16_t result = WiFi.scanNetworks(true);
  scanning = result == WIFI_SCAN_RUNNING;
  if (!scanning) {
    scanCount = result;
    if (result < 0) notice = "Could not start a Wi-Fi scan. Try again.";
  }
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "Scanning...");
}
}  // namespace

void wifiSetupBegin() {
  pinMode(bootButtonPin, INPUT_PULLUP);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_STA);
  // Wi-Fi callbacks run on a separate task; share only an atomic reason code.
  WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) {
    disconnectReason.store(info.wifi_sta_disconnected.reason);
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  storageReady = preferences.begin("wifi-test", false);
  const bool haveCredentials = storageReady &&
      preferences.getBytesLength("credentials") == sizeof(credentials) &&
      preferences.getBytes("credentials", &credentials, sizeof(credentials)) == sizeof(credentials) &&
      credentials.ssid[0] != '\0' && credentials.ssid[32] == '\0' && credentials.password[64] == '\0';

  server.on("/", HTTP_GET, sendPage);
  server.on("/connect", HTTP_POST, handleConnect);
  server.on("/scan", HTTP_POST, handleScan);
  server.onNotFound([] { server.send(404, "text/plain", "Open http://192.168.4.1/ on the setup Wi-Fi."); });
  server.begin();
  if (haveCredentials) startConnection();
  else startSetup();
  Serial.println("Send setup followed by Enter in Serial Monitor to reopen Wi-Fi setup.");
  Serial.println("Hold BOOT for five seconds while running to erase saved Wi-Fi.");
}

void wifiSetupLoop() {
  server.handleClient();
  if (scanning) {
    const int16_t result = WiFi.scanComplete();
    if (result != WIFI_SCAN_RUNNING) {
      scanning = false;
      scanCount = result;
      if (result < 0) notice = "Wi-Fi scan failed. Try again.";
      Serial.printf("Wi-Fi scan completed: %d networks\n", result);
    }
  }
  while (Serial.available()) {
    const char character = Serial.read();
    if (character == '\n' || character == '\r') {
      if (serialCommand == "setup") {
        reopenSetup();
      }
      serialCommand = "";
    } else if (serialCommand.length() < 32) {
      serialCommand += character;
    }
  }

  const uint32_t now = millis();
  serviceResetButton(now);
  // Give the browser time to receive the redirect before changing radio channels.
  if (state == State::pending && now - stateSince >= 500) {
    startConnection();
    // startConnection updates stateSince after Wi-Fi calls. Sample time again
    // next loop so unsigned subtraction cannot use an older timestamp.
    return;
  }
  if (state == State::connecting) {
    if (WiFi.status() == WL_CONNECTED) {
      state = State::connected;
      connectedSince = now;
      closeSetup = true;
      if (saveOnConnect) {
        if (!storageReady || preferences.putBytes("credentials", &credentials, sizeof(credentials)) != sizeof(credentials)) {
          notice = "Connected, but could not save credentials. Setup stays open. Restart and try again.";
          closeSetup = false;
        } else {
          notice = "Wi-Fi saved. The board will reconnect after a restart.";
        }
        saveOnConnect = false;
      }
      Serial.print("Connected. Open http://");
      Serial.println(WiFi.localIP());
      if (notice.length()) Serial.println(notice);
    } else if (now - stateSince >= connectTimeoutMs) {
      notice = connectionFailure();
      WiFi.disconnect();
      state = State::setup;
      saveOnConnect = false;
      startSetup();
      Serial.println(notice);
    }
  }
  if (state == State::connected) {
    if (WiFi.status() != WL_CONNECTED) {
      startConnection();
    } else if (setupActive && closeSetup && now - connectedSince >= setupGraceMs) {
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      setupActive = false;
      Serial.println("Setup Wi-Fi closed.");
    }
  }
}

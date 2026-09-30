#include "pov.h"
#include "pattern.h"
#include "lamp.h"
#include "web_assets.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include <esp32-hal-rmt.h>

namespace {
constexpr uint8_t ledPin = 0;
constexpr uint8_t buttonPin = 1;
constexpr uint32_t debounceMs = 30;
Pattern pattern;
Pattern incoming;
LampSettings lamp;
bool lampMode = false;
Preferences storage;
SemaphoreHandle_t patternMutex = nullptr;
bool storageReady = false;
bool outputReady = false;
bool powerOn = false;
uint32_t revision = 0;

void setPower(bool enabled) {
  xSemaphoreTake(patternMutex, portMAX_DELAY);
  powerOn = enabled;
  ++revision;
  xSemaphoreGive(patternMutex);
}

// A separate task keeps HTTP requests and flash writes out of LED timing.
void playbackTask(void*) {
  rmt_data_t symbols[size_t(maxLeds) * 24 + 1]{};
  uint8_t column = 0;
  uint32_t seenRevision = UINT32_MAX;
  uint32_t previousFrame = 0;
  bool wasOn = true;
  bool candidatePressed = false;
  bool stablePressed = false;
  uint32_t buttonChangedAt = 0;
  while (true) {
    const uint32_t nowMs = millis();
    const bool pressed = digitalRead(buttonPin) == LOW;
    if (pressed != candidatePressed) {
      candidatePressed = pressed;
      buttonChangedAt = nowMs;
    }
    if (candidatePressed != stablePressed && nowMs - buttonChangedAt >= debounceMs) {
      stablePressed = candidatePressed;
      if (stablePressed) {
        xSemaphoreTake(patternMutex, portMAX_DELAY);
        powerOn = !powerOn;
        ++revision;
        xSemaphoreGive(patternMutex);
      }
    }

    const uint32_t nowUs = micros();
    xSemaphoreTake(patternMutex, portMAX_DELAY);
    const bool enabled = powerOn;
    const bool changed = seenRevision != revision;
    if (changed) {
      seenRevision = revision;
      column = 0;
    }
    const uint32_t intervalUs = lampMode ? 20000 : uint32_t(pattern.columnMs) * 1000;
    const uint8_t ledCount = lampMode ? lamp.ledCount : pattern.ledCount;
    const uint8_t brightness = lampMode ? lamp.brightness : pattern.brightness;
    const bool sendFrame = changed || (enabled && nowUs - previousFrame >= intervalUs) || (wasOn && !enabled);
    if (sendFrame) {
      size_t symbol = 0;
      // Always clear trailing LEDs when the configured count shrinks.
      for (uint8_t led = 0; led < maxLeds; ++led) {
        Color color{};
        if (enabled && led < ledCount) {
          color = lampMode ? lampColor(lamp, led, nowMs) : pattern.pixels[size_t(column) * pattern.ledCount + led];
        }
        const uint8_t grb[] = {
          uint8_t(uint16_t(color.green) * brightness / 100),
          uint8_t(uint16_t(color.red) * brightness / 100),
          uint8_t(uint16_t(color.blue) * brightness / 100)
        };
        for (uint8_t value : grb) {
          for (int bit = 7; bit >= 0; --bit) {
            const bool one = value & (1 << bit);
            symbols[symbol].level0 = 1;
            symbols[symbol].duration0 = one ? 8 : 4;
            symbols[symbol].level1 = 0;
            symbols[symbol].duration1 = one ? 5 : 9;
            ++symbol;
          }
        }
      }
      // 300 microseconds low at a 10 MHz tick rate latches the frame.
      symbols[symbol].level0 = 0;
      symbols[symbol].duration0 = 3000;
      symbols[symbol].level1 = 0;
      symbols[symbol].duration1 = 0;
      column = (column + 1) % pattern.columnCount;
    }
    xSemaphoreGive(patternMutex);
    if (sendFrame) {
      if (!rmtWrite(ledPin, symbols, sizeof(symbols) / sizeof(symbols[0]), 20)) {
        Serial.println("LED transfer failed.");
      }
      previousFrame = nowUs;
    }
    wasOn = enabled;
    vTaskDelay(1);
  }
}

String stateJson(bool includePattern) {
  xSemaphoreTake(patternMutex, portMAX_DELAY);
  String body = "{\"on\":" + String(powerOn ? "true" : "false") +
                ",\"ready\":" + String(outputReady ? "true" : "false") +
                ",\"mode\":\"" + String(lampMode ? "lamp" : "pov") + "\"";
  char lampHex[7];
  snprintf(lampHex, sizeof(lampHex), "%02x%02x%02x", lamp.color.red, lamp.color.green, lamp.color.blue);
  body += ",\"lamp\":{\"preset\":" + String(uint8_t(lamp.preset)) +
          ",\"ledCount\":" + String(lamp.ledCount) + ",\"brightness\":" + String(lamp.brightness) +
          ",\"color\":\"" + String(lampHex) + "\"}";
  if (includePattern) {
    body += ",\"ledCount\":" + String(pattern.ledCount) + ",\"columns\":" + String(pattern.columnCount) +
            ",\"columnMs\":" + String(pattern.columnMs) + ",\"brightness\":" + String(pattern.brightness) + ",\"pixels\":\"";
    body.reserve(body.length() + size_t(pattern.ledCount) * pattern.columnCount * 6 + 3);
    char hex[7];
    for (size_t i = 0; i < size_t(pattern.ledCount) * pattern.columnCount; ++i) {
      const Color& color = pattern.pixels[i];
      snprintf(hex, sizeof(hex), "%02x%02x%02x", color.red, color.green, color.blue);
      body += hex;
    }
    body += '"';
  }
  body += '}';
  xSemaphoreGive(patternMutex);
  return body;
}
}

void povBegin() {
  pinMode(buttonPin, INPUT_PULLUP);
  patternMutex = xSemaphoreCreateMutex();
  if (!patternMutex) {
    Serial.println("Could not allocate the LED mutex. Restart the board.");
    return;
  }
  storageReady = storage.begin("pov-picture", false);
  if (storageReady && storage.getBytesLength("image-v1") == sizeof(Pattern) &&
      storage.getBytes("image-v1", &incoming, sizeof(incoming)) == sizeof(incoming) && validPattern(incoming)) {
    pattern = incoming;
  }
  lamp.ledCount = pattern.ledCount;
  LampSettings savedLamp;
  if (storageReady && storage.getBytesLength("lamp-v1") == sizeof(savedLamp) &&
      storage.getBytes("lamp-v1", &savedLamp, sizeof(savedLamp)) == sizeof(savedLamp) && validLamp(savedLamp)) lamp = savedLamp;
  lampMode = storageReady && storage.getUChar("mode", 0) == 1;
  outputReady = rmtInit(ledPin, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_2, 10000000);
  if (outputReady) {
    rmtSetEOT(ledPin, 0);
    outputReady = xTaskCreate(playbackTask, "pov-leds", 6144, nullptr, 2, nullptr) == pdPASS;
  }
  Serial.println(outputReady ? "Lamp/POV ready: LED DIN GPIO0, on/off button GPIO1 to GND." : "LED output could not start. Restart the board.");
}

void povRegisterRoutes(WebServer& server) {
  server.on("/editor", HTTP_GET, [&server] {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", editorHtml);
  });
  server.on("/api/pattern", HTTP_GET, [&server] {
    if (!patternMutex) { server.send(503, "text/plain", "LED state unavailable."); return; }
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", stateJson(true));
  });
  server.on("/api/pattern", HTTP_POST, [&server] {
    if (!outputReady) { server.send(503, "text/plain", "LED output unavailable. Restart the board."); return; }
    const String body = server.arg("plain");
    if (!parsePattern(body.c_str(), body.length(), incoming)) {
      server.send(400, "text/plain", "Invalid picture. Use 1-32 LEDs, 1-64 columns, 2-100 ms, 1-40% brightness, and one RGB color per pixel.");
      return;
    }
    if (!storageReady || storage.putBytes("image-v1", &incoming, sizeof(incoming)) != sizeof(incoming)) {
      server.send(500, "text/plain", "Could not save the picture. The previous picture is still active.");
      return;
    }
    if (storage.putUChar("mode", 0) != 1) {
      server.send(500, "text/plain", "Picture saved, but could not select POV mode. Try again.");
      return;
    }
    xSemaphoreTake(patternMutex, portMAX_DELAY);
    pattern = incoming;
    lampMode = false;
    powerOn = true;
    ++revision;
    xSemaphoreGive(patternMutex);
    server.send(200, "application/json", stateJson(false));
  });
  server.on("/api/mode", HTTP_POST, [&server] {
    if (!outputReady) { server.send(503, "text/plain", "LED output unavailable."); return; }
    const String mode = server.arg("plain");
    if (mode != "lamp" && mode != "pov") { server.send(400, "text/plain", "Use lamp or pov."); return; }
    const bool nextLampMode = mode == "lamp";
    if (!storageReady || storage.putUChar("mode", nextLampMode ? 1 : 0) != 1) {
      server.send(500, "text/plain", "Could not save mode. Try again.");
      return;
    }
    xSemaphoreTake(patternMutex, portMAX_DELAY);
    lampMode = nextLampMode;
    ++revision;
    xSemaphoreGive(patternMutex);
    server.send(200, "application/json", stateJson(false));
  });
  server.on("/api/lamp", HTTP_POST, [&server] {
    if (!outputReady) { server.send(503, "text/plain", "LED output unavailable."); return; }
    const String body = server.arg("plain");
    LampSettings nextLamp;
    if (!parseLamp(body.c_str(), body.length(), nextLamp)) {
      server.send(400, "text/plain", "Invalid lamp setting. Use preset 0-4, 1-32 LEDs, 1-40% brightness, and an RGB color.");
      return;
    }
    if (!storageReady || storage.putBytes("lamp-v1", &nextLamp, sizeof(nextLamp)) != sizeof(nextLamp) ||
        storage.putUChar("mode", 1) != 1) {
      server.send(500, "text/plain", "Could not save lamp settings. Try again.");
      return;
    }
    xSemaphoreTake(patternMutex, portMAX_DELAY);
    lamp = nextLamp;
    lampMode = true;
    powerOn = true;
    ++revision;
    xSemaphoreGive(patternMutex);
    server.send(200, "application/json", stateJson(false));
  });
  server.on("/api/power", HTTP_GET, [&server] {
    if (!patternMutex) { server.send(503, "text/plain", "LED state unavailable."); return; }
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", stateJson(false));
  });
  server.on("/api/power", HTTP_POST, [&server] {
    if (!outputReady) { server.send(503, "text/plain", "LED output unavailable. Restart the board."); return; }
    const String command = server.arg("plain");
    if (command != "on" && command != "off") {
      server.send(400, "text/plain", "Use on or off.");
      return;
    }
    setPower(command == "on");
    server.send(200, "application/json", stateJson(false));
  });
}

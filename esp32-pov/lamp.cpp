#include "lamp.h"

namespace {
uint32_t noise(uint32_t seed) {
  seed ^= seed >> 16;
  seed *= 0x7feb352d;
  seed ^= seed >> 15;
  seed *= 0x846ca68b;
  return seed ^ (seed >> 16);
}

uint8_t triangle(uint32_t time, uint32_t period) {
  const uint32_t phase = time % period;
  const uint32_t half = period / 2;
  return uint8_t((phase <= half ? phase : period - phase) * 255 / half);
}
}

bool validLamp(const LampSettings& settings) {
  return settings.preset <= LampPreset::warm && settings.ledCount >= 1 &&
         settings.ledCount <= maxLeds && settings.brightness >= 1 && settings.brightness <= 40;
}

bool parseLamp(const char* body, size_t length, LampSettings& output) {
  if (length < 9 || body[0] < '0' || body[0] > '4' || body[1] != ',') return false;
  size_t comma = 2;
  while (comma < length && body[comma] != ',') ++comma;
  size_t newline = comma + 1;
  while (newline < length && body[newline] != '\n') ++newline;
  if (comma >= length || newline >= length || length - newline - 1 != 6) return false;
  if (comma - 2 < 1 || comma - 2 > 2) return false;
  uint16_t ledCount = 0;
  for (size_t i = 2; i < comma; ++i) {
    if (body[i] < '0' || body[i] > '9') return false;
    ledCount = ledCount * 10 + body[i] - '0';
  }
  if (ledCount < 1 || ledCount > maxLeds || newline - comma - 1 < 1 || newline - comma - 1 > 2) return false;
  uint16_t brightness = 0;
  for (size_t i = comma + 1; i < newline; ++i) {
    if (body[i] < '0' || body[i] > '9') return false;
    brightness = brightness * 10 + body[i] - '0';
  }
  if (brightness < 1 || brightness > 40) return false;
  uint8_t channels[3]{};
  for (size_t i = 0; i < 6; ++i) {
    const char c = body[newline + 1 + i];
    const int value = c >= '0' && c <= '9' ? c - '0' :
                      c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                      c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
    if (value < 0) return false;
    channels[i / 2] = uint8_t(channels[i / 2] * 16 + value);
  }
  output = {LampPreset(body[0] - '0'), uint8_t(ledCount), uint8_t(brightness), {channels[0], channels[1], channels[2]}};
  return true;
}

Color lampColor(const LampSettings& settings, uint8_t led, uint32_t elapsedMs) {
  switch (settings.preset) {
    case LampPreset::solid: return settings.color;
    case LampPreset::warm: return {255, 170, 95};
    case LampPreset::cozy: {
      const uint16_t glow = 170 + uint16_t(triangle(elapsedMs, 6000)) / 3;
      return {uint8_t(glow), uint8_t(glow * 110 / 255), uint8_t(glow * 35 / 255)};
    }
    case LampPreset::fireplace: {
      const uint32_t frame = elapsedMs / 120;
      const uint32_t seed = uint32_t(led) * 193;
      const uint16_t first = 40 + noise(seed + frame) % 130;
      const uint16_t next = 40 + noise(seed + frame + 1) % 130;
      const uint16_t heat = (first * (120 - elapsedMs % 120) + next * (elapsedMs % 120)) / 120;
      return {uint8_t(190 + heat / 3), uint8_t(heat), uint8_t(heat / 30)};
    }
    case LampPreset::snow: {
      const uint32_t shifted = elapsedMs + uint32_t(led) * 137;
      const bool sparkle = noise(uint32_t(led) * 61 + shifted / 1400) % 4 == 0;
      const uint16_t glow = sparkle ? triangle(shifted, 1400) : 0;
      return {uint8_t(8 + glow * 220 / 255), uint8_t(18 + glow * 220 / 255), uint8_t(40 + glow * 210 / 255)};
    }
  }
  return {};
}

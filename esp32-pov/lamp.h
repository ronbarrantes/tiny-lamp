#pragma once

#include "pattern.h"

enum class LampPreset : uint8_t { solid, fireplace, snow, cozy, warm };

struct LampSettings {
  LampPreset preset = LampPreset::solid;
  uint8_t ledCount = 15;
  uint8_t brightness = 20;
  Color color{255, 179, 71};
};

bool validLamp(const LampSettings& settings);
// preset,ledCount,brightness\n followed by one six-digit RGB color.
bool parseLamp(const char* body, size_t length, LampSettings& output);
// Deterministic animation colors, before the shared brightness scaling.
Color lampColor(const LampSettings& settings, uint8_t led, uint32_t elapsedMs);

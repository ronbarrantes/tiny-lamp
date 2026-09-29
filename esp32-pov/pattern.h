#pragma once

#include <cstddef>
#include <cstdint>

constexpr uint8_t maxLeds = 32;
constexpr uint8_t maxColumns = 64;
constexpr size_t maxPixels = size_t(maxLeds) * maxColumns;

struct Color {
  uint8_t red = 0;
  uint8_t green = 0;
  uint8_t blue = 0;
};

struct Pattern {
  uint8_t ledCount = 11;
  uint8_t columnCount = 10;
  uint16_t columnMs = 5;
  uint8_t brightness = 20;
  // Column-major: each group of ledCount pixels is one physical LED frame.
  Color pixels[maxPixels]{};
};

bool validPattern(const Pattern& pattern);

// Wire format: ledCount,columnCount,columnMs,brightness\n followed by exactly
// ledCount * columnCount RGB colors, each six hexadecimal characters.
// On failure the caller's pattern is untouched.
bool parsePattern(const char* body, size_t length, Pattern& output);

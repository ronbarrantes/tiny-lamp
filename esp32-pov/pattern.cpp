#include "pattern.h"

namespace {
int hexDigit(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}
}

bool validPattern(const Pattern& pattern) {
  return pattern.ledCount >= 1 && pattern.ledCount <= maxLeds &&
         pattern.columnCount >= 1 && pattern.columnCount <= maxColumns &&
         pattern.columnMs >= 2 && pattern.columnMs <= 100 &&
         pattern.brightness >= 1 && pattern.brightness <= 40;
}

bool parsePattern(const char* body, size_t length, Pattern& output) {
  uint16_t values[4]{};
  size_t offset = 0;
  for (size_t field = 0; field < 4; ++field) {
    const size_t start = offset;
    while (offset < length && body[offset] >= '0' && body[offset] <= '9') {
      if (offset - start >= 3) return false;
      values[field] = values[field] * 10 + body[offset++] - '0';
    }
    const char delimiter = field == 3 ? '\n' : ',';
    if (offset == start || offset >= length || body[offset++] != delimiter) return false;
  }
  // Validate before narrowing values to uint8_t.
  if (values[0] < 1 || values[0] > maxLeds || values[1] < 1 || values[1] > maxColumns ||
      values[2] < 2 || values[2] > 100 || values[3] < 1 || values[3] > 40) return false;
  const size_t pixelCount = size_t(values[0]) * values[1];
  if (length - offset != pixelCount * 6) return false;
  for (size_t i = offset; i < length; ++i) {
    if (hexDigit(body[i]) < 0) return false;
  }
  output = {};
  output.ledCount = values[0];
  output.columnCount = values[1];
  output.columnMs = values[2];
  output.brightness = values[3];
  for (size_t pixel = 0; pixel < pixelCount; ++pixel) {
    uint8_t channels[3];
    for (uint8_t channel = 0; channel < 3; ++channel) {
      channels[channel] = hexDigit(body[offset]) * 16 + hexDigit(body[offset + 1]);
      offset += 2;
    }
    output.pixels[pixel] = {channels[0], channels[1], channels[2]};
  }
  return true;
}

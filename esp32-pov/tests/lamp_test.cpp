#include "lamp.h"
#include <cassert>
#include <cstring>
#include <cstdio>

bool equal(Color a, Color b) {
  return a.red == b.red && a.green == b.green && a.blue == b.blue;
}

int main() {
  LampSettings lamp;
  const char* input = "0,15,20\n12aBcD";
  assert(parseLamp(input, strlen(input), lamp));
  assert(lamp.ledCount == 15 && lamp.brightness == 20);
  assert(equal(lampColor(lamp, 0, 0), {0x12, 0xab, 0xcd}));
  assert(equal(lampColor(lamp, 14, 9999), lamp.color));
  const LampSettings previous = lamp;
  const char* invalid[] = {"5,15,20\nffffff", "0,0,20\nffffff", "0,33,20\nffffff", "0,15,0\nffffff", "0,15,41\nffffff", "0,15,20\nzzffff", "0,15,20\nffffff0", "0,15,20\n", "0,15,-1\nffffff", "0,15,20,3\nffffff"};
  for (const char* bad : invalid) {
    assert(!parseLamp(bad, strlen(bad), lamp));
    assert(memcmp(&lamp, &previous, sizeof(lamp)) == 0);
  }
  const char* maximum = "4,32,40\nffffff";
  assert(parseLamp(maximum, strlen(maximum), lamp));
  assert(equal(lampColor(lamp, 0, 0), {255, 170, 95}));
  assert(equal(lampColor(lamp, 31, 999999), {255, 170, 95}));
  lamp.preset = LampPreset::fireplace;
  assert(!equal(lampColor(lamp, 0, 0), lampColor(lamp, 0, 120)));
  lamp.preset = LampPreset::snow;
  assert(!equal(lampColor(lamp, 0, 0), lampColor(lamp, 0, 700)));
  lamp.preset = LampPreset::cozy;
  assert(!equal(lampColor(lamp, 0, 0), lampColor(lamp, 0, 3000)));
  assert(equal(lampColor(lamp, 0, 0), lampColor(lamp, 0, 6000)));
  puts("PASS: solid RGB, saved-setting bounds, invalid input unchanged, warm steady, fire/snow/cozy animation");
}

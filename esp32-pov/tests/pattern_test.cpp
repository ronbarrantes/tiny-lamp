#include "pattern.h"
#include <cassert>
#include <cstring>
#include <string>
#include <cstdio>

int main() {
  Pattern pattern;
  const char* input = "2,2,5,20\nff000000ff000000ffffffff";
  assert(parsePattern(input, strlen(input), pattern));
  assert(pattern.ledCount == 2 && pattern.columnCount == 2);
  assert(pattern.pixels[0].red == 255 && pattern.pixels[0].green == 0);
  assert(pattern.pixels[1].green == 255 && pattern.pixels[2].blue == 255);
  assert(pattern.pixels[3].red == 255 && pattern.pixels[3].green == 255);
  const Pattern previous = pattern;
  const char* invalid[] = {"0,2,5,20\n", "33,1,5,20\n", "1,65,5,20\n", "1,1,1,20\n000000", "1,1,5,41\n000000", "1,1,5,20\nffffff0", "1,1,5,20\nzz0000", "256,1,5,20\n", "1,1,5,20\n", "1,1,5,20\nffffff\n"};
  for (const char* bad : invalid) {
    assert(!parsePattern(bad, strlen(bad), pattern));
    assert(memcmp(&pattern, &previous, sizeof(pattern)) == 0);
  }
  std::string maximum = "32,64,100,40\n";
  for (size_t i = 0; i < maxPixels; ++i) maximum += "aBcDeF";
  assert(parsePattern(maximum.data(), maximum.size(), pattern));
  assert(pattern.pixels[maxPixels - 1].red == 0xab);
  assert(pattern.pixels[maxPixels - 1].green == 0xcd);
  assert(pattern.pixels[maxPixels - 1].blue == 0xef);
  puts("PASS: column-major RGB upload, invalid input preserves picture, maximum dimensions");
}

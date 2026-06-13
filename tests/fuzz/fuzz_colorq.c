/* libFuzzer harness for the pure OSC color-reply parser. */
#include "internal.h"

#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  uint32_t rgb;
  /* Select the query target `what` across the full valid range
   * (LT_COLOR_DEFAULT_BG..255 = 258 values), so FG, BG, and *every* palette
   * index is reachable as the target. One byte spans only 256, so consume two
   * (little-endian) when available; the remainder is the reply payload. */
  const int span = 255 - LT_COLOR_DEFAULT_BG + 1; /* 258: -2..255 inclusive */
  int what = LT_COLOR_DEFAULT_BG;
  size_t off = 0;
  if (size >= 2) {
    unsigned sel = (unsigned)data[0] | ((unsigned)data[1] << 8);
    what = LT_COLOR_DEFAULT_BG + (int)(sel % (unsigned)span);
    off = 2;
  } else if (size == 1) {
    what = LT_COLOR_DEFAULT_BG + (data[0] % span);
    off = 1;
  }
  lt__color_parse_osc_reply((const char *)data + off, size - off, what, &rgb);
  return 0;
}

/* libFuzzer harness for the pure OSC color-reply parser. */
#include "internal.h"

#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  uint32_t rgb;
  /* Cycle `what` over the full valid range (LT_COLOR_DEFAULT_BG..255) using
   * one input byte, so FG / BG / palette-index reply shapes are all reached. */
  int what = LT_COLOR_DEFAULT_BG;
  size_t off = 0;
  if (size) {
    int span = 255 - LT_COLOR_DEFAULT_BG + 1; /* -2..255 inclusive */
    what = LT_COLOR_DEFAULT_BG + (data[0] % span);
    off = 1;
  }
  lt__color_parse_osc_reply((const char *)data + off, size - off, what, &rgb);
  return 0;
}

/* libFuzzer harness for the pure UTF-8 decoder. */
#include "internal.h"

#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  lt_uchar out;
  lt__utf8_decode((const char *)data, size, &out);
  return 0;
}

/* libFuzzer harness for the pure input decoder lt__key_decode.
 * Drives the parser over adversarial byte streams; asserts nothing —
 * ASan/UBSan catch OOB reads and overflow. */
#include "internal.h"

#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  struct lt_event ev;
  size_t consumed;
  /* Use one input bit to reach both the COMPAT and modern control-byte
   * branches without splitting the corpus. */
  int mode = (size && (data[0] & 1)) ? LT_INPUT_COMPAT : 0;
  lt__key_decode((const unsigned char *)data, size, mode, &ev, &consumed);
  return 0;
}

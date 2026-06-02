/*
 * libterm - pure key-sequence decoder.
 *
 * Interprets bytes already read from the terminal (by the platform input
 * layer) into an lt_event. No globals, no syscalls: the mirror of
 * lt__utf8_decode on the input side, so it can be unit-tested without a tty
 * and reused by any platform backend.
 */
#include "internal.h"
#include "libterm/libterm.h"

enum lt__key_match lt__key_decode(const unsigned char *seq, size_t len,
                                  int input_mode, struct lt_event *out,
                                  size_t *consumed) {
  (void)input_mode;
  if (!seq || !out || !consumed)
    return LT__KEY_NOMATCH;
  if (len == 0)
    return LT__KEY_PARTIAL;
  return LT__KEY_NOMATCH;
}

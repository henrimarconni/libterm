#include "internal.h"
#include "libterm/libterm.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

static struct lt_event decode_ok(const char *bytes, size_t len, int mode) {
  struct lt_event ev;
  memset(&ev, 0, sizeof(ev));
  size_t consumed = 0;
  enum lt__key_match m =
      lt__key_decode((const unsigned char *)bytes, len, mode, &ev, &consumed);
  assert(m == LT__KEY_MATCH);
  assert(consumed == len);
  return ev;
}

int main(void) {
  struct lt_event ev;
  size_t consumed = 0;

  /* Empty input is a (degenerate) partial. */
  memset(&ev, 0, sizeof(ev));
  assert(lt__key_decode((const unsigned char *)"", 0, 0, &ev, &consumed) ==
         LT__KEY_PARTIAL);

  /* Garbage is no-match. */
  memset(&ev, 0, sizeof(ev));
  assert(lt__key_decode((const unsigned char *)"\x90", 1, 0, &ev, &consumed) ==
         LT__KEY_NOMATCH);

  (void)decode_ok; /* used by later tasks */
  return 0;
}

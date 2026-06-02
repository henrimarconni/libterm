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

  /* xterm normal cursor + Home/End. */
  assert(decode_ok("\x1b[A", 3, 0).key == LT_KEY_ARROW_UP);
  assert(decode_ok("\x1b[B", 3, 0).key == LT_KEY_ARROW_DOWN);
  assert(decode_ok("\x1b[C", 3, 0).key == LT_KEY_ARROW_RIGHT);
  assert(decode_ok("\x1b[D", 3, 0).key == LT_KEY_ARROW_LEFT);
  assert(decode_ok("\x1b[H", 3, 0).key == LT_KEY_HOME);
  assert(decode_ok("\x1b[F", 3, 0).key == LT_KEY_END);
  assert(decode_ok("\x1b[Z", 3, 0).key == LT_KEY_BACK_TAB);

  /* SS3 application-cursor + F1-F4. */
  assert(decode_ok("\x1bOA", 3, 0).key == LT_KEY_ARROW_UP);
  assert(decode_ok("\x1bOH", 3, 0).key == LT_KEY_HOME);
  assert(decode_ok("\x1bOP", 3, 0).key == LT_KEY_F1);
  assert(decode_ok("\x1bOS", 3, 0).key == LT_KEY_F4);

  /* rxvt: Shift+arrows (lowercase letters), Ctrl+arrows via SS3 lowercase. */
  {
    struct lt_event e = decode_ok("\x1b[a", 3, 0);
    assert(e.key == LT_KEY_ARROW_UP && e.mod == LT_MOD_SHIFT);
  }
  {
    struct lt_event e = decode_ok("\x1bOa", 3, 0);
    assert(e.key == LT_KEY_ARROW_UP && e.mod == LT_MOD_CTRL);
  }

  /* Linux console F1-F5: ESC [ [ A..E. */
  assert(decode_ok("\x1b[[A", 4, 0).key == LT_KEY_F1);
  assert(decode_ok("\x1b[[E", 4, 0).key == LT_KEY_F5);

  /* A complete-but-unknown final is no-match; a bare prefix is partial. */
  memset(&ev, 0, sizeof(ev));
  assert(lt__key_decode((const unsigned char *)"\x1b[", 2, 0, &ev, &consumed) ==
         LT__KEY_PARTIAL);

  return 0;
}

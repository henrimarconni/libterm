#include "libterm/libterm.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

/* Expose parser helpers from plat_input.c for unit testing. */
#define static
#define lt__plat_read_event lt__plat_read_event__impl_for_test
#include "../src/platform/posix/plat_input.c"
#undef lt__plat_read_event
#undef static

static void expect_seq(const unsigned char *seq, size_t len, uint16_t want_key,
                       uint8_t want_mod) {
  struct lt_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = LT_EVENT_KEY;

  int rc = lt__posix_parse_esc_seq(seq, len, &ev);
  assert(rc == LT_OK);
  assert(ev.key == want_key);
  assert(ev.mod == want_mod);
}

int main(void) {
  {
    const unsigned char s[] = {'\x1b', '[', '1', ';', '5', 'A'};
    expect_seq(s, sizeof(s), LT_KEY_ARROW_UP, LT_MOD_CTRL);
  }
  {
    const unsigned char s[] = {'\x1b', '[', '1', ';', '2', 'C'};
    expect_seq(s, sizeof(s), LT_KEY_ARROW_RIGHT, LT_MOD_SHIFT);
  }
  {
    const unsigned char s[] = {'\x1b', '[', '1', ';', '3', 'D'};
    expect_seq(s, sizeof(s), LT_KEY_ARROW_LEFT, LT_MOD_ALT);
  }
  {
    const unsigned char s[] = {'\x1b', '[', '1', ';', '5', 'H'};
    expect_seq(s, sizeof(s), LT_KEY_HOME, LT_MOD_CTRL);
  }
  {
    const unsigned char s[] = {'\x1b', '[', '1', ';', '2', 'F'};
    expect_seq(s, sizeof(s), LT_KEY_END, LT_MOD_SHIFT);
  }

  /* Existing tilde-path mappings should remain intact. */
  {
    const unsigned char s[] = {'\x1b', '[', '1', '5', '~'};
    expect_seq(s, sizeof(s), LT_KEY_F5, 0);
  }
  {
    const unsigned char s[] = {'\x1b', '[', '6', '~'};
    expect_seq(s, sizeof(s), LT_KEY_PGDN, 0);
  }

  return 0;
}

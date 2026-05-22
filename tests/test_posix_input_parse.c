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

/* Parser must not invent a key mapping for malformed/unhandled sequences.
 * The parser never returns an error code; instead key stays 0. */
static void expect_no_key(const unsigned char *seq, size_t len) {
  struct lt_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = LT_EVENT_KEY;

  int rc = lt__posix_parse_esc_seq(seq, len, &ev);
  assert(rc == LT_OK);
  assert(ev.key == 0);
}

struct mod_case {
  unsigned char digit;
  uint8_t want_mod;
};

struct letter_case {
  unsigned char final;
  uint16_t want_key;
};

static void run_letter_mod_matrix(void) {
  const struct mod_case mods[] = {
      {'2', LT_MOD_SHIFT},
      {'3', LT_MOD_ALT},
      {'4', LT_MOD_SHIFT | LT_MOD_ALT},
      {'5', LT_MOD_CTRL},
      {'6', LT_MOD_SHIFT | LT_MOD_CTRL},
      {'7', LT_MOD_ALT | LT_MOD_CTRL},
      {'8', LT_MOD_SHIFT | LT_MOD_ALT | LT_MOD_CTRL},
  };
  const struct letter_case keys[] = {
      {'A', LT_KEY_ARROW_UP},   {'B', LT_KEY_ARROW_DOWN},
      {'C', LT_KEY_ARROW_RIGHT}, {'D', LT_KEY_ARROW_LEFT},
      {'H', LT_KEY_HOME},       {'F', LT_KEY_END},
  };

  for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
    for (size_t m = 0; m < sizeof(mods) / sizeof(mods[0]); m++) {
      const unsigned char s[] = {'\x1b', '[', '1', ';', mods[m].digit,
                                 keys[k].final};
      expect_seq(s, sizeof(s), keys[k].want_key, mods[m].want_mod);
    }
  }
}

static void run_bare_letter_keys(void) {
  /* No modifier — ESC [ <final>. */
  const struct letter_case keys[] = {
      {'A', LT_KEY_ARROW_UP},   {'B', LT_KEY_ARROW_DOWN},
      {'C', LT_KEY_ARROW_RIGHT}, {'D', LT_KEY_ARROW_LEFT},
      {'H', LT_KEY_HOME},       {'F', LT_KEY_END},
  };

  for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
    const unsigned char s[] = {'\x1b', '[', keys[k].final};
    expect_seq(s, sizeof(s), keys[k].want_key, 0);
  }
}

static void run_ss3_keys(void) {
  const struct letter_case keys[] = {
      {'H', LT_KEY_HOME}, {'F', LT_KEY_END}, {'P', LT_KEY_F1},
      {'Q', LT_KEY_F2},   {'R', LT_KEY_F3},  {'S', LT_KEY_F4},
  };

  for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
    const unsigned char s[] = {'\x1b', 'O', keys[k].final};
    expect_seq(s, sizeof(s), keys[k].want_key, 0);
  }
}

static void run_tilde_happy(void) {
  /* Tilde-final without modifier. */
  {
    const unsigned char s[] = {'\x1b', '[', '2', '~'};
    expect_seq(s, sizeof(s), LT_KEY_INSERT, 0);
  }
  {
    const unsigned char s[] = {'\x1b', '[', '3', '~'};
    expect_seq(s, sizeof(s), LT_KEY_DELETE, 0);
  }
  {
    const unsigned char s[] = {'\x1b', '[', '5', '~'};
    expect_seq(s, sizeof(s), LT_KEY_PGUP, 0);
  }
  {
    const unsigned char s[] = {'\x1b', '[', '6', '~'};
    expect_seq(s, sizeof(s), LT_KEY_PGDN, 0);
  }
  {
    const unsigned char s[] = {'\x1b', '[', '1', '5', '~'};
    expect_seq(s, sizeof(s), LT_KEY_F5, 0);
  }
  {
    const unsigned char s[] = {'\x1b', '[', '2', '4', '~'};
    expect_seq(s, sizeof(s), LT_KEY_F12, 0);
  }
}

static void run_tilde_with_mod(void) {
  /* Representative tilde + modifier samples. */
  {
    const unsigned char s[] = {'\x1b', '[', '3', ';', '5', '~'};
    expect_seq(s, sizeof(s), LT_KEY_DELETE, LT_MOD_CTRL);
  }
  {
    const unsigned char s[] = {'\x1b', '[', '5', ';', '2', '~'};
    expect_seq(s, sizeof(s), LT_KEY_PGUP, LT_MOD_SHIFT);
  }
  {
    const unsigned char s[] = {'\x1b', '[', '1', '5', ';', '8', '~'};
    expect_seq(s, sizeof(s), LT_KEY_F5,
               LT_MOD_SHIFT | LT_MOD_ALT | LT_MOD_CTRL);
  }
}

static void run_bare_esc(void) {
  /* Lone ESC, and ESC followed by a newline/CR, both map to LT_KEY_ESC. */
  {
    const unsigned char s[] = {'\x1b'};
    expect_seq(s, sizeof(s), LT_KEY_ESC, 0);
  }
  {
    const unsigned char s[] = {'\x1b', '\n'};
    expect_seq(s, sizeof(s), LT_KEY_ESC, 0);
  }
  {
    const unsigned char s[] = {'\x1b', '\r'};
    expect_seq(s, sizeof(s), LT_KEY_ESC, 0);
  }
}

static void run_negative_cases(void) {
  /* Truncated CSI: ESC [ alone — too short for any branch, no key. */
  {
    const unsigned char s[] = {'\x1b', '['};
    expect_no_key(s, sizeof(s));
  }
  /* Unknown 3-byte CSI final ('Z' = shift-tab in some terms, not mapped). */
  {
    const unsigned char s[] = {'\x1b', '[', 'Z'};
    expect_no_key(s, sizeof(s));
  }
  /* Garbage CSI final. */
  {
    const unsigned char s[] = {'\x1b', '[', 'Q'};
    expect_no_key(s, sizeof(s));
  }
  /* Unknown SS3 final. */
  {
    const unsigned char s[] = {'\x1b', 'O', 'Z'};
    expect_no_key(s, sizeof(s));
  }
  /* Oversized but well-formed tilde code that maps to no key. */
  {
    const unsigned char s[] = {'\x1b', '[', '9', '9', '9', '~'};
    expect_no_key(s, sizeof(s));
  }
  /* Tilde with non-digit in code resets to 0 → no mapping. */
  {
    const unsigned char s[] = {'\x1b', '[', '1', 'X', '~'};
    expect_no_key(s, sizeof(s));
  }
  /* Bare ESC O — too short for SS3 lookup. */
  {
    const unsigned char s[] = {'\x1b', 'O'};
    expect_no_key(s, sizeof(s));
  }
  /* Empty modifier slot: ESC [ 1 ; A — letter-mod helper returns 0
   * (non-digit in mod position), so mod stays 0 but the letter still
   * resolves. This pins current behavior. */
  {
    const unsigned char s[] = {'\x1b', '[', '1', ';', 'A'};
    expect_seq(s, sizeof(s), LT_KEY_ARROW_UP, 0);
  }
  /* Non-CSI / non-SS3 second byte. */
  {
    const unsigned char s[] = {'\x1b', 'X', 'Y'};
    expect_no_key(s, sizeof(s));
  }
}

int main(void) {
  run_letter_mod_matrix();
  run_bare_letter_keys();
  run_ss3_keys();
  run_tilde_happy();
  run_tilde_with_mod();
  run_bare_esc();
  run_negative_cases();
  return 0;
}

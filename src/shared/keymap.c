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
#include <string.h>

struct lt__key_seq {
  const char *bytes;
  uint16_t key;
  uint8_t mod;
};

/* Fixed, non-parametric escape sequences. Longest entries are fine in any
 * order; the matcher checks full equality and separately tracks prefixes. */
static const struct lt__key_seq lt__fixed_seqs[] = {
    /* xterm normal cursor / edit */
    {"\x1b[A", LT_KEY_ARROW_UP, 0},
    {"\x1b[B", LT_KEY_ARROW_DOWN, 0},
    {"\x1b[C", LT_KEY_ARROW_RIGHT, 0},
    {"\x1b[D", LT_KEY_ARROW_LEFT, 0},
    {"\x1b[H", LT_KEY_HOME, 0},
    {"\x1b[F", LT_KEY_END, 0},
    {"\x1b[Z", LT_KEY_BACK_TAB, 0},
    /* SS3 application cursor / Home / End */
    {"\x1bOA", LT_KEY_ARROW_UP, 0},
    {"\x1bOB", LT_KEY_ARROW_DOWN, 0},
    {"\x1bOC", LT_KEY_ARROW_RIGHT, 0},
    {"\x1bOD", LT_KEY_ARROW_LEFT, 0},
    {"\x1bOH", LT_KEY_HOME, 0},
    {"\x1bOF", LT_KEY_END, 0},
    /* SS3 function keys F1-F4 (xterm, screen, PuTTY) */
    {"\x1bOP", LT_KEY_F1, 0},
    {"\x1bOQ", LT_KEY_F2, 0},
    {"\x1bOR", LT_KEY_F3, 0},
    {"\x1bOS", LT_KEY_F4, 0},
    /* rxvt Shift+arrows (lowercase CSI finals) */
    {"\x1b[a", LT_KEY_ARROW_UP, LT_MOD_SHIFT},
    {"\x1b[b", LT_KEY_ARROW_DOWN, LT_MOD_SHIFT},
    {"\x1b[c", LT_KEY_ARROW_RIGHT, LT_MOD_SHIFT},
    {"\x1b[d", LT_KEY_ARROW_LEFT, LT_MOD_SHIFT},
    /* rxvt Ctrl+arrows (SS3 lowercase) */
    {"\x1bOa", LT_KEY_ARROW_UP, LT_MOD_CTRL},
    {"\x1bOb", LT_KEY_ARROW_DOWN, LT_MOD_CTRL},
    {"\x1bOc", LT_KEY_ARROW_RIGHT, LT_MOD_CTRL},
    {"\x1bOd", LT_KEY_ARROW_LEFT, LT_MOD_CTRL},
    /* Linux console F1-F5 */
    {"\x1b[[A", LT_KEY_F1, 0},
    {"\x1b[[B", LT_KEY_F2, 0},
    {"\x1b[[C", LT_KEY_F3, 0},
    {"\x1b[[D", LT_KEY_F4, 0},
    {"\x1b[[E", LT_KEY_F5, 0},
};

/* Try the fixed table. Returns MATCH (out filled, *consumed set), PARTIAL (seq
 * is a strict prefix of some entry), or NOMATCH. */
static enum lt__key_match lt__match_fixed(const unsigned char *seq, size_t len,
                                          struct lt_event *out,
                                          size_t *consumed) {
  bool any_prefix = false;
  for (size_t i = 0; i < sizeof(lt__fixed_seqs) / sizeof(lt__fixed_seqs[0]);
       i++) {
    const char *p = lt__fixed_seqs[i].bytes;
    size_t plen = strlen(p);
    if (len == plen && memcmp(seq, p, plen) == 0) {
      out->type = LT_EVENT_KEY;
      out->key = lt__fixed_seqs[i].key;
      out->mod = lt__fixed_seqs[i].mod;
      out->ch = 0;
      out->action = LT_KEY_PRESS;
      *consumed = len;
      return LT__KEY_MATCH;
    }
    if (len < plen && memcmp(seq, p, len) == 0)
      any_prefix = true;
  }
  return any_prefix ? LT__KEY_PARTIAL : LT__KEY_NOMATCH;
}

enum lt__key_match lt__key_decode(const unsigned char *seq, size_t len,
                                  int input_mode, struct lt_event *out,
                                  size_t *consumed) {
  (void)input_mode;
  if (!seq || !out || !consumed)
    return LT__KEY_NOMATCH;
  if (len == 0)
    return LT__KEY_PARTIAL;

  /* Lone ESC is a prefix of every escape sequence. */
  if (len == 1 && seq[0] == 0x1b)
    return LT__KEY_PARTIAL;

  if (seq[0] == 0x1b) {
    enum lt__key_match m = lt__match_fixed(seq, len, out, consumed);
    if (m != LT__KEY_NOMATCH)
      return m;
  }

  return LT__KEY_NOMATCH;
}

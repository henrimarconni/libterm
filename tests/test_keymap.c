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

  /* Tilde edit/function keys. */
  assert(decode_ok("\x1b[2~", 4, 0).key == LT_KEY_INSERT);
  assert(decode_ok("\x1b[3~", 4, 0).key == LT_KEY_DELETE);
  assert(decode_ok("\x1b[5~", 4, 0).key == LT_KEY_PGUP);
  assert(decode_ok("\x1b[6~", 4, 0).key == LT_KEY_PGDN);
  assert(decode_ok("\x1b[1~", 4, 0).key == LT_KEY_HOME);
  assert(decode_ok("\x1b[7~", 4, 0).key == LT_KEY_HOME); /* rxvt */
  assert(decode_ok("\x1b[4~", 4, 0).key == LT_KEY_END);
  assert(decode_ok("\x1b[8~", 4, 0).key == LT_KEY_END); /* rxvt */
  assert(decode_ok("\x1b[15~", 5, 0).key == LT_KEY_F5);
  assert(decode_ok("\x1b[24~", 5, 0).key == LT_KEY_F12);

  /* Modified arrows: ESC [ 1 ; mods A. mods = 1 + bitmask. */
  {
    struct lt_event e = decode_ok("\x1b[1;2A", 6, 0); /* shift+up */
    assert(e.key == LT_KEY_ARROW_UP && e.mod == LT_MOD_SHIFT);
  }
  {
    struct lt_event e = decode_ok("\x1b[1;5C", 6, 0); /* ctrl+right */
    assert(e.key == LT_KEY_ARROW_RIGHT && e.mod == LT_MOD_CTRL);
  }
  {
    struct lt_event e = decode_ok("\x1b[3;3~", 6, 0); /* alt+delete */
    assert(e.key == LT_KEY_DELETE && e.mod == LT_MOD_ALT);
  }

  /* Overflowing numeric run must not crash; key resolves by first field. */
  assert(decode_ok("\x1b[3;9999999999~", 15, 0).key == LT_KEY_DELETE);

  /* SGR mouse: ESC [ < Cb ; Cx ; Cy (M|m). */
  {
    struct lt_event e = decode_ok("\x1b[<0;10;5M", 10, 0);
    assert(e.type == LT_EVENT_MOUSE);
    assert(e.key == LT_KEY_MOUSE_LEFT);
    assert(e.x == 9 && e.y == 4); /* 1-based -> 0-based */
  }
  {
    struct lt_event e = decode_ok("\x1b[<0;1;1m", 9, 0); /* release */
    assert(e.type == LT_EVENT_MOUSE && e.key == LT_KEY_MOUSE_RELEASE);
  }
  {
    struct lt_event e = decode_ok("\x1b[<64;3;3M", 10, 0); /* wheel up */
    assert(e.type == LT_EVENT_MOUSE && e.key == LT_KEY_MOUSE_WHEEL_UP);
  }
  /* Incomplete mouse report is partial. */
  memset(&ev, 0, sizeof(ev));
  assert(lt__key_decode((const unsigned char *)"\x1b[<0;1", 6, 0, &ev,
                        &consumed) == LT__KEY_PARTIAL);

  /* kitty CSI-u: ESC [ <code> [; <mods>[:<event>]] u  (modern mode). */
  {
    struct lt_event e = decode_ok("\x1b[97u", 5, 0); /* 'a' */
    assert(e.type == LT_EVENT_KEY && e.ch == 'a' && e.mod == 0);
    assert(e.action == LT_KEY_PRESS);
  }
  {
    struct lt_event e = decode_ok("\x1b[97;5u", 7, 0); /* ctrl+a */
    assert(e.ch == 'a' && e.mod == LT_MOD_CTRL);
  }
  {
    struct lt_event e = decode_ok("\x1b[97;2u", 7, 0); /* shift+a */
    assert(e.ch == 'a' && e.mod == LT_MOD_SHIFT);
  }
  {
    struct lt_event e = decode_ok("\x1b[97;1:3u", 9, 0); /* 'a' release */
    assert(e.ch == 'a' && e.action == LT_KEY_RELEASE);
  }
  {
    struct lt_event e = decode_ok("\x1b[97;1:2u", 9, 0); /* 'a' repeat */
    assert(e.action == LT_KEY_REPEAT);
  }
  /* Bare modifier: kitty left-shift functional code 57441. */
  {
    struct lt_event e = decode_ok("\x1b[57441u", 8, 0);
    assert(e.key == LT_KEY_LEFT_SHIFT && e.ch == 0);
  }
  /* Caps Lock: kitty functional code 57358. */
  {
    struct lt_event e = decode_ok("\x1b[57358u", 8, 0);
    assert(e.key == LT_KEY_CAPS_LOCK && e.ch == 0);
  }
  /* Under report-all-keys, kitty sends Enter/Tab/Backspace/Esc as codepoints;
   * they must yield the SAME named keys as the control-byte path (consistency).
   */
  assert(decode_ok("\x1b[13u", 5, 0).key == LT_KEY_ENTER);
  assert(decode_ok("\x1b[9u", 4, 0).key == LT_KEY_TAB);
  assert(decode_ok("\x1b[127u", 6, 0).key == LT_KEY_BACKSPACE2);
  assert(decode_ok("\x1b[27u", 5, 0).key == LT_KEY_ESC);
  /* A complete but unrecognized CSI (final byte present) is NOMATCH, not a
   * stall. */
  {
    struct lt_event e;
    memset(&e, 0, sizeof(e));
    size_t c = 0;
    assert(lt__key_decode((const unsigned char *)"\x1b[1X", 4, 0, &e, &c) ==
           LT__KEY_NOMATCH);
  }
  /* Partial CSI-u (no final u yet). */
  memset(&ev, 0, sizeof(ev));
  assert(lt__key_decode((const unsigned char *)"\x1b[97;5", 6, 0, &ev,
                        &consumed) == LT__KEY_PARTIAL);

  /* Standalone control byte, modern model: normalize to ch + mod. */
  {
    struct lt_event e = decode_ok("\x01", 1, 0); /* Ctrl+A */
    assert(e.ch == 'a' && e.mod == LT_MOD_CTRL && e.key == 0);
  }
  {
    struct lt_event e = decode_ok("\x1a", 1, 0); /* Ctrl+Z */
    assert(e.ch == 'z' && e.mod == LT_MOD_CTRL);
  }
  /* Named control keys stay named even in modern mode. */
  assert(decode_ok("\r", 1, 0).key == LT_KEY_ENTER);
  assert(decode_ok("\t", 1, 0).key == LT_KEY_TAB);
  assert(decode_ok("\x7f", 1, 0).key == LT_KEY_BACKSPACE2);

  /* Compat model: control byte reported in key, ch=0. */
  {
    struct lt_event e;
    memset(&e, 0, sizeof(e));
    size_t c = 0;
    assert(lt__key_decode((const unsigned char *)"\x01", 1, LT_INPUT_COMPAT, &e,
                          &c) == LT__KEY_MATCH);
    assert(e.key == 0x01 && e.ch == 0 && e.mod == 0);
  }

  /* Compat mode reproduces termbox2 control-byte semantics across the set.
   * Skip 0x1b (ESC): a lone ESC is a sequence prefix (LT__KEY_PARTIAL), not a
   * standalone control-byte MATCH. */
  for (unsigned char b = 0x01; b <= 0x1f; b++) {
    if (b == 0x1b)
      continue;
    struct lt_event e;
    memset(&e, 0, sizeof(e));
    size_t c = 0;
    char in = (char)b;
    enum lt__key_match m =
        lt__key_decode((const unsigned char *)&in, 1, LT_INPUT_COMPAT, &e, &c);
    assert(m == LT__KEY_MATCH);
    assert(e.key == b && e.ch == 0 && e.mod == 0);
  }
  /* Named-key sequences are mode-independent. */
  assert(decode_ok("\x1b[A", 3, LT_INPUT_COMPAT).key == LT_KEY_ARROW_UP);
  assert(decode_ok("\x1b[3~", 4, LT_INPUT_COMPAT).key == LT_KEY_DELETE);

  return 0;
}

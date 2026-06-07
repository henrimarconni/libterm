/* Key-release delivery filter test: feeds kitty CSI-u press/release sequences
 * through an openpty() master and asserts lt_peek_event drops release events
 * by default (a caller that ignores ev.action sees each keystroke once) and
 * delivers them once LT_INPUT_RELEASE is OR'd into the input mode. Also
 * asserts end to end that the kitty associated-text codepoint wins for ch
 * (Shift+j arrives as 'J', not the base codepoint 'j').
 *
 * POSIX-only; returns 77 (CTest "skip") when no pty is available. */
#define _DEFAULT_SOURCE
#include "libterm/libterm.h"

#include <assert.h>
#if defined(__APPLE__)
#include <util.h> /* openpty lives here on macOS (no <pty.h>, no libutil) */
#else
#include <pty.h>
#endif
#include <string.h>
#include <unistd.h>

static void feed(int master, const char *bytes) {
  size_t n = strlen(bytes);
  ssize_t w = write(master, bytes, n);
  assert(w == (ssize_t)n);
}

static struct lt_event next_event(void) {
  struct lt_event ev;
  memset(&ev, 0, sizeof(ev));
  int rc = lt_peek_event(&ev, 3000);
  assert(rc == LT_OK);
  return ev;
}

int main(void) {
  int master = -1, slave = -1;
  struct winsize ws;
  memset(&ws, 0, sizeof ws);
  ws.ws_row = 24;
  ws.ws_col = 80;

  if (openpty(&master, &slave, NULL, NULL, &ws) != 0)
    return 77; /* CTest skip */

  assert(lt_init_fd(slave) == LT_OK);

  /* Default mode: Shift+j press (code 106, mods 2, text 'J'=74), its release,
   * then a plain 'a' press as the sentinel. The release must be dropped: the
   * second delivered event is the sentinel, not the release. */
  feed(master, "\x1b[106;2;74u" /* press   */
               "\x1b[106;2:3u"  /* release */
               "\x1b[97;1;97u" /* 'a' press (sentinel) */);
  {
    struct lt_event ev = next_event();
    assert(ev.type == LT_EVENT_KEY);
    assert(ev.action == LT_KEY_PRESS);
    assert(ev.ch == 'J'); /* associated text, not base codepoint 'j' */
    assert(ev.mod == LT_MOD_SHIFT);
  }
  {
    struct lt_event ev = next_event();
    assert(ev.type == LT_EVENT_KEY);
    assert(ev.action == LT_KEY_PRESS);
    assert(ev.ch == 'a'); /* the release between never surfaced */
  }
  {
    struct lt_event ev;
    memset(&ev, 0, sizeof(ev));
    assert(lt_peek_event(&ev, 100) == LT_ERR_NO_EVENT); /* queue is empty */
  }

  /* Opt-in: with LT_INPUT_RELEASE the release event is delivered. */
  lt_set_input_mode(LT_INPUT_ALT | LT_INPUT_RELEASE);
  feed(master, "\x1b[106;1;106u" /* 'j' press   */
               "\x1b[106;1:3u" /* 'j' release */);
  {
    struct lt_event ev = next_event();
    assert(ev.action == LT_KEY_PRESS && ev.ch == 'j');
  }
  {
    struct lt_event ev = next_event();
    assert(ev.action == LT_KEY_RELEASE && ev.ch == 'j');
  }

  assert(lt_shutdown() == LT_OK);
  close(slave);
  close(master);
  return 0;
}

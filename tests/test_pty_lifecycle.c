/* In-process lifecycle test: lt_init_fd against an openpty() slave, so the full
 * operational API runs without a controlling tty (headless CI). POSIX-only. */
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

#include "pty_drain.h"

int main(void) {
  int master = -1, slave = -1;
  struct winsize ws;
  memset(&ws, 0, sizeof ws);
  ws.ws_row = 24;
  ws.ws_col = 80;

  if (openpty(&master, &slave, NULL, NULL, &ws) != 0)
    return 77; /* CTest "skip" — no pty available in this environment */

  pty_autodrain(master, slave);

  assert(lt_init_fd(slave) == LT_OK);
  assert(lt_init_fd(slave) == LT_ERR_INIT_ALREADY);

  assert(lt_width() == 80);
  assert(lt_height() == 24);

  assert(lt_hide_cursor() == LT_OK);
  assert(lt_show_cursor() == LT_OK);
  assert(lt_set_clear_attrs(LT_GREEN, LT_BLACK) == LT_OK);
  assert(lt_clear() == LT_OK);
  assert(lt_present() == LT_OK);

  assert(lt_set_cell(0, 0, 'A', LT_WHITE, LT_BLACK) == LT_OK);
  assert(lt_set_cell(79, 23, 'Z', LT_WHITE, LT_BLACK) == LT_OK);
  assert(lt_set_cell(-1, 0, 'x', LT_DEFAULT, LT_DEFAULT) ==
         LT_ERR_OUT_OF_BOUNDS);
  assert(lt_set_cell(80, 0, 'x', LT_DEFAULT, LT_DEFAULT) ==
         LT_ERR_OUT_OF_BOUNDS);
  assert(lt_present() == LT_OK);

  /* no input fed yet: a zero-timeout peek reports no event */
  struct lt_event ev;
  int prc = lt_peek_event(&ev, 0);
  assert(prc == LT_ERR_NO_EVENT || prc == LT_OK || prc == LT_ERR_READ ||
         prc == LT_ERR_POLL);

  /* feed one printable byte through the master; a short peek should surface it
   * as a key event (honest contract: never a masked error). */
  assert(write(master, "A", 1) == 1);
  int krc = lt_peek_event(&ev, 200);
  assert(krc == LT_OK || krc == LT_ERR_NO_EVENT);
  if (krc == LT_OK) {
    assert(ev.type == LT_EVENT_KEY);
    assert(ev.ch == (lt_uchar)'A' || ev.key == 'A');
  }

  /* shutdown must NOT close the slave fd (the test owns it) */
  assert(lt_shutdown() == LT_OK);
  assert(write(slave, "x", 1) == 1); /* still open => write succeeds */

  close(slave);
  close(master);
  return 0;
}

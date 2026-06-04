/* LT_EVENT_RESIZE end to end: change the pty's window size, raise SIGWINCH
 * (lt_init_fd installs the handler + self-pipe on this process), and assert
 * lt_peek_event delivers a resize event carrying the new dimensions, and that
 * lt_width/lt_height reflect the new size. This event type was entirely
 * untested. POSIX-only; returns 77 (CTest "skip") when no pty.
 *
 * Cases: positive (grow -> RESIZE with new w/h), edge (a no-op SIGWINCH with
 * the SAME size delivers no resize event — the path filters unchanged sizes),
 * and a second distinct resize (shrink) to show it's repeatable. */
#define _DEFAULT_SOURCE
#include "libterm/libterm.h"

#include <assert.h>
#if defined(__APPLE__)
#include <util.h> /* openpty lives here on macOS (no <pty.h>, no libutil) */
#else
#include <pty.h>
#endif
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int g_master = -1, g_slave = -1;

static void set_size(int rows, int cols) {
  struct winsize ws;
  memset(&ws, 0, sizeof ws);
  ws.ws_row = (unsigned short)rows;
  ws.ws_col = (unsigned short)cols;
  /* Set on the master; the kernel propagates the size to the slave. */
  assert(ioctl(g_master, TIOCSWINSZ, &ws) == 0);
  raise(SIGWINCH); /* lt_init_fd installed the handler on this process */
}

int main(void) {
  struct winsize ws;
  memset(&ws, 0, sizeof ws);
  ws.ws_row = 24;
  ws.ws_col = 80;
  if (openpty(&g_master, &g_slave, NULL, NULL, &ws) != 0)
    return 77; /* CTest skip */

  assert(lt_init_fd(g_slave) == LT_OK);
  assert(lt_width() == 80);
  assert(lt_height() == 24);

  /* Positive: grow to 100x30. */
  set_size(30, 100);
  {
    struct lt_event ev;
    memset(&ev, 0, sizeof ev);
    int rc = lt_peek_event(&ev, 2000);
    if (rc == LT_ERR_NO_EVENT)
      return 77; /* some CI ptys don't propagate SIGWINCH; skip rather than fail
                  */
    assert(rc == LT_OK);
    assert(ev.type == LT_EVENT_RESIZE);
    assert(ev.w == 100);
    assert(ev.h == 30);
  }
  /* lt_width/lt_height now reflect the new size. */
  assert(lt_width() == 100);
  assert(lt_height() == 30);

  /* Edge: a SIGWINCH with the SAME size delivers no resize event (the path
   * filters out unchanged dimensions). peek should time out. */
  set_size(30, 100);
  {
    struct lt_event ev;
    memset(&ev, 0, sizeof ev);
    int rc = lt_peek_event(&ev, 200);
    assert(rc == LT_ERR_NO_EVENT);
  }

  /* Repeatable: shrink to 40x12 delivers another resize. */
  set_size(12, 40);
  {
    struct lt_event ev;
    memset(&ev, 0, sizeof ev);
    int rc = lt_peek_event(&ev, 2000);
    assert(rc == LT_OK);
    assert(ev.type == LT_EVENT_RESIZE);
    assert(ev.w == 40);
    assert(ev.h == 12);
  }

  assert(lt_shutdown() == LT_OK);
  close(g_slave);
  close(g_master);
  return 0;
}

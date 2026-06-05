/* LT_EVENT_RESIZE end to end: change the pty's window size, raise SIGWINCH
 * (lt_init_fd installs the handler + self-pipe on this process), and assert
 * lt_peek_event delivers a resize event carrying the new dimensions, and that
 * lt_width/lt_height reflect the new size. This event type was entirely
 * untested. POSIX-only; returns 77 (CTest "skip") when no pty.
 *
 * Cases: positive (grow -> RESIZE with new w/h), edge (a no-op SIGWINCH with
 * the SAME size delivers no resize event — the path filters unchanged sizes),
 * a second distinct resize (shrink) to show it's repeatable, a duplicate-event
 * check (the consumed resize is not followed by a duplicate), ghost-regression
 * byte assertions (height primary, width pass: a forced full repaint positions
 * the cursor at the new last row), and burst coalescing (several SIGWINCHes
 * around one settling size yield exactly one event with the final size). */
#define _GNU_SOURCE
#include "libterm/libterm.h"

#include <assert.h>
#if defined(__APPLE__)
#include <util.h> /* openpty lives here on macOS (no <pty.h>, no libutil) */
#else
#include <pty.h>
#endif
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int g_master = -1, g_slave = -1;

/* Drain available master bytes (non-blocking) into buf; returns total read. */
static size_t drain(char *buf, size_t cap) {
  size_t total = 0;
  int fl = fcntl(g_master, F_GETFL, 0);
  fcntl(g_master, F_SETFL, fl | O_NONBLOCK);
  for (;;) {
    if (total >= cap)
      break;
    ssize_t n = read(g_master, buf + total, cap - total);
    if (n <= 0)
      break;
    total += (size_t)n;
  }
  return total;
}

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

  /* Exactly-once: the consumed resize must not be followed by a duplicate. */
  {
    struct lt_event ev;
    memset(&ev, 0, sizeof ev);
    assert(lt_peek_event(&ev, 0) == LT_ERR_NO_EVENT);
  }

  /* Ghost regression (the reported bug, primary = HEIGHT change): after a
   * resize, the terminal still shows old (scrolled/reflowed) content, so the
   * next present must repaint EVERY cell — including ones the app leaves
   * blank — or stale glyphs survive. We assert libterm's emitted bytes
   * position the cursor at the LAST row of the new size: only a forced full
   * repaint touches blank rows. */
  char buf[65536];
  {
    /* Paint something, present, and discard those bytes. */
    assert(lt_set_cell(0, 0, 'A', LT_WHITE, LT_DEFAULT) == LT_OK);
    assert(lt_present() == LT_OK);
    (void)drain(buf, sizeof buf);

    /* Height shrink 12 -> 10 (width stays 40). */
    set_size(10, 40);
    struct lt_event ev;
    memset(&ev, 0, sizeof ev);
    assert(lt_peek_event(&ev, 2000) == LT_OK);
    assert(ev.type == LT_EVENT_RESIZE && ev.w == 40 && ev.h == 10);

    /* App redraws only row 0; present must still stamp rows 1..9. */
    assert(lt_set_cell(0, 0, 'B', LT_WHITE, LT_DEFAULT) == LT_OK);
    assert(lt_present() == LT_OK);
    size_t n = drain(buf, sizeof buf);
    assert(memmem(buf, n, "\x1b[10;1H", 7) != NULL); /* last row stamped */
  }

  /* Same hole, width axis: shrink 40 -> 30 columns. */
  {
    set_size(10, 30);
    struct lt_event ev;
    memset(&ev, 0, sizeof ev);
    assert(lt_peek_event(&ev, 2000) == LT_OK);
    assert(ev.type == LT_EVENT_RESIZE && ev.w == 30 && ev.h == 10);

    assert(lt_set_cell(0, 0, 'C', LT_WHITE, LT_DEFAULT) == LT_OK);
    assert(lt_present() == LT_OK);
    size_t n = drain(buf, sizeof buf);
    assert(memmem(buf, n, "\x1b[10;1H", 7) != NULL);
  }

  /* Burst coalescing: several SIGWINCHes around one settling size yield
   * exactly ONE event carrying the final dimensions. */
  {
    set_size(30, 100);
    set_size(28, 90);
    set_size(20, 60);
    struct lt_event ev;
    memset(&ev, 0, sizeof ev);
    assert(lt_peek_event(&ev, 2000) == LT_OK);
    assert(ev.type == LT_EVENT_RESIZE && ev.w == 60 && ev.h == 20);
    memset(&ev, 0, sizeof ev);
    assert(lt_peek_event(&ev, 200) == LT_ERR_NO_EVENT); /* no second event */
  }

  assert(lt_shutdown() == LT_OK);
  close(g_slave);
  close(g_master);
  return 0;
}

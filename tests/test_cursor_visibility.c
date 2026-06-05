/* Cursor-visibility parity with termbox2, asserted at the byte level via a
 * pty: hidden at init (\x1b[?25l after alt-screen entry), lt_set_cursor
 * implies show (+\x1b[?25h) and records the user cursor, lt_present reparks
 * the physical cursor at the user cursor right before the sync-close,
 * lt_hide_cursor re-hides idempotently, and shutdown restores visibility.
 * POSIX-only; returns 77 (CTest skip) when no pty. */
#define _GNU_SOURCE
#include "libterm/libterm.h"

#include <assert.h>
#include <fcntl.h>
#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif
#include <string.h>
#include <unistd.h>

static char g_buf[65536];
static size_t g_len = 0;
static int g_master = -1;

/* Drain everything currently readable on the (non-blocking) master. */
static void drain(void) {
  for (;;) {
    char tmp[4096];
    ssize_t n = read(g_master, tmp, sizeof tmp);
    if (n <= 0)
      break;
    if (g_len + (size_t)n > sizeof g_buf)
      n = (ssize_t)(sizeof g_buf - g_len);
    if (n <= 0)
      break;
    memcpy(g_buf + g_len, tmp, (size_t)n);
    g_len += (size_t)n;
  }
}

static void reset(void) { g_len = 0; }

int main(void) {
  int slave = -1;
  struct winsize ws;
  memset(&ws, 0, sizeof ws);
  ws.ws_row = 24;
  ws.ws_col = 80;
  if (openpty(&g_master, &slave, NULL, NULL, &ws) != 0)
    return 77; /* CTest skip */
  fcntl(g_master, F_SETFL, fcntl(g_master, F_GETFL, 0) | O_NONBLOCK);

  /* (1) Init flushes: ?25l present, after the alt-screen entry. */
  assert(lt_init_fd(slave) == LT_OK);
  drain();
  const char *alt = memmem(g_buf, g_len, "\x1b[?1049h", 8);
  const char *hide = memmem(g_buf, g_len, "\x1b[?25l", 6);
  assert(alt != NULL);
  assert(hide != NULL);
  assert(hide > alt);

  /* (2) lt_set_cursor implies show and records the user cursor (x=3,y=2 ->
   * row 3 col 4, 1-based); cursor ops only buffer, so lt_present is the flush
   * point. A present then reparks the physical cursor at the user cursor as
   * the last move before the sync-close, so the repark bytes sit contiguous
   * with the sync-close. */
  reset();
  assert(lt_set_cursor(3, 2) == LT_OK);
  assert(lt_set_cell(0, 0, 'X', LT_WHITE, LT_DEFAULT) == LT_OK);
  assert(lt_present() == LT_OK);
  drain();
  assert(memmem(g_buf, g_len, "\x1b[?25h", 6) != NULL);
  assert(memmem(g_buf, g_len, "\x1b[3;4H", 6) != NULL);
  assert(memmem(g_buf, g_len, "\x1b[3;4H\x1b[?2026l", 14) != NULL);

  /* (3) Hide emits ?25l; the following present must NOT repark (no user
   * cursor while hidden). */
  reset();
  assert(lt_hide_cursor() == LT_OK);
  assert(lt_present() == LT_OK);
  drain();
  assert(memmem(g_buf, g_len, "\x1b[?25l", 6) != NULL);
  assert(memmem(g_buf, g_len, "\x1b[3;4H\x1b[?2026l", 14) == NULL);

  /* (4) A second hide is idempotent: the present may flush nothing but must
   * not re-emit ?25l. */
  reset();
  assert(lt_hide_cursor() == LT_OK);
  assert(lt_present() == LT_OK);
  drain();
  assert(memmem(g_buf, g_len, "\x1b[?25l", 6) == NULL);

  /* (5) show_cursor adopts the current position as the user cursor, so a
   * following hide really hides (state coherence). */
  reset();
  assert(lt_show_cursor() == LT_OK);
  assert(lt_present() == LT_OK);
  drain();
  assert(memmem(g_buf, g_len, "\x1b[?25h", 6) != NULL);
  reset();
  assert(lt_hide_cursor() == LT_OK);
  assert(lt_present() == LT_OK);
  drain();
  assert(memmem(g_buf, g_len, "\x1b[?25l", 6) != NULL);

  /* (6) Negative contract: out-of-range coordinates are rejected (kept
   * v0.1.0 contract — only the present-tail repark clamps). */
  assert(lt_set_cursor(-1, 0) == LT_ERR_OUT_OF_BOUNDS);
  assert(lt_set_cursor(0, 9999) == LT_ERR_OUT_OF_BOUNDS);

  /* (7) Shutdown flushes and restores visibility. */
  reset();
  assert(lt_shutdown() == LT_OK);
  drain();
  assert(memmem(g_buf, g_len, "\x1b[?25h", 6) != NULL);

  close(slave);
  close(g_master);
  return 0;
}

/* Cursor ops emit the right escape bytes. Previously only return codes were
 * asserted; the actual sequences (hide/show DECTCEM, absolute move) were never
 * checked. Captured through a pty master. Cursor ops buffer into the output
 * buffer and are flushed by lt_present (or the next flush), so we present to
 * force the bytes out. POSIX-only; returns 77 (CTest "skip") when no pty.
 *
 * Cases: positive (hide -> \x1b[?25l, show -> \x1b[?25h, move(x,y) ->
 * \x1b[<y+1>;<x+1>H with 1-based VT coords), edge (move to 0,0 -> row/col 1;
 * a 2-digit coordinate), negative (move with a negative coord -> error,
 * pre-init already covered in test_api). */
#define _DEFAULT_SOURCE
#include "libterm/libterm.h"

#include <assert.h>
#include <fcntl.h>
#include <pty.h>
#include <string.h>
#include <unistd.h>

static int g_master = -1;
static unsigned char cap[65536];
static size_t caplen;

static void drain(void) {
  caplen = 0;
  for (;;) {
    unsigned char tmp[4096];
    ssize_t n = read(g_master, tmp, sizeof tmp);
    if (n <= 0)
      break;
    if (caplen + (size_t)n > sizeof cap)
      n = (ssize_t)(sizeof cap - caplen);
    if (n <= 0)
      break;
    memcpy(cap + caplen, tmp, (size_t)n);
    caplen += (size_t)n;
  }
}

static int has(const char *needle) {
  size_t nl = strlen(needle);
  if (nl == 0 || caplen < nl)
    return 0;
  for (size_t i = 0; i + nl <= caplen; i++)
    if (memcmp(cap + i, needle, nl) == 0)
      return 1;
  return 0;
}

int main(void) {
  int slave = -1;
  struct winsize ws;
  memset(&ws, 0, sizeof ws);
  ws.ws_row = 24;
  ws.ws_col = 80;
  if (openpty(&g_master, &slave, NULL, NULL, &ws) != 0)
    return 77; /* CTest skip */
  int fl = fcntl(g_master, F_GETFL, 0);
  fcntl(g_master, F_SETFL, fl | O_NONBLOCK);

  assert(lt_init_fd(slave) == LT_OK);
  /* Settle the buffer: a clear + present drains the initial dirty frame, so a
   * subsequent present (with nothing dirty) is a pure flush. That lets us
   * capture ONLY the cursor op's bytes, with no render output interleaved. */
  assert(lt_clear() == LT_OK);
  assert(lt_present() == LT_OK);
  drain();

  /* Positive: hide emits DECTCEM reset (?25l). The second present is a pure
   * flush of the buffered escape (no dirty rows after the clear above). */
  assert(lt_hide_cursor() == LT_OK);
  assert(lt_present() == LT_OK);
  drain();
  assert(has("\x1b[?25l"));

  /* Positive: show emits DECTCEM set (?25h). */
  assert(lt_show_cursor() == LT_OK);
  assert(lt_present() == LT_OK);
  drain();
  assert(has("\x1b[?25h"));

  /* Positive + edge: move uses 1-based VT coordinates, row;col order. (5,9) ->
   * row 10, col 6 -> "\x1b[10;6H". */
  assert(lt_set_cursor(5, 9) == LT_OK);
  assert(lt_present() == LT_OK);
  drain();
  assert(has("\x1b[10;6H"));

  /* Edge: origin (0,0) -> row 1, col 1. */
  assert(lt_set_cursor(0, 0) == LT_OK);
  assert(lt_present() == LT_OK);
  drain();
  assert(has("\x1b[1;1H"));

  /* Negative: a negative coordinate is rejected and emits nothing for it. */
  assert(lt_set_cursor(-1, 0) == LT_ERR_OUT_OF_BOUNDS);
  assert(lt_set_cursor(0, -1) == LT_ERR_OUT_OF_BOUNDS);

  assert(lt_shutdown() == LT_OK);
  close(slave);
  close(g_master);
  return 0;
}

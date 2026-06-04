/* lt_invalidate: the next lt_present must repaint every cell even when nothing
 * changed in the back buffer, where a normal present would emit nothing. Bytes
 * are captured through an openpty() master. POSIX-only; returns 77 (CTest
 * "skip") when no pty is available. */
#define _DEFAULT_SOURCE
#include "libterm/libterm.h"

#include <assert.h>
#include <fcntl.h>
#if defined(__APPLE__)
#include <util.h> /* openpty lives here on macOS (no <pty.h>, no libutil) */
#else
#include <pty.h>
#endif
#include <string.h>
#include <unistd.h>

static char g_buf[65536];
static size_t g_len = 0;

static void drain(int master) {
  for (;;) {
    char tmp[4096];
    ssize_t n = read(master, tmp, sizeof tmp);
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

static int contains(const char *needle) {
  size_t nl = strlen(needle);
  if (nl == 0 || g_len < nl)
    return 0;
  for (size_t i = 0; i + nl <= g_len; i++)
    if (memcmp(g_buf + i, needle, nl) == 0)
      return 1;
  return 0;
}

int main(void) {
  int master = -1, slave = -1;
  struct winsize ws;
  memset(&ws, 0, sizeof ws);
  ws.ws_row = 24;
  ws.ws_col = 80;

  if (openpty(&master, &slave, NULL, NULL, &ws) != 0)
    return 77; /* CTest skip */

  int fl = fcntl(master, F_GETFL, 0);
  fcntl(master, F_SETFL, fl | O_NONBLOCK);

  assert(lt_init_fd(slave) == LT_OK);
  assert(lt_clear() == LT_OK);

  /* Paint a glyph and present it; this is the established on-screen state. */
  assert(lt_set_cell(0, 0, 'Q', LT_DEFAULT, LT_DEFAULT) == LT_OK);
  assert(lt_present() == LT_OK);
  drain(master);
  assert(contains("Q"));

  /* A present with nothing changed emits no payload (diff fast-path). */
  g_len = 0;
  assert(lt_present() == LT_OK);
  drain(master);
  assert(!contains("Q")); /* the glyph is NOT re-sent */

  /* After invalidate, the next present repaints the full screen even though the
   * back buffer is unchanged: the 'Q' is re-emitted. */
  g_len = 0;
  assert(lt_invalidate() == LT_OK);
  assert(lt_present() == LT_OK);
  drain(master);
  assert(contains("Q")); /* re-sent by the forced repaint */

  /* Invalidate is consumed once: the present after it is back to diffing and
   * emits nothing for an unchanged buffer. */
  g_len = 0;
  assert(lt_present() == LT_OK);
  drain(master);
  assert(!contains("Q"));

  /* Before init it reports NOT_INIT. */
  assert(lt_shutdown() == LT_OK);
  assert(lt_invalidate() == LT_ERR_NOT_INIT);

  close(slave);
  close(master);
  return 0;
}

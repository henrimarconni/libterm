/* POSIX SGR output-byte test: captures emitted escape sequences through an
 * openpty() slave and asserts mode-aware color output. POSIX-only; returns 77
 * (CTest "skip") when no pty is available. */
#define _DEFAULT_SOURCE
#include "libterm/libterm.h"

#include <assert.h>
#include <fcntl.h>
#include <pty.h>
#include <string.h>
#include <unistd.h>

static char g_buf[65536];
static size_t g_len = 0;

/* Drain everything currently readable on the (non-blocking) master into g_buf. */
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

  /* normal: LT_RED foreground -> ESC[31m */
  /* Each phase resets g_len so contains() inspects only that frame's bytes.
   * The SGR cache (cur_fg/bg/attrs) is seeded to 0xFFFFFFFF at init, so the
   * FIRST emitted frame carries a reset ("ESC[0;...") while later frames are
   * clean; assertions use distinctive payload substrings that survive both. */

  /* normal: LT_RED fg. First frame -> "ESC[0;31;49m", so assert the framing-
   * independent ";31;" fragment. (This first present also clears the cache
   * sentinel so the following clean-frame assertions are stable.) */
  lt_set_output_mode(LT_OUTPUT_NORMAL);
  g_len = 0;
  assert(lt_set_cell(0, 0, 'R', LT_RED, LT_DEFAULT) == LT_OK);
  assert(lt_present() == LT_OK);
  drain(master);
  assert(contains(";31;"));

  /* relocated attribute: green + bold, clean frame -> "ESC[32;1m" */
  g_len = 0;
  assert(lt_set_cell(1, 0, 'B', LT_GREEN | LT_BOLD, LT_DEFAULT) == LT_OK);
  assert(lt_present() == LT_OK);
  drain(master);
  assert(contains("32;1m"));

  /* 256: palette index 200 -> 38;5;200 */
  lt_set_output_mode(LT_OUTPUT_256);
  g_len = 0;
  assert(lt_set_cell(2, 0, 'X', 200, LT_DEFAULT) == LT_OK);
  assert(lt_present() == LT_OK);
  drain(master);
  assert(contains("38;5;200"));

  /* truecolor: 0xFF8000 -> 38;2;255;128;0 (raw hex; LT_RGB checked in Task 2) */
  lt_set_output_mode(LT_OUTPUT_TRUECOLOR);
  g_len = 0;
  assert(lt_set_cell(3, 0, 'T', 0xFF8000, LT_DEFAULT) == LT_OK);
  assert(lt_present() == LT_OK);
  drain(master);
  assert(contains("38;2;255;128;0"));

  assert(lt_shutdown() == LT_OK);
  close(slave);
  close(master);
  return 0;
}

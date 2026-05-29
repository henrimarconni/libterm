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

  /* (still in LT_OUTPUT_TRUECOLOR from Task 1's last phase, cur_fg == 0xFF8000)

     IMPORTANT — the SGR cache: lt__plat_emit_sgr short-circuits when
     (fg,bg,attrs) equals the last-emitted (cur_fg/cur_bg/cur_attrs), so a phase
     whose fg repeats the previous cell's fg emits NO color bytes and its
     isolated frame would miss the payload. Every phase below therefore uses an
     fg value distinct from the immediately-preceding cell:
       x3 (Task 1) 0xFF8000 -> x4 0x0A141E -> x5 0x80000000 -> x6 0 -> x7 0x80000000
     (x5 and x7 share 0x80000000 but are separated by x6=0, so each re-emits). */

  /* LT_RGB packs a distinct 24-bit value -> 38;2;10;20;30 (validates the macro) */
  g_len = 0;
  assert(lt_set_cell(4, 0, 't', LT_RGB(10, 20, 30), LT_DEFAULT) == LT_OK);
  assert(lt_present() == LT_OK);
  drain(master);
  assert(contains("38;2;10;20;30"));

  /* 256 palette black via the sentinel -> 38;5;0 (cur_fg was 0x0A141E) */
  lt_set_output_mode(LT_OUTPUT_256);
  g_len = 0;
  assert(lt_set_cell(5, 0, 'h', LT_HI_BLACK, LT_DEFAULT) == LT_OK);
  assert(lt_present() == LT_OK);
  drain(master);
  assert(contains("38;5;0"));

  /* truecolor terminal-default fg (0, no HI_BLACK) -> clean frame "ESC[39m",
   * and the frame must NOT contain a 38;2;... RGB sequence (cur_fg was
   * 0x80000000, so this re-emits) */
  lt_set_output_mode(LT_OUTPUT_TRUECOLOR);
  g_len = 0;
  assert(lt_set_cell(6, 0, 'd', LT_DEFAULT, LT_DEFAULT) == LT_OK);
  assert(lt_present() == LT_OK);
  drain(master);
  assert(contains("\x1b[39m"));
  assert(!contains("38;2"));

  /* truecolor real black via the sentinel -> 38;2;0;0;0 (cur_fg was 0, so this
   * re-emits even though 0x80000000 matches the x5 256-mode cell) */
  g_len = 0;
  assert(lt_set_cell(7, 0, 'k', LT_RGB(0, 0, 0) | LT_HI_BLACK, LT_DEFAULT) ==
         LT_OK);
  assert(lt_present() == LT_OK);
  drain(master);
  assert(contains("38;2;0;0;0"));

  assert(lt_shutdown() == LT_OK);
  close(slave);
  close(master);
  return 0;
}

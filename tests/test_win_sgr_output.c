/* Windows SGR output-byte test: the POSIX byte test (test_posix_sgr_output.c)
 * captures through an openpty() slave, which Windows lacks, and lt_init needs a
 * real console so it can't run headless. Instead we redirect STD_OUTPUT_HANDLE
 * to an anonymous pipe and drive the shared emission path directly
 * (lt__render_run -> lt__emit_sgr -> lt__plat_flush -> WriteFile), then read
 * the pipe and assert the mode-aware SGR payloads. This is the Windows-side
 * guard that the shared src/shared/sgr.c keeps emitting correct bytes.
 *
 * White-box: links libterm_static and calls lt__ internals (same pattern as
 * test_simd_diff). Forcing the SGR cache (cur_fg/cur_bg/cur_attrs) to the
 * 0xFFFFFFFF sentinel before each case guarantees emission; the leading reset
 * frame ("\x1b[0;...") it produces is why assertions match framing-robust
 * payload fragments rather than whole frames. */
#include "internal.h"
#include "libterm/libterm.h"
#include "platform.h"

#include <assert.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static char g_buf[65536];
static size_t g_len = 0;

/* Drain everything currently readable on the pipe into g_buf. */
static void drain(HANDLE rd) {
  for (;;) {
    DWORD avail = 0;
    if (!PeekNamedPipe(rd, NULL, 0, NULL, &avail, NULL) || avail == 0)
      return;

    char tmp[4096];
    DWORD want = avail < sizeof tmp ? avail : (DWORD)sizeof tmp;
    DWORD got = 0;
    if (!ReadFile(rd, tmp, want, &got, NULL) || got == 0)
      return;

    if (g_len + got > sizeof g_buf)
      got = (DWORD)(sizeof g_buf - g_len);
    if (got == 0)
      return;
    memcpy(g_buf + g_len, tmp, got);
    g_len += got;
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

/* Emit one cell (fg packs color+attrs, bg packs color) under `mode`, forcing a
 * fresh SGR emission, and capture the bytes into g_buf. */
static void emit(HANDLE rd, int mode, lt_attr fg, lt_attr bg) {
  lt__g.output_mode = mode;
  lt__g.cur_fg = 0xFFFFFFFF;
  lt__g.cur_bg = 0xFFFFFFFF;
  lt__g.cur_attrs = 0xFFFFFFFF;

  struct lt_cell cell;
  memset(&cell, 0, sizeof cell);
  cell.ch = (lt_uchar)'X';
  cell.fg = fg;
  cell.bg = bg;

  g_len = 0;
  assert(lt__render_run(&cell, 1) == LT_OK);
  assert(lt__plat_flush() == LT_OK);
  drain(rd);
}

int main(void) {
  HANDLE rd = NULL, wr = NULL;
  SECURITY_ATTRIBUTES sa;
  memset(&sa, 0, sizeof sa);
  sa.nLength = sizeof sa;
  sa.bInheritHandle = TRUE;

  if (!CreatePipe(&rd, &wr, &sa, 0))
    return 1;

  HANDLE saved = GetStdHandle(STD_OUTPUT_HANDLE);
  SetStdHandle(STD_OUTPUT_HANDLE, wr);

  /* NORMAL: named fg/bg and attribute codes. The forced reset frames each emit
   * as "\x1b[0;<fg>;<bg>[;<attrs>]m", so assert framing-robust fragments. */
  emit(rd, LT_OUTPUT_NORMAL, LT_RED, LT_DEFAULT);
  assert(contains(";31;49")); /* red fg, default bg */

  emit(rd, LT_OUTPUT_NORMAL, LT_GREEN | LT_BOLD, LT_DEFAULT);
  assert(contains("32;49;1")); /* green fg, default bg, bold */

  emit(rd, LT_OUTPUT_NORMAL, LT_YELLOW | LT_BOLD | LT_UNDERLINE, LT_DEFAULT);
  assert(contains("1;4m")); /* bold + underline tail */

  emit(rd, LT_OUTPUT_NORMAL, LT_DEFAULT, LT_BLUE);
  assert(contains("44m")); /* blue bg */

  /* 256 / 216 / grayscale palette indices (termbox2 sub-range mapping). */
  emit(rd, LT_OUTPUT_256, 200, LT_DEFAULT);
  assert(contains("38;5;200"));

  emit(rd, LT_OUTPUT_256, LT_HI_BLACK, LT_DEFAULT); /* real black -> index 0 */
  assert(contains("38;5;0"));

  emit(rd, LT_OUTPUT_216, 1, LT_DEFAULT); /* 1 -> 16 (cube base) */
  assert(contains("38;5;16"));

  emit(rd, LT_OUTPUT_216, 216, LT_DEFAULT); /* 216 -> 231 (cube top) */
  assert(contains("38;5;231"));

  emit(rd, LT_OUTPUT_GRAYSCALE, 1, LT_DEFAULT); /* 1 -> 232 (ramp base) */
  assert(contains("38;5;232"));

  emit(rd, LT_OUTPUT_GRAYSCALE, 24, LT_DEFAULT); /* 24 -> 255 (ramp top) */
  assert(contains("38;5;255"));

  /* TRUECOLOR: 24-bit fg/bg, the LT_RGB macro, and the default-vs-real-black
   * distinction via LT_HI_BLACK. */
  emit(rd, LT_OUTPUT_TRUECOLOR, 0xFF8000, LT_DEFAULT);
  assert(contains("38;2;255;128;0"));

  emit(rd, LT_OUTPUT_TRUECOLOR, LT_RGB(10, 20, 30), LT_DEFAULT);
  assert(contains("38;2;10;20;30"));

  emit(rd, LT_OUTPUT_TRUECOLOR, LT_DEFAULT, LT_RGB(0, 0, 0) | LT_HI_BLACK);
  assert(contains("48;2;0;0;0")); /* real black bg */

  emit(rd, LT_OUTPUT_TRUECOLOR, LT_DEFAULT, LT_DEFAULT);
  assert(contains(";39;"));  /* terminal-default fg */
  assert(!contains("38;2")); /* and NOT an RGB sequence */

  SetStdHandle(STD_OUTPUT_HANDLE, saved);
  CloseHandle(rd);
  CloseHandle(wr);
  return 0;
}

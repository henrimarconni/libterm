#include <assert.h>
#include <sys/types.h>

#include "internal.h"
#include <string.h>

static char g_capture[4096];
static size_t g_capture_len = 0;

ssize_t lt__test_write(int fd, const void *buf, size_t len) {
  (void)fd;
  if (!buf || len == 0)
    return 0;

  assert(g_capture_len + len < sizeof(g_capture));
  memcpy(g_capture + g_capture_len, buf, len);
  g_capture_len += len;
  return (ssize_t)len;
}

int lt__posix_get_tty_fd(void) { return 1; }

#define static
#define write lt__test_write
#include "../src/platform/posix/plat_output.c"
#undef write
#undef static

/* Run/SGR emission moved out of plat_output.c into shared sgr.c (commit
 * 3e3db37); pull it into this TU so lt__render_run resolves against the same
 * captured output buffer. egc.c supplies lt__egc_get, which the cluster-aware
 * render path references. */
#include "../src/shared/egc.c"
#include "../src/shared/sgr.c"

struct lt__state lt__g;

int lt__utf8_encode(lt_uchar ch, char out[4]) {
  if (ch < 0x80) {
    out[0] = (char)ch;
    return 1;
  }

  return 0;
}

static void reset_capture(void) {
  memset(g_capture, 0, sizeof(g_capture));
  g_capture_len = 0;
}

static int count_sgr_sequences(void) {
  int count = 0;
  for (size_t i = 0; i + 1 < g_capture_len; i++) {
    if (g_capture[i] == '\x1b' && g_capture[i + 1] == '[') {
      for (size_t j = i + 2; j < g_capture_len; j++) {
        if (g_capture[j] == 'm') {
          count++;
          i = j;
          break;
        }
      }
    }
  }

  return count;
}

static int contains_substr(const char *buf, size_t len, const char *needle) {
  size_t nlen = strlen(needle);
  if (nlen == 0 || len < nlen)
    return 0;

  for (size_t i = 0; i + nlen <= len; i++) {
    if (memcmp(buf + i, needle, nlen) == 0)
      return 1;
  }
  return 0;
}

static void prepare_state(int mode) {
  lt__g.output_mode = mode;
  lt__g.cur_fg = 0xFFFFFFFFu;
  lt__g.cur_bg = 0xFFFFFFFFu;
  lt__g.cur_attrs = 0xFFFFFFFFu;
  reset_capture();
}

static struct lt_cell one_cell(lt_uchar ch, lt_attr fg, lt_attr bg) {
  struct lt_cell c;
  memset(&c, 0, sizeof(c));
  c.ch = ch;
  c.fg = fg;
  c.bg = bg;
  return c;
}

static void test_split_sgr_for_mixed_style_run(void) {
  prepare_state(LT_OUTPUT_NORMAL);

  struct lt_cell cells[2];
  cells[0] = one_cell('A', LT_RED, LT_DEFAULT);
  cells[1] = one_cell('B', LT_GREEN, LT_DEFAULT);

  assert(lt__render_run(cells, 2) == LT_OK);
  assert(lt__plat_flush() == LT_OK);

  assert(memchr(g_capture, 'A', g_capture_len) != NULL);
  assert(memchr(g_capture, 'B', g_capture_len) != NULL);
  assert(count_sgr_sequences() >= 2);
}

static void test_mode_normal_sgr_shape(void) {
  prepare_state(LT_OUTPUT_NORMAL);

  struct lt_cell c = one_cell('N', LT_RED, LT_BLUE);
  assert(lt__render_run(&c, 1) == LT_OK);
  assert(lt__plat_flush() == LT_OK);

  assert(contains_substr(g_capture, g_capture_len, "\x1b["));
  assert(memchr(g_capture, 'm', g_capture_len) != NULL);
  assert(contains_substr(g_capture, g_capture_len, ";3"));
  assert(contains_substr(g_capture, g_capture_len, ";4"));
}

static void test_mode_256_sgr_shape(void) {
  prepare_state(LT_OUTPUT_256);

  struct lt_cell c = one_cell('2', LT_RED, LT_BLUE);
  assert(lt__render_run(&c, 1) == LT_OK);
  assert(lt__plat_flush() == LT_OK);

  /* fail-first: should become true after LT_OUTPUT_256 implementation */
  assert(contains_substr(g_capture, g_capture_len, ";38;5;") ||
         contains_substr(g_capture, g_capture_len, ";48;5;"));
}

static void test_mode_truecolor_sgr_shape(void) {
  prepare_state(LT_OUTPUT_TRUECOLOR);

  struct lt_cell c = one_cell('T', LT_RED, LT_BLUE);
  assert(lt__render_run(&c, 1) == LT_OK);
  assert(lt__plat_flush() == LT_OK);

  /* fail-first: should become true after LT_OUTPUT_TRUECOLOR implementation
   */
  assert(contains_substr(g_capture, g_capture_len, ";38;2;") ||
         contains_substr(g_capture, g_capture_len, ";48;2;"));
}

static void test_mode_216_sgr_shape(void) {
  prepare_state(LT_OUTPUT_216);

  struct lt_cell c = one_cell('H', LT_RED, LT_BLUE);
  assert(lt__render_run(&c, 1) == LT_OK);
  assert(lt__plat_flush() == LT_OK);

  /* fail-first: 216 mode should map into 256 palette form */
  assert(contains_substr(g_capture, g_capture_len, ";38;5;") ||
         contains_substr(g_capture, g_capture_len, ";48;5;"));
}

static void test_mode_grayscale_sgr_shape(void) {
  prepare_state(LT_OUTPUT_GRAYSCALE);

  struct lt_cell c = one_cell('G', LT_RED, LT_BLUE);
  assert(lt__render_run(&c, 1) == LT_OK);
  assert(lt__plat_flush() == LT_OK);

  /* fail-first: grayscale mode should map into 256 palette form */
  assert(contains_substr(g_capture, g_capture_len, ";38;5;") ||
         contains_substr(g_capture, g_capture_len, ";48;5;"));
}

int main(void) {
  lt__g.cur_fg = 0xFFFFFFFFu;
  lt__g.cur_bg = 0xFFFFFFFFu;
  lt__g.cur_attrs = 0xFFFFFFFFu;

  struct lt_cell cells[2];
  memset(cells, 0, sizeof(cells));

  cells[0].ch = 'A';
  cells[0].fg = LT_RED;
  cells[0].bg = LT_DEFAULT;

  cells[1].ch = 'B';
  cells[1].fg = LT_GREEN;
  cells[1].bg = LT_DEFAULT;

  reset_capture();
  assert(lt__render_run(cells, 2) == LT_OK);
  assert(lt__plat_flush() == LT_OK);

  assert(memchr(g_capture, 'A', g_capture_len) != NULL);
  assert(memchr(g_capture, 'B', g_capture_len) != NULL);

  assert(count_sgr_sequences() >= 2);

  test_split_sgr_for_mixed_style_run();
  test_mode_normal_sgr_shape();
  test_mode_256_sgr_shape();
  test_mode_truecolor_sgr_shape();
  test_mode_216_sgr_shape();
  test_mode_grayscale_sgr_shape();

  return 0;
}

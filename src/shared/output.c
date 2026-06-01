#include "internal.h"
#include "intrinsics/diff.h"
#include "platform.h"
#include "string.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static int lt__present_abort(int err) {
  static const char sync_end[] = "\x1b[?2026l";
  (void)lt__plat_write(sync_end, sizeof(sync_end) - 1);
  (void)lt__plat_flush();
  return err;
}

int lt_clear(void) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;

  if (!lt__g.back || lt__g.width <= 0 || lt__g.height <= 0)
    return LT_ERR_NOT_INIT;

  const int count = lt__g.width * lt__g.height;
  lt__buffer_clear(lt__g.back, count, lt__g.clear_fg, lt__g.clear_bg);

  for (int y = 0; y < lt__g.height; y++)
    lt__g.dirty_rows[y] = true;

  return LT_OK;
}

int lt_set_clear_attrs(lt_attr fg, lt_attr bg) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;
  lt__g.clear_fg = fg;
  lt__g.clear_bg = bg;
  return LT_OK;
}

int lt_present(void) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;

  if (!lt__g.back || !lt__g.front || lt__g.width <= 0 || lt__g.height <= 0)
    return LT_ERR_NOT_INIT;

  /* Fast-path: no dirty rows means nothing to render. Skipping the
   * sync brackets here keeps no-change presents free (no WriteFile). */
  bool any_dirty = false;
  for (int y = 0; y < lt__g.height; y++)
    if (lt__g.dirty_rows[y]) {
      any_dirty = true;
      break;
    }

  if (!any_dirty)
    return lt__plat_flush();

#if LT_RENDER_STATS
  lt__g.stats.present_calls++;
#endif

  /* Begin syncronized update (DEC mode 2026). Terminals that don't
   * recognize this DECSET silently ignore it. Modern terminals atomically
   * swap to the new frame at the matching end-sync bellow, killing tearing
   * and letting the host coalesce reflow work. */
  static const char sync_begin[] = "\x1b[?2026h";
  (void)lt__plat_write(sync_begin, sizeof(sync_begin) - 1);

  lt__g.cur_x = -1;
  lt__g.cur_y = -1;

  size_t idx = 0;
  int mc = 0, rc = 0, rest = 0, run_len = 0, run_end = 0, skip = 0;

  for (int y = 0; y < lt__g.height; y++) {
    if (!lt__g.dirty_rows[y])
      continue;

#if LT_RENDER_STATS
    lt__g.stats.dirty_rows++;
#endif

    int x = 0;
    while (x < lt__g.width) {
      /* SIMD outer skip: advance past all equal cells */
      skip = lt__simd_diff_first_differ_cell(&lt__g.back[y * lt__g.width + x],
                                             &lt__g.front[y * lt__g.width + x],
                                             lt__g.width - x);
      x += skip;
      if (x >= lt__g.width)
        break;

      idx = (size_t)(y * lt__g.width + x);

      /* SIMD inner walk: find end of changed run by locating first equal */
      rest = lt__simd_diff_first_equal_cell(
          &lt__g.back[idx + 1], &lt__g.front[idx + 1], lt__g.width - x - 1);
      run_len = 1 + rest;
      run_end = x + run_len;
#if LT_RENDER_STATS
      lt__g.stats.diff_runs++;
      lt__g.stats.cells_rendered += (uint64_t)run_len;
#endif

      /* emit cursor jump if discontinuous from cache */
      if (x != lt__g.cur_x || y != lt__g.cur_y) {
        mc = lt__plat_move_cursor(x, y);
        if (mc != LT_OK)
          return lt__present_abort(mc);
#if LT_RENDER_STATS
        lt__g.stats.cursor_moves++;
#endif
      }

      /* emit the entire run in one block */
      rc = lt__render_run(&lt__g.back[idx], run_len);
      if (rc != LT_OK) {
        return lt__present_abort(rc);
      }

      /* update cache for end-of-run position */
      lt__g.cur_x = x + run_len;
      lt__g.cur_y = y;
      if (lt__g.cur_x >= lt__g.width) {
        lt__g.cur_x = -1;
        lt__g.cur_y = -1;
      }

      /* sync front for all cells in run */
      memcpy(&lt__g.front[idx], &lt__g.back[idx],
             (size_t)run_len * sizeof(struct lt_cell));

      x = run_end;
    }

    lt__g.dirty_rows[y] = false;
  }

  static const char sync_end[] = "\x1b[?2026l";
  (void)lt__plat_write(sync_end, sizeof(sync_end) - 1);

  return lt__plat_flush();
}

int lt_set_cursor(int x, int y) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;
  return lt__plat_move_cursor(x, y);
}

int lt_hide_cursor(void) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;
  return lt__plat_hide_cursor();
}

int lt_show_cursor(void) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;
  return lt__plat_show_cursor();
}

int lt_set_output_mode(int mode) {
  if (mode == LT_OUTPUT_CURRENT)
    return lt__g.output_mode;
  /* A mode change re-interprets color values, so the SGR cache (which keys on
   * color/attr values, not mode) must be invalidated to force re-emission. */
  if (mode != lt__g.output_mode) {
    lt__g.cur_fg = 0xFFFFFFFF;
    lt__g.cur_bg = 0xFFFFFFFF;
    lt__g.cur_attrs = 0xFFFFFFFF;
  }
  lt__g.output_mode = mode;
  return mode;
}

int lt_detect_color_depth(void) {
  const char *ct = getenv("COLORTERM");
  if (ct && (strcmp(ct, "truecolor") == 0 || strcmp(ct, "24bit") == 0))
    return LT_OUTPUT_TRUECOLOR;

  const char *term = getenv("TERM");
  if (term && strstr(term, "256color"))
    return LT_OUTPUT_256;

  return LT_OUTPUT_NORMAL;
}

int lt_send(const char *buf, size_t nbuf) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;
  if (!buf)
    return LT_ERR;
  if (nbuf == 0)
    return LT_OK;

  int rc = lt__plat_write(buf, nbuf);
  if (rc != LT_OK)
    return rc;
  return lt__plat_flush();
}

int lt_sendf(const char *fmt, ...) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;
  if (!fmt)
    return LT_ERR;

  char out[1024];
  va_list vl;
  va_start(vl, fmt);
  int n = vsnprintf(out, sizeof(out), fmt, vl);
  va_end(vl);

  if (n < 0)
    return LT_ERR;
  /* vsnprintf returns the length it *would* have written; clamp to what
   * actually landed in the buffer so an over-long format sends truncated
   * rather than reading past `out`. */
  size_t len = ((size_t)n < sizeof(out)) ? (size_t)n : sizeof(out) - 1;
  return lt_send(out, len);
}

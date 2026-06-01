#include "internal.h"
#include <stdarg.h>
#include <stdio.h>

int lt_set_cell(int x, int y, lt_uchar ch, lt_attr fg, lt_attr bg) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;

  if (!lt__g.back || x < 0 || y < 0 || x >= lt__g.width || y >= lt__g.height)
    return LT_ERR_OUT_OF_BOUNDS;

  lt__g.dirty_rows[y] = true;

  const size_t idx = (size_t)(y * lt__g.width + x);
  lt__g.back[idx].ch = ch;
  lt__g.back[idx].fg = fg;
  lt__g.back[idx].bg = bg;
  lt__g.back[idx]._reserved = 0;

  return LT_OK;
}

int lt_print_ex(int x, int y, lt_attr fg, lt_attr bg, size_t *out_w,
                const char *str) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;
  if (!str)
    return LT_ERR;

  /* The starting cell must be in bounds; cells we walk off the edge are clipped
   * (skipped) rather than erroring, matching termbox2's tb_print_ex. */
  if (!lt__g.back || x < 0 || y < 0 || x >= lt__g.width || y >= lt__g.height)
    return LT_ERR_OUT_OF_BOUNDS;

  const int start_x = x;
  size_t line_w = 0;  /* columns advanced on the current line */
  size_t max_w = 0;   /* widest line seen */

  while (*str) {
    int need = lt__utf8_char_length(*str);
    lt_uchar cp;

    if (need <= 0 || need > 4) {
      /* Invalid lead byte: emit U+FFFD and resync by one byte. */
      cp = 0xFFFD;
      str += 1;
    } else {
      /* Stop the decode at the NUL terminator if the sequence is truncated, so
       * we never read past the string. */
      size_t avail = 0;
      while (avail < (size_t)need && str[avail])
        avail++;

      int dec = lt__utf8_decode(str, avail, &cp);
      if (dec <= 0) {
        cp = 0xFFFD;
        str += 1; /* resync one byte on a malformed/truncated sequence */
      } else {
        str += dec;
      }
    }

    if (cp == '\n') {
      if (line_w > max_w)
        max_w = line_w;
      line_w = 0;
      x = start_x;
      y += 1;
      continue;
    }

    int w = lt__wcwidth(cp);
    if (w < 0) {
      /* Non-printable (control char etc.): show a replacement glyph, width 1. */
      cp = 0xFFFD;
      w = 1;
    }

    if (w == 0) {
      /* Zero-width (combining mark): no cell of its own, no column advance. A
       * full grapheme renderer would merge it into the previous cell; libterm
       * has no extend-cell yet, so it is simply not placed. */
      continue;
    }

    /* Clip off-buffer cells but keep advancing, so a later '\n' still resets to
     * start_x and trailing lines render. A 2-column character occupies the
     * starting cell; the second column is left for the renderer (no cell of its
     * own, matching how termbox2 advances without writing a spacer). */
    (void)lt_set_cell(x, y, cp, fg, bg);
    x += w;
    line_w += (size_t)w;
  }

  if (line_w > max_w)
    max_w = line_w;
  if (out_w)
    *out_w = max_w;

  return LT_OK;
}

int lt_print(int x, int y, lt_attr fg, lt_attr bg, const char *str) {
  return lt_print_ex(x, y, fg, bg, NULL, str);
}

int lt_printf(int x, int y, lt_attr fg, lt_attr bg, const char *fmt, ...) {
  char buf[1024];
  va_list vl;
  va_start(vl, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, vl);
  va_end(vl);

  if (n < 0)
    return LT_ERR;
  /* vsnprintf truncates into buf and NUL-terminates; an over-long result is
   * printed truncated rather than rejected. */
  return lt_print(x, y, fg, bg, buf);
}

/*
 * Responsive layout demo: a centered box + live info text that re-center on
 * every terminal resize. The one pattern this file teaches: derive ALL
 * layout from lt_width()/lt_height() inside draw(), wrap text to the width you
 * computed, and redraw whenever
 * LT_EVENT_RESIZE arrives — after a resize the library repaints the full
 * screen on the next lt_present, so there is nothing to clear or invalidate
 * manually. Drag the window; press q (or Esc / Ctrl+C) to quit.
 */

#include "libterm/libterm.h"

#include <stdio.h>
#include <string.h>

/* Print a text segment centered on row y, clipped to [x0, x1); every write
 * is a bounds-checked lt_set_cell, so degenerate terminal sizes are safe. */
static void print_centered(int y, int x0, int x1, const char *s, int len,
                           lt_attr fg) {
  int x = x0 + (x1 - x0 - len) / 2;
  for (int i = 0; i < len; i++) {
    int cx = x + i;
    if (cx >= x0 && cx < x1)
      lt_set_cell(cx, y, (lt_uchar)s[i], fg, LT_DEFAULT);
  }
}

struct text_row {
  const char *s;
  int len;
  lt_attr attr;
};

/* Word-wrap s into rows of at most `width` chars: break at the last space
 * that fits, hard-split words longer than the width, and skip the leading
 * spaces of continuation rows. Appends to rows[n..]; returns the new count. */
static int wrap_into(struct text_row *rows, int n, int cap, const char *s,
                     lt_attr attr, int width) {
  int len = (int)strlen(s);
  if (width < 1)
    return n;
  int pos = 0;
  while (pos < len && n < cap) {
    while (pos < len && s[pos] == ' ')
      pos++;
    if (pos >= len)
      break;
    int take = len - pos < width ? len - pos : width;
    if (pos + take < len && s[pos + take] != ' ') {
      int k = take;
      while (k > 0 && s[pos + k - 1] != ' ')
        k--;
      if (k > 0)
        take = k;
    }
    rows[n].s = s + pos;
    rows[n].len = take;
    rows[n].attr = attr;
    n++;
    pos += take;
  }
  return n;
}

static void draw(int resizes) {
  int w = lt_width(), h = lt_height();

  lt_clear();

  /* Width adapts to the terminal: capped at 44, clamped to >= 2. */
  int bw = w - 2 < 44 ? w - 2 : 44;
  if (bw < 2)
    bw = 2;

  /* Wrap the info lines to the box's inner width — narrowing the terminal
   * flows the text onto new rows (wrapping is app-level layout in a
   * cell-grid library; this is the pattern). */
  char size_line[32], count_line[32];
  snprintf(size_line, sizeof size_line, "terminal: %d x %d", w, h);
  snprintf(count_line, sizeof count_line, "resizes: %d", resizes);
  struct text_row rows[16];
  int iw = bw - 2;
  int n = 0;
  n = wrap_into(rows, n, 16, "libterm resize demo", LT_WHITE | LT_BOLD, iw);
  n = wrap_into(rows, n, 16, size_line, LT_WHITE, iw);
  n = wrap_into(rows, n, 16, count_line, LT_WHITE, iw);
  n = wrap_into(rows, n, 16, "drag the window - q quits", LT_WHITE | LT_DIM,
                iw);

  /* Height fits the wrapped text (+ borders), clamped to the terminal;
   * rows that still don't fit are dropped from the bottom. */
  int bh = n + 2;
  if (bh > h - 2)
    bh = h - 2;
  if (bh < 2)
    bh = 2;

  int x0 = (w - bw) / 2;
  int y0 = (h - bh) / 2;
  if (x0 < 0)
    x0 = 0;
  if (y0 < 0)
    y0 = 0;
  int x1 = x0 + bw, y1 = y0 + bh; /* exclusive */

  /* Border: Unicode box drawing, written as raw codepoints. */
  for (int x = x0 + 1; x < x1 - 1; x++) {
    lt_set_cell(x, y0, 0x2500, LT_CYAN, LT_DEFAULT);     /* ─ */
    lt_set_cell(x, y1 - 1, 0x2500, LT_CYAN, LT_DEFAULT); /* ─ */
  }
  for (int y = y0 + 1; y < y1 - 1; y++) {
    lt_set_cell(x0, y, 0x2502, LT_CYAN, LT_DEFAULT);     /* │ */
    lt_set_cell(x1 - 1, y, 0x2502, LT_CYAN, LT_DEFAULT); /* │ */
  }
  lt_set_cell(x0, y0, 0x250C, LT_CYAN, LT_DEFAULT);         /* ┌ */
  lt_set_cell(x1 - 1, y0, 0x2510, LT_CYAN, LT_DEFAULT);     /* ┐ */
  lt_set_cell(x0, y1 - 1, 0x2514, LT_CYAN, LT_DEFAULT);     /* └ */
  lt_set_cell(x1 - 1, y1 - 1, 0x2518, LT_CYAN, LT_DEFAULT); /* ┘ */

  /* Wrapped rows, vertically centered inside the box. */
  int ty = y0 + 1 + (bh - 2 - n) / 2;
  if (ty <= y0)
    ty = y0 + 1;
  for (int i = 0; i < n && ty + i < y1 - 1; i++)
    print_centered(ty + i, x0 + 1, x1 - 1, rows[i].s, rows[i].len,
                   rows[i].attr);

  lt_present();
}

int main(void) {
  int rc = lt_init();
  if (rc != LT_OK) {
    fprintf(stderr, "lt_init failed: %s\n", lt_strerror(rc));
    return 1;
  }

  lt_hide_cursor(); /* cursor is visible by default; it would otherwise park
                     * (and blink) at the end of the last painted row */

  int resizes = 0;
  draw(resizes);

  for (;;) {
    struct lt_event ev;
    rc = lt_poll_event(&ev);
    if (rc != LT_OK)
      continue;

    if (ev.type == LT_EVENT_RESIZE) {
      resizes++;
      draw(resizes);
    } else if (ev.type == LT_EVENT_KEY &&
               (ev.ch == 'q' || ev.ch == 'Q' || ev.key == LT_KEY_ESC ||
                ev.key == LT_KEY_CTRL_C)) {
      break;
    }
  }

  lt_shutdown();
  return 0;
}

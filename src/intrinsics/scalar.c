#include "diff.h"

/* _reserved is the grapheme-cluster id (0 = none, nonzero = an interned
 * cluster; equal id <=> equal cluster). It is part of what makes a cell render
 * differently, so it MUST be in the equality test — otherwise swapping two
 * clusters that share the same base ch/fg/bg (e.g. e+acute -> e+grave) is
 * wrongly treated as "unchanged" and the glyph goes stale. This matches the
 * byte-wise SIMD backends, which compare all 16 bytes including _reserved. */
int lt__simd_diff_first_differ_cell(const struct lt_cell *a,
                                    const struct lt_cell *b, int count) {
  for (int i = 0; i < count; i++)
    if (a[i].ch != b[i].ch || a[i].fg != b[i].fg || a[i].bg != b[i].bg ||
        a[i]._reserved != b[i]._reserved)
      return i;
  return count;
}

int lt__simd_diff_first_equal_cell(const struct lt_cell *a,
                                   const struct lt_cell *b, int count) {
  for (int i = 0; i < count; i++)
    if (a[i].ch == b[i].ch && a[i].fg == b[i].fg && a[i].bg == b[i].bg &&
        a[i]._reserved == b[i]._reserved)
      return i;

  return count;
}

void lt__simd_fill_cells(struct lt_cell *buf, int count, lt_attr fg,
                         lt_attr bg) {
  for (int i = 0; i < count; i++) {
    buf[i].ch = ' ';
    buf[i].fg = fg;
    buf[i].bg = bg;
    buf[i]._reserved = 0;
  }
}

/*
 * libterm - internal shared declarations.
 * Not installed. Do not expose lt__ (double-underscore) symbols publicly.
 */
#ifndef LIBTERM_INTERNAL_H
#define LIBTERM_INTERNAL_H

#include "lib/arena.h"
#include "libterm/libterm.h"
#include <stdbool.h>

#if defined(LIBTERM_ENABLE_RENDER_STATS)
#include <stdint.h>
#define LT_RENDER_STATS 1
#else
#define LT_RENDER_STATS 0
#endif

#if LT_RENDER_STATS
struct lt__render_stats {
  uint64_t present_calls;
  uint64_t dirty_rows;
  uint64_t diff_runs;
  uint64_t cursor_moves;
  uint64_t cells_rendered;
  uint64_t bytes_buffered;
  uint64_t flushes;
};
#endif

/* ---- fg/bg bit layout (see LT_* macros in libterm.h) ---- */
#define LT__COLOR_MASK 0x00FFFFFFu /* bits 0-23: color value */
#define LT__ATTR_MASK 0x7F000000u  /* bits 24-30: the 7 attribute bits */

/* ---- global state (single terminal instance) ---- */

struct lt__state {
  bool initialized;
  bool *dirty_rows;
  bool force_repaint; /* lt_invalidate: next present repaints every cell */
  int width;
  int height;
  int input_mode;
  int output_mode;
  int cur_x;
  int cur_y;
  lt_attr cur_fg;    /* SGR cache: last-emitted fg (color bits 0-23 + HI_BLACK) */
  lt_attr cur_bg;    /* SGR cache: last-emitted bg (color bits 0-23 + HI_BLACK) */
  lt_attr cur_attrs; /* SGR cache: last-emitted attr bits (LT__ATTR_MASK) */
  lt_attr clear_fg;
  lt_attr clear_bg;
  struct lt_cell *back;  /* back buffer  (w*h cells) */
  struct lt_cell *front; /* front buffer (w*h cells) */
#if LT_RENDER_STATS
  struct lt__render_stats stats;
#endif
  Arena *arena; /* arena for back/front buffers */
};

extern struct lt__state lt__g;

/* ---- buffer ops (shared/buffer.c) ---- */
int lt__buffer_resize(int w, int h);
void lt__buffer_free(void);
void lt__buffer_clear(struct lt_cell *buf, int count, lt_attr fg, lt_attr bg);

/* ---- utf8 (shared/utf8.c) ---- */
int lt__utf8_char_length(char c);
int lt__utf8_decode(const char *s, size_t len, lt_uchar *out);
int lt__utf8_encode(lt_uchar ch, char out[4]);

/* ---- character width (shared/wcwidth.c) ----
 * Columns a codepoint occupies: 0 (combining/zero-width), 2 (wide CJK/emoji),
 * 1 (normal), or -1 (non-printable). Markus Kuhn reference ranges. */
int lt__wcwidth(lt_uchar ch);

/* ---- SGR / run emission (shared/sgr.c) ----
 * Platform-independent VT byte construction; calls lt__plat_reserve/commit
 * (platform.h) to reach the per-platform output buffer. */
int lt__write_uint(char *buf, int v); /* int -> decimal ASCII; returns digits */
int lt__emit_sgr(lt_attr fg, lt_attr bg, lt_attr attrs);
int lt__render_run(const struct lt_cell *cells, int count);

#endif /* LIBTERM_INTERNAL_H */

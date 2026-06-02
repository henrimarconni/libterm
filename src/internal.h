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
  int last_errno; /* errno captured at the last failing syscall (lt_last_errno)
                   */
  int width;
  int height;
  int input_mode;
  int output_mode;
  int cur_x;
  int cur_y;
  lt_attr cur_fg; /* SGR cache: last-emitted fg (color bits 0-23 + HI_BLACK) */
  lt_attr cur_bg; /* SGR cache: last-emitted bg (color bits 0-23 + HI_BLACK) */
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
/* (Re)allocate the back/front buffers to w*h cells (one arena allocation holds
 * both, reset on each call), clear them to the current clear attrs, mark every
 * row dirty, and update lt__g.width/height. No-op returning LT_OK if the size
 * is unchanged. Returns LT_ERR for a non-positive dimension, LT_ERR_MEM on
 * allocation failure (or a w*h that would overflow the size computation). */
int lt__buffer_resize(int w, int h);
/* Release the cell buffers and their arena and zero the size; safe to call when
 * nothing is allocated. */
void lt__buffer_free(void);
/* Fill `count` cells of `buf` with a blank glyph in fg/bg (dispatches to the
 * SIMD fill backend). */
void lt__buffer_clear(struct lt_cell *buf, int count, lt_attr fg, lt_attr bg);

/* ---- utf8 (shared/utf8.c) ---- */
/* Bytes in the UTF-8 sequence lead byte `c` begins: 1-4, or 0 for an invalid
 * lead byte. Inspects only `c`. */
int lt__utf8_char_length(char c);
/* Decode up to `len` bytes of `s` into *out, rejecting overlong encodings,
 * surrogates, and codepoints above U+10FFFF. Returns the number of bytes
 * consumed (1-4), or 0 on a NULL argument, len 0, or a malformed/incomplete
 * sequence. */
int lt__utf8_decode(const char *s, size_t len, lt_uchar *out);
/* Encode codepoint `ch` into `out` (up to 4 bytes, not NUL-terminated). Returns
 * the byte count (1-4), or 0 for a surrogate or out-of-range codepoint. */
int lt__utf8_encode(lt_uchar ch, char out[4]);

/* ---- character width (shared/wcwidth.c) ----
 * Columns a codepoint occupies: 0 (combining/zero-width), 2 (wide CJK/emoji),
 * 1 (normal), or -1 (non-printable). Markus Kuhn reference ranges. */
int lt__wcwidth(lt_uchar ch);

/* ---- grapheme-cluster table (shared/egc.c) ----
 * Interns a cluster (base + trailing codepoints) and returns a nonzero id to
 * store in lt_cell._reserved; identical clusters share one id (content dedup),
 * which keeps the SIMD byte-equality diff invariant valid. id 0 = no cluster.
 * lt__egc_intern returns 0 on a single-codepoint cluster (caller stores the
 * base in ch directly) or on allocation failure (*ok reports which). */
uint32_t lt__egc_intern(const lt_uchar *chs, size_t nch, bool *ok);
/* Look up an interned cluster by id: returns its codepoint array and sets
 * *nch. Returns NULL for id 0 or an unknown id. */
const lt_uchar *lt__egc_get(uint32_t id, size_t *nch);
/* Drop all interned clusters (called from lt_shutdown so a fresh session
 * starts with an empty table). */
void lt__egc_reset(void);

/* ---- SGR / run emission (shared/sgr.c) ----
 * Platform-independent VT byte construction; calls lt__plat_reserve/commit
 * (platform.h) to reach the per-platform output buffer. */
int lt__write_uint(char *buf, int v); /* int -> decimal ASCII; returns digits */
/* Emit a CSI ... m sequence to move the terminal's SGR state to fg/bg/attrs,
 * diffing against the cached cur_fg/cur_bg/cur_attrs so an unchanged style
 * emits nothing. Negative attr deltas force a reset-then-reapply. Returns
 * LT_OK, or LT_ERR if the output buffer can't be reserved. */
int lt__emit_sgr(lt_attr fg, lt_attr bg, lt_attr attrs);
/* Emit `count` consecutive cells: split into same-style spans (each prefixed by
 * lt__emit_sgr) and write their glyph bytes, falling back to per-cell emission
 * for spans containing a grapheme cluster. Returns LT_OK or a write error. */
int lt__render_run(const struct lt_cell *cells, int count);

#endif /* LIBTERM_INTERNAL_H */

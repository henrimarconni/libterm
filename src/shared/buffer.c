#define LT_ARENA_IMPLEMENTATION
#include "internal.h"
#include "intrinsics/diff.h"
#include "lib/arena.h"
#include <stdlib.h>
#include <string.h>

#define LT_ARENA_BYTES_INIT 4096

int lt__buffer_resize(int w, int h) {
  if (w <= 0 || h <= 0)
    return LT_ERR;

  if (lt__g.width == w && lt__g.height == h)
    return LT_OK;

  /* Cast before multiplying: w/h are int (up to ~65535 from ws_col/ws_row), so
   * w * h in int arithmetic can exceed INT_MAX — signed overflow is UB. */
  const size_t n = (size_t)w * (size_t)h;
  if (n == 0 || n > (SIZE_MAX / (2 * sizeof(struct lt_cell))))
    return LT_ERR_MEM;

  const size_t cells_bytes = n * sizeof(struct lt_cell);
  if (!lt__g.arena) {
    size_t arena_bytes = cells_bytes * 2;
    if (arena_bytes < LT_ARENA_BYTES_INIT)
      arena_bytes = LT_ARENA_BYTES_INIT;

    lt__g.arena = lt_arena_create(arena_bytes);
    if (!lt__g.arena)
      return LT_ERR_MEM;
  }

  lt_arena_reset(lt__g.arena);

  struct lt_cell *base = lt_arena_alloc(lt__g.arena, cells_bytes * 2,
                                        LT_ARENA_ALIGNOF(struct lt_cell));
  if (!base)
    return LT_ERR_MEM;

  lt__g.back = base;
  lt__g.front = base + n;

  if (lt__g.width != w)
    lt__g.width = w;

  if (lt__g.height != h)
    lt__g.height = h;

  lt__buffer_clear(lt__g.back, (int)n, lt__g.clear_fg, lt__g.clear_bg);
  lt__buffer_clear(lt__g.front, (int)n, lt__g.clear_fg, lt__g.clear_bg);

  lt__g.dirty_rows = lt_arena_alloc(lt__g.arena, (size_t)h * sizeof(bool),
                                    LT_ARENA_ALIGNOF(bool));
  if (!lt__g.dirty_rows)
    return LT_ERR_MEM;

  for (int i = 0; i < h; i++)
    lt__g.dirty_rows[i] = true;

  return LT_OK;
}

/* Apply a terminal size change end to end: resize the cell buffers, reset
 * the cursor cache, and force the next present to repaint every cell — the
 * terminal's reflowed content no longer matches the (freshly blanked) front
 * buffer, so a diffing present would skip blank-equal cells and leave stale
 * glyphs on screen (the resize ghost bug; same out-of-band desync rationale
 * as lt_invalidate). Fills *ev as the LT_EVENT_RESIZE to surface. Returns
 * LT_ERR_NO_EVENT for a non-positive or unchanged size (callers continue
 * their read loop), a buffer error code on allocation failure, else LT_OK. */
int lt__handle_resize(int new_w, int new_h, struct lt_event *ev) {
  if (new_w <= 0 || new_h <= 0)
    return LT_ERR_NO_EVENT;
  if (new_w == lt__g.width && new_h == lt__g.height)
    return LT_ERR_NO_EVENT;

  int rc = lt__buffer_resize(new_w, new_h);
  if (rc != LT_OK)
    return rc;

  lt__g.cur_x = -1;
  lt__g.cur_y = -1;
  lt__g.force_repaint = true;

  memset(ev, 0, sizeof(*ev));
  ev->type = LT_EVENT_RESIZE;
  ev->w = new_w;
  ev->h = new_h;
  return LT_OK;
}

void lt__buffer_free(void) {
  if (lt__g.arena)
    lt_arena_free(lt__g.arena);

  lt__g.arena = NULL;
  lt__g.back = NULL;
  lt__g.front = NULL;
  lt__g.dirty_rows = NULL;
  lt__g.width = 0;
  lt__g.height = 0;
}

void lt__buffer_clear(struct lt_cell *buf, int count, lt_attr fg, lt_attr bg) {
  lt__simd_fill_cells(buf, count, fg, bg);
}

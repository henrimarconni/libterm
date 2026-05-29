#include "internal.h"
#include "platform.h"
#include "posix_internal.h"
#include <string.h>
#include <unistd.h>

#define LT__OUTBUF_CAP (64 * 1024)

static char lt__outbuf[LT__OUTBUF_CAP];
static size_t lt__outbuf_len = 0;

static int lt__posix_raw_write(const char *buf, size_t len) {

#if defined(LIBTERM_BENCH_HEADLESS_OUTPUT)
  (void)buf;
  (void)len;
  return LT_OK;
#endif

  int ttyfd = lt__posix_get_tty_fd();
  if (ttyfd < 0)
    return LT_ERR;

  while (len > 0) {
    ssize_t n = write(ttyfd, buf, len);
    if (n < 0)
      return LT_ERR;
    if (n == 0)
      return LT_ERR;

    buf += (size_t)n;
    len -= (size_t)n;
  }

  return LT_OK;
}

static char *lt__plat_reserve(size_t max) {
  if (max > LT__OUTBUF_CAP)
    return NULL;

  if (lt__outbuf_len + max > LT__OUTBUF_CAP)
    if (lt__plat_flush() != LT_OK)
      return NULL;

  return lt__outbuf + lt__outbuf_len;
}

static void lt__plat_commit(size_t actual) {
#if LT_RENDER_STATS
  lt__g.stats.bytes_buffered += (uint64_t)actual;
#endif
  lt__outbuf_len += actual;
}

static int lt__posix_write_uint(char *buf, int v) {
  if (v < 0)
    v = 0;

  if (v == 0) {
    buf[0] = '0';
    return 1;
  }

  char tmp[10];
  int n = 0;
  while (v > 0) {
    tmp[n++] = (char)('0' + (v % 10));
    v /= 10;
  }

  for (int i = 0; i < n; i++)
    buf[i] = tmp[n - i - 1];

  return n;
}

int lt__plat_write(const char *buf, size_t len) {
  if (len == 0)
    return LT_OK;

  if (len > LT__OUTBUF_CAP) {
    int rc = lt__plat_flush();
    if (rc != LT_OK)
      return rc;

    return lt__posix_raw_write(buf, len);
  }

  if (lt__outbuf_len + len > LT__OUTBUF_CAP) {
    int rc = lt__plat_flush();
    if (rc != LT_OK)
      return rc;
  }

  memcpy(lt__outbuf + lt__outbuf_len, buf, len);
#if LT_RENDER_STATS
  lt__g.stats.bytes_buffered += (uint64_t)len;
#endif
  lt__outbuf_len += len;
  return LT_OK;
}

int lt__plat_flush(void) {
  if (lt__outbuf_len == 0)
    return LT_OK;

#if LT_RENDER_STATS
  lt__g.stats.flushes++;
#endif

  int rc = lt__posix_raw_write(lt__outbuf, lt__outbuf_len);
  lt__outbuf_len = 0;
  return rc;
}

int lt__plat_clear_screen(void) {
  static const char seq[] = "\x1b[2J\x1b[H";
  return lt__plat_write(seq, sizeof(seq) - 1);
}

int lt__plat_move_cursor(int x, int y) {
  if (x < 0 || y < 0)
    return LT_ERR_OUT_OF_BOUNDS;

  char *p = lt__plat_reserve(24);
  if (!p)
    return LT_ERR;

  size_t pos = 0;
  p[pos++] = '\x1b';
  p[pos++] = '[';
  pos += (size_t)lt__posix_write_uint(p + pos, y + 1);
  p[pos++] = ';';
  pos += (size_t)lt__posix_write_uint(p + pos, x + 1);
  p[pos++] = 'H';

  lt__plat_commit(pos);
  return LT_OK;
}

int lt__plat_hide_cursor(void) {
  static const char seq[] = "\x1b[?25l";
  return lt__plat_write(seq, sizeof(seq) - 1);
}

int lt__plat_show_cursor(void) {
  static const char seq[] = "\x1b[?25h";
  return lt__plat_write(seq, sizeof(seq) - 1);
}

int lt__plat_render_cell(int x, int y, const struct lt_cell *c) {
  (void)x;
  (void)y;

  lt_uchar ch = c->ch ? c->ch : (lt_uchar)' ';

  if (ch < 0x80) {
    char b = (char)ch;
    return lt__plat_write(&b, 1);
  }

  char utf8[4];
  int ub = lt__utf8_encode(ch, utf8);
  if (ub <= 0) {
    utf8[0] = ' ';
    ub = 1;
  }

  return lt__plat_write(utf8, (size_t)ub);
}

static void lt__posix_emit_sep(char *p, size_t *pos, bool *wrote_any) {
  if (*wrote_any)
    p[(*pos)++] = ';';
  *wrote_any = true;
}

static void lt__posix_emit_sgr_256(char *p, size_t *pos, bool *wrote_any,
                                   int is_fg, lt_attr v) {
  int idx = (int)(v & 0xFF);
  lt__posix_emit_sep(p, pos, wrote_any);
  *pos += (size_t)lt__posix_write_uint(p + *pos, is_fg ? 38 : 48);
  p[(*pos)++] = ';';
  p[(*pos)++] = '5';
  p[(*pos)++] = ';';
  *pos += (size_t)lt__posix_write_uint(p + *pos, idx);
}

static void lt__posix_emit_sgr_truecolor(char *p, size_t *pos, bool *wrote_any,
                                         int is_fg, lt_attr v) {
  int r = (int)((v >> 16) & 0xFF);
  int g = (int)((v >> 8) & 0xFF);
  int b = (int)(v & 0xFF);

  lt__posix_emit_sep(p, pos, wrote_any);
  *pos += (size_t)lt__posix_write_uint(p + *pos, is_fg ? 38 : 48);
  p[(*pos)++] = ';';
  p[(*pos)++] = '2';
  p[(*pos)++] = ';';
  *pos += (size_t)lt__posix_write_uint(p + *pos, r);
  p[(*pos)++] = ';';
  *pos += (size_t)lt__posix_write_uint(p + *pos, g);
  p[(*pos)++] = ';';
  *pos += (size_t)lt__posix_write_uint(p + *pos, b);
}

static int lt__plat_emit_sgr(lt_attr fg, lt_attr bg, lt_attr attrs) {
  if (fg == lt__g.cur_fg && bg == lt__g.cur_bg && attrs == lt__g.cur_attrs)
    return LT_OK;

  char *p = lt__plat_reserve(64);
  if (!p)
    return LT_ERR;

  int mode = lt__g.output_mode;
  if (mode == LT_OUTPUT_CURRENT)
    mode = LT_OUTPUT_NORMAL;

  /* Depends only on mode, so compute once (after the no-change early return).
   * The per-fg/bg color masks stay inside their branches — those depend on the
   * fg/bg values and are wasted work on a span that re-emits only one of them. */
  bool indexed = (mode == LT_OUTPUT_256 || mode == LT_OUTPUT_216 ||
                  mode == LT_OUTPUT_GRAYSCALE);

  size_t pos = 0;
  p[pos++] = '\x1b';
  p[pos++] = '[';

  bool need_reset = false;

  /* If any attr bit was present and now removed, delta codes are messy;
   * reset then re-apply target style for correctness. */
  if ((lt__g.cur_attrs & ~attrs) != 0)
    need_reset = true;

  bool wrote_any = false;
  if (need_reset) {
    p[pos++] = '0';
    wrote_any = true;
  }

  /* fg */
  if (need_reset || fg != lt__g.cur_fg) {
    lt_attr fg_color = fg & LT__COLOR_MASK;
    /* A 0 color with no HI_BLACK sentinel means "terminal default". */
    bool fg_default = (fg_color == 0) && !(fg & LT_HI_BLACK);
    if (fg_default) {
      lt__posix_emit_sep(p, &pos, &wrote_any);
      p[pos++] = '3';
      p[pos++] = '9';
    } else if (indexed) {
      lt__posix_emit_sgr_256(p, &pos, &wrote_any, 1, fg_color);
    } else if (mode == LT_OUTPUT_TRUECOLOR) {
      lt__posix_emit_sgr_truecolor(p, &pos, &wrote_any, 1, fg_color);
    } else {
      lt__posix_emit_sep(p, &pos, &wrote_any);
      if (fg_color >= LT_BLACK && fg_color <= LT_WHITE) {
        p[pos++] = '3';
        p[pos++] = (char)('0' + (fg_color - LT_BLACK));
      } else {
        p[pos++] = '3';
        p[pos++] = '9';
      }
    }
  }

  /* bg */
  if (need_reset || bg != lt__g.cur_bg) {
    lt_attr bg_color = bg & LT__COLOR_MASK;
    bool bg_default = (bg_color == 0) && !(bg & LT_HI_BLACK);
    if (bg_default) {
      lt__posix_emit_sep(p, &pos, &wrote_any);
      p[pos++] = '4';
      p[pos++] = '9';
    } else if (indexed) {
      lt__posix_emit_sgr_256(p, &pos, &wrote_any, 0, bg_color);
    } else if (mode == LT_OUTPUT_TRUECOLOR) {
      lt__posix_emit_sgr_truecolor(p, &pos, &wrote_any, 0, bg_color);
    } else {
      lt__posix_emit_sep(p, &pos, &wrote_any);
      if (bg_color >= LT_BLACK && bg_color <= LT_WHITE) {
        p[pos++] = '4';
        p[pos++] = (char)('0' + (bg_color - LT_BLACK));
      } else {
        p[pos++] = '4';
        p[pos++] = '9';
      }
    }
  }

  /* attrs: after a reset we re-apply every target attr; otherwise only the
   * bits newly turned on. Both reduce to attrs_to_emit, so spans with no
   * attribute work (the common case) skip the table walk entirely. */
  static const struct {
    lt_attr bit;
    char code;
  } attr_map[] = {
      {LT_BOLD, '1'},  {LT_DIM, '2'},     {LT_ITALIC, '3'}, {LT_UNDERLINE, '4'},
      {LT_BLINK, '5'}, {LT_REVERSE, '7'}, {LT_STRIKE, '9'},
  };

  lt_attr attrs_to_emit = need_reset ? attrs : (attrs & ~lt__g.cur_attrs);
  if (attrs_to_emit) {
    for (size_t i = 0; i < sizeof(attr_map) / sizeof(attr_map[0]); i++) {
      if (!(attrs_to_emit & attr_map[i].bit))
        continue;
      if (wrote_any)
        p[pos++] = ';';
      p[pos++] = attr_map[i].code;
      wrote_any = true;
    }
  }

  /* Safety: if somehow nothing got written, avoid emitting malformed ESC[ m. */
  if (!wrote_any)
    return LT_OK;

  p[pos++] = 'm';
  lt__plat_commit(pos);
  return LT_OK;
}

int lt__plat_render_run(const struct lt_cell *cells, int count) {
  if (count <= 0)
    return LT_OK;

  lt_attr span_fg = 0;
  lt_attr span_bg = 0;
  lt_attr span_attrs = 0;

  int sgr_rc = 0, span_len = 0, rc = 0, ub = 0;
  size_t max = 0, pos = 0;
  char *p = NULL;

  lt_uchar ch = (lt_uchar)' ';

  int i = 0, j = 0;
  while (i < count) {
    span_fg = cells[i].fg & (LT__COLOR_MASK | LT_HI_BLACK);
    span_bg = cells[i].bg & (LT__COLOR_MASK | LT_HI_BLACK);
    span_attrs = cells[i].fg & LT__ATTR_MASK;

    if (span_fg != lt__g.cur_fg || span_bg != lt__g.cur_bg ||
        span_attrs != lt__g.cur_attrs) {
      sgr_rc = lt__plat_emit_sgr(span_fg, span_bg, span_attrs);
      if (sgr_rc != LT_OK)
        return sgr_rc;

      lt__g.cur_fg = span_fg;
      lt__g.cur_bg = span_bg;
      lt__g.cur_attrs = span_attrs;
    }

    j = i + 1;
    while (j < count) {
      if ((cells[j].fg & (LT__COLOR_MASK | LT_HI_BLACK)) != span_fg ||
          (cells[j].bg & (LT__COLOR_MASK | LT_HI_BLACK)) != span_bg ||
          (cells[j].fg & LT__ATTR_MASK) != span_attrs)
        break;
      j++;
    }

    span_len = j - i;
    max = (size_t)span_len * 4;
    p = lt__plat_reserve(max);
    if (!p) {
      for (int k = i; k < j; k++) {
        rc = lt__plat_render_cell(0, 0, &cells[k]);
        if (rc != LT_OK)
          return rc;
      }
      i = j;
      continue;
    }

    pos = 0;
    for (int k = i; k < j; k++) {
      ch = cells[k].ch ? cells[k].ch : (lt_uchar)' ';

      if (ch < 0x80) {
        p[pos++] = (char)ch;
        continue;
      }

      char tmp[4];
      ub = lt__utf8_encode(ch, tmp);
      if (ub <= 0) {
        p[pos++] = ' ';
        continue;
      }

      for (int t = 0; t < ub; t++)
        p[pos++] = tmp[t];
    }

    lt__plat_commit(pos);
    i = j;
  }

  return LT_OK;
}

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

static int lt__plat_emit_sgr(lt_attr fg, lt_attr bg, lt_attr attrs) {
  char *p = lt__plat_reserve(32);
  if (!p)
    return LT_ERR;

  size_t pos = 0;
  p[pos++] = '\x1b';
  p[pos++] = '[';
  p[pos++] = '0';

  /* fg: 30..37 for LT_BLACK..LT_WHITE, 39 for default. */
  if (fg >= LT_BLACK && fg <= LT_WHITE) {
    p[pos++] = ';';
    p[pos++] = '3';
    p[pos++] = (char)('0' + (fg - LT_BLACK));
  } else {
    p[pos++] = ';';
    p[pos++] = '3';
    p[pos++] = '9';
  }

  /* bg: 40..47 for LT_BLACK..LT_WHITE, 49 for default. */
  if (bg >= LT_BLACK && bg <= LT_WHITE) {
    p[pos++] = ';';
    p[pos++] = '4';
    p[pos++] = (char)('0' + (bg - LT_BLACK));
  } else {
    p[pos++] = ';';
    p[pos++] = '4';
    p[pos++] = '9';
  }

  /* attr bits in stable order for test assertions. */
  if (attrs & LT_BOLD) {
    p[pos++] = ';';
    p[pos++] = '1';
  }
  if (attrs & LT_DIM) {
    p[pos++] = ';';
    p[pos++] = '2';
  }
  if (attrs & LT_ITALIC) {
    p[pos++] = ';';
    p[pos++] = '3';
  }
  if (attrs & LT_UNDERLINE) {
    p[pos++] = ';';
    p[pos++] = '4';
  }
  if (attrs & LT_BLINK) {
    p[pos++] = ';';
    p[pos++] = '5';
  }
  if (attrs & LT_REVERSE) {
    p[pos++] = ';';
    p[pos++] = '7';
  }
  if (attrs & LT_STRIKE) {
    p[pos++] = ';';
    p[pos++] = '9';
  }

  p[pos++] = 'm';

  lt__plat_commit(pos);
  return LT_OK;
}

int lt__plat_render_run(const struct lt_cell *cells, int count) {
  if (count <= 0)
    return LT_OK;

  lt_attr run_fg = cells[0].fg & 0x00FF;
  lt_attr run_bg = cells[0].bg & 0x00FF;
  lt_attr run_attrs = cells[0].fg & 0xFF00;

  if (run_fg != lt__g.cur_fg || run_bg != lt__g.cur_bg ||
      run_attrs != lt__g.cur_attrs) {
    int sgr_rc = lt__plat_emit_sgr(run_fg, run_bg, run_attrs);
    if (sgr_rc != LT_OK)
      return sgr_rc;
    lt__g.cur_fg = run_fg;
    lt__g.cur_bg = run_bg;
    lt__g.cur_attrs = run_attrs;
  }

  size_t max = (size_t)count * 4;
  char *p = lt__plat_reserve(max);
  if (!p) {
    int rc = 0;
    for (int i = 0; i < count; i++) {
      rc = lt__plat_render_cell(0, 0, &cells[i]);
      if (rc != LT_OK)
        return rc;
    }
    return LT_OK;
  }

  size_t pos = 0;
  int ub = 0;
  lt_uchar ch = (lt_uchar)' ';
  for (int i = 0; i < count; i++) {
    ch = cells[i].ch ? cells[i].ch : (lt_uchar)' ';
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

    for (int j = 0; j < ub; j++)
      p[pos++] = tmp[j];
  }

  lt__plat_commit(pos);
  return LT_OK;
}

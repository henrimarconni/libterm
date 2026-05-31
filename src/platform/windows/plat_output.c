#include "internal.h"
#include "libterm/libterm.h"
#include "platform.h"
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* conhost is within ~10% of fast-pipe so long
 * as it receives large writes. 64 KiB confortably holds a full screen
 * frame's VT bytes (incl. cursor jumps and later, SGR), so most presents
 * become a single WriteFile call. */
#define LT__OUTBUF_CAP (64 * 1024)

static char lt__outbuf[LT__OUTBUF_CAP];
static size_t lt__outbuf_len = 0;

char *lt__plat_reserve(size_t max) {
  if (max > LT__OUTBUF_CAP)
    return NULL;

  if (lt__outbuf_len + max > LT__OUTBUF_CAP) {
    if (lt__plat_flush() != LT_OK)
      return NULL;
  }

  return lt__outbuf + lt__outbuf_len;
}

void lt__plat_commit(size_t actual) {
#if LT_RENDER_STATS
  lt__g.stats.bytes_buffered += (uint64_t)actual;
#endif

  lt__outbuf_len += actual;
}

static int lt__win_raw_write(const char *buf, size_t len) {
  if (len == 0)
    return LT_OK;

#if defined(LIBTERM_BENCH_HEADLESS_OUTPUT)
  (void)buf;
  (void)len;
  return LT_OK;
#endif

  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  if (out == NULL || out == INVALID_HANDLE_VALUE)
    return LT_ERR;

  DWORD written = 0;
  if (!WriteFile(out, buf, (DWORD)len, &written, NULL))
    return LT_ERR;

  if (written != (DWORD)len)
    return LT_ERR;

  return LT_OK;
}

int lt__plat_write(const char *buf, size_t len) {
  if (len == 0)
    return LT_OK;

  if (len > LT__OUTBUF_CAP) {
    int rc = lt__plat_flush();
    if (rc != LT_OK)
      return rc;

    return lt__win_raw_write(buf, len);
  }

  if (lt__outbuf_len + len > LT__OUTBUF_CAP) {
    int rc = lt__plat_flush();
    if (rc != LT_OK)
      return rc;
  }

#if LT_RENDER_STATS
  lt__g.stats.bytes_buffered += (uint64_t)len;
#endif

  memcpy(lt__outbuf + lt__outbuf_len, buf, len);
  lt__outbuf_len += len;

  return LT_OK;
}

int lt__plat_flush(void) {
  if (lt__outbuf_len == 0)
    return LT_OK;

#if LT_RENDER_STATS
  lt__g.stats.flushes++;
#endif

  int rc = lt__win_raw_write(lt__outbuf, lt__outbuf_len);
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

  /* worst case: '\x1b[' (2) + 10 digits + ';' (1) + 10 digits + 'H' (1) = 24 */
  char *p = lt__plat_reserve(24);
  if (!p)
    return LT_ERR;

  size_t pos = 0;
  p[pos++] = '\x1b';
  p[pos++] = '[';
  pos += (size_t)lt__write_uint(p + pos, y + 1);
  p[pos++] = ';';
  pos += (size_t)lt__write_uint(p + pos, x + 1);
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

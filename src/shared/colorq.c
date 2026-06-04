/*
 * libterm - color querying (shared half).
 *
 * The OSC reply parser plus, from Task 4 on, the public lt_query_color /
 * lt_is_dark_background entry points. The platform mechanism (POSIX OSC
 * round-trip, Windows console color table) lives behind
 * lt__plat_query_color (platform.h).
 */
#include "internal.h"
#include "libterm/libterm.h"
#include "platform.h"
#include <string.h>

static int lt__hexval(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

/* Parse one rgb: channel of 1-4 hex digits starting at *i; advance *i past
 * them. Scale to 8 bits with rounding: out = round(v * 255 / (16^n - 1)),
 * so doubled-byte 16-bit forms ("1c1c") map exactly to their byte ("1c").
 * Returns LT_OK, or LT_ERR if no hex digit is present. */
static int lt__parse_channel(const char *s, size_t len, size_t *i,
                             unsigned *out) {
  unsigned v = 0;
  int n = 0;
  while (*i < len && n < 4) {
    int hv = lt__hexval(s[*i]);
    if (hv < 0)
      break;
    v = v * 16u + (unsigned)hv;
    (*i)++;
    n++;
  }
  if (n == 0)
    return LT_ERR;
  unsigned max = (1u << (4 * n)) - 1u;
  *out = (v * 255u + max / 2u) / max;
  return LT_OK;
}

int lt__color_parse_osc_reply(const char *payload, size_t len, int what,
                              uint32_t *rgb) {
  size_t i = 0;

  /* The reply echoes the query's selector: "10;" (default fg, what == -1),
   * "11;" (default bg, what == -2), or "4;<idx>;" (palette). The -1/-2
   * values are LT_COLOR_DEFAULT_FG / LT_COLOR_DEFAULT_BG (libterm.h). */
  if (what == -1 || what == -2) {
    char want = (what == -1) ? '0' : '1';
    if (len < 3 || payload[0] != '1' || payload[1] != want ||
        payload[2] != ';')
      return LT_ERR;
    i = 3;
  } else {
    if (len < 2 || payload[0] != '4' || payload[1] != ';')
      return LT_ERR;
    i = 2;
    int idx = 0;
    int any = 0;
    while (i < len && payload[i] >= '0' && payload[i] <= '9') {
      idx = idx * 10 + (payload[i] - '0');
      /* Reject early: indexes never exceed 255, and an unbounded digit run
       * is untrusted input (same caution as lt__accum_digit). */
      if (idx > 255)
        return LT_ERR;
      any = 1;
      i++;
    }
    if (!any || i >= len || payload[i] != ';' || idx != what)
      return LT_ERR;
    i++;
  }

  if (len < i + 4 || memcmp(payload + i, "rgb:", 4) != 0)
    return LT_ERR;
  i += 4;

  unsigned ch[3];
  for (int c = 0; c < 3; c++) {
    if (lt__parse_channel(payload, len, &i, &ch[c]) != LT_OK)
      return LT_ERR;
    if (c < 2) {
      if (i >= len || payload[i] != '/')
        return LT_ERR;
      i++;
    }
  }
  if (i != len)
    return LT_ERR; /* trailing junk (e.g. a fourth channel) */

  *rgb = ((uint32_t)ch[0] << 16) | ((uint32_t)ch[1] << 8) | (uint32_t)ch[2];
  return LT_OK;
}

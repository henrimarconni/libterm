#include "internal.h"

int lt__utf8_char_length(char c) {
  unsigned char u = (unsigned char)c;
  if ((u & 0x80) == 0x00)
    return 1;
  if ((u & 0xE0) == 0xC0)
    return 2;
  if ((u & 0xF0) == 0xE0)
    return 3;
  if ((u & 0xF8) == 0xF0)
    return 4;
  return 0;
}

int lt__utf8_decode(const char *s, size_t len, lt_uchar *out) {
  if (!s || !out || len == 0)
    return 0;

  const unsigned char b0 = (unsigned char)s[0];

  /* 1-byte ASCII */
  if (b0 < 0x80) {
    *out = (lt_uchar)b0;
    return 1;
  }

  /* Continuation bytes cannot start a sequence*/
  if ((b0 & 0xC0) == 0x80)
    return 0;

  /* 2-byte sequence */
  if (b0 >= 0xC2 && b0 <= 0xDF) {
    if (len < 2)
      return 0;

    const unsigned char b1 = (unsigned char)s[1];
    if ((b1 & 0xC0) != 0x80)
      return 0;

    *out = (lt_uchar)(((b0 & 0x1F) << 6) | (b1 & 0x3F));
    return 2;
  }

  /* 3-byte sequence */
  if (b0 >= 0xE0 && b0 <= 0xEF) {
    if (len < 3)
      return 0;

    const unsigned char b1 = (unsigned char)s[1];
    const unsigned char b2 = (unsigned char)s[2];

    if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80)
      return 0;

    /* Overlong and surrogates checks */
    if (b0 == 0xE0 && b1 < 0xA0)
      return 0; // overlong

    if (b0 == 0xED && b1 >= 0xA0)
      return 0; // surrogate

    lt_uchar cp =
        (lt_uchar)(((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F));

    if (cp >= 0xD800 && cp <= 0xDFFF)
      return 0;

    *out = cp;
    return 3;
  }

  /* 4-byte sequence */
  if (b0 >= 0xF0 && b0 <= 0xF4) {
    if (len < 4)
      return 0;

    const unsigned char b1 = (unsigned char)s[1];
    const unsigned char b2 = (unsigned char)s[2];
    const unsigned char b3 = (unsigned char)s[3];

    if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80)
      return 0;

    /* Overlong and max U+10FFFF checks */
    if (b0 == 0xF0 && b1 < 0x90)
      return 0; // overlong

    if (b0 == 0xF4 && b1 > 0x8F)
      return 0; // max U+10FFFF

    lt_uchar cp = (lt_uchar)(((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) |
                             ((b2 & 0x3F) << 6) | (b3 & 0x3F));

    if (cp > 0x10FFFF)
      return 0;

    *out = cp;
    return 4;
  }

  return 0;
}

int lt__utf8_encode(lt_uchar ch, char out[4]) {
  if (ch < 0x80) {
    out[0] = (char)ch;
    return 1;
  }

  if (ch < 0x800) {
    out[0] = (char)(0xC0 | (ch >> 6));
    out[1] = (char)(0x80 | (ch & 0x3F));
    return 2;
  }

  if (ch < 0x10000) {
    if (ch >= 0xD800 && ch <= 0xDFFF)
      return 0;

    out[0] = (char)(0xE0 | (ch >> 12));
    out[1] = (char)(0x80 | ((ch >> 6) & 0x3F));
    out[2] = (char)(0x80 | (ch & 0x3F));
    return 3;
  }

  if (ch < 0x110000) {
    out[0] = (char)(0xF0 | (ch >> 18));
    out[1] = (char)(0x80 | ((ch >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((ch >> 6) & 0x3F));
    out[3] = (char)(0x80 | (ch & 0x3F));
    return 4;
  }

  return 0;
}

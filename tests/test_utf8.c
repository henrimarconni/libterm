#include "internal.h"
#include <assert.h>
#include <string.h>

static void expect_ok(const unsigned char *s, size_t len, int want_n,
                      lt_uchar want_cp) {
  lt_uchar cp = 0;
  int n = lt__utf8_decode((const char *)s, len, &cp);
  assert(n == want_n);
  assert(cp == want_cp);
}

static void expect_bad(const unsigned char *s, size_t len) {
  lt_uchar cp = 0xDEADBEEF;
  int n = lt__utf8_decode((const char *)s, len, &cp);
  assert(n == 0);
}

int main(void) {
  expect_ok((const unsigned char *)"A", 1, 1, (lt_uchar)'A');

  {
    const unsigned char s[] = {0xC2, 0xA2}; /* ¢ U+00A2 */
    expect_ok(s, sizeof(s), 2, (lt_uchar)0x00A2);
  }
  {
    const unsigned char s[] = {0xE2, 0x82, 0xAC}; /* € U+20AC */
    expect_ok(s, sizeof(s), 3, (lt_uchar)0x20AC);
  }
  {
    const unsigned char s[] = {0xF0, 0x9F, 0x98, 0x80}; /* 😀 U+1F600 */
    expect_ok(s, sizeof(s), 4, (lt_uchar)0x1F600);
  }

  /* invalid: bad leading/continuation/truncated */
  {
    const unsigned char s[] = {0x80};
    expect_bad(s, sizeof(s));
  }
  {
    const unsigned char s[] = {0xC2, 0x41};
    expect_bad(s, sizeof(s));
  }
  {
    const unsigned char s[] = {0xE2, 0x82};
    expect_bad(s, sizeof(s));
  }
  {
    const unsigned char s[] = {0xF0, 0x9F, 0x98};
    expect_bad(s, sizeof(s));
  }

  /* invalid: overlong encodings */
  {
    const unsigned char s[] = {0xC0, 0xAF};
    expect_bad(s, sizeof(s));
  }
  {
    const unsigned char s[] = {0xE0, 0x80, 0x80};
    expect_bad(s, sizeof(s));
  }
  {
    const unsigned char s[] = {0xF0, 0x80, 0x80, 0x80};
    expect_bad(s, sizeof(s));
  }

  /* invalid: surrogate range U+D800..U+DFFF */
  {
    const unsigned char s[] = {0xED, 0xA0, 0x80}; /* U+D800 */
    expect_bad(s, sizeof(s));
  }

  /* invalid: > U+10FFFF */
  {
    const unsigned char s[] = {0xF4, 0x90, 0x80, 0x80};
    expect_bad(s, sizeof(s));
  }

  return 0;
}

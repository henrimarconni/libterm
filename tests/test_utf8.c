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
  assert(lt_utf8_char_length('A') == 1);
  assert(lt_utf8_char_length((char)0xC2) == 2);
  assert(lt_utf8_char_length((char)0xE2) == 3);
  assert(lt_utf8_char_length((char)0xF0) == 4);
  assert(lt_utf8_char_length((char)0x80) == 0);

  {
    uint32_t cp = 0;
    const char s[] = {(char)0xE2, (char)0x82, (char)0xAC, '\0'};
    assert(lt_utf8_char_to_unicode(&cp, s) == 3);
    assert(cp == 0x20AC);
  }

  {
    char out[5] = {0};
    assert(lt_utf8_unicode_to_char(out, 0x1F600) == 4);
    assert((unsigned char)out[0] == 0xF0);
    assert((unsigned char)out[1] == 0x9F);
    assert((unsigned char)out[2] == 0x98);
    assert((unsigned char)out[3] == 0x80);
    assert(out[4] == '\0');
  }

  {
    uint32_t cp = 0xDEADBEEF;
    assert(lt_utf8_char_to_unicode(&cp, "") == 0);
    assert(cp == 0xDEADBEEF);
  }

  {
    uint32_t cp = 0xDEADBEEF;
    const char s[] = {(char)0xE2, (char)0x82, '\0'};
    assert(lt_utf8_char_to_unicode(&cp, s) == -2);
    assert(cp == 0xDEADBEEF);
  }

  {
    uint32_t cp = 0xDEADBEEF;
    const char s[] = {(char)0xC2, 'A', '\0'};
    assert(lt_utf8_char_to_unicode(&cp, s) == 0);
    assert(cp == 0xDEADBEEF);
  }

  {
    char out[5] = {'x', 'x', 'x', 'x', 'x'};
    assert(lt_utf8_unicode_to_char(out, 0xD800) == 0);
    assert(out[0] == '\0');
  }

  {
    char out[5] = {'x', 'x', 'x', 'x', 'x'};
    assert(lt_utf8_unicode_to_char(out, 0x110000) == 0);
    assert(out[0] == '\0');
  }

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

  /* invalid: 0xC1 overlong 2-byte lead (only 0xC2..0xDF are legal) */
  {
    const unsigned char s[] = {0xC1, 0xBF};
    expect_bad(s, sizeof(s));
  }

  /* boundary: U+D7FF — last codepoint just below the surrogate block, valid */
  {
    const unsigned char s[] = {0xED, 0x9F, 0xBF};
    expect_ok(s, sizeof(s), 3, (lt_uchar)0xD7FF);
  }

  /* boundary: U+E000 — first codepoint just above the surrogate block, valid */
  {
    const unsigned char s[] = {0xEE, 0x80, 0x80};
    expect_ok(s, sizeof(s), 3, (lt_uchar)0xE000);
  }

  /* boundary: U+10FFFF — the maximum valid scalar value */
  {
    const unsigned char s[] = {0xF4, 0x8F, 0xBF, 0xBF};
    expect_ok(s, sizeof(s), 4, (lt_uchar)0x10FFFF);
  }

  return 0;
}

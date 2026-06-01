/* lt_wcwidth / lt_iswprint: column width of representative codepoints. Pure
 * logic, no terminal needed. */
#include "libterm/libterm.h"
#include <assert.h>

int main(void) {
  /* NUL is zero-width; control chars and DEL are non-printable. */
  assert(lt_wcwidth(0x00) == 0);
  assert(lt_wcwidth(0x07) == -1); /* BEL */
  assert(lt_wcwidth(0x1B) == -1); /* ESC */
  assert(lt_wcwidth(0x7F) == -1); /* DEL */
  assert(lt_wcwidth(0x9B) == -1); /* C1 control */

  /* Normal 1-column characters. */
  assert(lt_wcwidth('A') == 1);
  assert(lt_wcwidth(' ') == 1);
  assert(lt_wcwidth(0x00E9) == 1); /* é */
  assert(lt_wcwidth(0x20AC) == 1); /* € */

  /* Combining / zero-width marks. */
  assert(lt_wcwidth(0x0301) == 0); /* combining acute accent */
  assert(lt_wcwidth(0x0300) == 0); /* combining grave accent */
  assert(lt_wcwidth(0xFE0F) == 0); /* variation selector-16 */
  assert(lt_wcwidth(0x20D0) == 0); /* combining left harpoon */

  /* Wide East-Asian and emoji = 2 columns. */
  assert(lt_wcwidth(0x4F60) == 2);  /* 你 */
  assert(lt_wcwidth(0x597D) == 2);  /* 好 */
  assert(lt_wcwidth(0xAC00) == 2);  /* 가 Hangul syllable */
  assert(lt_wcwidth(0x3042) == 2);  /* あ Hiragana */
  assert(lt_wcwidth(0xFF21) == 2);  /* fullwidth A */
  assert(lt_wcwidth(0x1F600) == 2); /* 😀 emoji */
  assert(lt_wcwidth(0x1F9FF) == 2); /* supplemental symbol */
  assert(lt_wcwidth(0x20000) == 2); /* CJK Ext B */

  /* Boundary: just below the first wide block is still narrow. */
  assert(lt_wcwidth(0x10FF) == 1);
  assert(lt_wcwidth(0x1100) == 2); /* first wide (Hangul Jamo) */

  /* lt_iswprint mirrors the sign of lt_wcwidth. */
  assert(lt_iswprint('A') == 1);
  assert(lt_iswprint(0x4F60) == 1); /* wide but printable */
  assert(lt_iswprint(0x0301) == 1); /* zero-width but printable */
  assert(lt_iswprint(0x1B) == 0);   /* ESC not printable */
  assert(lt_iswprint(0x07) == 0);

  /* lt_attr_width: lt_attr is uint32_t, so always 4. */
  assert(lt_attr_width() == 4);
  assert(lt_attr_width() == (int)sizeof(lt_attr));

  return 0;
}

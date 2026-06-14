/* LT_HEX color-packing macro: proves hex packing is identical to LT_RGB and
 * that the high bits (the 7 attribute flags + the LT_HI_BLACK sentinel) are
 * masked off, so LT_HEX can never leak an attribute. Pure compile-time macro
 * checks via _Static_assert — no terminal init, runs everywhere, immune to
 * NDEBUG. */
#include "libterm/libterm.h"

/* Packing parity: a 0xRRGGBB hex literal equals the three-channel LT_RGB form.
 */
_Static_assert(LT_HEX(0xFF8000) == LT_RGB(255, 128, 0),
               "LT_HEX packing must match LT_RGB");

/* High bits masked: a value with every bit set yields pure white, with no
 * attribute or LT_HI_BLACK bits leaking through. */
_Static_assert(LT_HEX(0xFFFFFFFF) == LT_RGB(255, 255, 255),
               "LT_HEX must mask bits above the 24-bit color field");
_Static_assert((LT_HEX(0xFFFFFFFF) & 0xFF000000u) == 0u,
               "LT_HEX must not set any attribute/sentinel bit");

/* Default-vs-black semantics match LT_RGB: a zero color is the terminal
 * default; OR in LT_HI_BLACK for real black. */
_Static_assert(LT_HEX(0x000000) == LT_DEFAULT,
               "LT_HEX(0) must equal the terminal default");
_Static_assert((LT_HEX(0) | LT_HI_BLACK) == LT_HI_BLACK,
               "LT_HEX(0) | LT_HI_BLACK must select real black");

int main(void) { return 0; }

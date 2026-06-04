/* Unit tests for the OSC color-reply payload parser (shared/colorq.c).
 * White-box: links libterm_static and calls lt__ internals (pattern of
 * test_keymap). Payloads are the bytes between "ESC ]" and the BEL/ST
 * terminator. `what`: -1 = default fg, -2 = default bg, 0..255 = palette
 * index (matching LT_COLOR_DEFAULT_FG/BG added in the public header). */
#include "internal.h"
#include "libterm/libterm.h"
#include <assert.h>
#include <string.h>

static uint32_t parse_ok(const char *payload, int what) {
  uint32_t rgb = 0xDEADBEEF;
  assert(lt__color_parse_osc_reply(payload, strlen(payload), what, &rgb) ==
         LT_OK);
  return rgb;
}

static void parse_err(const char *payload, int what) {
  uint32_t rgb = 0;
  assert(lt__color_parse_osc_reply(payload, strlen(payload), what, &rgb) !=
         LT_OK);
}

int main(void) {
  /* 16-bit channels: the common xterm reply form (doubled bytes are exact). */
  assert(parse_ok("11;rgb:1c1c/1c1c/1c1c", -2) == 0x1C1C1C);
  assert(parse_ok("10;rgb:ffff/ffff/ffff", -1) == 0xFFFFFF);
  assert(parse_ok("11;rgb:0000/0000/0000", -2) == 0x000000);
  /* Rounded scaling: 0x8000/0xFFFF -> round(127.503) = 128. */
  assert(parse_ok("11;rgb:8000/8000/8000", -2) == 0x808080);
  /* 8-bit channels. */
  assert(parse_ok("11;rgb:ff/80/00", -2) == 0xFF8000);
  /* 12-bit channels: 0x888/0xFFF == 0x88/0xFF exactly. */
  assert(parse_ok("11;rgb:fff/888/000", -2) == 0xFF8800);
  /* 4-bit channels: 0x8/0xF -> round(136.0) = 0x88. */
  assert(parse_ok("11;rgb:f/8/0", -2) == 0xFF8800);
  /* Uppercase hex. */
  assert(parse_ok("11;rgb:1C1C/1C1C/1C1C", -2) == 0x1C1C1C);
  /* Palette index replies echo "4;<idx>;". 0x00ff/0xffff -> round(0.996)=1;
   * 0x8080/0xffff == 0x80 exactly. */
  assert(parse_ok("4;42;rgb:00ff/8080/ffff", 42) == 0x0180FF);
  assert(parse_ok("4;0;rgb:0000/0000/0000", 0) == 0x000000);
  assert(parse_ok("4;255;rgb:ffff/ffff/ffff", 255) == 0xFFFFFF);

  /* URxvt rgba: extension — R/G/B/A, alpha last, ignored. */
  assert(parse_ok("11;rgba:1c1c/1c1c/1c1c/ffff", -2) == 0x1C1C1C);
  assert(parse_ok("11;rgba:ffff/0000/0000/0000", -2) == 0xFF0000);
  assert(parse_ok("4;42;rgba:00ff/8080/ffff/dddd", 42) == 0x0180FF);

  /* Selector mismatches. */
  parse_err("10;rgb:0000/0000/0000", -2);   /* fg reply, bg expected */
  parse_err("11;rgb:0000/0000/0000", -1);   /* bg reply, fg expected */
  parse_err("4;41;rgb:0000/0000/0000", 42); /* wrong palette index */
  parse_err("4;42;rgb:0000/0000/0000", -2); /* palette reply, bg expected */

  /* Malformed payloads. */
  parse_err("11;rgb:zzzz/0000/0000", -2);      /* non-hex */
  parse_err("11;rgb:0000/0000", -2);           /* two channels */
  parse_err("11;rgb:0000/0000/0000/0000", -2); /* four channels */
  parse_err("11;rgba:0000/0000/0000", -2);     /* rgba: needs four channels */
  parse_err("11;rgba:0000/0000/0000/0000/0000", -2); /* five channels */
  parse_err("11;rgb:0000/0000/0000/ffff", -2);       /* alpha on rgb: */
  parse_err("11;rgb:00000/0000/0000", -2);           /* 5-digit channel */
  parse_err("11;rgb:/0000/0000", -2);                /* empty channel */
  parse_err("11;", -2);
  parse_err("11", -2);
  parse_err("", -2);
  parse_err("4;;rgb:0000/0000/0000", 42);    /* empty index */
  parse_err("4;999;rgb:0000/0000/0000", 42); /* index out of range */
  return 0;
}

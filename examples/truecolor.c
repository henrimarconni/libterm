/*
 * Demonstrates 24-bit truecolor SGR output: an RGB gradient, LT_RGB swatches,
 * the LT_HI_BLACK sentinel (real black vs terminal default), and truecolor
 * combined with an attribute. Best viewed in a truecolor-capable terminal.
 * Press q or Esc to quit.
 *
 * Note: the output mode is interpreted globally at present time, so this demo
 * stays in LT_OUTPUT_TRUECOLOR for the whole frame.
 */

#include "libterm/libterm.h"

#include <stdio.h>

static void puts_at(int x, int y, const char *s, lt_attr fg, lt_attr bg) {
  for (int i = 0; s[i]; i++)
    lt_set_cell(x + i, y, (lt_uchar)s[i], fg, bg);
}

int main(void) {
  int rc = lt_init();
  if (rc != LT_OK) {
    fprintf(stderr, "lt_init failed: %d\n", rc);
    return 1;
  }

  lt_set_output_mode(LT_OUTPUT_TRUECOLOR);
  lt_hide_cursor();

  puts_at(2, 1, "libterm truecolor demo (press q or Esc to quit)", LT_DEFAULT,
          LT_DEFAULT);

  /* 24-bit RGB gradient: blue -> red across the row, drawn as cell
   * backgrounds (space glyphs). Each column is a distinct true color. */
  puts_at(2, 3, "24-bit gradient:", LT_DEFAULT, LT_DEFAULT);
  for (int i = 0; i < 36; i++) {
    int r = i * 7;
    int b = 255 - i * 7;
    if (r > 255)
      r = 255;
    if (b < 0)
      b = 0;
    lt_set_cell(2 + i, 4, ' ', LT_DEFAULT, LT_RGB(r, 0, b));
  }

  /* Named LT_RGB swatches: a label drawn on its own background color. The
   * orange swatch uses LT_RGB(0,0,0)|LT_HI_BLACK for genuinely-black text. */
  puts_at(2, 6, "LT_RGB swatches:", LT_DEFAULT, LT_DEFAULT);
  puts_at(2, 7, " orange ", LT_RGB(0, 0, 0) | LT_HI_BLACK, LT_RGB(255, 128, 0));
  puts_at(11, 7, " teal ", LT_RGB(255, 255, 255), LT_RGB(0, 128, 128));
  puts_at(18, 7, " purple ", LT_RGB(255, 255, 255), LT_RGB(128, 0, 128));

  /* LT_HI_BLACK sentinel: terminal default vs real black. The left cell uses
   * the terminal's default background; the right forces RGB(0,0,0). */
  puts_at(2, 9, "HI_BLACK vs default:", LT_DEFAULT, LT_DEFAULT);
  puts_at(2, 10, " default-bg ", LT_DEFAULT, LT_DEFAULT);
  puts_at(15, 10, " real-black ", LT_RGB(255, 255, 255),
          LT_RGB(0, 0, 0) | LT_HI_BLACK);

  /* truecolor color composed with an attribute (bold) — proves attrs still
   * work after their bits moved above the 24-bit color field. */
  puts_at(2, 12, "truecolor + bold", LT_RGB(255, 96, 0) | LT_BOLD, LT_DEFAULT);

  lt_present();

  /* Poll until Esc or q. Tolerate any non-OK return code by retrying;
   * the only way out is a matching key event. */
  struct lt_event ev;
  for (;;) {
    int prc = lt_peek_event(&ev, 100);
    if (prc == LT_OK && ev.type == LT_EVENT_KEY &&
        (ev.key == LT_KEY_ESC || ev.ch == 'q'))
      break;
  }

  lt_show_cursor();
  lt_shutdown();
  return 0;
}

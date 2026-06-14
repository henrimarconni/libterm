/*
 * Demonstrates 24-bit truecolor SGR output: an RGB gradient, LT_RGB swatches,
 * the LT_HI_BLACK sentinel (real black vs terminal default), and truecolor
 * combined with an attribute. Best viewed in a truecolor-capable terminal.
 * Press q or Esc to quit.
 *
 * Note: the output mode is chosen at runtime via lt_detect_color_depth() and
 * reported in the title bar; the RGB content only renders correctly under
 * truecolor, so a lower-depth terminal shows the note but degraded color.
 */

#include "libterm/libterm.h"

#include <stdio.h>

int main(void) {
  int rc = lt_init();
  if (rc != LT_OK) {
    fprintf(stderr, "lt_init failed: %d\n", rc);
    return 1;
  }

  int depth = lt_detect_color_depth();
  lt_set_output_mode(depth);
  const char *depth_note =
      depth == LT_OUTPUT_TRUECOLOR
          ? "truecolor detected"
          : (depth == LT_OUTPUT_256 ? "256-color detected" : "normal detected");

  char title[128];
  snprintf(title, sizeof title,
           "libterm truecolor demo [%s] (press q or Esc to quit)", depth_note);
  lt_print(2, 1, LT_DEFAULT, LT_DEFAULT, title);

  /* 24-bit RGB gradient: blue -> red across the row, drawn as cell
   * backgrounds (space glyphs). Each column is a distinct true color. */
  lt_print(2, 3, LT_DEFAULT, LT_DEFAULT, "24-bit gradient:");
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
  lt_print(2, 6, LT_DEFAULT, LT_DEFAULT, "LT_RGB swatches:");
  lt_print(2, 7, LT_RGB(0, 0, 0) | LT_HI_BLACK, LT_RGB(255, 128, 0),
           " orange ");
  lt_print(11, 7, LT_RGB(255, 255, 255), LT_RGB(0, 128, 128), " teal ");
  lt_print(18, 7, LT_RGB(255, 255, 255), LT_RGB(128, 0, 128), " purple ");

  /* LT_HI_BLACK sentinel: terminal default vs real black. The left cell uses
   * the terminal's default background; the right forces RGB(0,0,0). */
  lt_print(2, 9, LT_DEFAULT, LT_DEFAULT, "HI_BLACK vs default:");
  lt_print(2, 10, LT_DEFAULT, LT_DEFAULT, " default-bg ");
  lt_print(15, 10, LT_RGB(255, 255, 255), LT_RGB(0, 0, 0) | LT_HI_BLACK,
           " real-black ");

  /* truecolor color composed with an attribute (bold) — proves attrs still
   * work after their bits moved above the 24-bit color field. */
  lt_print(2, 12, LT_RGB(255, 96, 0) | LT_BOLD, LT_DEFAULT, "truecolor + bold");

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

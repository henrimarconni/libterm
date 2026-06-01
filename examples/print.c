/* Demonstrates lt_print / lt_printf: draw some colored, formatted text and a
 * multi-line block, then wait for a keypress. Run: ./build/examples/print */
#include "libterm/libterm.h"

int main(void) {
  if (lt_init() != LT_OK)
    return 1;

  lt_clear();

  lt_print(2, 1, LT_RED, LT_DEFAULT, "lt_print: red on default");
  lt_print(2, 2, LT_GREEN, LT_DEFAULT, "UTF-8: \xE2\x82\xAC \xC3\xA9 \xF0\x9F\x98\x80");
  lt_printf(2, 3, LT_YELLOW, LT_DEFAULT, "lt_printf: %d + %d = %d", 2, 40, 42);
  lt_print(2, 5, LT_CYAN, LT_DEFAULT, "multi-line:\n  second line\n  third line");
  lt_print(2, 9, LT_DEFAULT, LT_DEFAULT, "press any key to quit");

  lt_present();

  struct lt_event ev;
  while (lt_poll_event(&ev) == LT_OK) {
    if (ev.type == LT_EVENT_KEY)
      break;
  }

  lt_shutdown();
  return 0;
}

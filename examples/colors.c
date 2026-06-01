/*
 * Demonstrates layer 8 SGR emission: 8 base colors as fg, attrs (bold,
 * underline, reverse), and combinations. Press q or Esc to quit.
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

  lt_hide_cursor();

  puts_at(2, 1, "libterm color demo (press q or Esc to quit)", LT_DEFAULT,
          LT_DEFAULT);

  const char *names[8] = {"BLACK", "RED",     "GREEN", "YELLOW",
                          "BLUE",  "MAGENTA", "CYAN",  "WHITE"};
  const lt_attr colors[8] = {LT_BLACK, LT_RED,     LT_GREEN, LT_YELLOW,
                             LT_BLUE,  LT_MAGENTA, LT_CYAN,  LT_WHITE};

  for (int i = 0; i < 8; i++) {
    puts_at(2, 3 + i, names[i], colors[i], LT_DEFAULT);
    puts_at(12, 3 + i, names[i], LT_DEFAULT, colors[i]);
  }

  puts_at(2, 12, "bold", LT_RED | LT_BOLD, LT_DEFAULT);
  puts_at(2, 13, "underline", LT_GREEN | LT_UNDERLINE, LT_DEFAULT);
  puts_at(2, 14, "reverse", LT_BLUE | LT_REVERSE, LT_DEFAULT);
  puts_at(2, 15, "italic", LT_CYAN | LT_ITALIC, LT_DEFAULT);
  puts_at(2, 16, "bold+underline", LT_YELLOW | LT_BOLD | LT_UNDERLINE,
          LT_DEFAULT);

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

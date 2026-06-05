#include "libterm/libterm.h"

#include <stdio.h>

int main(void) {
  int rc = lt_init();
  if (rc != LT_OK) {
    fprintf(stderr, "lt_init failed: %d\n", rc);
    return 1;
  }

  const char *msg = "resize me; press q or Esc to quit";
  for (int i = 0; msg[i]; i++)
    lt_set_cell(2 + i, 1, (lt_uchar)msg[i], 0, 0);
  lt_present();

  struct lt_event ev;
  for (;;) {
    rc = lt_peek_event(&ev, 1000);
    if (rc == LT_ERR_NO_EVENT)
      continue;

    if (rc != LT_OK) {
      fprintf(stderr, "peek rc=%d (error)\n", rc);
      break;
    }

    fprintf(stderr, "event: type=%d key=%u ch=%u w=%d h=%d\n", ev.type, ev.key,
            (unsigned)ev.ch, ev.w, ev.h);

    if (ev.type == LT_EVENT_KEY) {
      if (ev.key == LT_KEY_ESC || ev.ch == 'q')
        break;
    }
  }

  lt_show_cursor();
  lt_shutdown();
  return 0;
}

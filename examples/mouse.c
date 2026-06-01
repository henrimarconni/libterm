#include "libterm/libterm.h"
#include <stdbool.h>
#include <stdio.h>

int main(void) {
  if (lt_init() != LT_OK)
    return 1;

  lt_set_input_mode(LT_INPUT_MOUSE);

  while (true) {
    struct lt_event ev;
    if (lt_poll_event(&ev) != LT_OK)
      continue;

    if (ev.type == LT_EVENT_MOUSE) {
      fprintf(stderr, "MOUSE  key=0x%04x x=%d y=%d mod=0x%02x\n",
              (unsigned)ev.key, ev.x, ev.y, (unsigned)ev.mod);
    } else if (ev.type == LT_EVENT_KEY) {
      fprintf(stderr, "KEY    key=0x%04x ch=0x%08x mod=0x%02x\n",
              (unsigned)ev.key, (unsigned)ev.ch, (unsigned)ev.mod);
      if (ev.ch == 'q')
        break;
    }
  }

  lt_shutdown();
  return 0;
}

#define _DEFAULT_SOURCE
#include "libterm/libterm.h"

#include <assert.h>
#include <stdlib.h>

int main(void) {
  unsetenv("TERM");
  setenv("COLORTERM", "truecolor", 1);
  assert(lt_detect_color_depth() == LT_OUTPUT_TRUECOLOR);

  setenv("COLORTERM", "24bit", 1);
  assert(lt_detect_color_depth() == LT_OUTPUT_TRUECOLOR);

  setenv("COLORTERM", "gnome-terminal", 1);
  setenv("TERM", "xterm-256color", 1);
  assert(lt_detect_color_depth() == LT_OUTPUT_256);

  unsetenv("COLORTERM");
  setenv("TERM", "screen-256color", 1);
  assert(lt_detect_color_depth() == LT_OUTPUT_256);

  setenv("TERM", "xterm", 1);
  assert(lt_detect_color_depth() == LT_OUTPUT_NORMAL);

  setenv("TERM", "dumb", 1);
  assert(lt_detect_color_depth() == LT_OUTPUT_NORMAL);

  unsetenv("COLORTERM");
  unsetenv("TERM");
  assert(lt_detect_color_depth() == LT_OUTPUT_NORMAL);

  return 0;
}

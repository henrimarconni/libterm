#include "libterm/libterm.h"

/* Build-and-link smoke only — never executed (needs a tty). */
int main(void) {
  if (lt_init() != LT_OK)
    return 1;
  lt_shutdown();
  return 0;
}

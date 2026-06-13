/* Compile+link proof that compat/termbox2.h resolves every aliased symbol to a
 * real libterm symbol. Does not initialize a terminal — it references the API
 * surface (types, function addresses, constants) so the linker must find them. */
#include "termbox2.h" /* resolves to compat/termbox2.h via the include path */

#include <stdint.h>

int main(void) {
  struct tb_cell cell;
  struct tb_event ev;
  uintattr_t attr = TB_RED | TB_BOLD;

  /* Take function addresses so the aliases must resolve to real linkable
   * symbols, without running anything. */
  int (*pinit)(void) = tb_init;
  int (*pshutdown)(void) = tb_shutdown;
  int (*ppresent)(void) = tb_present;
  int (*pclear)(void) = tb_clear;
  int (*pset)(int, int, uint32_t, uintattr_t, uintattr_t) = tb_set_cell;
  int (*ppoll)(struct tb_event *) = tb_poll_event;
  int (*pcursor)(int, int) = tb_set_cursor;
  int (*pget)(int, int, int, struct tb_cell **) = tb_get_cell;
  const char *(*pstrerror)(int) = tb_strerror;

  int keys = TB_KEY_ENTER + TB_KEY_ESC + TB_KEY_ARROW_UP + TB_KEY_F1 +
             TB_KEY_CTRL_C + TB_KEY_BACKSPACE + TB_KEY_TAB + TB_KEY_HOME +
             TB_KEY_BACK_TAB + TB_KEY_MOUSE_LEFT;
  int misc = TB_OK + TB_ERR + TB_ERR_NOT_INIT + TB_EVENT_KEY + TB_EVENT_RESIZE +
             TB_EVENT_MOUSE + TB_INPUT_ESC + TB_OUTPUT_NORMAL +
             (int)(TB_MOD_ALT | TB_MOD_CTRL | TB_MOD_SHIFT | TB_MOD_MOTION);

  (void)cell; (void)ev; (void)attr;
  (void)pinit; (void)pshutdown; (void)ppresent; (void)pclear; (void)pset;
  (void)ppoll; (void)pcursor; (void)pget; (void)pstrerror;
  (void)keys; (void)misc;

  if (0) { tb_put_cell(0, 0, &cell); } /* prove the macro expands + links */
  return 0;
}

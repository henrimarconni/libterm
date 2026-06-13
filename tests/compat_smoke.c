/* Exhaustive compile+link proof that compat/termbox2.h resolves EVERY aliased
 * symbol to a real libterm symbol. References every value constant and every
 * function alias, so a typo in any alias (e.g. TB_KEY_X -> a nonexistent
 * LT_KEY_X) fails the build here rather than silently when a consumer first
 * uses it. Does not initialize a terminal. The bucket-C unsupported macros
 * (tb_init_rwfd / tb_set_func / tb_has_truecolor / tb_cell_buffer / tb_key_i)
 * are deliberately NOT referenced — using one is a compile error by design. */
#include "termbox2.h" /* resolves to compat/termbox2.h via the include path */

#include <stdint.h>

int main(void) {
  struct tb_cell cell;
  struct tb_event ev;
  uintattr_t attr = TB_RGB(1, 2, 3);
  long long sink = 0;

  /* A few typed function pointers prove real linkage (not just declaration). */
  int (*pset)(int, int, uint32_t, uintattr_t, uintattr_t) = tb_set_cell;
  int (*ppoll)(struct tb_event *) = tb_poll_event;
  int (*pget)(int, int, int, struct tb_cell **) = tb_get_cell;
  (void)cell;
  (void)ev;
  (void)attr;
  (void)pset;
  (void)ppoll;
  (void)pget;

  /* Every value constant resolves (sum forces each token to be defined). */
  sink += (long long)(TB_OK);
  sink += (long long)(TB_ERR);
  sink += (long long)(TB_ERR_NEED_MORE);
  sink += (long long)(TB_ERR_INIT_ALREADY);
  sink += (long long)(TB_ERR_INIT_OPEN);
  sink += (long long)(TB_ERR_MEM);
  sink += (long long)(TB_ERR_NO_EVENT);
  sink += (long long)(TB_ERR_NO_TERM);
  sink += (long long)(TB_ERR_NOT_INIT);
  sink += (long long)(TB_ERR_OUT_OF_BOUNDS);
  sink += (long long)(TB_ERR_READ);
  sink += (long long)(TB_ERR_RESIZE_IOCTL);
  sink += (long long)(TB_ERR_RESIZE_PIPE);
  sink += (long long)(TB_ERR_RESIZE_SIGACTION);
  sink += (long long)(TB_ERR_POLL);
  sink += (long long)(TB_ERR_TCGETATTR);
  sink += (long long)(TB_ERR_TCSETATTR);
  sink += (long long)(TB_ERR_UNSUPPORTED_TERM);
  sink += (long long)(TB_ERR_RESIZE_WRITE);
  sink += (long long)(TB_ERR_RESIZE_POLL);
  sink += (long long)(TB_ERR_RESIZE_READ);
  sink += (long long)(TB_ERR_RESIZE_SSCANF);
  sink += (long long)(TB_ERR_CAP_COLLISION);
  sink += (long long)(TB_EVENT_KEY);
  sink += (long long)(TB_EVENT_RESIZE);
  sink += (long long)(TB_EVENT_MOUSE);
  sink += (long long)(TB_MOD_ALT);
  sink += (long long)(TB_MOD_CTRL);
  sink += (long long)(TB_MOD_SHIFT);
  sink += (long long)(TB_MOD_MOTION);
  sink += (long long)(TB_INPUT_CURRENT);
  sink += (long long)(TB_INPUT_ESC);
  sink += (long long)(TB_INPUT_ALT);
  sink += (long long)(TB_INPUT_MOUSE);
  sink += (long long)(TB_OUTPUT_CURRENT);
  sink += (long long)(TB_OUTPUT_NORMAL);
  sink += (long long)(TB_OUTPUT_256);
  sink += (long long)(TB_OUTPUT_216);
  sink += (long long)(TB_OUTPUT_GRAYSCALE);
  sink += (long long)(TB_OUTPUT_TRUECOLOR);
  sink += (long long)(TB_DEFAULT);
  sink += (long long)(TB_BLACK);
  sink += (long long)(TB_RED);
  sink += (long long)(TB_GREEN);
  sink += (long long)(TB_YELLOW);
  sink += (long long)(TB_BLUE);
  sink += (long long)(TB_MAGENTA);
  sink += (long long)(TB_CYAN);
  sink += (long long)(TB_WHITE);
  sink += (long long)(TB_BOLD);
  sink += (long long)(TB_UNDERLINE);
  sink += (long long)(TB_REVERSE);
  sink += (long long)(TB_ITALIC);
  sink += (long long)(TB_BLINK);
  sink += (long long)(TB_DIM);
  sink += (long long)(TB_STRIKE);
  sink += (long long)(TB_KEY_CTRL_TILDE);
  sink += (long long)(TB_KEY_CTRL_2);
  sink += (long long)(TB_KEY_CTRL_A);
  sink += (long long)(TB_KEY_CTRL_B);
  sink += (long long)(TB_KEY_CTRL_C);
  sink += (long long)(TB_KEY_CTRL_D);
  sink += (long long)(TB_KEY_CTRL_E);
  sink += (long long)(TB_KEY_CTRL_F);
  sink += (long long)(TB_KEY_CTRL_G);
  sink += (long long)(TB_KEY_BACKSPACE);
  sink += (long long)(TB_KEY_CTRL_H);
  sink += (long long)(TB_KEY_TAB);
  sink += (long long)(TB_KEY_CTRL_I);
  sink += (long long)(TB_KEY_CTRL_J);
  sink += (long long)(TB_KEY_CTRL_K);
  sink += (long long)(TB_KEY_CTRL_L);
  sink += (long long)(TB_KEY_ENTER);
  sink += (long long)(TB_KEY_CTRL_M);
  sink += (long long)(TB_KEY_CTRL_N);
  sink += (long long)(TB_KEY_CTRL_O);
  sink += (long long)(TB_KEY_CTRL_P);
  sink += (long long)(TB_KEY_CTRL_Q);
  sink += (long long)(TB_KEY_CTRL_R);
  sink += (long long)(TB_KEY_CTRL_S);
  sink += (long long)(TB_KEY_CTRL_T);
  sink += (long long)(TB_KEY_CTRL_U);
  sink += (long long)(TB_KEY_CTRL_V);
  sink += (long long)(TB_KEY_CTRL_W);
  sink += (long long)(TB_KEY_CTRL_X);
  sink += (long long)(TB_KEY_CTRL_Y);
  sink += (long long)(TB_KEY_CTRL_Z);
  sink += (long long)(TB_KEY_ESC);
  sink += (long long)(TB_KEY_CTRL_LSQ_BRACKET);
  sink += (long long)(TB_KEY_CTRL_3);
  sink += (long long)(TB_KEY_CTRL_4);
  sink += (long long)(TB_KEY_CTRL_BACKSLASH);
  sink += (long long)(TB_KEY_CTRL_5);
  sink += (long long)(TB_KEY_CTRL_RSQ_BRACKET);
  sink += (long long)(TB_KEY_CTRL_6);
  sink += (long long)(TB_KEY_CTRL_7);
  sink += (long long)(TB_KEY_CTRL_SLASH);
  sink += (long long)(TB_KEY_CTRL_UNDERSCORE);
  sink += (long long)(TB_KEY_SPACE);
  sink += (long long)(TB_KEY_BACKSPACE2);
  sink += (long long)(TB_KEY_CTRL_8);
  sink += (long long)(TB_KEY_F1);
  sink += (long long)(TB_KEY_F2);
  sink += (long long)(TB_KEY_F3);
  sink += (long long)(TB_KEY_F4);
  sink += (long long)(TB_KEY_F5);
  sink += (long long)(TB_KEY_F6);
  sink += (long long)(TB_KEY_F7);
  sink += (long long)(TB_KEY_F8);
  sink += (long long)(TB_KEY_F9);
  sink += (long long)(TB_KEY_F10);
  sink += (long long)(TB_KEY_F11);
  sink += (long long)(TB_KEY_F12);
  sink += (long long)(TB_KEY_INSERT);
  sink += (long long)(TB_KEY_DELETE);
  sink += (long long)(TB_KEY_HOME);
  sink += (long long)(TB_KEY_END);
  sink += (long long)(TB_KEY_PGUP);
  sink += (long long)(TB_KEY_PGDN);
  sink += (long long)(TB_KEY_ARROW_UP);
  sink += (long long)(TB_KEY_ARROW_DOWN);
  sink += (long long)(TB_KEY_ARROW_LEFT);
  sink += (long long)(TB_KEY_ARROW_RIGHT);
  sink += (long long)(TB_KEY_BACK_TAB);
  sink += (long long)(TB_KEY_MOUSE_LEFT);
  sink += (long long)(TB_KEY_MOUSE_RIGHT);
  sink += (long long)(TB_KEY_MOUSE_MIDDLE);
  sink += (long long)(TB_KEY_MOUSE_RELEASE);
  sink += (long long)(TB_KEY_MOUSE_WHEEL_UP);
  sink += (long long)(TB_KEY_MOUSE_WHEEL_DOWN);

  /* Every function/allocator alias resolves (designator referenced +
   * discarded). */
  (void)(tb_init);
  (void)(tb_init_fd);
  (void)(tb_init_file);
  (void)(tb_shutdown);
  (void)(tb_width);
  (void)(tb_height);
  (void)(tb_clear);
  (void)(tb_set_clear_attrs);
  (void)(tb_present);
  (void)(tb_invalidate);
  (void)(tb_set_cursor);
  (void)(tb_hide_cursor);
  (void)(tb_set_cell);
  (void)(tb_set_cell_ex);
  (void)(tb_extend_cell);
  (void)(tb_print);
  (void)(tb_print_ex);
  (void)(tb_printf);
  (void)(tb_printf_ex);
  (void)(tb_send);
  (void)(tb_sendf);
  (void)(tb_peek_event);
  (void)(tb_poll_event);
  (void)(tb_get_fds);
  (void)(tb_set_input_mode);
  (void)(tb_set_output_mode);
  (void)(tb_last_errno);
  (void)(tb_strerror);
  (void)(tb_has_egc);
  (void)(tb_attr_width);
  (void)(tb_version);
  (void)(tb_iswprint);
  (void)(tb_wcwidth);
  (void)(tb_utf8_char_length);
  (void)(tb_utf8_char_to_unicode);
  (void)(tb_utf8_unicode_to_char);
  (void)(tb_malloc);
  (void)(tb_realloc);
  (void)(tb_free);

  (void)tb_get_cell; /* static-inline adapter designator */
  if (0) {
    tb_put_cell(0, 0, &cell);
  } /* function-like macro expands + links */

  (void)sink;
  return 0;
}

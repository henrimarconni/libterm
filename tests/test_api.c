#include "libterm/libterm.h"
#include <assert.h>
#include <string.h>

int main(void) {
  struct lt_event ev;

  /* stable metadata */
  assert(strcmp(lt_version(), "0.1.0") == 0);
  assert(lt_strerror(LT_OK) != 0);
  assert(lt_strerror(LT_ERR_NOT_INIT) != 0);

  /* not initialized yet */
  assert(lt_present() == LT_ERR_NOT_INIT);
  assert(lt_clear() == LT_ERR_NOT_INIT);
  assert(lt_set_cell(0, 0, 'x', LT_DEFAULT, LT_DEFAULT) == LT_ERR_NOT_INIT);
  assert(lt_set_cursor(0, 0) == LT_ERR_NOT_INIT);
  assert(lt_hide_cursor() == LT_ERR_NOT_INIT);
  assert(lt_show_cursor() == LT_ERR_NOT_INIT);
  assert(lt_poll_event(&ev) == LT_ERR_NOT_INIT);
  assert(lt_peek_event(&ev, 0) == LT_ERR_NOT_INIT);
  assert(lt_shutdown() == LT_ERR_NOT_INIT);

  /* mode getters/setters are stateful API and should roundtrip */
  assert(lt_set_input_mode(LT_INPUT_CURRENT) == 0);
  assert(lt_set_input_mode(LT_INPUT_ESC) == LT_INPUT_ESC);
  assert(lt_set_input_mode(LT_INPUT_CURRENT) == LT_INPUT_ESC);

  assert(lt_set_output_mode(LT_OUTPUT_CURRENT) == 0);
  assert(lt_set_output_mode(LT_OUTPUT_256) == LT_OUTPUT_256);
  assert(lt_set_output_mode(LT_OUTPUT_CURRENT) == LT_OUTPUT_256);

  /* lifecycle happy path (may be unavailable in non-tty runners) */
  int irc = lt_init();
  if (irc == LT_ERR_INIT_OPEN)
    return 0;
  assert(irc == LT_OK);
  assert(lt_init() == LT_ERR_INIT_ALREADY);

  assert(lt_width() > 0);
  assert(lt_height() > 0);

  /* cursor + clear + present path */
  assert(lt_hide_cursor() == LT_OK);
  assert(lt_show_cursor() == LT_OK);
  assert(lt_set_clear_attrs(LT_GREEN, LT_BLACK) == LT_OK);
  assert(lt_clear() == LT_OK);
  assert(lt_present() == LT_OK);

  /* set_cell bounds */
  assert(lt_set_cell(0, 0, 'A', LT_WHITE, LT_BLACK) == LT_OK);
  assert(lt_set_cell(lt_width() - 1, lt_height() - 1, 'Z', LT_WHITE, LT_BLACK) ==
         LT_OK);
  assert(lt_set_cell(-1, 0, 'x', LT_DEFAULT, LT_DEFAULT) == LT_ERR_OUT_OF_BOUNDS);
  assert(lt_set_cell(0, -1, 'x', LT_DEFAULT, LT_DEFAULT) == LT_ERR_OUT_OF_BOUNDS);
  assert(lt_set_cell(lt_width(), 0, 'x', LT_DEFAULT, LT_DEFAULT) ==
         LT_ERR_OUT_OF_BOUNDS);
  assert(lt_set_cell(0, lt_height(), 'x', LT_DEFAULT, LT_DEFAULT) ==
         LT_ERR_OUT_OF_BOUNDS);

  assert(lt_present() == LT_OK);

  /* non-blocking peek should either get an event or timeout */
  int prc = lt_peek_event(&ev, 0);
  assert(prc == LT_OK || prc == LT_ERR_NO_EVENT);

  assert(lt_shutdown() == LT_OK);

  /* after shutdown, API should reject operational calls */
  assert(lt_present() == LT_ERR_NOT_INIT);
  assert(lt_clear() == LT_ERR_NOT_INIT);
  assert(lt_set_cell(0, 0, 'x', LT_DEFAULT, LT_DEFAULT) == LT_ERR_NOT_INIT);

  return 0;
}

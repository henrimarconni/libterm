/* lt_get_cell: round-trips what lt_set_cell / lt_print wrote into the back
 * buffer, and reports the documented errors. POSIX-only (needs a pty for
 * lt_init_fd); returns 77 (CTest "skip") when no pty is available. */
#define _DEFAULT_SOURCE
#include "libterm/libterm.h"

#include <assert.h>
#include <pty.h>
#include <string.h>
#include <unistd.h>

int main(void) {
  int master = -1, slave = -1;
  struct winsize ws;
  memset(&ws, 0, sizeof ws);
  ws.ws_row = 24;
  ws.ws_col = 80;

  if (openpty(&master, &slave, NULL, NULL, &ws) != 0)
    return 77; /* CTest skip */

  assert(lt_init_fd(slave) == LT_OK);
  assert(lt_clear() == LT_OK);

  /* What lt_set_cell writes, lt_get_cell reads back verbatim. */
  assert(lt_set_cell(3, 4, 0x20AC, LT_RED, LT_BLUE) == LT_OK); /* € */
  {
    struct lt_cell c;
    memset(&c, 0, sizeof c);
    assert(lt_get_cell(3, 4, &c) == LT_OK);
    assert(c.ch == 0x20AC);
    assert(c.fg == (lt_attr)LT_RED);
    assert(c.bg == (lt_attr)LT_BLUE);
  }

  /* A cell never written reads back as the cleared default: lt_clear fills the
   * buffer with spaces, so ch == ' '. */
  {
    struct lt_cell c;
    memset(&c, 0xAB, sizeof c); /* poison: prove get_cell overwrites it */
    assert(lt_get_cell(10, 10, &c) == LT_OK);
    assert(c.ch == ' ');
  }

  /* lt_print output is observable through lt_get_cell. */
  assert(lt_print(0, 6, LT_GREEN, LT_DEFAULT, "Hi") == LT_OK);
  {
    struct lt_cell c;
    memset(&c, 0, sizeof c);
    assert(lt_get_cell(0, 6, &c) == LT_OK);
    assert(c.ch == 'H');
    assert(c.fg == (lt_attr)LT_GREEN);
    assert(lt_get_cell(1, 6, &c) == LT_OK);
    assert(c.ch == 'i');
  }

  /* Errors: out of bounds, and NULL out. */
  {
    struct lt_cell c;
    assert(lt_get_cell(-1, 0, &c) == LT_ERR_OUT_OF_BOUNDS);
    assert(lt_get_cell(0, -1, &c) == LT_ERR_OUT_OF_BOUNDS);
    assert(lt_get_cell(80, 0, &c) == LT_ERR_OUT_OF_BOUNDS);
    assert(lt_get_cell(0, 24, &c) == LT_ERR_OUT_OF_BOUNDS);
    assert(lt_get_cell(0, 0, NULL) == LT_ERR);
  }

  assert(lt_shutdown() == LT_OK);
  close(slave);
  close(master);
  return 0;
}

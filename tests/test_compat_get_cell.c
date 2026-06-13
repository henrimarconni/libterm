/* tb_get_cell compat adapter: round-trips what tb_set_cell wrote into the back
 * buffer via the snapshot pointer, and enforces the back-buffer-only contract
 * (back==0 -> LT_ERR). POSIX-only (needs a pty for tb_init_fd); returns 77
 * (CTest "skip") when no pty is available. */
#define _DEFAULT_SOURCE
#include "termbox2.h" /* compat layer */

#include <assert.h>
#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif
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

  assert(tb_init_fd(slave) == TB_OK);
  assert(tb_clear() == TB_OK);

  /* What tb_set_cell writes, tb_get_cell (back buffer) reads back. */
  assert(tb_set_cell(3, 4, 0x20AC, TB_RED, TB_BLUE) == TB_OK); /* euro sign */
  {
    struct tb_cell *c = NULL;
    assert(tb_get_cell(3, 4, 1, &c) == TB_OK);
    assert(c != NULL);
    assert(c->ch == 0x20AC);
    assert(c->fg == (uintattr_t)TB_RED);
    assert(c->bg == (uintattr_t)TB_BLUE);
  }

  /* back==0 (front buffer) is unsupported and must report LT_ERR. */
  {
    struct tb_cell *c = NULL;
    assert(tb_get_cell(3, 4, 0, &c) == TB_ERR);
  }

  /* Out-of-bounds passes lt_get_cell's own error through. */
  {
    struct tb_cell *c = NULL;
    assert(tb_get_cell(-1, 0, 1, &c) == TB_ERR_OUT_OF_BOUNDS);
  }

  assert(tb_shutdown() == TB_OK);
  close(slave);
  close(master);
  return 0;
}

/* lt_set_clear_attrs + lt_clear effect: a cleared cell must actually carry the
 * fg/bg set via lt_set_clear_attrs (previously only the return code was
 * checked, never the resulting cell colors). Driven over a pty so lt_init_fd
 * works headlessly. POSIX-only; returns 77 (CTest "skip") when no pty.
 *
 * Cases: positive (set attrs -> clear -> cells carry them), edge (default
 * attrs after init are the terminal-default 0/0), and re-set (changing attrs
 * and re-clearing updates every cell). */
#define _DEFAULT_SOURCE
#include "internal.h"
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

  /* Edge: with no explicit clear attrs, a fresh clear fills with the
   * terminal-default colors (fg/bg == 0) and a space. */
  assert(lt_clear() == LT_OK);
  {
    struct lt_cell c;
    assert(lt_get_cell(0, 0, &c) == LT_OK);
    assert(c.ch == (lt_uchar)' ');
    assert(c.fg == 0);
    assert(c.bg == 0);
  }

  /* Positive: set clear attrs, clear, and confirm EVERY sampled cell carries
   * them — corners and an interior cell. */
  assert(lt_set_clear_attrs(LT_GREEN, LT_BLUE) == LT_OK);
  assert(lt_clear() == LT_OK);
  {
    const int w = lt_width(), h = lt_height();
    const int pts[][2] = {
        {0, 0}, {w - 1, 0}, {0, h - 1}, {w - 1, h - 1}, {w / 2, h / 2}};
    for (size_t i = 0; i < sizeof pts / sizeof pts[0]; i++) {
      struct lt_cell c;
      assert(lt_get_cell(pts[i][0], pts[i][1], &c) == LT_OK);
      assert(c.ch == (lt_uchar)' ');
      assert(c.fg == (lt_attr)LT_GREEN);
      assert(c.bg == (lt_attr)LT_BLUE);
    }
  }

  /* Re-set: changing the clear attrs and clearing again overwrites the prior
   * fill across the buffer (not just newly-touched cells). */
  assert(lt_set_clear_attrs(LT_RED, LT_DEFAULT) == LT_OK);
  assert(lt_clear() == LT_OK);
  {
    struct lt_cell c;
    assert(lt_get_cell(0, 0, &c) == LT_OK);
    assert(c.fg == (lt_attr)LT_RED);
    assert(c.bg == (lt_attr)LT_DEFAULT);
    assert(lt_get_cell(lt_width() - 1, lt_height() - 1, &c) == LT_OK);
    assert(c.fg == (lt_attr)LT_RED);
  }

  /* A cell explicitly written then cleared reverts to the clear attrs. */
  assert(lt_set_cell(3, 3, 'X', LT_WHITE, LT_BLACK) == LT_OK);
  assert(lt_clear() == LT_OK);
  {
    struct lt_cell c;
    assert(lt_get_cell(3, 3, &c) == LT_OK);
    assert(c.ch == (lt_uchar)' ');
    assert(c.fg == (lt_attr)LT_RED);
  }

  assert(lt_shutdown() == LT_OK);
  close(slave);
  close(master);
  return 0;
}

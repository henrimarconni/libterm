/* lt_print / lt_print_ex / lt_printf: drive a pty-backed terminal, write into
 * the back buffer, and assert the cells (and out_w) are what we expect. The
 * back buffer is inspected directly via the internal lt__g state.
 * POSIX-only; returns 77 (CTest "skip") when no pty is available. */
#define _DEFAULT_SOURCE
#include "internal.h"
#include "libterm/libterm.h"

#include <assert.h>
#include <pty.h>
#include <string.h>
#include <unistd.h>

static lt_uchar cell_ch(int x, int y) {
  return lt__g.back[(size_t)(y * lt__g.width + x)].ch;
}
static lt_attr cell_fg(int x, int y) {
  return lt__g.back[(size_t)(y * lt__g.width + x)].fg;
}

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

  /* Plain ASCII lands one codepoint per cell, carrying fg/bg. */
  assert(lt_print(2, 1, LT_RED, LT_DEFAULT, "Hi") == LT_OK);
  assert(cell_ch(2, 1) == 'H');
  assert(cell_ch(3, 1) == 'i');
  assert(cell_fg(2, 1) == (lt_attr)LT_RED);

  /* Multi-byte UTF-8 decodes to one codepoint in one cell. */
  assert(lt_print(0, 2, LT_DEFAULT, LT_DEFAULT, "\xE2\x82\xAC") == LT_OK); /* € */
  assert(cell_ch(0, 2) == 0x20AC);

  /* Malformed UTF-8 becomes U+FFFD and we resync to the trailing ASCII. */
  assert(lt_print(0, 3, LT_DEFAULT, LT_DEFAULT, "\xC3" "Z") == LT_OK);
  assert(cell_ch(0, 3) == 0xFFFD);
  assert(cell_ch(1, 3) == 'Z');

  /* '\n' returns to the start column on the next row. */
  assert(lt_print(5, 4, LT_DEFAULT, LT_DEFAULT, "ab\ncd") == LT_OK);
  assert(cell_ch(5, 4) == 'a');
  assert(cell_ch(6, 4) == 'b');
  assert(cell_ch(5, 5) == 'c'); /* back to column 5, next row */
  assert(cell_ch(6, 5) == 'd');

  /* out_w reports the widest line; "ab\ncde" -> 3. */
  {
    size_t w = 0;
    assert(lt_print_ex(0, 6, LT_DEFAULT, LT_DEFAULT, &w, "ab\ncde") == LT_OK);
    assert(w == 3);
  }

  /* lt_printf formats then prints. */
  assert(lt_printf(0, 7, LT_GREEN, LT_DEFAULT, "n=%d", 42) == LT_OK);
  assert(cell_ch(0, 7) == 'n');
  assert(cell_ch(1, 7) == '=');
  assert(cell_ch(2, 7) == '4');
  assert(cell_ch(3, 7) == '2');

  /* Wide characters advance two columns: 你 at col 0, 好 lands at col 2, and
   * 'X' after it lands at col 4. out_w reflects the column span (5). */
  {
    size_t w = 0;
    /* "你好X" = U+4F60 U+597D 'X' */
    assert(lt_print_ex(0, 10, LT_DEFAULT, LT_DEFAULT, &w,
                       "\xE4\xBD\xA0\xE5\xA5\xBDX") == LT_OK);
    assert(cell_ch(0, 10) == 0x4F60); /* 你 */
    assert(cell_ch(2, 10) == 0x597D); /* 好 (col 1 left for the wide glyph) */
    assert(cell_ch(4, 10) == 'X');
    assert(w == 5);
  }

  /* A combining mark occupies zero columns and is not placed in its own cell:
   * "e" + U+0301 then "Z" -> 'e' at col 0, 'Z' at col 1. */
  {
    size_t w = 0;
    assert(lt_print_ex(0, 11, LT_DEFAULT, LT_DEFAULT, &w,
                       "e\xCC\x81Z") == LT_OK);
    assert(cell_ch(0, 11) == 'e');
    assert(cell_ch(1, 11) == 'Z');
    assert(w == 2);
  }

  /* Off-buffer cells are clipped, not errored: start in bounds, run past the
   * right edge -> still LT_OK, no crash (ASAN guards the OOB write). */
  assert(lt_print(78, 8, LT_DEFAULT, LT_DEFAULT, "ABCDE") == LT_OK);
  assert(cell_ch(78, 8) == 'A');
  assert(cell_ch(79, 8) == 'B'); /* C, D, E fall off the edge, silently */

  /* A starting position out of bounds is a hard error. */
  assert(lt_print(-1, 0, LT_DEFAULT, LT_DEFAULT, "x") == LT_ERR_OUT_OF_BOUNDS);
  assert(lt_print(0, 999, LT_DEFAULT, LT_DEFAULT, "x") == LT_ERR_OUT_OF_BOUNDS);

  /* NULL string is rejected; not-init is rejected. */
  assert(lt_print(0, 0, LT_DEFAULT, LT_DEFAULT, NULL) == LT_ERR);

  assert(lt_shutdown() == LT_OK);
  close(slave);
  close(master);
  return 0;
}

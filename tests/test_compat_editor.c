/* White-box regression test for the compat editor's buffer model. Includes the
 * editor translation unit with its main() compiled out, then drives the row
 * functions directly — no terminal needed, so it runs everywhere.
 *
 * Guards the use-after-free fixed in insert_newline: it captured a pointer into
 * g_rows before append_row()'s realloc could move the array, then dereferenced
 * the stale pointer — a garbage length led to a huge allocation ("out of
 * memory" when Enter split a line). This test splits and rejoins lines and
 * asserts the contents (incl. the row AFTER the split, which the stale-pointer
 * path corrupted), so a regression fails here (and trips ASan in CI). */
#define EDITOR_NO_MAIN
#include "editor.c"

#include <assert.h>
#include <string.h>

static void reset_buffer(void) {
  for (size_t i = 0; i < g_nrows; i++)
    free(g_rows[i].chars);
  free(g_rows);
  g_rows = NULL;
  g_nrows = 0;
  g_cx = 0;
  g_cy = 0;
  g_rowoff = 0;
}

int main(void) {
  /* Build two rows: "hello", "world". */
  append_row("hello", 5);
  append_row("world", 5);
  assert(g_nrows == 2);

  /* Split "hello" after "he" (Enter at row 0, col 2). This is the exact path
   * that used to OOM. */
  g_cy = 0;
  g_cx = 2;
  insert_newline();

  assert(g_nrows == 3);
  assert(g_rows[0].len == 2 && strcmp(g_rows[0].chars, "he") == 0);
  assert(g_rows[1].len == 3 && strcmp(g_rows[1].chars, "llo") == 0);
  /* The row after the split point must be preserved — the stale-pointer bug
   * corrupted exactly this. */
  assert(g_rows[2].len == 5 && strcmp(g_rows[2].chars, "world") == 0);
  assert(g_cy == 1 && g_cx == 0);

  /* Backspace at the start of "llo" rejoins it onto "he" -> "hello". */
  del_char();
  assert(g_nrows == 2);
  assert(g_rows[0].len == 5 && strcmp(g_rows[0].chars, "hello") == 0);
  assert(g_rows[1].len == 5 && strcmp(g_rows[1].chars, "world") == 0);
  assert(g_cy == 0 && g_cx == 2);

  /* Splitting at the very end of the last row (tail length 0) must not
   * underflow or over-read. */
  g_cy = 1;
  g_cx = (int)g_rows[1].len;
  insert_newline();
  assert(g_nrows == 3);
  assert(g_rows[1].len == 5 && strcmp(g_rows[1].chars, "world") == 0);
  assert(g_rows[2].len == 0 && g_rows[2].chars[0] == '\0');

  /* Type a character into the empty trailing row. */
  g_cy = 2;
  g_cx = 0;
  insert_char('!');
  assert(g_rows[2].len == 1 && g_rows[2].chars[0] == '!');

  reset_buffer();

  /* Reference the static functions that are only reachable from the compiled-
   * out main(), so -Werror -Wunused-function stays satisfied. */
  (void)&load_file;
  (void)&save_file;
  (void)&draw;
  (void)&scroll_into_view;
  (void)&move_cursor;
  return 0;
}

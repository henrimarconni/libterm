#define _DEFAULT_SOURCE
/* editor.c — a minimal but real text editor built ONLY against the termbox2
 * compat layer (no lt_/LT_ symbols). Opens the file named on argv[1] (or an
 * empty buffer), supports cursor movement, character insert, backspace,
 * newline, save (Ctrl-S) and quit (Ctrl-Q). DoD #8 drop-in proof. */
#include "termbox2.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct erow {
  char *chars;
  size_t len;
};

static struct erow *g_rows;
static size_t g_nrows;
static int g_cx, g_cy; /* cursor, in text coords */
static int g_rowoff;   /* first visible row */
static const char *g_filename;
static int g_dirty;

static void die(const char *msg) {
  tb_shutdown();
  fprintf(stderr, "editor: %s\n", msg);
  exit(1);
}

static void append_row(const char *s, size_t len) {
  struct erow *nr =
      (struct erow *)realloc(g_rows, (g_nrows + 1) * sizeof *g_rows);
  if (!nr)
    die("out of memory");
  g_rows = nr;
  g_rows[g_nrows].chars = (char *)malloc(len + 1);
  if (!g_rows[g_nrows].chars)
    die("out of memory");
  memcpy(g_rows[g_nrows].chars, s, len);
  g_rows[g_nrows].chars[len] = '\0';
  g_rows[g_nrows].len = len;
  g_nrows++;
}

static void load_file(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) {
    append_row("", 0); /* new file: start with one empty line */
    return;
  }
  char *line = NULL;
  size_t cap = 0;
  ssize_t n;
  while ((n = getline(&line, &cap, f)) != -1) {
    size_t len = (size_t)n;
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
      len--;
    append_row(line, len);
  }
  free(line);
  fclose(f);
  if (g_nrows == 0)
    append_row("", 0);
}

static void save_file(void) {
  if (!g_filename)
    return;
  FILE *f = fopen(g_filename, "w");
  if (!f)
    return;
  for (size_t i = 0; i < g_nrows; i++) {
    fwrite(g_rows[i].chars, 1, g_rows[i].len, f);
    fputc('\n', f);
  }
  fclose(f);
  g_dirty = 0;
}

static void row_insert_char(struct erow *row, int at, char c) {
  char *nc = (char *)realloc(row->chars, row->len + 2);
  if (!nc)
    die("out of memory");
  row->chars = nc;
  memmove(&row->chars[at + 1], &row->chars[at], row->len - (size_t)at + 1);
  row->chars[at] = c;
  row->len++;
}

static void insert_char(char c) {
  row_insert_char(&g_rows[g_cy], g_cx, c);
  g_cx++;
  g_dirty = 1;
}

static void insert_newline(void) {
  struct erow *row = &g_rows[g_cy];
  append_row("", 0); /* grow the array; positions are overwritten below */
  memmove(&g_rows[g_cy + 2], &g_rows[g_cy + 1],
          (g_nrows - (size_t)g_cy - 2) * sizeof *g_rows);
  size_t tail = row->len - (size_t)g_cx;
  g_rows[g_cy + 1].chars = (char *)malloc(tail + 1);
  if (!g_rows[g_cy + 1].chars)
    die("out of memory");
  memcpy(g_rows[g_cy + 1].chars, &row->chars[g_cx], tail);
  g_rows[g_cy + 1].chars[tail] = '\0';
  g_rows[g_cy + 1].len = tail;
  row->chars[g_cx] = '\0';
  row->len = (size_t)g_cx;
  g_cy++;
  g_cx = 0;
  g_dirty = 1;
}

static void del_char(void) {
  if (g_cx == 0 && g_cy == 0)
    return;
  struct erow *row = &g_rows[g_cy];
  if (g_cx > 0) {
    memmove(&row->chars[g_cx - 1], &row->chars[g_cx],
            row->len - (size_t)g_cx + 1);
    row->len--;
    g_cx--;
  } else {
    struct erow *prev = &g_rows[g_cy - 1];
    int newcx = (int)prev->len;
    char *nc = (char *)realloc(prev->chars, prev->len + row->len + 1);
    if (!nc)
      die("out of memory");
    prev->chars = nc;
    memcpy(&prev->chars[prev->len], row->chars, row->len + 1);
    prev->len += row->len;
    free(row->chars);
    memmove(&g_rows[g_cy], &g_rows[g_cy + 1],
            (g_nrows - (size_t)g_cy - 1) * sizeof *g_rows);
    g_nrows--;
    g_cy--;
    g_cx = newcx;
  }
  g_dirty = 1;
}

static void draw(void) {
  int h = tb_height();
  int w = tb_width();
  tb_clear();
  for (int y = 0; y < h - 1; y++) {
    size_t fr = (size_t)(y + g_rowoff);
    if (fr >= g_nrows)
      continue;
    struct erow *row = &g_rows[fr];
    int limit = (int)row->len < w ? (int)row->len : w;
    for (int x = 0; x < limit; x++)
      tb_set_cell(x, y, (uint32_t)(unsigned char)row->chars[x], TB_DEFAULT,
                  TB_DEFAULT);
  }
  char status[128];
  int sl = snprintf(status, sizeof status, " %s %s | Ctrl-S save  Ctrl-Q quit",
                    g_filename ? g_filename : "[no file]",
                    g_dirty ? "(modified)" : "");
  for (int x = 0; x < w; x++) {
    char ch = x < sl ? status[x] : ' ';
    tb_set_cell(x, h - 1, (uint32_t)(unsigned char)ch, TB_DEFAULT, TB_REVERSE);
  }
  tb_set_cursor(g_cx, g_cy - g_rowoff);
  tb_present();
}

static void scroll_into_view(void) {
  int h = tb_height();
  if (g_cy < g_rowoff)
    g_rowoff = g_cy;
  if (g_cy >= g_rowoff + h - 1)
    g_rowoff = g_cy - (h - 1) + 1;
}

static void move_cursor(uint16_t key) {
  struct erow *row = &g_rows[g_cy];
  if (key == TB_KEY_ARROW_LEFT && g_cx > 0)
    g_cx--;
  else if (key == TB_KEY_ARROW_RIGHT && g_cx < (int)row->len)
    g_cx++;
  else if (key == TB_KEY_ARROW_UP && g_cy > 0)
    g_cy--;
  else if (key == TB_KEY_ARROW_DOWN && (size_t)(g_cy + 1) < g_nrows)
    g_cy++;
  if ((size_t)g_cx > g_rows[g_cy].len)
    g_cx = (int)g_rows[g_cy].len;
}

int main(int argc, char **argv) {
  g_filename = argc > 1 ? argv[1] : NULL;
  if (g_filename)
    load_file(g_filename);
  else
    append_row("", 0);

  if (tb_init() != TB_OK)
    die("tb_init failed");

  for (;;) {
    scroll_into_view();
    draw();
    struct tb_event ev;
    if (tb_poll_event(&ev) != TB_OK)
      continue;
    if (ev.type != TB_EVENT_KEY)
      continue;
    if (ev.key == TB_KEY_CTRL_Q)
      break;
    if (ev.key == TB_KEY_CTRL_S) {
      save_file();
      continue;
    }
    if (ev.key == TB_KEY_ENTER) {
      insert_newline();
    } else if (ev.key == TB_KEY_BACKSPACE || ev.key == TB_KEY_BACKSPACE2) {
      del_char();
    } else if (ev.key == TB_KEY_ARROW_LEFT || ev.key == TB_KEY_ARROW_RIGHT ||
               ev.key == TB_KEY_ARROW_UP || ev.key == TB_KEY_ARROW_DOWN) {
      move_cursor(ev.key);
    } else if (ev.ch != 0 && ev.ch < 128) {
      insert_char((char)ev.ch);
    }
  }

  tb_shutdown();
  for (size_t i = 0; i < g_nrows; i++)
    free(g_rows[i].chars);
  free(g_rows);
  return 0;
}

/*
 * On-screen keyboard demo (shared-wall grid).
 *
 * Draws a full keyboard as joined key rows: adjacent keys in a row share a
 * vertical wall (┬/┴/│ joints), and each row's outer corners are rounded.
 * Each key's interior flashes with a bright fill and fades back over ~200ms
 * when its physical key is pressed. The fade is driven by frame counting; a
 * key *release* (ev.action == LT_KEY_RELEASE, delivered on kitty-capable
 * terminals) does not re-light the cap.
 *
 * It is also an HONEST probe of the libterm input API: a live event inspector
 * prints the raw lt_event fields (type/key/ch/mod/action, plus mouse x,y and
 * resize w,h) for whatever you press, type, click, scroll, or resize. Nothing
 * is faked — a cap lights only from real event fields.
 *
 * libterm's modern input model is the default: on a kitty-capable terminal
 * (kitty, foot, ghostty, recent xterm) the keyboard protocol is negotiated, so
 * modifiers arrive on EVERY key — Shift+letter carries LT_MOD_SHIFT, a bare
 * modifier arrives as its own LT_KEY_LEFT_SHIFT/.../RIGHT_SUPER code, and
 * ev.action distinguishes press/repeat/release. On a legacy terminal those
 * bits simply aren't on the wire and degrade away; the inspector shows the raw
 * fields either way. (Run with LT_INPUT_COMPAT for termbox2 control-byte
 * semantics instead.)
 *
 * Showcases: the special-key set (ev.key), modifiers (ev.mod), key actions
 * (ev.action), bare-modifier keys, mouse + resize events,
 * lt_detect_color_depth() color fallback, and box-drawing / arrow Unicode
 * cells decoded via the lt_utf8_* helpers.
 *
 * Press Ctrl+C to quit. Esc is just another key that flashes.
 */
#include "libterm/libterm.h"

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FLASH_FRAMES 7 /* ~7 * TICK_MS = ~210ms fade */
#define TICK_MS 30
#define CAP_H 3                /* every key is 3 rows tall */
#define TITLE_TOP 1            /* blank rows above the title (top padding) */
#define KB_TOP (TITLE_TOP + 2) /* first key row: title row + one blank line */
#define LEFT_MARGIN 1
#define NROWS 6   /* main keyboard rows 0..5 */
#define INSP_W 84 /* inspector line width (cleared each redraw) */

/* Box-drawing codepoints (all single-width). */
#define BX_TL 0x256Du /* rounded top-left  */
#define BX_TR 0x256Eu /* rounded top-right */
#define BX_BL 0x2570u /* rounded bot-left  */
#define BX_BR 0x256Fu /* rounded bot-right */
#define BX_H 0x2500u  /* horizontal */
#define BX_V 0x2502u  /* vertical   */
#define BX_TD 0x252Cu /* T down (top joint)    */
#define BX_TU 0x2534u /* T up   (bottom joint) */

/* Arrow-cluster placement sentinels stored in cap.row (negative = manual,
 * each its own single-key group -> standalone rounded box). */
#define ROW_UP (-1)
#define ROW_LEFT (-2)
#define ROW_DOWN (-3)
#define ROW_RIGHT (-4)

struct cap {
  uint32_t ch;       /* printable match, lowercased ASCII; 0 if none */
  uint16_t key;      /* special-key match (LT_KEY_*); 0 if none */
  uint8_t mod;       /* modifier this cap represents (LT_MOD_*); 0 if none */
  const char *label; /* UTF-8 label drawn centered in the key */
  int w;             /* key width in columns (>= 5) */
  int row;           /* 0..NROWS-1, or a ROW_* sentinel for arrows */
  int x, y;          /* top-left, filled by layout() */
  int flash;         /* frames of flash remaining (0 = idle) */
};

/* Caps are grouped by row in array order, left-to-right. Each contiguous run
 * of equal row values is one joined group; arrow sentinels are unique so each
 * arrow is its own (standalone) group. */
static struct cap keeb[] = {
    /* row 0 — function row */
    {0, LT_KEY_ESC, 0, "Esc", 5, 0, 0, 0, 0},
    {0, LT_KEY_F1, 0, "F1", 5, 0, 0, 0, 0},
    {0, LT_KEY_F2, 0, "F2", 5, 0, 0, 0, 0},
    {0, LT_KEY_F3, 0, "F3", 5, 0, 0, 0, 0},
    {0, LT_KEY_F4, 0, "F4", 5, 0, 0, 0, 0},
    {0, LT_KEY_F5, 0, "F5", 5, 0, 0, 0, 0},
    {0, LT_KEY_F6, 0, "F6", 5, 0, 0, 0, 0},
    {0, LT_KEY_F7, 0, "F7", 5, 0, 0, 0, 0},
    {0, LT_KEY_F8, 0, "F8", 5, 0, 0, 0, 0},
    {0, LT_KEY_F9, 0, "F9", 5, 0, 0, 0, 0},
    {0, LT_KEY_F10, 0, "F10", 5, 0, 0, 0, 0},
    {0, LT_KEY_F11, 0, "F11", 5, 0, 0, 0, 0},
    {0, LT_KEY_F12, 0, "F12", 5, 0, 0, 0, 0},

    /* row 1 — number row */
    {'`', 0, 0, "`", 5, 1, 0, 0, 0},
    {'1', 0, 0, "1", 5, 1, 0, 0, 0},
    {'2', 0, 0, "2", 5, 1, 0, 0, 0},
    {'3', 0, 0, "3", 5, 1, 0, 0, 0},
    {'4', 0, 0, "4", 5, 1, 0, 0, 0},
    {'5', 0, 0, "5", 5, 1, 0, 0, 0},
    {'6', 0, 0, "6", 5, 1, 0, 0, 0},
    {'7', 0, 0, "7", 5, 1, 0, 0, 0},
    {'8', 0, 0, "8", 5, 1, 0, 0, 0},
    {'9', 0, 0, "9", 5, 1, 0, 0, 0},
    {'0', 0, 0, "0", 5, 1, 0, 0, 0},
    {'-', 0, 0, "-", 5, 1, 0, 0, 0},
    {'=', 0, 0, "=", 5, 1, 0, 0, 0},
    {0, LT_KEY_BACKSPACE2, 0, "Bksp", 6, 1, 0, 0, 0},

    /* row 2 — qwerty row */
    {0, LT_KEY_TAB, 0, "Tab", 6, 2, 0, 0, 0},
    {'q', 0, 0, "Q", 5, 2, 0, 0, 0},
    {'w', 0, 0, "W", 5, 2, 0, 0, 0},
    {'e', 0, 0, "E", 5, 2, 0, 0, 0},
    {'r', 0, 0, "R", 5, 2, 0, 0, 0},
    {'t', 0, 0, "T", 5, 2, 0, 0, 0},
    {'y', 0, 0, "Y", 5, 2, 0, 0, 0},
    {'u', 0, 0, "U", 5, 2, 0, 0, 0},
    {'i', 0, 0, "I", 5, 2, 0, 0, 0},
    {'o', 0, 0, "O", 5, 2, 0, 0, 0},
    {'p', 0, 0, "P", 5, 2, 0, 0, 0},
    {'[', 0, 0, "[", 5, 2, 0, 0, 0},
    {']', 0, 0, "]", 5, 2, 0, 0, 0},
    {'\\', 0, 0, "\\", 5, 2, 0, 0, 0},

    /* row 3 — home row */
    {0, LT_KEY_CAPS_LOCK, 0, "Caps", 6, 3, 0, 0, 0}, /* lights on kitty terms */
    {'a', 0, 0, "A", 5, 3, 0, 0, 0},
    {'s', 0, 0, "S", 5, 3, 0, 0, 0},
    {'d', 0, 0, "D", 5, 3, 0, 0, 0},
    {'f', 0, 0, "F", 5, 3, 0, 0, 0},
    {'g', 0, 0, "G", 5, 3, 0, 0, 0},
    {'h', 0, 0, "H", 5, 3, 0, 0, 0},
    {'j', 0, 0, "J", 5, 3, 0, 0, 0},
    {'k', 0, 0, "K", 5, 3, 0, 0, 0},
    {'l', 0, 0, "L", 5, 3, 0, 0, 0},
    {';', 0, 0, ";", 5, 3, 0, 0, 0},
    {'\'', 0, 0, "'", 5, 3, 0, 0, 0},
    {0, LT_KEY_ENTER, 0, "Enter", 7, 3, 0, 0, 0},

    /* row 4 — shift row */
    {0, 0, LT_MOD_SHIFT, "Shift", 7, 4, 0, 0, 0},
    {'z', 0, 0, "Z", 5, 4, 0, 0, 0},
    {'x', 0, 0, "X", 5, 4, 0, 0, 0},
    {'c', 0, 0, "C", 5, 4, 0, 0, 0},
    {'v', 0, 0, "V", 5, 4, 0, 0, 0},
    {'b', 0, 0, "B", 5, 4, 0, 0, 0},
    {'n', 0, 0, "N", 5, 4, 0, 0, 0},
    {'m', 0, 0, "M", 5, 4, 0, 0, 0},
    {',', 0, 0, ",", 5, 4, 0, 0, 0},
    {'.', 0, 0, ".", 5, 4, 0, 0, 0},
    {'/', 0, 0, "/", 5, 4, 0, 0, 0},
    {0, 0, LT_MOD_SHIFT, "Shift", 7, 4, 0, 0, 0},

    /* row 5 — bottom row */
    {0, 0, LT_MOD_CTRL, "Ctrl", 6, 5, 0, 0, 0},
    {0, 0, LT_MOD_ALT, "Alt", 5, 5, 0, 0, 0},
    {' ', 0, 0, "Space", 31, 5, 0, 0, 0},
    {0, 0, LT_MOD_ALT, "Alt", 5, 5, 0, 0, 0},
    {0, 0, LT_MOD_CTRL, "Ctrl", 6, 5, 0, 0, 0},

    /* arrow cluster — standalone rounded boxes, right of the main block */
    {0, LT_KEY_ARROW_UP, 0, "\xE2\x86\x91", 5, ROW_UP, 0, 0, 0},
    {0, LT_KEY_ARROW_LEFT, 0, "\xE2\x86\x90", 5, ROW_LEFT, 0, 0, 0},
    {0, LT_KEY_ARROW_DOWN, 0, "\xE2\x86\x93", 5, ROW_DOWN, 0, 0, 0},
    {0, LT_KEY_ARROW_RIGHT, 0, "\xE2\x86\x92", 5, ROW_RIGHT, 0, 0, 0},
};

static const int NCAPS = (int)(sizeof keeb / sizeof keeb[0]);

static int g_need_w, g_need_h;
static int
    g_last_flash[256]; /* per-cap last-drawn flash level (NCAPS <= 256) */

static struct lt_event
    g_last_ev;        /* most recent real event (for the inspector) */
static int g_have_ev; /* 0 until the first event arrives */
static int g_insp_y;  /* top row of the inspector, set by layout() */

/* Inferred input model (POSIX). There is no public call to ask whether the
 * kitty keyboard protocol is actually live — negotiation only means we *asked*,
 * and the terminal may ignore it — so we infer it HONESTLY from events the
 * legacy control-byte model cannot produce. See observe_model(). */
enum input_model { MODEL_UNKNOWN, MODEL_KITTY, MODEL_LEGACY };
static enum input_model g_model = MODEL_UNKNOWN;
static const char *g_model_why = "";

/* ---- text helpers ---- */

static int draw_text(int x, int y, const char *s, lt_attr fg, lt_attr bg) {
  int col = 0;
  while (*s) {
    int n = lt_utf8_char_length(*s);
    if (n <= 0)
      n = 1;
    uint32_t cp = (unsigned char)*s;
    lt_utf8_char_to_unicode(&cp, s);
    lt_set_cell(x + col, y, cp, fg, bg);
    int cw = lt_wcwidth(cp);
    col += (cw > 0) ? cw : 1;
    s += n;
  }
  return col;
}

static int label_cols(const char *s) {
  int col = 0;
  while (*s) {
    int n = lt_utf8_char_length(*s);
    if (n <= 0)
      n = 1;
    uint32_t cp = (unsigned char)*s;
    lt_utf8_char_to_unicode(&cp, s);
    int cw = lt_wcwidth(cp);
    col += (cw > 0) ? cw : 1;
    s += n;
  }
  return col;
}

/* ---- color selection ---- */

static void cap_colors(const struct cap *c, int depth, lt_attr *fg,
                       lt_attr *bg) {
  if (c->flash <= 0) {
    *fg = LT_DEFAULT;
    *bg = LT_DEFAULT;
    return;
  }
  switch (depth) {
  case LT_OUTPUT_TRUECOLOR: {
    int t = c->flash; /* FLASH_FRAMES .. 1 */
    int b = 40 + (215 * t) / FLASH_FRAMES;
    int g = 20 + (120 * t) / FLASH_FRAMES;
    *bg = LT_RGB(20, (unsigned)g, (unsigned)b);
    *fg = LT_RGB(255, 255, 255) | LT_BOLD;
    break;
  }
  case LT_OUTPUT_256:
    *bg = (lt_attr)33;
    *fg = (lt_attr)231 | LT_BOLD;
    break;
  default:
    *bg = LT_BLUE;
    *fg = LT_WHITE | LT_BOLD;
    break;
  }
}

/* ---- drawing ---- */

/* Repaint just one key's interior (middle-row inner cells + centered label).
 * The shared walls are static and never repainted here. */
static void draw_cap_interior(const struct cap *c, int depth) {
  lt_attr fg, bg;
  cap_colors(c, depth, &fg, &bg);
  int inner = c->w - 2;
  for (int i = 0; i < inner; i++)
    lt_set_cell(c->x + 1 + i, c->y + 1, (uint32_t)' ', fg, bg);
  int start = (inner - label_cols(c->label)) / 2;
  if (start < 0)
    start = 0;
  draw_text(c->x + 1 + start, c->y + 1, c->label, fg, bg);
}

/* Draw the static frame for a group of k contiguous keys that share walls,
 * then fill each key's interior. k == 1 yields a standalone rounded box. */
static void draw_group(struct cap *caps, int k, int depth) {
  int y = caps[0].y;
  int x0 = caps[0].x;
  int xN = caps[k - 1].x + caps[k - 1].w - 1;

  /* top / bottom horizontals */
  lt_set_cell(x0, y, BX_TL, LT_DEFAULT, LT_DEFAULT);
  lt_set_cell(x0, y + 2, BX_BL, LT_DEFAULT, LT_DEFAULT);
  for (int col = x0 + 1; col < xN; col++) {
    lt_set_cell(col, y, BX_H, LT_DEFAULT, LT_DEFAULT);
    lt_set_cell(col, y + 2, BX_H, LT_DEFAULT, LT_DEFAULT);
  }
  lt_set_cell(xN, y, BX_TR, LT_DEFAULT, LT_DEFAULT);
  lt_set_cell(xN, y + 2, BX_BR, LT_DEFAULT, LT_DEFAULT);

  /* outer side walls */
  lt_set_cell(x0, y + 1, BX_V, LT_DEFAULT, LT_DEFAULT);
  lt_set_cell(xN, y + 1, BX_V, LT_DEFAULT, LT_DEFAULT);

  /* internal joints at each shared column */
  for (int m = 0; m < k - 1; m++) {
    int sc = caps[m].x + caps[m].w - 1;
    lt_set_cell(sc, y, BX_TD, LT_DEFAULT, LT_DEFAULT);
    lt_set_cell(sc, y + 1, BX_V, LT_DEFAULT, LT_DEFAULT);
    lt_set_cell(sc, y + 2, BX_TU, LT_DEFAULT, LT_DEFAULT);
  }

  for (int m = 0; m < k; m++)
    draw_cap_interior(&caps[m], depth);
}

/* Walk the table, drawing one group per contiguous run of equal row values. */
static void draw_frames(int depth) {
  int i = 0;
  while (i < NCAPS) {
    int j = i;
    while (j < NCAPS && keeb[j].row == keeb[i].row)
      j++;
    draw_group(&keeb[i], j - i, depth);
    i = j;
  }
}

static void draw_title(void) {
  draw_text(LEFT_MARGIN, TITLE_TOP,
            "libterm keyboard demo  -  press keys to light them up  -  Ctrl+C "
            "to quit",
            LT_DEFAULT | LT_BOLD, LT_DEFAULT);
}

/* ---- event inspector (honest: prints the raw lt_event fields) ---- */

static const char *type_name(uint8_t t) {
  switch (t) {
  case LT_EVENT_KEY:
    return "KEY";
  case LT_EVENT_RESIZE:
    return "RESIZE";
  case LT_EVENT_MOUSE:
    return "MOUSE";
  default:
    return "?";
  }
}

static const char *action_name(uint8_t a) {
  switch (a) {
  case LT_KEY_PRESS:
    return "PRESS";
  case LT_KEY_REPEAT:
    return "REPEAT";
  case LT_KEY_RELEASE:
    return "RELEASE";
  default:
    return "?";
  }
}

static const char *mod_str(uint8_t mod) {
  static char buf[40];
  if (!mod)
    return "(none)";
  buf[0] = '\0';
  if (mod & LT_MOD_CTRL)
    strcat(buf, "CTRL ");
  if (mod & LT_MOD_ALT)
    strcat(buf, "ALT ");
  if (mod & LT_MOD_SHIFT)
    strcat(buf, "SHIFT ");
  if (mod & LT_MOD_MOTION)
    strcat(buf, "MOTION ");
  return buf;
}

/* Decode a key code to its LT_KEY_* name. Several low codes are shared (e.g.
 * 0x0D is both ENTER and CTRL_M) — we name the conventional one. */
static const char *key_name(uint16_t key) {
  static char buf[28];
  switch (key) {
  case 0:
    return "none";
  case LT_KEY_F1:
    return "LT_KEY_F1";
  case LT_KEY_F2:
    return "LT_KEY_F2";
  case LT_KEY_F3:
    return "LT_KEY_F3";
  case LT_KEY_F4:
    return "LT_KEY_F4";
  case LT_KEY_F5:
    return "LT_KEY_F5";
  case LT_KEY_F6:
    return "LT_KEY_F6";
  case LT_KEY_F7:
    return "LT_KEY_F7";
  case LT_KEY_F8:
    return "LT_KEY_F8";
  case LT_KEY_F9:
    return "LT_KEY_F9";
  case LT_KEY_F10:
    return "LT_KEY_F10";
  case LT_KEY_F11:
    return "LT_KEY_F11";
  case LT_KEY_F12:
    return "LT_KEY_F12";
  case LT_KEY_INSERT:
    return "LT_KEY_INSERT";
  case LT_KEY_DELETE:
    return "LT_KEY_DELETE";
  case LT_KEY_HOME:
    return "LT_KEY_HOME";
  case LT_KEY_END:
    return "LT_KEY_END";
  case LT_KEY_PGUP:
    return "LT_KEY_PGUP";
  case LT_KEY_PGDN:
    return "LT_KEY_PGDN";
  case LT_KEY_ARROW_UP:
    return "LT_KEY_ARROW_UP";
  case LT_KEY_ARROW_DOWN:
    return "LT_KEY_ARROW_DOWN";
  case LT_KEY_ARROW_LEFT:
    return "LT_KEY_ARROW_LEFT";
  case LT_KEY_ARROW_RIGHT:
    return "LT_KEY_ARROW_RIGHT";
  case LT_KEY_BACK_TAB:
    return "LT_KEY_BACK_TAB";
  case LT_KEY_MOUSE_LEFT:
    return "LT_KEY_MOUSE_LEFT";
  case LT_KEY_MOUSE_RIGHT:
    return "LT_KEY_MOUSE_RIGHT";
  case LT_KEY_MOUSE_MIDDLE:
    return "LT_KEY_MOUSE_MIDDLE";
  case LT_KEY_MOUSE_RELEASE:
    return "LT_KEY_MOUSE_RELEASE";
  case LT_KEY_MOUSE_WHEEL_UP:
    return "LT_KEY_MOUSE_WHEEL_UP";
  case LT_KEY_MOUSE_WHEEL_DOWN:
    return "LT_KEY_MOUSE_WHEEL_DOWN";
  case LT_KEY_BACKSPACE:
    return "LT_KEY_BACKSPACE/CTRL_H";
  case LT_KEY_TAB:
    return "LT_KEY_TAB/CTRL_I";
  case LT_KEY_ENTER:
    return "LT_KEY_ENTER/CTRL_M";
  case LT_KEY_ESC:
    return "LT_KEY_ESC";
  case LT_KEY_SPACE:
    return "LT_KEY_SPACE";
  case LT_KEY_BACKSPACE2:
    return "LT_KEY_BACKSPACE2/CTRL_8";
  case LT_KEY_LEFT_SHIFT:
    return "LT_KEY_LEFT_SHIFT";
  case LT_KEY_RIGHT_SHIFT:
    return "LT_KEY_RIGHT_SHIFT";
  case LT_KEY_LEFT_CTRL:
    return "LT_KEY_LEFT_CTRL";
  case LT_KEY_RIGHT_CTRL:
    return "LT_KEY_RIGHT_CTRL";
  case LT_KEY_LEFT_ALT:
    return "LT_KEY_LEFT_ALT";
  case LT_KEY_RIGHT_ALT:
    return "LT_KEY_RIGHT_ALT";
  case LT_KEY_LEFT_SUPER:
    return "LT_KEY_LEFT_SUPER";
  case LT_KEY_RIGHT_SUPER:
    return "LT_KEY_RIGHT_SUPER";
  case LT_KEY_CAPS_LOCK:
    return "LT_KEY_CAPS_LOCK";
  default:
    break;
  }
  if (key >= LT_KEY_CTRL_A && key <= LT_KEY_CTRL_Z) {
    snprintf(buf, sizeof buf, "LT_KEY_CTRL_%c", (char)('A' + (key - 1)));
    return buf;
  }
  snprintf(buf, sizeof buf, "0x%04X", (unsigned)key);
  return buf;
}

static void clear_line(int y) {
  for (int x = LEFT_MARGIN; x < LEFT_MARGIN + INSP_W; x++)
    lt_set_cell(x, y, (uint32_t)' ', LT_DEFAULT, LT_DEFAULT);
}

/* Infer the input model from a real event. Some events are impossible in the
 * legacy control-byte model and therefore prove the modern kitty protocol is
 * live: a key REPEAT/RELEASE action, a bare-modifier key, or a printable letter
 * that still carries Ctrl/Shift (legacy folds Shift into the case and Ctrl into
 * a bare control byte, dropping the modifier — Alt is excluded since libterm
 * folds ESC+key to Alt even on legacy terminals). The opposite tell — a
 * Ctrl+letter that arrives as a bare control byte (ch=0, mod=0) — is the legacy
 * collapse the kitty protocol removes: it is exactly why Ctrl+I looks like Tab,
 * Ctrl+M like Enter, and Ctrl+J like LF. On Windows there is no kitty protocol;
 * draw_model_line() reports the native console there instead of inferring. */
static void observe_model(const struct lt_event *e) {
  if (e->type != LT_EVENT_KEY)
    return;
  if (e->action == LT_KEY_REPEAT) {
    g_model = MODEL_KITTY;
    g_model_why = "repeat event";
    return;
  }
  if (e->action == LT_KEY_RELEASE) {
    g_model = MODEL_KITTY;
    g_model_why = "release event";
    return;
  }
  if ((e->key >= LT_KEY_LEFT_SHIFT && e->key <= LT_KEY_RIGHT_SUPER) ||
      e->key == LT_KEY_CAPS_LOCK) {
    g_model = MODEL_KITTY;
    g_model_why = "bare-modifier key";
    return;
  }
  int letter = (e->ch >= 'a' && e->ch <= 'z') || (e->ch >= 'A' && e->ch <= 'Z');
  if (letter && (e->mod & (LT_MOD_CTRL | LT_MOD_SHIFT))) {
    g_model = MODEL_KITTY;
    g_model_why = "letter carrying a modifier";
    return;
  }
  /* Legacy tell: Ctrl+letter collapsed to a bare control byte (the Ctrl+I/Tab,
   * Ctrl+J/LF, Ctrl+M/Enter ambiguity). Skip the three that legitimately ARE
   * named keys. Never downgrade once kitty has been confirmed. */
  if (g_model != MODEL_KITTY && e->ch == 0 && e->mod == 0 &&
      e->key >= LT_KEY_CTRL_A && e->key <= LT_KEY_CTRL_Z &&
      e->key != LT_KEY_BACKSPACE && e->key != LT_KEY_TAB &&
      e->key != LT_KEY_ENTER) {
    g_model = MODEL_LEGACY;
    g_model_why = "Ctrl+letter collapsed";
  }
}

/* Persistent one-line input-model status (bold). Honest: on Windows it names
 * the native console source; on POSIX it reports the inferred model. */
static void draw_model_line(int y) {
  clear_line(y);
#ifdef _WIN32
  /* libterm's modern model reconstructs Ctrl+letter from the console
   * virtual-key (VK_I distinct from VK_TAB) -> ch+CTRL, matching POSIX+kitty.
   * Best-effort: it only works when the console reports the letter key. ConPTY
   * / Windows Terminal pre-collapse Ctrl+I->Tab (VK_TAB) and Ctrl+J->Enter
   * (VK_RETURN) before libterm, so there the letter is unrecoverable — as on
   * POSIX legacy. */
  draw_text(LEFT_MARGIN, y,
            "input model: Win32 console (native) - Ctrl+letter disambiguated "
            "(best-effort)",
            LT_DEFAULT | LT_BOLD, LT_DEFAULT);
#else
  char buf[110];
  const char *s;
  switch (g_model) {
  case MODEL_KITTY:
    snprintf(buf, sizeof buf,
             "input model: kitty protocol ACTIVE - confirmed by %s",
             g_model_why);
    s = buf;
    break;
  case MODEL_LEGACY:
    s = "input model: legacy control-byte (Ctrl+letter collapsed) - not kitty";
    break;
  default:
    s = "input model: modern (kitty negotiated) - unconfirmed; press Shift+a "
        "letter";
    break;
  }
  draw_text(LEFT_MARGIN, y, s, LT_DEFAULT | LT_BOLD, LT_DEFAULT);
#endif
}

static void draw_inspector(void) {
  int y = g_insp_y;
  clear_line(y);
  clear_line(y + 1);
  clear_line(y + 2);
  draw_model_line(y +
                  3); /* persistent input-model status (kitty/legacy/Win32) */
  draw_text(LEFT_MARGIN, y, "last libterm event (raw lt_peek_event fields):",
            LT_DEFAULT | LT_BOLD, LT_DEFAULT);
  if (!g_have_ev) {
    draw_text(LEFT_MARGIN + 2, y + 1,
              "press any key, type a combo, click, scroll, or resize",
              LT_DEFAULT, LT_DEFAULT);
    return;
  }
  const struct lt_event *e = &g_last_ev;
  char chrepr = (e->ch >= 0x20 && e->ch < 0x7F) ? (char)e->ch : '.';
  char line[128];
  snprintf(line, sizeof line,
           "type=%s  key=0x%04X %s  ch=0x%02X '%c'  mod=0x%02X %s",
           type_name(e->type), (unsigned)e->key, key_name(e->key),
           (unsigned)e->ch, chrepr, (unsigned)e->mod, mod_str(e->mod));
  draw_text(LEFT_MARGIN + 2, y + 1, line, LT_DEFAULT, LT_DEFAULT);

  char line2[96];
  line2[0] = '\0';
  if (e->type == LT_EVENT_MOUSE)
    snprintf(line2, sizeof line2, "pos  x=%d  y=%d", e->x, e->y);
  else if (e->type == LT_EVENT_RESIZE)
    snprintf(line2, sizeof line2, "size  w=%d  h=%d", e->w, e->h);
  else if (e->type == LT_EVENT_KEY)
    snprintf(line2, sizeof line2, "action=%s", action_name(e->action));
  if (line2[0])
    draw_text(LEFT_MARGIN + 2, y + 2, line2, LT_DEFAULT, LT_DEFAULT);
}

static void draw_all(int depth) {
  lt_clear();
  draw_title();
  draw_frames(depth);
  for (int i = 0; i < NCAPS; i++)
    g_last_flash[i] = keeb[i].flash;
  draw_inspector();
}

/* Incremental redraw: only the interiors of keys whose flash level changed. */
static void render(int depth) {
  for (int i = 0; i < NCAPS; i++) {
    if (keeb[i].flash != g_last_flash[i]) {
      draw_cap_interior(&keeb[i], depth);
      g_last_flash[i] = keeb[i].flash;
    }
  }
}

/* ---- layout ---- */

static void layout(void) {
  int rowx[NROWS];
  for (int r = 0; r < NROWS; r++)
    rowx[r] = LEFT_MARGIN;
  int maxx = 0;

  /* main rows: adjacent keys share a vertical wall, so advance by w - 1 */
  for (int i = 0; i < NCAPS; i++) {
    struct cap *c = &keeb[i];
    if (c->row < 0)
      continue;
    c->x = rowx[c->row];
    c->y = KB_TOP + c->row * CAP_H;
    rowx[c->row] += c->w - 1;
    if (c->x + c->w > maxx)
      maxx = c->x + c->w;
  }

  /* arrow cluster: standalone boxes (1-col gaps) right of the main block,
     up centered above down */
  int ax = maxx + 2;
  for (int i = 0; i < NCAPS; i++) {
    struct cap *c = &keeb[i];
    if (c->row >= 0)
      continue;
    switch (c->row) {
    case ROW_UP:
      c->x = ax + 6;
      c->y = KB_TOP + 4 * CAP_H;
      break;
    case ROW_LEFT:
      c->x = ax;
      c->y = KB_TOP + 5 * CAP_H;
      break;
    case ROW_DOWN:
      c->x = ax + 6;
      c->y = KB_TOP + 5 * CAP_H;
      break;
    case ROW_RIGHT:
      c->x = ax + 12;
      c->y = KB_TOP + 5 * CAP_H;
      break;
    default:
      break;
    }
    if (c->x + c->w > maxx)
      maxx = c->x + c->w;
  }

  g_need_w = maxx + LEFT_MARGIN;
  if (g_need_w < LEFT_MARGIN + INSP_W)
    g_need_w = LEFT_MARGIN + INSP_W;
  g_insp_y = KB_TOP + NROWS * CAP_H + 1; /* one blank row below the keyboard */
  g_need_h = g_insp_y + 4 + 1; /* inspector: 3 event lines + 1 model line */
}

/* ---- event matching ---- */

static void flash_event(const struct lt_event *ev) {
  /* HONEST: caps light only from real event fields — nothing is fabricated.
   * On a kitty-capable terminal (modern model, the default) modifiers arrive on
   * every key, so Shift+letter carries LT_MOD_SHIFT and a bare modifier arrives
   * as its own LT_KEY_LEFT_SHIFT/.../RIGHT_ALT key; both light the matching
   * cap. On a legacy terminal those bits aren't on the wire and degrade away.
   *   - Ctrl+letter, modern: ch=letter + LT_MOD_CTRL (control byte normalized),
   *     handled by the generic ch/mod paths below.
   *   - Ctrl+letter, compat (LT_INPUT_COMPAT): a LT_KEY_CTRL_* code instead;
   *     normalized below so the demo lights the same caps in either mode. The
   *     three that alias Backspace/Tab/Enter stay those keys. */

  /* A key release must not re-light the cap (kitty delivers release events). */
  if (ev->action == LT_KEY_RELEASE)
    return;

  uint8_t mod = ev->mod;
  uint32_t lc = ev->ch;
  if (lc && lc < 128)
    lc = (uint32_t)tolower((int)lc);

  if (ev->ch == 0 && ev->key >= LT_KEY_CTRL_A && ev->key <= LT_KEY_CTRL_Z &&
      ev->key != LT_KEY_BACKSPACE && ev->key != LT_KEY_TAB &&
      ev->key != LT_KEY_ENTER) {
    mod |= LT_MOD_CTRL;
    lc = (uint32_t)('a' + (ev->key - 1)); /* 0x01 -> 'a' ... 0x1A -> 'z' */
  }

  /* A bare modifier press (modern model) is a dedicated key code with no ch;
   * light the corresponding modifier cap. */
  switch (ev->key) {
  case LT_KEY_LEFT_SHIFT:
  case LT_KEY_RIGHT_SHIFT:
    mod |= LT_MOD_SHIFT;
    break;
  case LT_KEY_LEFT_CTRL:
  case LT_KEY_RIGHT_CTRL:
    mod |= LT_MOD_CTRL;
    break;
  case LT_KEY_LEFT_ALT:
  case LT_KEY_RIGHT_ALT:
    mod |= LT_MOD_ALT;
    break;
  default:
    break;
  }

  for (int i = 0; i < NCAPS; i++) {
    if (keeb[i].mod && (mod & keeb[i].mod))
      keeb[i].flash = FLASH_FRAMES;
  }
  for (int i = 0; i < NCAPS; i++) {
    struct cap *c = &keeb[i];
    int hit = (c->ch && lc == c->ch) || (c->key && ev->key == c->key);
    if (hit) {
      c->flash = FLASH_FRAMES;
      break;
    }
  }
}

static void tick_flashes(void) {
  for (int i = 0; i < NCAPS; i++) {
    if (keeb[i].flash > 0)
      keeb[i].flash--;
  }
}

int main(void) {
  int rc = lt_init();
  if (rc != LT_OK) {
    fprintf(stderr, "lt_init failed: %s\n", lt_strerror(rc));
    return 1;
  }

  int depth = lt_detect_color_depth();
  lt_set_output_mode(depth);
  /* Modern model is the default (no LT_INPUT_COMPAT): the kitty keyboard
   * protocol is negotiated, so modifiers arrive on every key, bare modifiers
   * arrive as LT_KEY_* codes, and ev.action carries press/repeat/release on
   * capable terminals (legacy terminals degrade gracefully). MOUSE adds
   * click/scroll as LT_EVENT_MOUSE for the inspector. */
  lt_set_input_mode(LT_INPUT_MOUSE);
  layout();

  int tw = lt_width(), th = lt_height();
  if (tw < g_need_w || th < g_need_h) {
    lt_shutdown();
    fprintf(stderr, "terminal too small: need %dx%d, have %dx%d\n", g_need_w,
            g_need_h, tw, th);
    return 1;
  }

  lt_hide_cursor();
  draw_all(depth);
  lt_present();

  struct lt_event ev;
  for (;;) {
    rc = lt_peek_event(&ev, TICK_MS);
    if (rc == LT_OK) {
      /* Ctrl+C quits in both models: modern reports ch='c' + LT_MOD_CTRL,
       * compat reports the LT_KEY_CTRL_C control-byte code. */
      if (ev.type == LT_EVENT_KEY &&
          (((ev.ch == 'c' || ev.ch == 'C') && (ev.mod & LT_MOD_CTRL)) ||
           ev.key == LT_KEY_CTRL_C))
        break;
      g_last_ev = ev; /* record every real event for the inspector */
      g_have_ev = 1;
      observe_model(&ev); /* update the inferred input-model status (POSIX) */
      if (ev.type == LT_EVENT_RESIZE) {
        draw_all(depth); /* redraws keyboard + inspector for the new size */
      } else {
        if (ev.type == LT_EVENT_KEY)
          flash_event(&ev);
        draw_inspector();
      }
    } else if (rc == LT_ERR_NO_EVENT) {
      tick_flashes();
    }
    render(depth);
    lt_present();
  }

  lt_show_cursor();
  lt_shutdown();
  return 0;
}

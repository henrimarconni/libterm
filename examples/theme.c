/*
 * Demonstrates the color-query API: lt_query_color (default fg/bg) and
 * lt_is_dark_background, with the recommended fallback — if the terminal
 * doesn't answer (LT_ERR_NO_EVENT after the timeout), assume a dark
 * background and theme accordingly. The panel is themed by the verdict, so
 * running it on a light vs dark terminal profile renders differently.
 * Press q to quit.
 *
 * Windows note: queries answer natively from the console color table
 * (GetConsoleScreenBufferInfoEx) — immediate, no escape round-trip.
 */

#include "libterm/libterm.h"

#include <stdio.h>

/* One theme: fg/bg pairs for the panel's three element classes. Truecolor
 * values; on lower depths lt_set_output_mode still maps named fallbacks, so
 * we pick attrs per detected depth in main(). */
struct theme {
  lt_attr header_fg, header_bg;
  lt_attr body_fg, body_bg;
  lt_attr accent_fg, accent_bg;
};

static const struct theme dark_theme_rgb = {
    .header_fg = LT_RGB(0xEC, 0xEF, 0xF4),
    .header_bg = LT_RGB(0x3B, 0x42, 0x52),
    .body_fg = LT_RGB(0xD8, 0xDE, 0xE9),
    .body_bg = LT_DEFAULT,
    .accent_fg = LT_RGB(0x88, 0xC0, 0xD0),
    .accent_bg = LT_DEFAULT,
};

static const struct theme light_theme_rgb = {
    .header_fg = LT_RGB(0xFA, 0xFA, 0xFA),
    .header_bg = LT_RGB(0x44, 0x71, 0x7C),
    .body_fg = LT_RGB(0x37, 0x47, 0x4F),
    .body_bg = LT_DEFAULT,
    .accent_fg = LT_RGB(0x00, 0x60, 0x6B),
    .accent_bg = LT_DEFAULT,
};

static const struct theme dark_theme_named = {
    .header_fg = LT_BLACK,
    .header_bg = LT_WHITE,
    .body_fg = LT_WHITE,
    .body_bg = LT_DEFAULT,
    .accent_fg = LT_CYAN,
    .accent_bg = LT_DEFAULT,
};

static const struct theme light_theme_named = {
    .header_fg = LT_WHITE,
    .header_bg = LT_BLUE,
    .body_fg = LT_BLACK,
    .body_bg = LT_DEFAULT,
    .accent_fg = LT_BLUE,
    .accent_bg = LT_DEFAULT,
};

int main(void) {
  int rc = lt_init();
  if (rc != LT_OK) {
    fprintf(stderr, "lt_init failed: %d\n", rc);
    return 1;
  }

  int depth = lt_detect_color_depth();
  lt_set_output_mode(depth);
  lt_hide_cursor();

  /* ---- query phase: 500 ms timeouts so a silent terminal stalls briefly ----
   */
  uint32_t fg = 0, bg = 0;
  int fg_ok = lt_query_color(LT_COLOR_DEFAULT_FG, &fg, 500) == LT_OK;
  int bg_ok = lt_query_color(LT_COLOR_DEFAULT_BG, &bg, 500) == LT_OK;
  int dark = lt_is_dark_background(500);
  /* The recommended app pattern: a query error means "unknown" — fall back
   * to a default theme (dark is the safer assumption) and carry on. */
  int no_reply = dark < 0;
  if (no_reply)
    dark = 1;

  const int truecolor = depth == LT_OUTPUT_TRUECOLOR;
  const struct theme *t =
      dark ? (truecolor ? &dark_theme_rgb : &dark_theme_named)
           : (truecolor ? &light_theme_rgb : &light_theme_named);

  /* ---- render once ---- */
  const char *verdict = no_reply ? "no reply — defaulting to DARK"
                                 : (dark ? "detected: DARK background"
                                         : "detected: LIGHT background");
  lt_printf(2, 1, t->header_fg, t->header_bg, "  libterm theme demo — %s  ",
            verdict);

  /* The hex value is printed in the very color it names (with a filled
   * swatch beside it, in case the text lands on a matching background).
   * Queried colors are 0x00RRGGBB — exactly the truecolor lt_attr packing;
   * real black needs the LT_HI_BLACK sentinel (a bare 0 means "terminal
   * default"). Only meaningful in truecolor mode; lower depths would
   * reinterpret the bits as palette indexes, so they print plainly. */
  if (fg_ok) {
    lt_print(2, 3, t->body_fg, t->body_bg, "queried default fg:");
    lt_attr c = fg ? (lt_attr)fg : LT_HI_BLACK;
    lt_printf(22, 3, truecolor ? c : t->body_fg, t->body_bg, "#%06X", fg);
    if (truecolor)
      lt_print(30, 3, t->body_fg, c, "    ");
  } else {
    lt_print(2, 3, t->body_fg, t->body_bg, "queried default fg: n/a");
  }
  if (bg_ok) {
    lt_print(2, 4, t->body_fg, t->body_bg, "queried default bg:");
    lt_attr c = bg ? (lt_attr)bg : LT_HI_BLACK;
    lt_printf(22, 4, truecolor ? c : t->body_fg, t->body_bg, "#%06X", bg);
    if (truecolor)
      lt_print(30, 4, t->body_fg, c, "    ");
  } else {
    lt_print(2, 4, t->body_fg, t->body_bg, "queried default bg: n/a");
  }

  lt_print(2, 6, t->accent_fg, t->accent_bg,
           "────────────────────────────────────────");
  lt_print(2, 7, t->body_fg, t->body_bg,
           "This panel picked its palette from the verdict above —");
  lt_print(2, 8, t->body_fg, t->body_bg,
           "run it on a light and a dark terminal profile to compare.");

  lt_print(2, 10, t->accent_fg | LT_BOLD, t->accent_bg, "press q to exit");
  lt_present();

  struct lt_event ev;
  while (lt_poll_event(&ev) == LT_OK) {
    if (ev.type == LT_EVENT_KEY && (ev.ch == 'q' || ev.ch == 'Q'))
      break;
  }

  lt_shutdown();
  return 0;
}

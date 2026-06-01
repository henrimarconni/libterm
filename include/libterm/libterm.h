/*
 * libterm - cross-platform terminal UI library
 *
 * Public API. All identifiers use the lt_ / LT_ prefix.
 * Derived from termbox2 (https://github.com/termbox/termbox2).
 */
#ifndef LIBTERM_H
#define LIBTERM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && defined(LIBTERM_SHARED)
#if defined(LIBTERM_BUILDING)
#define LT_API __declspec(dllexport)
#else
#define LT_API __declspec(dllimport)
#endif
#elif defined(LIBTERM_SHARED) && (defined(__GNUC__) || defined(__clang__))
#define LT_API __attribute__((visibility("default")))
#else
#define LT_API
#endif

/* ---- version ---- */
#define LT_VERSION_MAJOR 0
#define LT_VERSION_MINOR 1
#define LT_VERSION_PATCH 0

/* ---- return codes ---- */
#define LT_OK 0
#define LT_ERR -1
#define LT_ERR_NEED_MORE -2
#define LT_ERR_INIT_ALREADY -3
#define LT_ERR_INIT_OPEN -4
#define LT_ERR_MEM -5
#define LT_ERR_NO_EVENT -6
#define LT_ERR_NO_TERM -7
#define LT_ERR_NOT_INIT -8
#define LT_ERR_OUT_OF_BOUNDS -9
#define LT_ERR_READ -10
#define LT_ERR_RESIZE_IOCTL -11
#define LT_ERR_RESIZE_PIPE -12
#define LT_ERR_RESIZE_SIGACTION -13
#define LT_ERR_POLL -14
#define LT_ERR_TCGETATTR -15
#define LT_ERR_TCSETATTR -16
#define LT_ERR_UNSUPPORTED_TERM -17
#define LT_ERR_RESIZE_WRITE -18
#define LT_ERR_RESIZE_POLL -19
#define LT_ERR_RESIZE_READ -20
#define LT_ERR_RESIZE_SSCANF -21
#define LT_ERR_CAP_COLLISION -22

/* ---- event types ---- */
#define LT_EVENT_KEY 1
#define LT_EVENT_RESIZE 2
#define LT_EVENT_MOUSE 3

/* ---- keys (subset; mirror termbox2) ---- */
#define LT_KEY_F1 (0xFFFF - 0)
#define LT_KEY_F2 (0xFFFF - 1)
#define LT_KEY_F3 (0xFFFF - 2)
#define LT_KEY_F4 (0xFFFF - 3)
#define LT_KEY_F5 (0xFFFF - 4)
#define LT_KEY_F6 (0xFFFF - 5)
#define LT_KEY_F7 (0xFFFF - 6)
#define LT_KEY_F8 (0xFFFF - 7)
#define LT_KEY_F9 (0xFFFF - 8)
#define LT_KEY_F10 (0xFFFF - 9)
#define LT_KEY_F11 (0xFFFF - 10)
#define LT_KEY_F12 (0xFFFF - 11)
#define LT_KEY_INSERT (0xFFFF - 12)
#define LT_KEY_DELETE (0xFFFF - 13)
#define LT_KEY_HOME (0xFFFF - 14)
#define LT_KEY_END (0xFFFF - 15)
#define LT_KEY_PGUP (0xFFFF - 16)
#define LT_KEY_PGDN (0xFFFF - 17)
#define LT_KEY_ARROW_UP (0xFFFF - 18)
#define LT_KEY_ARROW_DOWN (0xFFFF - 19)
#define LT_KEY_ARROW_LEFT (0xFFFF - 20)
#define LT_KEY_ARROW_RIGHT (0xFFFF - 21)
#define LT_KEY_BACK_TAB (0xFFFF - 22) /* Shift+Tab, CSI Z; mirrors termbox2 */

/* ---- mouse buttons (reported in lt_event.key when type == LT_EVENT_MOUSE) ----
 * Values mirror termbox2 (TB_KEY_MOUSE_*) for drop-in parity. */
#define LT_KEY_MOUSE_LEFT (0xFFFF - 23)
#define LT_KEY_MOUSE_RIGHT (0xFFFF - 24)
#define LT_KEY_MOUSE_MIDDLE (0xFFFF - 25)
#define LT_KEY_MOUSE_RELEASE (0xFFFF - 26)
#define LT_KEY_MOUSE_WHEEL_UP (0xFFFF - 27)
#define LT_KEY_MOUSE_WHEEL_DOWN (0xFFFF - 28)

#define LT_KEY_ENTER 0x0D
#define LT_KEY_ESC 0x1B
#define LT_KEY_SPACE 0x20
#define LT_KEY_BACKSPACE 0x08
#define LT_KEY_BACKSPACE2 0x7F
#define LT_KEY_TAB 0x09

/* ---- Ctrl+key codes (mirror termbox2) ----
 * A standalone control byte is reported as one of these key codes (ch == 0).
 * Several intentionally share a value with the named keys above: CTRL_H ==
 * BACKSPACE (0x08), CTRL_I == TAB (0x09), CTRL_M == ENTER (0x0D),
 * CTRL_LSQ_BRACKET == ESC (0x1B), CTRL_8 == BACKSPACE2 (0x7F). */
#define LT_KEY_CTRL_TILDE 0x00
#define LT_KEY_CTRL_2 0x00 /* clash with CTRL_TILDE */
#define LT_KEY_CTRL_A 0x01
#define LT_KEY_CTRL_B 0x02
#define LT_KEY_CTRL_C 0x03
#define LT_KEY_CTRL_D 0x04
#define LT_KEY_CTRL_E 0x05
#define LT_KEY_CTRL_F 0x06
#define LT_KEY_CTRL_G 0x07
#define LT_KEY_CTRL_H 0x08 /* clash with BACKSPACE */
#define LT_KEY_CTRL_I 0x09 /* clash with TAB */
#define LT_KEY_CTRL_J 0x0A
#define LT_KEY_CTRL_K 0x0B
#define LT_KEY_CTRL_L 0x0C
#define LT_KEY_CTRL_M 0x0D /* clash with ENTER */
#define LT_KEY_CTRL_N 0x0E
#define LT_KEY_CTRL_O 0x0F
#define LT_KEY_CTRL_P 0x10
#define LT_KEY_CTRL_Q 0x11
#define LT_KEY_CTRL_R 0x12
#define LT_KEY_CTRL_S 0x13
#define LT_KEY_CTRL_T 0x14
#define LT_KEY_CTRL_U 0x15
#define LT_KEY_CTRL_V 0x16
#define LT_KEY_CTRL_W 0x17
#define LT_KEY_CTRL_X 0x18
#define LT_KEY_CTRL_Y 0x19
#define LT_KEY_CTRL_Z 0x1A
#define LT_KEY_CTRL_LSQ_BRACKET 0x1B /* clash with ESC */
#define LT_KEY_CTRL_3 0x1B           /* clash with ESC */
#define LT_KEY_CTRL_4 0x1C
#define LT_KEY_CTRL_BACKSLASH 0x1C /* clash with CTRL_4 */
#define LT_KEY_CTRL_5 0x1D
#define LT_KEY_CTRL_RSQ_BRACKET 0x1D /* clash with CTRL_5 */
#define LT_KEY_CTRL_6 0x1E
#define LT_KEY_CTRL_7 0x1F
#define LT_KEY_CTRL_SLASH 0x1F      /* clash with CTRL_7 */
#define LT_KEY_CTRL_UNDERSCORE 0x1F /* clash with CTRL_7 */
#define LT_KEY_CTRL_8 0x7F          /* clash with BACKSPACE2 */

/* ---- modifiers ---- */
#define LT_MOD_ALT 0x01
#define LT_MOD_CTRL 0x02
#define LT_MOD_SHIFT 0x04
#define LT_MOD_MOTION 0x08

/* ---- colors ---- */
#define LT_DEFAULT 0x0000
#define LT_BLACK 0x0001
#define LT_RED 0x0002
#define LT_GREEN 0x0003
#define LT_YELLOW 0x0004
#define LT_BLUE 0x0005
#define LT_MAGENTA 0x0006
#define LT_CYAN 0x0007
#define LT_WHITE 0x0008

/* ---- attributes ---- */
/* Color occupies bits 0-23 of fg/bg (24-bit RGB in TRUECOLOR mode, an 8-bit
 * palette index in 256/216/GRAYSCALE, a named color 0-8 in NORMAL). The 7
 * attribute bits live in bits 24-30 and are OR'd into fg only; the attribute
 * bits of bg are ignored. */
#define LT_BOLD 0x01000000u
#define LT_UNDERLINE 0x02000000u
#define LT_REVERSE 0x04000000u
#define LT_ITALIC 0x08000000u
#define LT_BLINK 0x10000000u
#define LT_DIM 0x20000000u
#define LT_STRIKE 0x40000000u

/* Sentinel (bit 31). A color whose bits 0-23 are 0 means "use the terminal
 * default"; OR in LT_HI_BLACK to select real black (palette index 0 or RGB
 * 0,0,0) instead. Mirrors termbox2's TB_HI_BLACK. */
#define LT_HI_BLACK 0x80000000u

/* Pack a 24-bit RGB color into bits 0-23 for LT_OUTPUT_TRUECOLOR. */
#define LT_RGB(r, g, b)                                                        \
  ((lt_attr)((((lt_attr)(r) & 0xFFu) << 16) | (((lt_attr)(g) & 0xFFu) << 8) |  \
             ((lt_attr)(b) & 0xFFu)))

/* ---- types ---- */
typedef uint32_t lt_uchar;
typedef uint32_t lt_attr;

struct lt_cell {
  lt_uchar ch;
  lt_attr fg;
  lt_attr bg;
  /* Pads the cell to 16 B (cell-aligned for SIMD scans). Also holds the
   * grapheme-cluster id: 0 means the cell is just `ch`; nonzero indexes an
   * internal table of the trailing combining/joined codepoints (see
   * lt_set_cell_ex / lt_extend_cell). Treat as opaque — do not set directly. */
  uint32_t _reserved;
};

struct lt_event {
  uint8_t type;
  uint8_t mod;
  uint16_t key;
  lt_uchar ch;
  int32_t w;
  int32_t h;
  int32_t x;
  int32_t y;
};

/* ---- lifecycle ---- */
LT_API int lt_init(void);
/* Initialize against a caller-supplied terminal fd (POSIX). The fd must be a
 * tty (validated with isatty); it is NOT closed by lt_shutdown — the caller
 * owns it. On non-POSIX platforms this returns LT_ERR_INIT_OPEN. */
LT_API int lt_init_fd(int ttyfd);
/* Initialize against a terminal opened from `path` (e.g. "/dev/tty",
 * "/dev/pts/3"), POSIX. Opens the path O_RDWR; the fd IS closed by lt_shutdown
 * (libterm owns it, unlike lt_init_fd). Returns LT_ERR_INIT_OPEN if the path
 * can't be opened or isn't a tty (lt_last_errno carries the cause). On
 * non-POSIX platforms this returns LT_ERR_INIT_OPEN. */
LT_API int lt_init_file(const char *path);
LT_API int lt_shutdown(void);

/* ---- screen ---- */
LT_API int lt_width(void);
LT_API int lt_height(void);
LT_API int lt_clear(void);
LT_API int lt_set_clear_attrs(lt_attr fg, lt_attr bg);
LT_API int lt_present(void);
/* Force the next lt_present to repaint every cell, bypassing the diff. Use
 * after emitting raw bytes via lt_send (or any out-of-band terminal write) that
 * desynced libterm's screen model from what is actually displayed. Returns
 * LT_ERR_NOT_INIT before lt_init. */
LT_API int lt_invalidate(void);
LT_API int lt_set_cursor(int x, int y);
LT_API int lt_hide_cursor(void);
LT_API int lt_show_cursor(void);
LT_API int lt_set_cell(int x, int y, lt_uchar ch, lt_attr fg, lt_attr bg);
/* Copy the back-buffer cell at (x, y) into *out (the buffer consumers write via
 * lt_set_cell). Returns LT_ERR_OUT_OF_BOUNDS for an off-buffer (x, y),
 * LT_ERR_NOT_INIT before lt_init, or LT_ERR if out is NULL. */
LT_API int lt_get_cell(int x, int y, struct lt_cell *out);

/* ---- grapheme clusters ---- */
/* Write a grapheme cluster (a base codepoint plus trailing combining/joined
 * codepoints, e.g. e + U+0301, or a ZWJ emoji sequence) into the cell at
 * (x, y). `chs[0]` is the base; `chs[1..nch-1]` are the continuation
 * codepoints. nch == 1 is equivalent to lt_set_cell. The cluster occupies one
 * cell and advances the width of its base codepoint. Returns
 * LT_ERR_OUT_OF_BOUNDS off-buffer, LT_ERR_NOT_INIT before init, LT_ERR on a
 * NULL/empty chs, or LT_ERR_MEM if the cluster table is exhausted. */
LT_API int lt_set_cell_ex(int x, int y, const lt_uchar *chs, size_t nch,
                          lt_attr fg, lt_attr bg);
/* Append one combining/joined codepoint to the cluster already in the cell at
 * (x, y) (a shortcut for growing a cluster one codepoint at a time). The cell's
 * base must already be set. Same error returns as lt_set_cell_ex. */
LT_API int lt_extend_cell(int x, int y, lt_uchar ch);
/* Non-zero if libterm was built with grapheme-cluster support (always 1 here;
 * mirrors termbox2's compile-time tb_has_egc). */
LT_API int lt_has_egc(void);

/* ---- print helpers ---- */
/* Write a UTF-8 string into the back buffer starting at (x, y), each cell in
 * fg/bg. Each codepoint advances one column (libterm has no wcwidth yet, so
 * wide and combining characters occupy a single cell). A '\n' moves to column
 * x of the next row. Malformed UTF-8 becomes U+FFFD. Cells outside the buffer
 * are clipped silently; only a starting (x, y) out of bounds returns
 * LT_ERR_OUT_OF_BOUNDS. Returns LT_ERR_NOT_INIT before lt_init. */
LT_API int lt_print(int x, int y, lt_attr fg, lt_attr bg, const char *str);
/* As lt_print, but if out_w is non-NULL it receives the number of columns
 * advanced on the widest line written. */
LT_API int lt_print_ex(int x, int y, lt_attr fg, lt_attr bg, size_t *out_w,
                       const char *str);
/* printf-style wrapper over lt_print. Formats into an internal fixed buffer
 * (truncated if the result would exceed it), then prints the result. */
LT_API int lt_printf(int x, int y, lt_attr fg, lt_attr bg, const char *fmt, ...);
/* As lt_printf, but if out_w is non-NULL it receives the number of columns
 * advanced on the widest line written (see lt_print_ex). */
LT_API int lt_printf_ex(int x, int y, lt_attr fg, lt_attr bg, size_t *out_w,
                        const char *fmt, ...);

/* ---- raw output ---- */
/* Write raw bytes straight to the terminal, bypassing the cell buffer — for
 * escape sequences libterm doesn't model (window title, custom modes, etc.).
 * Buffered and flushed immediately. Returns LT_ERR_NOT_INIT before lt_init,
 * LT_ERR on a NULL buffer. Bytes are emitted verbatim; the caller owns their
 * correctness and any interaction with the rendered screen state. */
LT_API int lt_send(const char *buf, size_t nbuf);
/* printf-style wrapper over lt_send. Formats into an internal fixed buffer
 * (truncated if the result would exceed it), then sends the result. */
LT_API int lt_sendf(const char *fmt, ...);

/* ---- input ---- */
/* Event reads return LT_OK with an event, or LT_ERR_NO_EVENT when none is
 * available (poll timeout / interrupted). On a genuine I/O failure they return
 * the real error (LT_ERR_READ / LT_ERR_POLL) — failures are never masked as
 * "no event". Before lt_init they return LT_ERR_NOT_INIT. */
LT_API int lt_poll_event(struct lt_event *event);
LT_API int lt_peek_event(struct lt_event *event, int timeout_ms);

/* Expose the underlying file descriptors so a caller can wait on libterm input
 * inside its own select/poll/epoll loop instead of blocking in lt_poll_event:
 * *ttyfd receives the terminal fd, *resizefd the read end of the SIGWINCH
 * self-pipe. When either becomes readable, call lt_peek_event(ev, 0) to drain
 * the event. Either pointer may be NULL to skip it. POSIX-only: returns
 * LT_ERR_UNSUPPORTED_TERM on platforms without pollable fds (Windows), and
 * LT_ERR_NOT_INIT before lt_init. */
LT_API int lt_get_fds(int *ttyfd, int *resizefd);

/* ---- input mode flags ---- */
#define LT_INPUT_CURRENT 0
#define LT_INPUT_ESC 1
#define LT_INPUT_ALT 2
#define LT_INPUT_MOUSE 4

LT_API int lt_set_input_mode(int mode);

/* ---- output mode flags ---- */
#define LT_OUTPUT_CURRENT 0
#define LT_OUTPUT_NORMAL 1
#define LT_OUTPUT_256 2
#define LT_OUTPUT_216 3
#define LT_OUTPUT_GRAYSCALE 4
#define LT_OUTPUT_TRUECOLOR 5

LT_API int lt_set_output_mode(int mode);

/* Inspect $COLORTERM / $TERM and return the best output mode the terminal
 * appears to support: LT_OUTPUT_TRUECOLOR, LT_OUTPUT_256, or LT_OUTPUT_NORMAL.
 * Stateless — safe to call before lt_init to decide the initial mode. */
LT_API int lt_detect_color_depth(void);

/* ---- UTF-8 helpers ---- */
LT_API int lt_utf8_char_length(char c);
LT_API int lt_utf8_char_to_unicode(uint32_t *out, const char *c);
LT_API int lt_utf8_unicode_to_char(char *out, uint32_t c);

/* ---- character width ---- */
/* Number of terminal columns a codepoint occupies: 0 for combining marks and
 * other zero-width characters, 2 for wide East-Asian / emoji characters, 1 for
 * the rest. Returns -1 for control characters and other non-printable
 * codepoints (mirrors POSIX wcwidth). Follows Markus Kuhn's reference ranges;
 * not locale-dependent. */
LT_API int lt_wcwidth(uint32_t ch);
/* Non-zero if the codepoint is printable (i.e. lt_wcwidth(ch) >= 0). */
LT_API int lt_iswprint(uint32_t ch);
/* Byte width of the lt_attr type used for fg/bg (always 4 — lt_attr is
 * uint32_t). Mirrors termbox2's tb_attr_width; libterm has no compile-time
 * attribute-width option, so this is constant. */
LT_API int lt_attr_width(void);

/* ---- misc ---- */
LT_API const char *lt_strerror(int code);
LT_API const char *lt_version(void);
/* The errno captured at the most recent failing syscall inside libterm (e.g.
 * the open/tcsetattr behind LT_ERR_INIT_OPEN, or the read/select behind
 * LT_ERR_READ / LT_ERR_POLL). Lets a caller that received such a code see the
 * underlying cause — pass it to strerror(3). 0 if nothing has failed. Only the
 * platform syscall paths set it; purely logical errors (LT_ERR_OUT_OF_BOUNDS,
 * LT_ERR_NOT_INIT, ...) leave it unchanged. */
LT_API int lt_last_errno(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBTERM_H */

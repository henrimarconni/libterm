/*
 * compat/termbox2.h — drop-in termbox2 compatibility layer for libterm.
 *
 * Replace `#include "termbox2.h"` with this header and link against libterm.
 * It maps the termbox2 public API (tb_/TB_) onto libterm (lt_/LT_).
 *
 * SUPPORTED: every termbox2 public function with a libterm equivalent; all
 * TB_KEY_* / TB_MOD_* / TB_EVENT_* / TB_OUTPUT_* / TB_INPUT_* constants,
 * colors, attributes, and return codes; struct tb_cell / tb_event;
 * uintattr_t.
 *
 * INPUT SEMANTICS: tb_init uses libterm's modern key model by default (single
 * Esc event; Ctrl+letter as ch+mod where the terminal supports it). For exact
 * termbox2 key semantics (two-event Esc/Alt, control-byte-in-key), call
 *   tb_set_input_mode(LT_INPUT_COMPAT);
 * after tb_init.
 *
 * UNSUPPORTED (using these is a compile error naming the libterm alternative):
 *   tb_init_rwfd     -> lt_init_fd / lt_init_file (libterm is single-fd)
 *   tb_set_func      -> (none; deprecated upstream)
 *   tb_has_truecolor -> lt_detect_color_depth
 *   tb_cell_buffer   -> lt_get_cell (no raw back-buffer pointer)
 *   tb_key_i         -> (none; no terminfo cap table)
 *
 * tb_get_cell CAVEATS: returns a pointer to an internal snapshot valid only
 * until the next tb_get_cell call (libterm copies; it does not expose a live
 * buffer pointer); reads the BACK buffer only, so back==0 (front) -> LT_ERR.
 *
 * NOTE: struct tb_cell IS libterm's struct lt_cell (16-byte POD: ch/fg/bg +
 * opaque _reserved). termbox2's per-cell ech/nech/cech EGC pointers are not
 * present; use lt_set_cell_ex / lt_extend_cell for grapheme clusters.
 */
#ifndef LIBTERM_COMPAT_TERMBOX2_H
#define LIBTERM_COMPAT_TERMBOX2_H

#include "libterm/libterm.h"

#include <stdlib.h> /* for the tb_malloc/realloc/free aliases */

/* ---- types: alias the struct TAGS (a typedef would leave `struct tb_cell`
 *      a separate incomplete type and break tb_get_cell/tb_poll_event). ---- */
#define tb_cell lt_cell
#define tb_event lt_event
typedef lt_attr uintattr_t;

/* ---- return codes ---- */
#define TB_OK LT_OK
#define TB_ERR LT_ERR
#define TB_ERR_NEED_MORE LT_ERR_NEED_MORE
#define TB_ERR_INIT_ALREADY LT_ERR_INIT_ALREADY
#define TB_ERR_INIT_OPEN LT_ERR_INIT_OPEN
#define TB_ERR_MEM LT_ERR_MEM
#define TB_ERR_NO_EVENT LT_ERR_NO_EVENT
#define TB_ERR_NO_TERM LT_ERR_NO_TERM
#define TB_ERR_NOT_INIT LT_ERR_NOT_INIT
#define TB_ERR_OUT_OF_BOUNDS LT_ERR_OUT_OF_BOUNDS
#define TB_ERR_READ LT_ERR_READ
#define TB_ERR_RESIZE_IOCTL LT_ERR_RESIZE_IOCTL
#define TB_ERR_RESIZE_PIPE LT_ERR_RESIZE_PIPE
#define TB_ERR_RESIZE_SIGACTION LT_ERR_RESIZE_SIGACTION
#define TB_ERR_POLL LT_ERR_POLL
#define TB_ERR_TCGETATTR LT_ERR_TCGETATTR
#define TB_ERR_TCSETATTR LT_ERR_TCSETATTR
#define TB_ERR_UNSUPPORTED_TERM LT_ERR_UNSUPPORTED_TERM
#define TB_ERR_RESIZE_WRITE LT_ERR_RESIZE_WRITE
#define TB_ERR_RESIZE_POLL LT_ERR_RESIZE_POLL
#define TB_ERR_RESIZE_READ LT_ERR_RESIZE_READ
#define TB_ERR_RESIZE_SSCANF LT_ERR_RESIZE_SSCANF
#define TB_ERR_CAP_COLLISION LT_ERR_CAP_COLLISION

/* ---- event types ---- */
#define TB_EVENT_KEY LT_EVENT_KEY
#define TB_EVENT_RESIZE LT_EVENT_RESIZE
#define TB_EVENT_MOUSE LT_EVENT_MOUSE

/* ---- modifiers ---- */
#define TB_MOD_ALT LT_MOD_ALT
#define TB_MOD_CTRL LT_MOD_CTRL
#define TB_MOD_SHIFT LT_MOD_SHIFT
#define TB_MOD_MOTION LT_MOD_MOTION

/* ---- input modes ---- */
#define TB_INPUT_CURRENT LT_INPUT_CURRENT
#define TB_INPUT_ESC LT_INPUT_ESC
#define TB_INPUT_ALT LT_INPUT_ALT
#define TB_INPUT_MOUSE LT_INPUT_MOUSE

/* ---- output modes ---- */
#define TB_OUTPUT_CURRENT LT_OUTPUT_CURRENT
#define TB_OUTPUT_NORMAL LT_OUTPUT_NORMAL
#define TB_OUTPUT_256 LT_OUTPUT_256
#define TB_OUTPUT_216 LT_OUTPUT_216
#define TB_OUTPUT_GRAYSCALE LT_OUTPUT_GRAYSCALE
#define TB_OUTPUT_TRUECOLOR LT_OUTPUT_TRUECOLOR

/* ---- colors ---- */
#define TB_DEFAULT LT_DEFAULT
#define TB_BLACK LT_BLACK
#define TB_RED LT_RED
#define TB_GREEN LT_GREEN
#define TB_YELLOW LT_YELLOW
#define TB_BLUE LT_BLUE
#define TB_MAGENTA LT_MAGENTA
#define TB_CYAN LT_CYAN
#define TB_WHITE LT_WHITE
#define TB_RGB(r, g, b) LT_RGB((r), (g), (b))

/* ---- attributes ---- */
#define TB_BOLD LT_BOLD
#define TB_UNDERLINE LT_UNDERLINE
#define TB_REVERSE LT_REVERSE
#define TB_ITALIC LT_ITALIC
#define TB_BLINK LT_BLINK
#define TB_DIM LT_DIM
#define TB_STRIKE LT_STRIKE

/* ---- keys (full termbox2 set; each has an LT_KEY_ twin) ---- */
#define TB_KEY_CTRL_TILDE LT_KEY_CTRL_TILDE
#define TB_KEY_CTRL_2 LT_KEY_CTRL_2
#define TB_KEY_CTRL_A LT_KEY_CTRL_A
#define TB_KEY_CTRL_B LT_KEY_CTRL_B
#define TB_KEY_CTRL_C LT_KEY_CTRL_C
#define TB_KEY_CTRL_D LT_KEY_CTRL_D
#define TB_KEY_CTRL_E LT_KEY_CTRL_E
#define TB_KEY_CTRL_F LT_KEY_CTRL_F
#define TB_KEY_CTRL_G LT_KEY_CTRL_G
#define TB_KEY_BACKSPACE LT_KEY_BACKSPACE
#define TB_KEY_CTRL_H LT_KEY_CTRL_H
#define TB_KEY_TAB LT_KEY_TAB
#define TB_KEY_CTRL_I LT_KEY_CTRL_I
#define TB_KEY_CTRL_J LT_KEY_CTRL_J
#define TB_KEY_CTRL_K LT_KEY_CTRL_K
#define TB_KEY_CTRL_L LT_KEY_CTRL_L
#define TB_KEY_ENTER LT_KEY_ENTER
#define TB_KEY_CTRL_M LT_KEY_CTRL_M
#define TB_KEY_CTRL_N LT_KEY_CTRL_N
#define TB_KEY_CTRL_O LT_KEY_CTRL_O
#define TB_KEY_CTRL_P LT_KEY_CTRL_P
#define TB_KEY_CTRL_Q LT_KEY_CTRL_Q
#define TB_KEY_CTRL_R LT_KEY_CTRL_R
#define TB_KEY_CTRL_S LT_KEY_CTRL_S
#define TB_KEY_CTRL_T LT_KEY_CTRL_T
#define TB_KEY_CTRL_U LT_KEY_CTRL_U
#define TB_KEY_CTRL_V LT_KEY_CTRL_V
#define TB_KEY_CTRL_W LT_KEY_CTRL_W
#define TB_KEY_CTRL_X LT_KEY_CTRL_X
#define TB_KEY_CTRL_Y LT_KEY_CTRL_Y
#define TB_KEY_CTRL_Z LT_KEY_CTRL_Z
#define TB_KEY_ESC LT_KEY_ESC
#define TB_KEY_CTRL_LSQ_BRACKET LT_KEY_CTRL_LSQ_BRACKET
#define TB_KEY_CTRL_3 LT_KEY_CTRL_3
#define TB_KEY_CTRL_4 LT_KEY_CTRL_4
#define TB_KEY_CTRL_BACKSLASH LT_KEY_CTRL_BACKSLASH
#define TB_KEY_CTRL_5 LT_KEY_CTRL_5
#define TB_KEY_CTRL_RSQ_BRACKET LT_KEY_CTRL_RSQ_BRACKET
#define TB_KEY_CTRL_6 LT_KEY_CTRL_6
#define TB_KEY_CTRL_7 LT_KEY_CTRL_7
#define TB_KEY_CTRL_SLASH LT_KEY_CTRL_SLASH
#define TB_KEY_CTRL_UNDERSCORE LT_KEY_CTRL_UNDERSCORE
#define TB_KEY_SPACE LT_KEY_SPACE
#define TB_KEY_BACKSPACE2 LT_KEY_BACKSPACE2
#define TB_KEY_CTRL_8 LT_KEY_CTRL_8
#define TB_KEY_F1 LT_KEY_F1
#define TB_KEY_F2 LT_KEY_F2
#define TB_KEY_F3 LT_KEY_F3
#define TB_KEY_F4 LT_KEY_F4
#define TB_KEY_F5 LT_KEY_F5
#define TB_KEY_F6 LT_KEY_F6
#define TB_KEY_F7 LT_KEY_F7
#define TB_KEY_F8 LT_KEY_F8
#define TB_KEY_F9 LT_KEY_F9
#define TB_KEY_F10 LT_KEY_F10
#define TB_KEY_F11 LT_KEY_F11
#define TB_KEY_F12 LT_KEY_F12
#define TB_KEY_INSERT LT_KEY_INSERT
#define TB_KEY_DELETE LT_KEY_DELETE
#define TB_KEY_HOME LT_KEY_HOME
#define TB_KEY_END LT_KEY_END
#define TB_KEY_PGUP LT_KEY_PGUP
#define TB_KEY_PGDN LT_KEY_PGDN
#define TB_KEY_ARROW_UP LT_KEY_ARROW_UP
#define TB_KEY_ARROW_DOWN LT_KEY_ARROW_DOWN
#define TB_KEY_ARROW_LEFT LT_KEY_ARROW_LEFT
#define TB_KEY_ARROW_RIGHT LT_KEY_ARROW_RIGHT
#define TB_KEY_BACK_TAB LT_KEY_BACK_TAB
#define TB_KEY_MOUSE_LEFT LT_KEY_MOUSE_LEFT
#define TB_KEY_MOUSE_RIGHT LT_KEY_MOUSE_RIGHT
#define TB_KEY_MOUSE_MIDDLE LT_KEY_MOUSE_MIDDLE
#define TB_KEY_MOUSE_RELEASE LT_KEY_MOUSE_RELEASE
#define TB_KEY_MOUSE_WHEEL_UP LT_KEY_MOUSE_WHEEL_UP
#define TB_KEY_MOUSE_WHEEL_DOWN LT_KEY_MOUSE_WHEEL_DOWN

/* ---- functions: clean 1:1 aliases ---- */
#define tb_init lt_init
#define tb_init_fd lt_init_fd
#define tb_init_file lt_init_file
#define tb_shutdown lt_shutdown
#define tb_width lt_width
#define tb_height lt_height
#define tb_clear lt_clear
#define tb_set_clear_attrs lt_set_clear_attrs
#define tb_present lt_present
#define tb_invalidate lt_invalidate
#define tb_set_cursor lt_set_cursor
#define tb_hide_cursor lt_hide_cursor
#define tb_set_cell lt_set_cell
#define tb_set_cell_ex lt_set_cell_ex
#define tb_extend_cell lt_extend_cell
#define tb_print lt_print
#define tb_print_ex lt_print_ex
#define tb_printf lt_printf
#define tb_printf_ex lt_printf_ex
#define tb_send lt_send
#define tb_sendf lt_sendf
#define tb_peek_event lt_peek_event
#define tb_poll_event lt_poll_event
#define tb_get_fds lt_get_fds
#define tb_set_input_mode lt_set_input_mode
#define tb_set_output_mode lt_set_output_mode
#define tb_last_errno lt_last_errno
#define tb_strerror lt_strerror
#define tb_has_egc lt_has_egc
#define tb_attr_width lt_attr_width
#define tb_version lt_version
#define tb_iswprint lt_iswprint
#define tb_wcwidth lt_wcwidth
#define tb_utf8_char_length lt_utf8_char_length
#define tb_utf8_char_to_unicode lt_utf8_char_to_unicode
#define tb_utf8_unicode_to_char lt_utf8_unicode_to_char

/* ---- allocator indirection -> libc ---- */
#define tb_malloc malloc
#define tb_realloc realloc
#define tb_free free

/* ---- bucket B: adapters (libterm-backed, divergent shape) ---- */
#define tb_put_cell(x, y, c) lt_set_cell((x), (y), (c)->ch, (c)->fg, (c)->bg)

static inline int tb_get_cell(int x, int y, int back, struct tb_cell **cell) {
  /* libterm copies into a caller struct rather than exposing a live buffer
   * pointer. Snapshot into a function-local static (termbox2 is a single
   * global instance, not thread-safe, so a plain static matches its model);
   * the returned pointer is valid until the next tb_get_cell call. libterm
   * reads the back buffer only, so back==0 (front) is unsupported. */
  static struct lt_cell scratch;
  int rc;
  if (back == 0)
    return LT_ERR;
  rc = lt_get_cell(x, y, &scratch);
  if (rc == LT_OK && cell)
    *cell = &scratch;
  return rc;
}

/* ---- bucket C: unsupported. Using one is a compile error; the message is
 *      carried in the (deliberately undeclared) identifier name. ---- */
#define tb_init_rwfd(rfd, wfd)                                                 \
  LIBTERM_COMPAT_UNSUPPORTED_tb_init_rwfd__use_lt_init_fd_or_lt_init_file
#define tb_set_func(type, fn)                                                  \
  LIBTERM_COMPAT_UNSUPPORTED_tb_set_func__deprecated_upstream_no_libterm_equivalent
#define tb_has_truecolor()                                                     \
  LIBTERM_COMPAT_UNSUPPORTED_tb_has_truecolor__use_lt_detect_color_depth
#define tb_cell_buffer()                                                       \
  LIBTERM_COMPAT_UNSUPPORTED_tb_cell_buffer__no_raw_buffer_use_lt_get_cell
#define tb_key_i(i) LIBTERM_COMPAT_UNSUPPORTED_tb_key_i__no_terminfo_cap_table

#endif /* LIBTERM_COMPAT_TERMBOX2_H */

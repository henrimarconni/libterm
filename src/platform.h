/*
 * libterm - platform abstraction interface.
 *
 * Shared code in src/shared/ calls these functions. Each platform
 * directory (src/platform/posix, src/platform/windows) provides its
 * own implementation. Shared code MUST NOT use #ifdef _WIN32 — add
 * a new lt__plat_ function here instead.
 */
#ifndef LIBTERM_PLATFORM_H
#define LIBTERM_PLATFORM_H

#include "libterm/libterm.h"
#include <stddef.h>

/* ---- lifecycle ---- */
/* Initialize the platform layer against the controlling terminal (POSIX opens
 * "/dev/tty"; the platform owns and closes that fd). Equivalent to
 * lt__plat_init_file with the platform's default terminal path. */
int lt__plat_init(void);
/* Initialize the platform layer against an already-open tty fd. `owned`
 * non-zero means the platform closes the fd on shutdown; zero means the caller
 * retains ownership. */
int lt__plat_init_fd(int ttyfd, int owned);
/* Open `path` as a tty and initialize against it; the platform owns and closes
 * the fd. Returns LT_ERR_INIT_OPEN if it can't be opened. */
int lt__plat_init_file(const char *path);
/* Tear down the platform layer: leave the alternate screen, restore the
 * original terminal mode, and close the fd if the platform owns it. Safe to
 * call when not initialized. */
int lt__plat_shutdown(void);

/* ---- terminal size ---- */
/* Report the terminal's current size in cells into *w and *h. Returns LT_ERR if
 * either pointer is NULL, the query syscall fails, or the reported size is
 * non-positive. */
int lt__plat_get_size(int *w, int *h);

/* ---- output ---- */
/* Append `len` bytes to the output buffer (flushing first if they don't fit; a
 * payload larger than the whole buffer is written directly). Returns LT_OK or a
 * write error. */
int lt__plat_write(const char *buf, size_t len);
/* Write any buffered output to the terminal and empty the buffer. Returns LT_OK
 * (no-op on an empty buffer) or a write error. */
int lt__plat_flush(void);
/* Buffer a clear-screen + cursor-home sequence. Returns LT_OK or a write error.
 */
int lt__plat_clear_screen(void);
/* Buffer a cursor-move to (x, y), 0-based. Returns LT_ERR_OUT_OF_BOUNDS for a
 * negative coordinate, otherwise LT_OK. */
int lt__plat_move_cursor(int x, int y);
/* Buffer the hide- / show-cursor sequence (DECTCEM). Each returns LT_OK or a
 * write error. */
int lt__plat_hide_cursor(void);
int lt__plat_show_cursor(void);
/* Buffer one cell's glyph bytes (UTF-8, a space for an empty cell); x/y are
 * unused (the caller positions the cursor). Returns LT_OK or a write error. */
int lt__plat_render_cell(int x, int y, const struct lt_cell *cell);

/* Reserve `max` contiguous bytes in the platform output buffer (flushing first
 * if needed) and return a writable pointer, or NULL if `max` exceeds capacity
 * or a flush failed. After writing `actual` (<= max) bytes, call commit. Shared
 * SGR/run emission (src/shared/sgr.c) fills the buffer through this pair. */
char *lt__plat_reserve(size_t max);
void lt__plat_commit(size_t actual);

/* ---- input ----
 * lt__plat_read_event: blocks up to timeout_ms (-1 = wait forever, 0 =
 * nonblock) returns LT_OK on event filled, LT_ERR_NO_EVENT on timeout, negative
 * on error
 */
int lt__plat_read_event(struct lt_event *ev, int timeout_ms);

/* Negotiate the kitty keyboard protocol. enable() pushes the desired flags,
 * disable() pops. Best-effort: a terminal that doesn't support it ignores the
 * bytes. No-op on platforms without escape-sequence negotiation (Windows). */
int lt__plat_kitty_enable(void);
int lt__plat_kitty_disable(void);

/* Fill *ttyfd / *resizefd (either may be NULL) with the pollable input fds.
 * Returns LT_ERR_UNSUPPORTED_TERM on platforms that have none (Windows). */
int lt__plat_get_fds(int *ttyfd, int *resizefd);

#endif /* LIBTERM_PLATFORM_H */

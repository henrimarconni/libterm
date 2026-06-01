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
int lt__plat_init(void);
/* Initialize the platform layer against an already-open tty fd. `owned`
 * non-zero means the platform closes the fd on shutdown; zero means the caller
 * retains ownership. */
int lt__plat_init_fd(int ttyfd, int owned);
int lt__plat_shutdown(void);

/* ---- terminal size ---- */
int lt__plat_get_size(int *w, int *h);

/* ---- output ---- */
int lt__plat_write(const char *buf, size_t len);
int lt__plat_flush(void);
int lt__plat_clear_screen(void);
int lt__plat_move_cursor(int x, int y);
int lt__plat_hide_cursor(void);
int lt__plat_show_cursor(void);
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

/* Fill *ttyfd / *resizefd (either may be NULL) with the pollable input fds.
 * Returns LT_ERR_UNSUPPORTED_TERM on platforms that have none (Windows). */
int lt__plat_get_fds(int *ttyfd, int *resizefd);

#endif /* LIBTERM_PLATFORM_H */

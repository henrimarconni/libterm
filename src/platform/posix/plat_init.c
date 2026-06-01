#define _DEFAULT_SOURCE
#include <termios.h>

#include "internal.h"
#include "libterm/libterm.h"
#include "platform.h"
#include "posix_internal.h"
#include "posix_resize.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifndef LIBTERM_BENCH_WIDTH
#define LIBTERM_BENCH_WIDTH 40
#endif

#ifndef LIBTERM_BENCH_HEIGHT
#define LIBTERM_BENCH_HEIGHT 12
#endif

static int lt__posix_tty_fd = -1;
static struct termios lt__posix_orig_tios;
static int lt__posix_has_orig_tios = 0;
static int lt__posix_tty_fd_owned = 0;

int lt__posix_get_tty_fd(void) { return lt__posix_tty_fd; }

int lt__plat_init_fd(int ttyfd, int owned) {
#if defined(LIBTERM_BENCH_HEADLESS_OUTPUT)
  (void)ttyfd;
  (void)owned;
  return LT_OK;
#endif

  if (lt__posix_tty_fd >= 0)
    return LT_ERR_INIT_ALREADY;

  if (ttyfd < 0 || !isatty(ttyfd)) {
    /* isatty sets ENOTTY for a non-terminal fd; surface it. For ttyfd < 0
     * there was no syscall, so report EBADF as the closest cause. */
    lt__g.last_errno = (ttyfd < 0) ? EBADF : errno;
    return LT_ERR_INIT_OPEN;
  }

  lt__posix_tty_fd = ttyfd;
  lt__posix_tty_fd_owned = owned;

  int rc = LT_ERR_INIT_OPEN;

  /* orig_tios not captured yet -> nothing to restore (goto fail, not fail_restore) */
  if (tcgetattr(lt__posix_tty_fd, &lt__posix_orig_tios) != 0) {
    lt__g.last_errno = errno;
    goto fail;
  }

  lt__posix_has_orig_tios = 1;

  struct termios raw = lt__posix_orig_tios;
  cfmakeraw(&raw);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;
  /* raw mode failed to apply -> terminal still in orig mode, do not restore */
  if (tcsetattr(lt__posix_tty_fd, TCSAFLUSH, &raw) != 0) {
    lt__g.last_errno = errno;
    goto fail;
  }

  rc = lt__posix_resize_init();
  if (rc != LT_OK)
    goto fail_restore;

  static const char enter_alt[] = "\x1b[?1049h";
  if (lt__plat_write(enter_alt, sizeof(enter_alt) - 1) != LT_OK) {
    rc = LT_ERR_INIT_OPEN;
    goto fail_restore;
  }

  if (lt__plat_flush() != LT_OK) {
    rc = LT_ERR_INIT_OPEN;
    goto fail_restore;
  }

  return LT_OK;

fail_restore:
  (void)tcsetattr(lt__posix_tty_fd, TCSAFLUSH, &lt__posix_orig_tios);
fail:
  if (owned)
    close(lt__posix_tty_fd);
  lt__posix_tty_fd = -1;
  lt__posix_has_orig_tios = 0;
  return rc;
}

int lt__plat_init(void) {
#if defined(LIBTERM_BENCH_HEADLESS_OUTPUT)
  return LT_OK;
#endif

  if (lt__posix_tty_fd >= 0)
    return LT_ERR_INIT_ALREADY;

  int fd = open("/dev/tty", O_RDWR);
  if (fd < 0) {
    lt__g.last_errno = errno;
    return LT_ERR_INIT_OPEN;
  }

  return lt__plat_init_fd(fd, 1);
}

int lt__plat_shutdown(void) {
#if defined(LIBTERM_BENCH_HEADLESS_OUTPUT)
  return LT_OK;
#endif

  if (lt__posix_tty_fd >= 0) {
    (void)lt__plat_show_cursor();
    /* Unconditionally disable mouse reporting; harmless if it was never on,
     * and prevents the shell inheriting mouse reports after we exit. */
    static const char disable_mouse[] = "\x1b[?1006l\x1b[?1000l";
    (void)lt__plat_write(disable_mouse, sizeof(disable_mouse) - 1);
    static const char leave_alt[] = "\x1b[?1049l";
    (void)lt__plat_write(leave_alt, sizeof(leave_alt) - 1);
    (void)lt__plat_flush();
    (void)lt__posix_resize_shutdown();

    if (lt__posix_has_orig_tios) {
      (void)tcsetattr(lt__posix_tty_fd, TCSAFLUSH, &lt__posix_orig_tios);
    }

    if (lt__posix_tty_fd_owned)
      close(lt__posix_tty_fd);
  }

  lt__posix_tty_fd = -1;
  lt__posix_tty_fd_owned = 0;
  lt__posix_has_orig_tios = 0;
  return LT_OK;
}

int lt__plat_get_size(int *w, int *h) {
  if (!w || !h)
    return LT_ERR;

#if defined(LIBTERM_BENCH_HEADLESS_OUTPUT)
  *w = LIBTERM_BENCH_WIDTH;
  *h = LIBTERM_BENCH_HEIGHT;
  return LT_OK;
#endif

  if (lt__posix_tty_fd < 0)
    return LT_ERR;

  struct winsize ws;
  if (ioctl(lt__posix_tty_fd, TIOCGWINSZ, &ws) != 0)
    return LT_ERR;

  if (ws.ws_col <= 0 || ws.ws_row <= 0)
    return LT_ERR;

  *w = (int)ws.ws_col;
  *h = (int)ws.ws_row;
  return LT_OK;
}

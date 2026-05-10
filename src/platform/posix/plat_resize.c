#define _DEFAULT_SOURCE
#include "libterm/libterm.h"
#include "posix_resize.h"
#include <signal.h>

#include <fcntl.h>
#include <unistd.h>

static int lt__posix_resize_pipe[2] = {-1, -1};

static void lt__posix_sigwinch_handler(int signo) {
  (void)signo;

  if (lt__posix_resize_pipe[1] < 0)
    return;

  unsigned char b = 1;
  ssize_t n = write(lt__posix_resize_pipe[1], &b, 1);
  (void)n;
}

static int lt__posix_resize_init_cleanup(void) {
  close(lt__posix_resize_pipe[0]);
  close(lt__posix_resize_pipe[1]);
  lt__posix_resize_pipe[0] = -1;
  lt__posix_resize_pipe[1] = -1;
  return LT_ERR_INIT_OPEN;
}

int lt__posix_resize_init(void) {
  if (lt__posix_resize_pipe[0] >= 0 || lt__posix_resize_pipe[1] >= 0)
    return LT_OK;

  if (pipe(lt__posix_resize_pipe) != 0)
    return LT_ERR_INIT_OPEN;

  int flags = fcntl(lt__posix_resize_pipe[0], F_GETFL, 0);
  if (flags < 0)
    return lt__posix_resize_init_cleanup();

  if (fcntl(lt__posix_resize_pipe[0], F_SETFL, flags | O_NONBLOCK) != 0)
    return lt__posix_resize_init_cleanup();

  flags = fcntl(lt__posix_resize_pipe[1], F_GETFL, 0);
  if (flags < 0)
    return lt__posix_resize_init_cleanup();

  if (fcntl(lt__posix_resize_pipe[1], F_SETFL, flags | O_NONBLOCK) != 0)
    return lt__posix_resize_init_cleanup();

  struct sigaction sa = {0};
  sa.sa_handler = lt__posix_sigwinch_handler;

  if (sigaction(SIGWINCH, &sa, NULL) != 0)
    return lt__posix_resize_init_cleanup();

  return LT_OK;
}

int lt__posix_resize_shutdown(void) {
  if (lt__posix_resize_pipe[0] >= 0)
    close(lt__posix_resize_pipe[0]);
  if (lt__posix_resize_pipe[1] >= 0)
    close(lt__posix_resize_pipe[1]);

  lt__posix_resize_pipe[0] = -1;
  lt__posix_resize_pipe[1] = -1;

  struct sigaction sa = {0};
  sa.sa_handler = SIG_DFL;
  (void)sigaction(SIGWINCH, &sa, NULL);
  return LT_OK;
}

int lt__posix_resize_read_fd(void) { return lt__posix_resize_pipe[0]; }

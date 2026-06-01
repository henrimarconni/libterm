/* lt_get_fds: the returned tty fd integrates into a caller's own select() loop
 * — when bytes arrive on it, lt_peek_event(ev, 0) drains the event. Driven
 * through an openpty() master. POSIX-only; returns 77 (CTest "skip") when no
 * pty is available. */
#define _DEFAULT_SOURCE
#include "libterm/libterm.h"

#include <assert.h>
#include <pty.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

int main(void) {
  int master = -1, slave = -1;
  struct winsize ws;
  memset(&ws, 0, sizeof ws);
  ws.ws_row = 24;
  ws.ws_col = 80;

  if (openpty(&master, &slave, NULL, NULL, &ws) != 0)
    return 77; /* CTest skip */

  assert(lt_init_fd(slave) == LT_OK);

  /* Both fds come back, distinct and valid (>= 0). The tty fd is the one we
   * inited against; resizefd is the SIGWINCH self-pipe read end. */
  int ttyfd = -1, resizefd = -1;
  assert(lt_get_fds(&ttyfd, &resizefd) == LT_OK);
  assert(ttyfd == slave);
  assert(resizefd >= 0);
  assert(resizefd != ttyfd);

  /* Either pointer may be NULL. */
  assert(lt_get_fds(&ttyfd, NULL) == LT_OK);
  assert(lt_get_fds(NULL, &resizefd) == LT_OK);
  assert(lt_get_fds(NULL, NULL) == LT_OK);

  /* The fd works in a real select() loop: before input, it is not readable. */
  {
    fd_set r;
    FD_ZERO(&r);
    FD_SET(ttyfd, &r);
    struct timeval tv = {0, 0};
    int n = select(ttyfd + 1, &r, NULL, NULL, &tv);
    assert(n == 0); /* nothing pending yet */
  }

  /* After the master writes, select reports the tty fd readable, and
   * lt_peek_event drains the keypress without ever blocking. */
  {
    assert(write(master, "k", 1) == 1);

    fd_set r;
    FD_ZERO(&r);
    FD_SET(ttyfd, &r);
    struct timeval tv = {2, 0}; /* generous; a regression fails the assert */
    int n = select(ttyfd + 1, &r, NULL, NULL, &tv);
    assert(n == 1);
    assert(FD_ISSET(ttyfd, &r));

    struct lt_event ev;
    memset(&ev, 0, sizeof ev);
    assert(lt_peek_event(&ev, 0) == LT_OK);
    assert(ev.type == LT_EVENT_KEY);
    assert(ev.ch == (lt_uchar)'k');
  }

  assert(lt_shutdown() == LT_OK);
  close(slave);
  close(master);
  return 0;
}

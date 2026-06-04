/* Fork a child that continuously drains a pty master so libterm's writes to
 * the slave can never block on pty capacity (macOS buffers are ~1-2 KB vs
 * Linux's 64 KB; a single full-frame lt_present exceeds them). For tests that
 * do NOT assert emitted bytes — byte-asserting tests must read the master
 * themselves.
 *
 * The child closes its inherited stdin/stdout/stderr (ctest waits on that
 * pipe) and its inherited SLAVE fd (otherwise the master never reports EOF),
 * then drains until the master EOFs/errors — which happens exactly when the
 * parent exits and the last slave fd closes. No orphan, no signals, no
 * parent-polling. */
#ifndef LT_TEST_PTY_DRAIN_H
#define LT_TEST_PTY_DRAIN_H

#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

static inline void pty_autodrain(int master, int slave) {
  pid_t pid = fork();
  if (pid != 0)
    return; /* parent (or fork failure: fall back to no drain) */
  close(0);
  close(1);
  close(2);
  close(slave);
  for (;;) {
    char buf[4096];
    ssize_t n = read(master, buf, sizeof buf);
    if (n == 0)
      _exit(0); /* EOF: last slave fd closed (parent exited) */
    if (n < 0 && errno != EINTR)
      _exit(0); /* EIO on Linux when slave side is gone, or any real error */
  }
}

#endif /* LT_TEST_PTY_DRAIN_H */

/* lt_last_errno: after an init that fails in a syscall, the underlying errno is
 * retrievable. Driven with a non-tty fd (a pipe), which lt_init_fd rejects via
 * isatty -> ENOTTY. POSIX-only; returns 77 (CTest "skip") when a pipe can't be
 * created. */
#define _DEFAULT_SOURCE
#include "libterm/libterm.h"

#include <assert.h>
#include <errno.h>
#include <unistd.h>

int main(void) {
  /* Before anything fails, last_errno is 0. */
  assert(lt_last_errno() == 0);

  int fds[2];
  if (pipe(fds) != 0)
    return 77; /* CTest skip */

  /* A pipe read-end is a valid fd but not a tty: lt_init_fd rejects it with
   * LT_ERR_INIT_OPEN, and the cause (ENOTTY, set by isatty) is retrievable. */
  int rc = lt_init_fd(fds[0]);
  assert(rc == LT_ERR_INIT_OPEN);
  assert(lt_last_errno() == ENOTTY);

  /* A negative fd never reaches a syscall; we report EBADF as the cause. */
  rc = lt_init_fd(-1);
  assert(rc == LT_ERR_INIT_OPEN);
  assert(lt_last_errno() == EBADF);

  close(fds[0]);
  close(fds[1]);
  return 0;
}

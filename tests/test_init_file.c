/* lt_init_file: open a terminal from a path and init against it. The happy
 * path uses the slave device path of an openpty() pair (a real tty), so the
 * fd libterm opens is genuinely a terminal. Also covers the error paths.
 * POSIX-only; returns 77 (CTest "skip") when no pty is available. */
#define _DEFAULT_SOURCE
#include "libterm/libterm.h"

#include <assert.h>
#include <errno.h>
#if defined(__APPLE__)
#include <util.h> /* openpty lives here on macOS (no <pty.h>, no libutil) */
#else
#include <pty.h>
#endif
#include <string.h>
#include <unistd.h>

int main(void) {
  /* NULL path is a hard arg error, regardless of platform. */
  assert(lt_init_file(NULL) == LT_ERR);

  /* A non-tty path: opens fine but isatty() fails -> LT_ERR_INIT_OPEN, and the
   * cause (ENOTTY) is retrievable. /dev/null is a device but not a terminal. */
  int rc = lt_init_file("/dev/null");
  assert(rc == LT_ERR_INIT_OPEN);
  assert(lt_last_errno() == ENOTTY);

  /* A path that can't be opened at all -> LT_ERR_INIT_OPEN with ENOENT. */
  rc = lt_init_file("/nonexistent/libterm/tty");
  assert(rc == LT_ERR_INIT_OPEN);
  assert(lt_last_errno() == ENOENT);

  /* Happy path: get a real tty device path from an openpty slave, then init
   * libterm by opening that path itself. */
  int master = -1, slave = -1;
  struct winsize ws;
  memset(&ws, 0, sizeof ws);
  ws.ws_row = 24;
  ws.ws_col = 80;
  if (openpty(&master, &slave, NULL, NULL, &ws) != 0)
    return 77; /* CTest skip */

  char name[256];
  if (ttyname_r(slave, name, sizeof name) != 0) {
    close(master);
    close(slave);
    return 77; /* can't resolve the device path; skip */
  }
  /* We don't need our own fd; lt_init_file opens the path itself (and owns it).
   */
  close(slave);

  assert(lt_init_file(name) == LT_OK);
  assert(lt_width() == 80);
  assert(lt_height() == 24);
  assert(lt_clear() == LT_OK);
  assert(lt_set_cell(0, 0, 'F', LT_DEFAULT, LT_DEFAULT) == LT_OK);
  assert(lt_present() == LT_OK);
  /* lt_shutdown closes the fd libterm opened (owned=1). */
  assert(lt_shutdown() == LT_OK);

  /* After shutdown we can init again from the same path (fd was released). */
  assert(lt_init_file(name) == LT_OK);
  assert(lt_shutdown() == LT_OK);

  close(master);
  return 0;
}

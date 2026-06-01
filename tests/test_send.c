/* lt_send / lt_sendf: raw bytes emitted to the tty, captured through an
 * openpty() master and asserted verbatim. POSIX-only; returns 77 (CTest
 * "skip") when no pty is available. */
#define _DEFAULT_SOURCE
#include "libterm/libterm.h"

#include <assert.h>
#include <fcntl.h>
#include <pty.h>
#include <string.h>
#include <unistd.h>

static char g_buf[65536];
static size_t g_len = 0;

static void drain(int master) {
  for (;;) {
    char tmp[4096];
    ssize_t n = read(master, tmp, sizeof tmp);
    if (n <= 0)
      break;
    if (g_len + (size_t)n > sizeof g_buf)
      n = (ssize_t)(sizeof g_buf - g_len);
    if (n <= 0)
      break;
    memcpy(g_buf + g_len, tmp, (size_t)n);
    g_len += (size_t)n;
  }
}

static int contains(const char *needle) {
  size_t nl = strlen(needle);
  if (nl == 0 || g_len < nl)
    return 0;
  for (size_t i = 0; i + nl <= g_len; i++)
    if (memcmp(g_buf + i, needle, nl) == 0)
      return 1;
  return 0;
}

int main(void) {
  int master = -1, slave = -1;
  struct winsize ws;
  memset(&ws, 0, sizeof ws);
  ws.ws_row = 24;
  ws.ws_col = 80;

  if (openpty(&master, &slave, NULL, NULL, &ws) != 0)
    return 77; /* CTest skip */

  int fl = fcntl(master, F_GETFL, 0);
  fcntl(master, F_SETFL, fl | O_NONBLOCK);

  assert(lt_init_fd(slave) == LT_OK);
  drain(master); /* discard the init/alt-screen bytes */

  /* lt_send emits the exact bytes given (here an OSC set-window-title). */
  g_len = 0;
  {
    const char title[] = "\x1b]0;hello\x07";
    assert(lt_send(title, sizeof(title) - 1) == LT_OK);
    drain(master);
    assert(contains("\x1b]0;hello\x07"));
  }

  /* Embedded NULs are sent verbatim (length-counted, not C-string). */
  g_len = 0;
  {
    const char raw[] = {'A', '\0', 'B'};
    assert(lt_send(raw, sizeof(raw)) == LT_OK);
    drain(master);
    assert(g_len == 3);
    assert(g_buf[0] == 'A' && g_buf[1] == '\0' && g_buf[2] == 'B');
  }

  /* Zero length is a no-op success; nothing is written. */
  g_len = 0;
  assert(lt_send("ignored", 0) == LT_OK);
  drain(master);
  assert(g_len == 0);

  /* lt_sendf formats then sends. */
  g_len = 0;
  assert(lt_sendf("\x1b[%d;%dH", 5, 12) == LT_OK);
  drain(master);
  assert(contains("\x1b[5;12H"));

  /* Error cases. */
  assert(lt_send(NULL, 4) == LT_ERR);
  assert(lt_sendf(NULL) == LT_ERR);

  assert(lt_shutdown() == LT_OK);
  close(slave);
  close(master);
  return 0;
}

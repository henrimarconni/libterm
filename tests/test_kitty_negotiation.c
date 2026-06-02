#define _GNU_SOURCE
#include "libterm/libterm.h"
#include <assert.h>
#include <fcntl.h>
#include <pty.h>
#include <string.h>
#include <unistd.h>

/* Drain available bytes from fd into buf (non-blocking). Returns total read. */
static size_t drain(int fd, char *buf, size_t cap) {
  size_t total = 0;
  int fl = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, fl | O_NONBLOCK);
  for (;;) {
    if (total >= cap)
      break;
    ssize_t n = read(fd, buf + total, cap - total);
    if (n <= 0)
      break;
    total += (size_t)n;
  }
  return total;
}

int main(void) {
  /* A non-zero winsize is required: lt_init queries the size and fails on a
   * 0x0 pty, which would otherwise make this test self-skip. */
  struct winsize ws;
  memset(&ws, 0, sizeof ws);
  ws.ws_row = 24;
  ws.ws_col = 80;

  int master, slave;
  if (openpty(&master, &slave, NULL, NULL, &ws) != 0)
    return 77; /* skip: no pty */

  char buf[4096];

  /* Modern (default): init must emit the kitty push "ESC [ > 11 u". */
  if (lt_init_fd(slave) != LT_OK)
    return 77;
  size_t n = drain(master, buf, sizeof(buf));
  buf[n < sizeof(buf) ? n : sizeof(buf) - 1] = '\0';
  assert(memmem(buf, n, "\x1b[>11u", 6) != NULL);

  /* Shutdown must emit the pop "ESC [ < u". */
  lt_shutdown();
  n = drain(master, buf, sizeof(buf));
  assert(memmem(buf, n, "\x1b[<u", 4) != NULL);

  /* Compat mode: no kitty push at all. */
  int master2, slave2;
  if (openpty(&master2, &slave2, NULL, NULL, &ws) != 0)
    return 77;
  if (lt_init_fd(slave2) != LT_OK)
    return 77;
  /* init already negotiated modern; switch to compat then re-check a fresh
   * init is cleaner: drain, switch to compat, ensure a pop happened and no new
   * push appears afterwards. */
  (void)drain(master2, buf, sizeof(buf));
  lt_set_input_mode(LT_INPUT_COMPAT);
  n = drain(master2, buf, sizeof(buf));
  assert(memmem(buf, n, "\x1b[<u", 4) != NULL);   /* popped */
  assert(memmem(buf, n, "\x1b[>11u", 6) == NULL); /* no new push */
  lt_shutdown();

  close(master);
  close(master2);
  return 0;
}

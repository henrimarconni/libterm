/* Color-query round-trip test through a pty pair (pattern of
 * test_kitty_negotiation.c). The test pre-loads the terminal's reply on the
 * master side — the pty buffers it — then calls lt_query_color, then drains
 * the master and asserts the exact query bytes were emitted. Also covers:
 * BEL- and ST-terminated replies, palette-index queries, the
 * lt_is_dark_background threshold both ways, preservation of input typed
 * during the query window (plain chars AND an escape sequence), and the
 * no-reply timeout. */
#define _GNU_SOURCE
#include "libterm/libterm.h"
#include <assert.h>
#include <fcntl.h>
#if defined(__APPLE__)
#include <util.h> /* openpty lives here on macOS (no <pty.h>, no libutil) */
#else
#include <pty.h>
#endif
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

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

static void preload(int master, const char *bytes, size_t n) {
  assert(write(master, bytes, n) == (ssize_t)n);
}

int main(void) {
  struct winsize ws;
  memset(&ws, 0, sizeof ws);
  ws.ws_row = 24;
  ws.ws_col = 80;

  int master, slave;
  if (openpty(&master, &slave, NULL, NULL, &ws) != 0)
    return 77; /* skip: no pty */

  if (lt_init_fd(slave) != LT_OK)
    return 77;

  char buf[4096];
  (void)drain(master, buf, sizeof(buf)); /* discard init bytes */

  /* Default bg, BEL-terminated reply. */
  {
    static const char reply[] = "\x1b]11;rgb:1c1c/1c1c/1c1c\x07";
    preload(master, reply, sizeof(reply) - 1);
    uint32_t rgb = 0;
    assert(lt_query_color(LT_COLOR_DEFAULT_BG, &rgb, 2000) == LT_OK);
    assert(rgb == 0x1C1C1C);
    size_t n = drain(master, buf, sizeof(buf));
    assert(memmem(buf, n, "\x1b]11;?\x07", 7) != NULL);
  }

  /* Default fg, ST-terminated reply. */
  {
    static const char reply[] = "\x1b]10;rgb:e0e0/e0e0/e0e0\x1b\\";
    preload(master, reply, sizeof(reply) - 1);
    uint32_t rgb = 0;
    assert(lt_query_color(LT_COLOR_DEFAULT_FG, &rgb, 2000) == LT_OK);
    assert(rgb == 0xE0E0E0);
    size_t n = drain(master, buf, sizeof(buf));
    assert(memmem(buf, n, "\x1b]10;?\x07", 7) != NULL);
  }

  /* Palette index 42. */
  {
    static const char reply[] = "\x1b]4;42;rgb:00ff/8080/ffff\x07";
    preload(master, reply, sizeof(reply) - 1);
    uint32_t rgb = 0;
    assert(lt_query_color(42, &rgb, 2000) == LT_OK);
    assert(rgb == 0x0180FF);
    size_t n = drain(master, buf, sizeof(buf));
    assert(memmem(buf, n, "\x1b]4;42;?\x07", 9) != NULL);
  }

  /* lt_is_dark_background: dark, then light. */
  {
    static const char dark[] = "\x1b]11;rgb:1c1c/1c1c/1c1c\x07";
    preload(master, dark, sizeof(dark) - 1);
    assert(lt_is_dark_background(2000) == 1);
    (void)drain(master, buf, sizeof(buf));

    static const char light[] = "\x1b]11;rgb:ffff/ffff/ffff\x07";
    preload(master, light, sizeof(light) - 1);
    assert(lt_is_dark_background(2000) == 0);
    (void)drain(master, buf, sizeof(buf));
  }

  /* Input typed during the query window — plain chars AND an arrow-key
   * escape sequence — is preserved and re-delivered, in order. */
  {
    static const char typed[] = "abc\x1b[A\x1b]11;rgb:0000/0000/0000\x07";
    preload(master, typed, sizeof(typed) - 1);
    uint32_t rgb = 0xFFFFFFFF;
    assert(lt_query_color(LT_COLOR_DEFAULT_BG, &rgb, 2000) == LT_OK);
    assert(rgb == 0x000000);

    struct lt_event ev;
    assert(lt_peek_event(&ev, 1000) == LT_OK);
    assert(ev.ch == 'a');
    assert(lt_peek_event(&ev, 1000) == LT_OK);
    assert(ev.ch == 'b');
    assert(lt_peek_event(&ev, 1000) == LT_OK);
    assert(ev.ch == 'c');
    assert(lt_peek_event(&ev, 1000) == LT_OK);
    assert(ev.key == LT_KEY_ARROW_UP);
    (void)drain(master, buf, sizeof(buf));
  }

  /* Pushback overflow: 100 bytes typed during the window exceed the 64-byte
   * ring (capacity 63); the first 63 survive, the newest are dropped —
   * bounded loss, never corruption. */
  {
    char flood[100 + 32];
    memset(flood, 'x', 100);
    static const char reply[] = "\x1b]11;rgb:0000/0000/0000\x07";
    memcpy(flood + 100, reply, sizeof(reply) - 1);
    preload(master, flood, 100 + sizeof(reply) - 1);
    uint32_t rgb = 0xFFFFFFFF;
    assert(lt_query_color(LT_COLOR_DEFAULT_BG, &rgb, 2000) == LT_OK);
    assert(rgb == 0x000000);
    struct lt_event ev;
    for (int i = 0; i < 63; i++) {
      assert(lt_peek_event(&ev, 1000) == LT_OK);
      assert(ev.ch == 'x');
    }
    assert(lt_peek_event(&ev, 0) == LT_ERR_NO_EVENT);
    (void)drain(master, buf, sizeof(buf));
  }

  /* Resize during a query is not lost: the SIGWINCH self-pipe write survives
   * the query window (which selects only the tty fd) and surfaces as
   * LT_EVENT_RESIZE on the next normal poll. */
  {
    struct winsize nws;
    memset(&nws, 0, sizeof nws);
    nws.ws_row = 40;
    nws.ws_col = 100;
    assert(ioctl(master, TIOCSWINSZ, &nws) == 0);
    raise(SIGWINCH); /* handler writes the self-pipe immediately */
    static const char reply[] = "\x1b]11;rgb:0000/0000/0000\x07";
    preload(master, reply, sizeof(reply) - 1);
    uint32_t rgb = 0;
    assert(lt_query_color(LT_COLOR_DEFAULT_BG, &rgb, 2000) == LT_OK);
    struct lt_event ev;
    assert(lt_peek_event(&ev, 1000) == LT_OK);
    assert(ev.type == LT_EVENT_RESIZE);
    assert(ev.w == 100 && ev.h == 40);
    (void)drain(master, buf, sizeof(buf));
  }

  /* No reply at all: clean timeout. */
  {
    uint32_t rgb = 0;
    assert(lt_query_color(LT_COLOR_DEFAULT_BG, &rgb, 200) == LT_ERR_NO_EVENT);
    (void)drain(master, buf, sizeof(buf));
  }

  /* Argument validation. */
  {
    uint32_t rgb = 0;
    assert(lt_query_color(256, &rgb, 100) == LT_ERR_OUT_OF_BOUNDS);
    assert(lt_query_color(-3, &rgb, 100) == LT_ERR_OUT_OF_BOUNDS);
    assert(lt_query_color(LT_COLOR_DEFAULT_BG, NULL, 100) == LT_ERR);
  }

  lt_shutdown();
  close(master);
  return 0;
}

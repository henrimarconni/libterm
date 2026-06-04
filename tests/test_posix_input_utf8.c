/* POSIX UTF-8 input-assembly test: feeds raw bytes through an openpty() master
 * and asserts lt_peek_event reconstructs codepoints, emits U+FFFD for malformed
 * sequences, and resynchronizes — a stray non-continuation byte after a bad
 * multibyte lead is delivered as its own event instead of being swallowed.
 * POSIX-only; returns 77 (CTest "skip") when no pty is available. */
#define _DEFAULT_SOURCE
#include "libterm/libterm.h"

#include <assert.h>
#if defined(__APPLE__)
#include <util.h> /* openpty lives here on macOS (no <pty.h>, no libutil) */
#else
#include <pty.h>
#endif
#include <string.h>
#include <unistd.h>

static int g_master = -1;

static void feed(const unsigned char *bytes, size_t n) {
  size_t off = 0;
  while (off < n) {
    ssize_t w = write(g_master, bytes + off, n - off);
    if (w <= 0)
      break;
    off += (size_t)w;
  }
}

/* Pull the next key event and return its codepoint. A generous timeout keeps a
 * regression from hanging the suite — a missing event fails the rc assert. */
static lt_uchar next_ch(void) {
  struct lt_event ev;
  memset(&ev, 0, sizeof ev);
  int rc = lt_peek_event(&ev, 2000);
  assert(rc == LT_OK);
  assert(ev.type == LT_EVENT_KEY);
  return ev.ch;
}

int main(void) {
  int master = -1, slave = -1;
  struct winsize ws;
  memset(&ws, 0, sizeof ws);
  ws.ws_row = 24;
  ws.ws_col = 80;

  if (openpty(&master, &slave, NULL, NULL, &ws) != 0)
    return 77; /* CTest skip */
  g_master = master;

  assert(lt_init_fd(slave) == LT_OK);

  /* Valid 2/3/4-byte sequences reconstruct the right scalar value. */
  {
    const unsigned char s[] = {0xC2, 0xA2}; /* ¢ U+00A2 */
    feed(s, sizeof s);
    assert(next_ch() == 0x00A2);
  }
  {
    const unsigned char s[] = {0xE2, 0x82, 0xAC}; /* € U+20AC */
    feed(s, sizeof s);
    assert(next_ch() == 0x20AC);
  }
  {
    const unsigned char s[] = {0xF0, 0x9F, 0x98, 0x80}; /* 😀 U+1F600 */
    feed(s, sizeof s);
    assert(next_ch() == 0x1F600);
  }

  /* Plain ASCII. */
  {
    const unsigned char s[] = {'Z'};
    feed(s, sizeof s);
    assert(next_ch() == (lt_uchar)'Z');
  }

  /* A lone continuation byte is not a valid lead -> U+FFFD. */
  {
    const unsigned char s[] = {0x80};
    feed(s, sizeof s);
    assert(next_ch() == 0xFFFD);
  }

  /* Resync: a 2-byte lead followed by a non-continuation byte. The bad lead
   * yields U+FFFD and the trailing 'A' MUST surface as its own event. */
  {
    const unsigned char s[] = {0xC3, 'A'};
    feed(s, sizeof s);
    assert(next_ch() == 0xFFFD);
    assert(next_ch() == (lt_uchar)'A');
  }

  /* Resync: a 3-byte sequence whose final byte is not a continuation. */
  {
    const unsigned char s[] = {0xE2, 0x82, 'B'};
    feed(s, sizeof s);
    assert(next_ch() == 0xFFFD);
    assert(next_ch() == (lt_uchar)'B');
  }

  assert(lt_shutdown() == LT_OK);
  close(slave);
  close(master);
  return 0;
}

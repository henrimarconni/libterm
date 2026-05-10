#include "internal.h"
#include "platform.h"
#include "posix_internal.h"
#include "posix_resize.h"
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

static size_t lt__posix_read_esc_tail(unsigned char *buf, size_t cap) {
  ssize_t n;
  size_t len = 1;
  int rc = 0;

  int ttyfd = lt__posix_get_tty_fd();
  if (ttyfd < 0)
    return len;

  while (len < cap) {
    struct timeval tv = {.tv_sec = 0, .tv_usec = 0};

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(ttyfd, &rfds);

    rc = select(ttyfd + 1, &rfds, NULL, NULL, &tv);
    if (rc <= 0)
      break;

    n = read(ttyfd, &buf[len], 1);
    if (n != 1)
      break;

    len++;
  }

  return len;
}

static int lt__posix_emit_char_event(struct lt_event *ev, unsigned char ch) {
  ev->type = LT_EVENT_KEY;
  ev->mod = 0;
  ev->key = 0;
  ev->ch = (lt_uchar)ch;
  return LT_OK;
}

int lt__plat_read_event(struct lt_event *ev, int timeout_ms) {
  if (!ev)
    return LT_ERR;

  int ttyfd = lt__posix_get_tty_fd();
  if (ttyfd < 0)
    return LT_ERR_POLL;

  int rc = 0, new_w = 0, new_h = 0, szrc = 0, rrc = 0;
  int rfd = lt__posix_resize_read_fd();
  lt_uchar ch = 0;
  ssize_t n = 0;

  while (true) {
    memset(ev, 0, sizeof(*ev));

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(ttyfd, &rfds);
    if (rfd >= 0)
      FD_SET(rfd, &rfds);

    struct timeval tv;
    struct timeval *tv_ptr = NULL;

    if (timeout_ms >= 0) {
      tv.tv_sec = timeout_ms / 1000;
      tv.tv_usec = (timeout_ms % 1000) * 1000;
      tv_ptr = &tv;
    }

    int maxfd = ttyfd;
    if (rfd > maxfd)
      maxfd = rfd;

    rc = select(maxfd + 1, &rfds, NULL, NULL, tv_ptr);
    if (rc == 0)
      return LT_ERR_NO_EVENT;

    if (rc < 0) {
      if (errno == EINTR)
        return LT_ERR_NO_EVENT;
      return LT_ERR_POLL;
    }

    if (rfd >= 0 && FD_ISSET(rfd, &rfds)) {
      unsigned char drain[64];
      while (read(rfd, drain, sizeof(drain)) > 0) {
      }

      szrc = lt__plat_get_size(&new_w, &new_h);
      if (szrc != LT_OK)
        continue;

      if (new_w <= 0 || new_h <= 0)
        continue;

      if (new_w == lt__g.width && new_h == lt__g.height)
        continue;

      rrc = lt__buffer_resize(new_w, new_h);
      if (rrc != LT_OK)
        return rrc;

      lt__g.cur_x = -1;
      lt__g.cur_y = -1;

      ev->type = LT_EVENT_RESIZE;
      ev->w = new_w;
      ev->h = new_h;
      return LT_OK;
    }

    n = read(ttyfd, &ch, 1);

    if (n > 0) {
      if (ch == '\x1b') {
        unsigned char seq[8] = {'\x1b', 0, 0, 0, 0, 0, 0, 0};
        size_t seq_len = lt__posix_read_esc_tail(seq, sizeof(seq));

        ev->type = LT_EVENT_KEY;
        ev->mod = 0;
        ev->key = 0;

        if (seq_len == 3 && seq[1] == '[') {
          switch (seq[2]) {
          case 'A':
            ev->key = LT_KEY_ARROW_UP;
            break;

          case 'B':
            ev->key = LT_KEY_ARROW_DOWN;
            break;

          case 'C':
            ev->key = LT_KEY_ARROW_RIGHT;
            break;

          case 'D':
            ev->key = LT_KEY_ARROW_LEFT;
            break;
          case 'H':
            ev->key = LT_KEY_HOME;
            break;
          case 'F':
            ev->key = LT_KEY_END;
            break;
          default:
            break;
          }
        } else if (seq_len == 3 && seq[1] == 'O') {
          switch (seq[2]) {
          case 'H':
            ev->key = LT_KEY_HOME;
            break;
          case 'F':
            ev->key = LT_KEY_END;
            break;
          case 'P':
            ev->key = LT_KEY_F1;
            break;
          case 'Q':
            ev->key = LT_KEY_F2;
            break;
          case 'R':
            ev->key = LT_KEY_F3;
            break;
          case 'S':
            ev->key = LT_KEY_F4;
            break;
          default:
            break;
          }
        } else if (seq_len >= 4 && seq[1] == '[') {
          if (seq[seq_len - 1] == '~') {
            int code = 0;
            for (size_t i = 2; i + 1 < seq_len; i++) {
              if (seq[i] < '0' || seq[i] > '9') {
                code = 0;
                break;
              }
              code = code * 10 + (seq[i] - '0');
            }

            switch (code) {
            case 2:
              ev->key = LT_KEY_INSERT;
              break;
            case 3:
              ev->key = LT_KEY_DELETE;
              break;
            case 5:
              ev->key = LT_KEY_PGUP;
              break;
            case 6:
              ev->key = LT_KEY_PGDN;
              break;
            case 15:
              ev->key = LT_KEY_F5;
              break;
            case 17:
              ev->key = LT_KEY_F6;
              break;
            case 18:
              ev->key = LT_KEY_F7;
              break;
            case 19:
              ev->key = LT_KEY_F8;
              break;
            case 20:
              ev->key = LT_KEY_F9;
              break;
            case 21:
              ev->key = LT_KEY_F10;
              break;
            case 23:
              ev->key = LT_KEY_F11;
              break;
            case 24:
              ev->key = LT_KEY_F12;
              break;
            default:
              break;
            }
          }
        }

        if (seq_len == 1 ||
            (seq_len == 2 && (seq[1] == '\n' || seq[1] == '\r')))
          ev->key = LT_KEY_ESC;

        ev->ch = (ev->key == 0 && seq_len == 1) ? (lt_uchar)seq[0] : 0;
        return LT_OK;
      }

      return lt__posix_emit_char_event(ev, ch);
    }

    if (n == 0)
      return LT_ERR_NO_EVENT;

    if (errno == EINTR)
      continue;

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      if (timeout_ms < 0)
        continue;
      return LT_ERR_NO_EVENT;
    }

    return LT_ERR_READ;
  }
  return LT_OK;
}

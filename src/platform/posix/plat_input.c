#include "internal.h"
#include "libterm/libterm.h"
#include "platform.h"
#include "posix_internal.h"
#include "posix_resize.h"
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

static size_t lt__posix_read_utf8_tail(unsigned char *buf, size_t have,
                                       size_t want) {
  size_t len = have;
  int ttyfd = lt__posix_get_tty_fd();
  if (ttyfd < 0)
    return len;

  fd_set rfds = {0};
  struct timeval tv = {0};
  int rc = 0;

  while (len < want) {
    memset(&tv, 0, sizeof(tv));

    FD_ZERO(&rfds);
    FD_SET(ttyfd, &rfds);
    rc = select(ttyfd + 1, &rfds, NULL, NULL, &tv);
    if (rc <= 0)
      break;

    ssize_t n = read(ttyfd, &buf[len], 1);
    if (n != 1)
      break;

    len++;
  }

  return len;
}

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

static unsigned int lt__posix_parse_csi_mod(int mod_code) {
  switch (mod_code) {
  case 2:
    return LT_MOD_SHIFT;
  case 3:
    return LT_MOD_ALT;
  case 4:
    return LT_MOD_SHIFT | LT_MOD_ALT;
  case 5:
    return LT_MOD_CTRL;
  case 6:
    return LT_MOD_SHIFT | LT_MOD_CTRL;
  case 7:
    return LT_MOD_ALT | LT_MOD_CTRL;
  case 8:
    return LT_MOD_SHIFT | LT_MOD_ALT | LT_MOD_CTRL;
  default:
    return 0;
  }
}

static void lt__posix_parse_csi_final_key(unsigned char final,
                                          struct lt_event *ev) {
  switch (final) {
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
}

static void lt__posix_parse_ss3_final_key(unsigned char final,
                                          struct lt_event *ev) {
  switch (final) {
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
}

static int lt__posix_parse_csi_letter_mod(const unsigned char *seq,
                                          size_t seq_len) {
  int mod_code = 0;
  bool seen_semi = false;

  for (size_t i = 2; i + 1 < seq_len; i++) {
    if (seq[i] == ';') {
      seen_semi = true;
      mod_code = 0;
      continue;
    }

    if (seq[i] < '0' || seq[i] > '9')
      return 0;

    if (seen_semi)
      mod_code = mod_code * 10 + (seq[i] - '0');
  }

  return seen_semi ? mod_code : 0;
}

static void lt__posix_parse_csi_tilde_key(const unsigned char *seq,
                                          size_t seq_len, struct lt_event *ev) {
  int code = 0, mod_code = 0;
  bool in_modifier = false;

  for (size_t i = 2; i + 1 < seq_len; i++) {
    if (seq[i] == ';') {
      in_modifier = true;
      continue;
    }

    if (seq[i] < '0' || seq[i] > '9') {
      code = 0;
      mod_code = 0;
      break;
    }

    if (!in_modifier)
      code = code * 10 + (seq[i] - '0');
    else
      mod_code = mod_code * 10 + (seq[i] - '0');
  }

  ev->mod = lt__posix_parse_csi_mod(mod_code);

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

static unsigned int lt__posix_parse_esc_seq(const unsigned char *seq,
                                            size_t seq_len,
                                            struct lt_event *ev) {

  if (seq_len == 3 && seq[1] == '[') {
    lt__posix_parse_csi_final_key(seq[2], ev);
  } else if (seq_len == 3 && seq[1] == 'O') {
    lt__posix_parse_ss3_final_key(seq[2], ev);
  } else if (seq_len >= 5 && seq[1] == '[' &&
             (seq[seq_len - 1] == 'A' || seq[seq_len - 1] == 'B' ||
              seq[seq_len - 1] == 'C' || seq[seq_len - 1] == 'D' ||
              seq[seq_len - 1] == 'H' || seq[seq_len - 1] == 'F')) {
    int mod_code = lt__posix_parse_csi_letter_mod(seq, seq_len);
    ev->mod = lt__posix_parse_csi_mod(mod_code);
    lt__posix_parse_csi_final_key(seq[seq_len - 1], ev);
  } else if (seq_len >= 4 && seq[1] == '[') {
    if (seq[seq_len - 1] == '~') {
      lt__posix_parse_csi_tilde_key(seq, seq_len, ev);
    }
  }

  if (seq_len == 1 || (seq_len == 2 && (seq[1] == '\n' || seq[1] == '\r')))
    ev->key = LT_KEY_ESC;

  ev->ch = (ev->key == 0 && seq_len == 1) ? (lt_uchar)seq[0] : 0;
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

  struct timeval tv;
  struct timeval *tv_ptr = NULL;

  while (true) {
    memset(ev, 0, sizeof(*ev));

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(ttyfd, &rfds);
    if (rfd >= 0)
      FD_SET(rfd, &rfds);

    memset(&tv, 0, sizeof(tv));
    tv_ptr = NULL;

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

        return lt__posix_parse_esc_seq(seq, seq_len, ev);
      }

      unsigned char seq8[4] = {(unsigned char)ch, 0, 0, 0};
      int need = lt__utf8_char_length((char)seq8[0]);

      if (need <= 0 || need > 4) {
        ev->type = LT_EVENT_KEY;
        ev->mod = 0;
        ev->key = 0;
        ev->ch = 0xFFFD;
        return LT_OK;
      }

      size_t have = 1;
      if (need > 1)
        have = lt__posix_read_utf8_tail(seq8, have, (size_t)need);

      lt_uchar cp = 0;
      int dec = lt__utf8_decode((const char *)seq8, have, &cp);

      ev->type = LT_EVENT_KEY;
      ev->mod = 0;
      ev->key = 0;

      if (dec == need) {
        ev->ch = cp;
      } else {
        ev->ch = 0xFFFD;
      }

      return LT_OK;
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

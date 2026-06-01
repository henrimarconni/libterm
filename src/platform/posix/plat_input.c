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

/* Inter-byte grace period while assembling a multi-byte input sequence (escape
 * sequence or UTF-8 character). A terminal can deliver the bytes fragmented
 * (SSH, a loaded host, a slow pipe); a zero timeout would see an empty fd
 * between fragments and truncate the sequence — misreporting an arrow key as a
 * bare ESC, or a multi-byte character as U+FFFD. This is also the standard
 * "lone ESC vs. start of a sequence" disambiguation window: a real Escape
 * keypress costs at most this much latency before it resolves. */
#define LT__ESC_SEQ_TIMEOUT_MS 50

/* Bytes pending re-delivery as standalone events, replayed one at a time on the
 * following reads. Two producers stash here: LT_INPUT_ESC (an unrecognized
 * ESC-prefixed combo emits LT_KEY_ESC, then the trailing byte) and the UTF-8
 * assembler (a stray non-continuation byte, instead of swallowing it). The
 * consumer, lt__posix_pending_pop, sits lower down next to where it is drained. */
static unsigned char lt__posix_pending[8];
static size_t lt__posix_pending_len = 0;
static size_t lt__posix_pending_pos = 0;

static void lt__posix_pending_push(const unsigned char *bytes, size_t n) {
  if (n > sizeof(lt__posix_pending))
    n = sizeof(lt__posix_pending);
  memcpy(lt__posix_pending, bytes, n);
  lt__posix_pending_len = n;
  lt__posix_pending_pos = 0;
}

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
    /* Same grace period as the escape-sequence reader: a multi-byte UTF-8
     * character can arrive fragmented, and a zero timeout would truncate it
     * into a spurious U+FFFD. See LT__ESC_SEQ_TIMEOUT_MS. */
    tv.tv_sec = 0;
    tv.tv_usec = LT__ESC_SEQ_TIMEOUT_MS * 1000;

    FD_ZERO(&rfds);
    FD_SET(ttyfd, &rfds);
    rc = select(ttyfd + 1, &rfds, NULL, NULL, &tv);
    if (rc < 0) {
      if (errno == EINTR)
        continue;
      break;
    }
    if (rc == 0)
      break;

    ssize_t n = read(ttyfd, &buf[len], 1);
    if (n != 1)
      break;

    /* A byte that isn't a UTF-8 continuation (10xxxxxx) does not belong to this
     * sequence. Stash it for re-delivery as its own event and stop, so the
     * partial sequence decodes to U+FFFD without swallowing the next keypress. */
    if ((buf[len] & 0xC0) != 0x80) {
      unsigned char stray = buf[len];
      lt__posix_pending_push(&stray, 1);
      break;
    }

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
    /* Reset every iteration: Linux's select() updates tv with the unslept
     * remainder, so a shared tv would shrink toward zero across bytes. */
    struct timeval tv = {.tv_sec = 0, .tv_usec = LT__ESC_SEQ_TIMEOUT_MS * 1000};

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(ttyfd, &rfds);

    rc = select(ttyfd + 1, &rfds, NULL, NULL, &tv);
    if (rc < 0) {
      if (errno == EINTR)
        continue; /* interrupted before any byte was ready; wait again */
      break;
    }
    if (rc == 0)
      break; /* no more bytes within the grace period: sequence is complete */

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

/* termbox2 model: a standalone control byte (0x00-0x1F or 0x7F) is reported as
 * a key code with ch == 0 (LT_KEY_CTRL_*, which share values with the named
 * ENTER/TAB/BACKSPACE keys), not as a character. ESC (0x1B) is intercepted by
 * the escape-sequence path before this is reached. Returns true if `b` was a
 * control byte and the event was filled. */
static bool lt__posix_ctrl_byte_event(unsigned char b, struct lt_event *ev) {
  if (b >= 0x20 && b != 0x7F)
    return false;

  ev->type = LT_EVENT_KEY;
  ev->mod = 0;
  ev->key = (uint16_t)b;
  ev->ch = 0;
  return true;
}

/* Fill `ev` from the next pending byte. Returns true while bytes remain. Alt
 * combos are ASCII in practice, so a pending byte is either a control byte
 * (-> key code) or a single character (-> ch). */
static bool lt__posix_pending_pop(struct lt_event *ev) {
  if (lt__posix_pending_pos >= lt__posix_pending_len)
    return false;

  unsigned char b = lt__posix_pending[lt__posix_pending_pos++];
  if (lt__posix_pending_pos >= lt__posix_pending_len) {
    lt__posix_pending_len = 0;
    lt__posix_pending_pos = 0;
  }

  ev->type = LT_EVENT_KEY;
  if (!lt__posix_ctrl_byte_event(b, ev))
    ev->ch = (lt_uchar)b;
  return true;
}

/* Handle an ESC-prefixed sequence that no recognized-key branch claimed — i.e.
 * Alt+<key>, which a terminal encodes as ESC then the key byte(s). Behavior is
 * governed by lt_set_input_mode:
 *   LT_INPUT_ALT: one event, the key with LT_MOD_ALT set.
 *   LT_INPUT_ESC: emit LT_KEY_ESC now; replay the trailing byte(s) as the next
 *                 event(s) (termbox2's two-event default). */
static int lt__posix_handle_alt_combo(const unsigned char *seq, size_t seq_len,
                                      struct lt_event *ev) {
  if (lt__g.input_mode == LT_INPUT_ALT) {
    unsigned char b = seq[1];
    if (!lt__posix_ctrl_byte_event(b, ev))
      ev->ch = (lt_uchar)b;
    ev->mod |= LT_MOD_ALT;
    return LT_OK;
  }

  /* LT_INPUT_ESC (default). */
  ev->type = LT_EVENT_KEY;
  ev->mod = 0;
  ev->key = LT_KEY_ESC;
  ev->ch = 0;
  if (seq_len > 1)
    lt__posix_pending_push(seq + 1, seq_len - 1);
  return LT_OK;
}

/* Parse an SGR (1006) mouse report: ESC [ < Cb ; Cx ; Cy (M|m), where Cb is the
 * button code and Cx/Cy are 1-based column/row. Fills ev (type LT_EVENT_MOUSE,
 * key = LT_KEY_MOUSE_*, x/y 0-based, mod from the Cb modifier bits) and returns
 * true on a well-formed report; returns false (leaving ev untouched) otherwise,
 * so the caller can fall through to the key paths. Mirrors termbox2's TYPE_1006
 * branch (termbox2.h:3690-3755). */
static bool lt__posix_parse_mouse_sgr(const unsigned char *seq, size_t seq_len,
                                      struct lt_event *ev) {
  /* Shortest valid report is ESC [ < 0 ; 0 ; 0 M = 9 bytes. */
  if (seq_len < 9 || seq[1] != '[' || seq[2] != '<')
    return false;

  unsigned char final = seq[seq_len - 1];
  if (final != 'M' && final != 'm')
    return false;

  int num[3] = {0, 0, 0};
  int num_i = 0;
  bool seen_digit = false;

  for (size_t i = 3; i < seq_len - 1; i++) {
    unsigned char c = seq[i];
    if (c >= '0' && c <= '9') {
      /* Cap accumulation well below INT_MAX: a malformed report with enough
       * digits would otherwise overflow (signed overflow is UB). Any value
       * past this ceiling is already meaningless for a button code or screen
       * coordinate, so saturating is the right behavior, not just safe. */
      if (num[num_i] < 1000000)
        num[num_i] = num[num_i] * 10 + (c - '0');
      seen_digit = true;
    } else if (c == ';') {
      if (!seen_digit || num_i >= 2)
        return false;
      num_i++;
      seen_digit = false;
    } else {
      return false;
    }
  }

  /* Need exactly three fields, the last one non-empty. */
  if (num_i != 2 || !seen_digit)
    return false;

  int cb = num[0];

  /* Low two bits select the button; bit 6 (64) promotes 0/1 to wheel up/down. */
  switch (cb & 3) {
  case 0:
    ev->key = (cb & 64) ? LT_KEY_MOUSE_WHEEL_UP : LT_KEY_MOUSE_LEFT;
    break;
  case 1:
    ev->key = (cb & 64) ? LT_KEY_MOUSE_WHEEL_DOWN : LT_KEY_MOUSE_MIDDLE;
    break;
  case 2:
    ev->key = LT_KEY_MOUSE_RIGHT;
    break;
  default: /* 3 */
    ev->key = LT_KEY_MOUSE_RELEASE;
    break;
  }

  /* A lowercase 'm' final is xterm's button-release marker. */
  if (final == 'm')
    ev->key = LT_KEY_MOUSE_RELEASE;

  /* Modifier bits: shift=4, alt=8, ctrl=16; motion=32 (drag). */
  ev->mod = 0;
  if (cb & 4)
    ev->mod |= LT_MOD_SHIFT;
  if (cb & 8)
    ev->mod |= LT_MOD_ALT;
  if (cb & 16)
    ev->mod |= LT_MOD_CTRL;
  if (cb & 32)
    ev->mod |= LT_MOD_MOTION;

  /* Reports are 1-based; clamp to 0 so a stray 0 coordinate can't go negative. */
  ev->x = num[1] > 0 ? num[1] - 1 : 0;
  ev->y = num[2] > 0 ? num[2] - 1 : 0;
  ev->type = LT_EVENT_MOUSE;
  return true;
}

static unsigned int lt__posix_parse_esc_seq(const unsigned char *seq,
                                            size_t seq_len,
                                            struct lt_event *ev) {

  if (lt__posix_parse_mouse_sgr(seq, seq_len, ev))
    return LT_OK;

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

    /* Replay any bytes stashed by an LT_INPUT_ESC Alt-combo before blocking. */
    if (lt__posix_pending_pop(ev))
      return LT_OK;

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
      lt__g.last_errno = errno;
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
        /* Large enough for an SGR mouse report (ESC[<Cb;Cx;Cy M, up to ~16 B
         * with 3-digit coords) as well as the shorter CSI/SS3 key sequences. */
        unsigned char seq[32] = {'\x1b'};
        size_t seq_len = lt__posix_read_esc_tail(seq, sizeof(seq));

        ev->type = LT_EVENT_KEY;
        ev->mod = 0;
        ev->key = 0;

        lt__posix_parse_esc_seq(seq, seq_len, ev);

        /* Unrecognized ESC + byte that doesn't introduce a CSI/SS3 sequence is
         * an Alt-combo; dispatch it by input mode. Recognized keys, characters,
         * and bare ESC pass through unchanged. */
        if (ev->key == 0 && ev->ch == 0 && seq_len >= 2 && seq[1] != '[' &&
            seq[1] != 'O')
          return lt__posix_handle_alt_combo(seq, seq_len, ev);

        return LT_OK;
      }

      if (lt__posix_ctrl_byte_event((unsigned char)ch, ev))
        return LT_OK;

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

    lt__g.last_errno = errno;
    return LT_ERR_READ;
  }
  return LT_OK;
}

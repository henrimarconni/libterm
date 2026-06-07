#include "internal.h"
#include "platform.h"

/* Read one event from the platform layer, dropping key releases unless the
 * caller opted in via LT_INPUT_RELEASE. The drop happens here — the single
 * choke point both platforms' events flow through — so a caller that switches
 * on key/ch without checking `action` sees each keystroke once (the decoders
 * still produce releases; only delivery is gated). On a drop the wait is
 * re-entered with the same timeout: a release is a one-shot per keystroke, so
 * the over-wait is bounded by typing rate and irrelevant for timeout 0/-1. */
static int lt__read_event_filtered(struct lt_event *event, int timeout_ms) {
  for (;;) {
    int rc = lt__plat_read_event(event, timeout_ms);
    if (rc != LT_OK)
      return rc;
    if (event->type == LT_EVENT_KEY && event->action == LT_KEY_RELEASE &&
        !(lt__g.input_mode & LT_INPUT_RELEASE))
      continue;
    return rc;
  }
}

int lt_poll_event(struct lt_event *event) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;
  return lt__read_event_filtered(event, -1);
}

int lt_peek_event(struct lt_event *event, int timeout_ms) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;
  return lt__read_event_filtered(event, timeout_ms);
}

int lt_get_fds(int *ttyfd, int *resizefd) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;
  return lt__plat_get_fds(ttyfd, resizefd);
}

int lt_set_input_mode(int mode) {
  if (mode == LT_INPUT_CURRENT)
    return lt__g.input_mode;
  lt__g.input_mode = mode;

  /* Toggle mouse reporting to match the requested mode. The platform layer owns
   * the mechanism (POSIX: SGR 1000/1006 handshake; Windows:
   * ENABLE_MOUSE_INPUT). Guarded on init because the terminal isn't open before
   * lt_init. */
  if (lt__g.initialized) {
    (void)lt__plat_set_mouse(mode & LT_INPUT_MOUSE);

    bool want_kitty = !(mode & LT_INPUT_COMPAT);
    if (want_kitty && !lt__g.kitty_active) {
      if (lt__plat_kitty_enable() == LT_OK)
        lt__g.kitty_active = true;
    } else if (!want_kitty && lt__g.kitty_active) {
      (void)lt__plat_kitty_disable();
      lt__g.kitty_active = false;
    }
    (void)lt__plat_flush();
  }

  return mode;
}

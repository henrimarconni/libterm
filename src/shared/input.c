#include "internal.h"
#include "platform.h"

int lt_poll_event(struct lt_event *event) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;
  return lt__plat_read_event(event, -1);
}

int lt_peek_event(struct lt_event *event, int timeout_ms) {
  if (!lt__g.initialized)
    return LT_ERR_NOT_INIT;
  return lt__plat_read_event(event, timeout_ms);
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

  /* Toggle SGR (1006) mouse reporting to match the requested mode. The
   * terminal only emits mouse reports after the enable handshake, so this is
   * the gate between "clicks do nothing" and "clicks arrive as \x1b[< ...".
   * Guarded on init because the fd isn't open before lt_init. */
  if (lt__g.initialized) {
    if (mode & LT_INPUT_MOUSE) {
      static const char enable[] = "\x1b[?1000h\x1b[?1006h";
      (void)lt__plat_write(enable, sizeof(enable) - 1);
    } else {
      static const char disable[] = "\x1b[?1006l\x1b[?1000l";
      (void)lt__plat_write(disable, sizeof(disable) - 1);
    }

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

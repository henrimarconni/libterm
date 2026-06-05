#include "internal.h"
#include "platform.h"
#include "win_internal.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static WCHAR lt__win_pending_high = 0;

static HANDLE lt__win_input_handle(void) {
  return GetStdHandle(STD_INPUT_HANDLE);
}

static DWORD lt__win_wait_ms_from_timeout(int timeout_ms) {
  if (timeout_ms < 0)
    return INFINITE;

  return (DWORD)timeout_ms;
}

int lt__plat_read_event(struct lt_event *ev, int timeout_ms) {
  if (!ev)
    return LT_ERR;

  HANDLE in = lt__win_input_handle();
  if (in == NULL || in == INVALID_HANDLE_VALUE)
    return LT_ERR_POLL;

  while (true) {
    DWORD wait_ms = lt__win_wait_ms_from_timeout(timeout_ms);
    DWORD wrc = WaitForSingleObject(in, wait_ms);

    if (wrc == WAIT_TIMEOUT)
      return LT_ERR_NO_EVENT;
    if (wrc != WAIT_OBJECT_0)
      return LT_ERR_POLL;

    INPUT_RECORD rec;
    DWORD nread = 0;
    if (!ReadConsoleInputW(in, &rec, 1, &nread))
      return LT_ERR_READ;
    if (nread == 0)
      return LT_ERR_NO_EVENT;

    if (rec.EventType == WINDOW_BUFFER_SIZE_EVENT) {
      lt__win_pending_high = 0;

      int new_w = 0, new_h = 0;
      int rc = lt__plat_get_size(&new_w, &new_h);
      if (rc != LT_OK)
        return rc;

      rc = lt__handle_resize(new_w, new_h, ev);
      if (rc == LT_ERR_NO_EVENT)
        continue; /* spurious/unchanged: keep reading input records */
      return rc;
    }

    if (rec.EventType == MOUSE_EVENT) {
      /* Only surface mouse events when the consumer asked for them. */
      if (!(lt__g.input_mode & LT_INPUT_MOUSE))
        continue;
      lt__win_pending_high = 0;

      SHORT vx = 0, vy = 0;
      CONSOLE_SCREEN_BUFFER_INFO csbi;
      HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
      if (out != NULL && out != INVALID_HANDLE_VALUE &&
          GetConsoleScreenBufferInfo(out, &csbi)) {
        vx = csbi.srWindow.Left;
        vy = csbi.srWindow.Top;
      }

      if (lt__win_mouse_event(&rec.Event.MouseEvent, vx, vy, ev))
        return LT_OK;
      continue;
    }

    if (rec.EventType != KEY_EVENT)
      continue;

    KEY_EVENT_RECORD kev = rec.Event.KeyEvent;
    WCHAR ch16 = kev.uChar.UnicodeChar;

    /* Surrogate assembly spans records, so it stays in the loop. A lone high
     * surrogate is latched (on key-down) and never emitted on its own. */
    if (ch16 >= 0xD800 && ch16 <= 0xDBFF) {
      if (kev.bKeyDown)
        lt__win_pending_high = ch16;
      continue;
    }

    lt_uchar cp = 0;
    if (ch16 >= 0xDC00 && ch16 <= 0xDFFF) {
      if (lt__win_pending_high != 0) {
        cp = (lt_uchar)((lt__win_pending_high - 0xD800) << 10);
        cp |= (lt_uchar)(ch16 - 0xDC00);
        cp += 0x10000;
        lt__win_pending_high = 0;
      } else {
        cp = 0xFFFD;
      }
    } else {
      if (lt__win_pending_high != 0)
        lt__win_pending_high = 0;
      cp = (lt_uchar)ch16;
    }

    if (lt__win_key_event(&kev, cp, lt__g.input_mode, ev))
      return LT_OK;
    /* Record produced no event (e.g. a key-up in compat mode); keep waiting. */
  }

  return LT_ERR_NO_EVENT;
}

int lt__plat_get_fds(int *ttyfd, int *resizefd) {
  /* Windows reads input via ReadConsoleInputW on a console handle, not a
   * pollable fd, so there is nothing a select/poll loop could wait on. */
  (void)ttyfd;
  (void)resizefd;
  return LT_ERR_UNSUPPORTED_TERM;
}

int lt__plat_set_mouse(int enable) {
  HANDLE in = lt__win_input_handle();
  if (in == NULL || in == INVALID_HANDLE_VALUE)
    return LT_OK; /* best-effort: nothing to toggle */

  DWORD mode = 0;
  if (!GetConsoleMode(in, &mode))
    return LT_OK;

  if (enable)
    mode |= ENABLE_MOUSE_INPUT;
  else
    mode &= ~(DWORD)ENABLE_MOUSE_INPUT;

  (void)SetConsoleMode(in, mode);
  return LT_OK;
}

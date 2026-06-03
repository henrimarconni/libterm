/* White-box unit tests for the pure Windows mouse-event mapper
 * (lt__win_mouse_event). Hand-built MOUSE_EVENT_RECORD structs, no console I/O.
 * Parity target: the POSIX SGR-1006 mapping (button in ev->key, 0-based coords,
 * shift/ctrl/alt/motion mods). */
#include "libterm/libterm.h"
#include "platform/windows/win_internal.h"

#include <assert.h>
#include <string.h>

static MOUSE_EVENT_RECORD mk(SHORT x, SHORT y, DWORD buttons, DWORD flags,
                             DWORD ctrl) {
  MOUSE_EVENT_RECORD m;
  memset(&m, 0, sizeof m);
  m.dwMousePosition.X = x;
  m.dwMousePosition.Y = y;
  m.dwButtonState = buttons;
  m.dwEventFlags = flags;
  m.dwControlKeyState = ctrl;
  return m;
}

int main(void) {
  struct lt_event ev;

  /* Left-button press at (5,3), viewport origin (0,0) -> 0-based (5,3). */
  MOUSE_EVENT_RECORD lpress = mk(5, 3, FROM_LEFT_1ST_BUTTON_PRESSED, 0, 0);
  assert(lt__win_mouse_event(&lpress, 0, 0, &ev) == 1);
  assert(ev.type == LT_EVENT_MOUSE);
  assert(ev.key == LT_KEY_MOUSE_LEFT);
  assert(ev.action == LT_KEY_PRESS);
  assert(ev.x == 5 && ev.y == 3);

  /* Right and middle buttons. */
  MOUSE_EVENT_RECORD rpress = mk(0, 0, RIGHTMOST_BUTTON_PRESSED, 0, 0);
  assert(lt__win_mouse_event(&rpress, 0, 0, &ev) == 1);
  assert(ev.key == LT_KEY_MOUSE_RIGHT);

  MOUSE_EVENT_RECORD mpress = mk(0, 0, FROM_LEFT_2ND_BUTTON_PRESSED, 0, 0);
  assert(lt__win_mouse_event(&mpress, 0, 0, &ev) == 1);
  assert(ev.key == LT_KEY_MOUSE_MIDDLE);

  /* All buttons up (no flags) -> release. */
  MOUSE_EVENT_RECORD rel = mk(0, 0, 0, 0, 0);
  assert(lt__win_mouse_event(&rel, 0, 0, &ev) == 1);
  assert(ev.key == LT_KEY_MOUSE_RELEASE);

  /* Wheel up (positive delta in the high word) and down (negative). */
  MOUSE_EVENT_RECORD wup = mk(0, 0, (DWORD)(120 << 16), MOUSE_WHEELED, 0);
  assert(lt__win_mouse_event(&wup, 0, 0, &ev) == 1);
  assert(ev.key == LT_KEY_MOUSE_WHEEL_UP);

  /* Build the negative high word via unsigned widening to avoid shifting a
   * negative signed value (UB / -Wshift-negative-value): (WORD)-120 == 0xFF88.
   */
  MOUSE_EVENT_RECORD wdn =
      mk(0, 0, ((DWORD)(WORD)(SHORT)-120) << 16, MOUSE_WHEELED, 0);
  assert(lt__win_mouse_event(&wdn, 0, 0, &ev) == 1);
  assert(ev.key == LT_KEY_MOUSE_WHEEL_DOWN);

  /* Drag: MOUSE_MOVED with the left button held -> motion modifier set. */
  MOUSE_EVENT_RECORD drag =
      mk(2, 2, FROM_LEFT_1ST_BUTTON_PRESSED, MOUSE_MOVED, 0);
  assert(lt__win_mouse_event(&drag, 0, 0, &ev) == 1);
  assert(ev.key == LT_KEY_MOUSE_LEFT);
  assert(ev.mod & LT_MOD_MOTION);

  /* Viewport-relative coords: pos (10,10), viewport origin (3,2) -> (7,8). */
  MOUSE_EVENT_RECORD off = mk(10, 10, FROM_LEFT_1ST_BUTTON_PRESSED, 0, 0);
  assert(lt__win_mouse_event(&off, 3, 2, &ev) == 1);
  assert(ev.x == 7 && ev.y == 8);

  /* Modifier bits from the control-key state. */
  MOUSE_EVENT_RECORD modp = mk(0, 0, FROM_LEFT_1ST_BUTTON_PRESSED, 0,
                               SHIFT_PRESSED | LEFT_CTRL_PRESSED);
  assert(lt__win_mouse_event(&modp, 0, 0, &ev) == 1);
  assert(ev.mod & LT_MOD_SHIFT);
  assert(ev.mod & LT_MOD_CTRL);

  /* Horizontal wheel is not modeled -> dropped. */
  MOUSE_EVENT_RECORD hw = mk(0, 0, (DWORD)(120 << 16), MOUSE_HWHEELED, 0);
  assert(lt__win_mouse_event(&hw, 0, 0, &ev) == 0);

  return 0;
}

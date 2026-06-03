#ifndef LIBTERM_WIN_INTERNAL_H
#define LIBTERM_WIN_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>

#include "libterm/libterm.h"

/* Map a Win32 virtual-key code to a named LT_KEY_* (arrows, F-keys, nav,
 * Enter/Tab/Backspace/Esc). Returns 0 if `vk` is not a named key. */
WORD lt__win_vk_to_lt_key(WORD vk);

/* Map a bare modifier / lock key press (VK_SHIFT/CONTROL/MENU/LWIN/RWIN/CAPITAL,
 * or their explicit L/R variants) to an LT_KEY_* bare-modifier constant. Uses
 * wVirtualScanCode (0x2A left / 0x36 right shift) and the ENHANCED_KEY bit
 * (set -> right) to disambiguate generic VK_SHIFT/VK_CONTROL/VK_MENU. Returns 0
 * if `k` is not a bare modifier key. */
uint16_t lt__win_bare_modifier_key(const KEY_EVENT_RECORD *k);

/* Map one console KEY_EVENT_RECORD to an lt_event. `cp` is the Unicode
 * codepoint already assembled by the caller (surrogate pairs combined; 0 for
 * keys with no character). `input_mode` is lt__g.input_mode. Returns 1 if an
 * event was produced, 0 if the record yields nothing (e.g. a key-up in compat
 * mode, a bare modifier in compat mode, or a record with neither a mapped key
 * nor a character). Sets ev->type, ev->key/ch, ev->mod, ev->action. */
int lt__win_key_event(const KEY_EVENT_RECORD *k, lt_uchar cp, int input_mode,
                      struct lt_event *ev);

/* Map one console MOUSE_EVENT_RECORD to an LT_EVENT_MOUSE lt_event.
 * viewport_left/top are csbi.srWindow.Left/Top so coords are emitted 0-based
 * and viewport-relative. Returns 1 if an event was produced, 0 to drop (events
 * libterm does not model: MOUSE_HWHEELED). */
int lt__win_mouse_event(const MOUSE_EVENT_RECORD *m, SHORT viewport_left,
                        SHORT viewport_top, struct lt_event *ev);

#endif // LIBTERM_WIN_INTERNAL_H

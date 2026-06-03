# libterm

A small, cross-platform terminal UI library in C11 — a [termbox2](https://github.com/termbox/termbox2)-compatible API on POSIX and native Windows (Win32 Console API).

You draw into an off-screen cell buffer and call `lt_present()`; libterm diffs it against the previous frame and emits only the bytes that changed, wrapped in a synchronized-update bracket. All identifiers use the `lt_` / `LT_` prefix.

## Features

- **termbox2-compatible API** — `tb_`/`TB_` renamed to `lt_`/`LT_`; see the [Roadmap](ROADMAP.md) for per-symbol status.
- **POSIX + Windows** — `/dev/tty` + termios raw mode, or the Win32 Console API. No `#ifdef` in shared code.
- **Double-buffered diff rendering** with a SIMD-accelerated cell scan (AVX2 / AVX-512 / NEON / SVE / RVV, scalar fallback, auto-selected).
- **Unicode** — UTF-8 decode/encode, `wcwidth` (wide & zero-width handling), and grapheme clusters (combining marks, ZWJ emoji).
- **Color** — 8-color, 256, 216-cube, grayscale, and 24-bit truecolor output modes, plus 7 attributes (bold, dim, italic, underline, blink, reverse, strike).
- **Input** — modern by default, with the same `lt_event` surface on both platforms: modifiers on *every* key (including Shift+letter), bare-modifier keys, key press/repeat/release in `ev.action`, plus mouse and resize events. On POSIX the [kitty keyboard protocol](https://sw.kovidgoyal.net/kitty/keyboard-protocol/) is negotiated, with automatic fallback to broad legacy decoding (xterm normal/application, SS3, rxvt, Linux console, vt220 tilde forms + CSI modifier suffixes) on terminals that don't support it; on Windows the same events come straight from the Win32 console API (no escape sequences), including best-effort `Ctrl`+letter disambiguation. `LT_INPUT_COMPAT` restores termbox2 control-byte semantics for drop-in code.

## Build

Requires CMake ≥ 3.32 and a C11 compiler.

```sh
cmake -B build
cmake --build build
ctest --test-dir build      # run the test suite
./build/examples/hello      # try an example
```

## Quick start

```c
#include "libterm/libterm.h"

int main(void) {
  if (lt_init() != LT_OK)
    return 1;

  lt_print(2, 1, LT_GREEN, LT_DEFAULT, "hello, libterm — press q to quit");
  lt_present();

  struct lt_event ev;
  while (lt_poll_event(&ev) == LT_OK) {
    if (ev.type == LT_EVENT_KEY && (ev.ch == 'q' || ev.key == LT_KEY_ESC))
      break;
  }

  lt_shutdown();
  return 0;
}
```

## Use it in your project

With CMake, add libterm as a subdirectory (or `FetchContent`) and link the exported target:

```cmake
add_subdirectory(libterm)
target_link_libraries(myapp PRIVATE Libterm::libterm)
```

`Libterm::libterm` resolves to the static library by default; `Libterm::static` and `Libterm::shared` are also available. The public header is `include/libterm/libterm.h`.

## Examples

Built into `build/examples/`:

| Example | Shows |
|---|---|
| `hello` | minimal init / draw / event loop |
| `print` | `lt_print` / `lt_printf`, UTF-8, wide chars, multi-line |
| `colors` | named colors and attributes |
| `truecolor` | 24-bit RGB, `LT_RGB`, color-depth detection |
| `keyboard` | key and modifier reporting |
| `mouse` | SGR mouse events |
| `kbd` | on-screen keyboard + live event inspector (keys, modifiers, `action`, bare modifiers) with an input-model indicator (kitty / legacy / Win32) |

## Documentation

Full API reference and guides live in the [project wiki](https://github.com/rizukirr/libterm/wiki).

## License

MIT — derived from termbox2 (also MIT). See the source header notices.

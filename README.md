# libterm

**Fast. Native. Fluent in modern terminal protocols.** libterm is a small C11 terminal UI library with a [termbox2](https://github.com/termbox/termbox2)-compatible API — the same code drives POSIX terminals and the native Win32 Console, with no POSIX shims and no `#ifdef` in your app.

- **Fast** — draw into an off-screen cell buffer and call `lt_present()`: libterm diffs the frame with SIMD-accelerated scans (AVX2 / AVX-512 / NEON / SVE / RVV) and emits only the bytes that changed, inside a synchronized-update bracket. Flicker-free by construction.
- **Native** — `/dev/tty` + termios raw mode on POSIX; the real Win32 Console API on Windows. First-class citizens on both, not a port.
- **Modern protocols** — kitty keyboard protocol, 24-bit truecolor, OSC color querying with light/dark detection: negotiated automatically, degrading gracefully on terminals that lack them.

All identifiers use the `lt_` / `LT_` prefix.

## Features

- **termbox2-compatible API** — `tb_`/`TB_` renamed to `lt_`/`LT_`, or drop in `compat/termbox2.h` to build existing termbox2 code unchanged ([porting guide](https://github.com/rizukirr/libterm/wiki/Porting-from-termbox2)). See the [Roadmap](ROADMAP.md) for per-symbol status.
- **POSIX + Windows** — `/dev/tty` + termios raw mode, or the Win32 Console API. No `#ifdef` in shared code.
- **Double-buffered diff rendering** with a SIMD-accelerated cell scan (AVX2 / AVX-512 / NEON / SVE / RVV, scalar fallback) — all backends for your architecture are compiled in and the best one is **selected at runtime** from CPU capabilities, so one binary is fast on capable CPUs and safe on all of them. Pin a single backend with `-DLIBTERM_SIMD=<backend>`.
- **Unicode** — UTF-8 decode/encode, `wcwidth` (wide & zero-width handling), and grapheme clusters (combining marks, ZWJ emoji).
- **Color** — 8-color, 256, 216-cube, grayscale, and 24-bit truecolor output modes, plus 7 attributes (bold, dim, italic, underline, blink, reverse, strike).
- **Color querying** — `lt_query_color` / `lt_is_dark_background`: POSIX does a real OSC 10/11/4 round-trip (timeout-bounded; input typed during the query is preserved); Windows answers natively from the console color table (`GetConsoleScreenBufferInfoEx`) — immediate and version-independent, but palette indexes above 15 return `LT_ERR_UNSUPPORTED_TERM`. Under tmux, replies describe tmux's own colors (it answers OSC 10/11 itself); a terminal that doesn't answer yields `LT_ERR_NO_EVENT` after the timeout.
- **Input** — modern by default, with the same `lt_event` surface on both platforms: modifiers on *every* key (including Shift+letter), bare-modifier keys, key press/repeat/release in `ev.action`, plus mouse and resize events. On POSIX the [kitty keyboard protocol](https://sw.kovidgoyal.net/kitty/keyboard-protocol/) is negotiated, with automatic fallback to broad legacy decoding (xterm normal/application, SS3, rxvt, Linux console, vt220 tilde forms + CSI modifier suffixes) on terminals that don't support it; on Windows the same events come straight from the Win32 console API (no escape sequences), including best-effort `Ctrl`+letter disambiguation. `LT_INPUT_COMPAT` restores termbox2 control-byte semantics for drop-in code.

## Build

Requires CMake ≥ 3.32 and a C11 compiler.

```sh
cmake -B build
cmake --build build
ctest --test-dir build      # run the test suite
./build/examples/hello      # try an example
```

### Prebuilt binaries

Every [release](https://github.com/rizukirr/libterm/releases) ships prebuilt
install trees for Linux (x86_64 / aarch64 / riscv64), Windows (x86_64,
MinGW), and macOS (arm64 / x86_64), with a `SHA256SUMS` file. The x86_64 and
Linux-arm builds select their SIMD backend at runtime from the CPU, so one
binary is fast everywhere it can be and safe everywhere else.

```sh
tar -xzf libterm-v0.1.0-linux-x86_64.tar.gz
sha256sum -c SHA256SUMS --ignore-missing
cmake -B build -DCMAKE_PREFIX_PATH=$PWD/libterm-v0.1.0-linux-x86_64   # then find_package(Libterm)
```

macOS note: the binaries are unsigned — Gatekeeper users can
`xattr -d com.apple.quarantine` the extracted files or build from source.

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

With CMake, fetch libterm directly and link the exported target:

```cmake
include(FetchContent)
FetchContent_Declare(libterm
    GIT_REPOSITORY https://github.com/rizukirr/libterm
    GIT_TAG        v0.1.0)
FetchContent_MakeAvailable(libterm)

target_link_libraries(myapp PRIVATE Libterm::libterm)
```

(or `add_subdirectory(libterm)` on a vendored checkout). In subproject builds libterm builds only the static library — examples, benchmarks, tests, the shared library, and install rules are all skipped automatically; each is opt-in via its `LIBTERM_*` option (`cmake -LH` lists them).

`Libterm::libterm` resolves to the static library by default; `Libterm::static` and `Libterm::shared` are also available. The public header is `include/libterm/libterm.h`.

### Without CMake

Installed trees (a [release](https://github.com/rizukirr/libterm/releases)
tarball or `cmake --install`) ship a pkg-config file:

```sh
export PKG_CONFIG_PATH=/path/to/prefix/lib/pkgconfig  # only if not a system prefix
cc app.c $(pkg-config --cflags --libs libterm)
```

No build system at all? libterm is plain C11 with no dependencies — the
scalar build is one compiler invocation (CI runs exactly these lines:
`tests/smoke_manual_build.sh`):

```sh
cc -std=c11 -c src/shared/*.c src/platform/posix/*.c src/intrinsics/scalar.c \
    -Iinclude -Isrc
ar rcs libterm.a *.o
```

On Windows, swap `src/platform/posix/*.c` for `src/platform/windows/*.c`.
No defines are needed (`LT_API` expands to nothing in static builds).
Runtime-dispatched SIMD is a CMake-build feature — manual builds get the
scalar backend.

## Examples

Built into `build/examples/`:

| Example | Shows |
|---|---|
| `hello` | minimal init / draw / event loop |
| `print` | `lt_print` / `lt_printf`, UTF-8, wide chars, multi-line |
| `colors` | named colors and attributes |
| `truecolor` | 24-bit RGB, `LT_RGB`, color-depth detection |
| `keyboard` | termbox2's classic on-screen keyboard demo, ported by prefix swap (+ `LT_INPUT_COMPAT`) |
| `mouse` | SGR mouse events |
| `kbd` | on-screen keyboard + live event inspector (keys, modifiers, `action`, bare modifiers) with an input-model indicator (kitty / legacy / Win32) |
| `editor` | a small text editor built against the drop-in `compat/termbox2.h` — open a file, edit, Save As, `Ctrl-S` / `Ctrl-Q` |

## Documentation

Full API reference and guides live in the [project wiki](https://github.com/rizukirr/libterm/wiki).

- **[Porting from termbox2](https://github.com/rizukirr/libterm/wiki/Porting-from-termbox2)** — the drop-in `compat/termbox2.h`, what's supported, and the one `Ctrl`+letter input-model gotcha (with both the compat and modern fixes).

## License

MIT

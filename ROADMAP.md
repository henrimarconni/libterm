# libterm — Roadmap

Tracks the full termbox2 public API, renamed from the `tb_` / `TB_` prefix to `lt_` / `LT_`, and the current implementation status of each symbol on POSIX and Windows.

A symbol counts as **working** only when it actually performs the documented effect end-to-end on a real terminal — declarations and no-op stubs do not count. POSIX and Windows are tracked independently.

Legend: `[x]` working · `[~]` partial / stubbed · `[ ]` not implemented · `[—]` not applicable

---

## Public functions

### Lifecycle

| termbox2 | libterm | POSIX | Windows | Notes |
|---|---|---|---|---|
| `tb_init` | `lt_init` | [x] | [x] | POSIX now opens `/dev/tty`, enters raw mode, enters alt-screen, and initializes SIGWINCH self-pipe. Windows enters raw mode + alt-screen (`\x1b[?1049h`) so prior scrollback is preserved across the libterm session |
| `tb_init_file` | `lt_init_file` | [ ] | [ ] | Not declared in `libterm.h` |
| `tb_init_fd` | `lt_init_fd` | [x] | [—] | Inits against a caller-supplied tty fd (`isatty`-validated; **not** closed on shutdown — caller retains ownership). Mirrors termbox2's `tb_init_fd`; `lt_init` now delegates to it via shared `lt__finish_init`. Windows: stub returns `LT_ERR_INIT_OPEN` (POSIX-only). Exercised headlessly by `tests/test_pty_lifecycle.c` |
| `tb_init_rwfd` | `lt_init_rwfd` | [ ] | [—] | Not declared |
| `tb_shutdown` | `lt_shutdown` | [x] | [x] | POSIX restores cursor visibility, leaves alt-screen, restores saved termios, tears down resize pipe/signal state, and closes tty fd. Windows leaves alt-screen (`\x1b[?1049l`) after cursor-show + flush, then restores both console modes |

### Screen geometry & rendering

| termbox2 | libterm | POSIX | Windows | Notes |
|---|---|---|---|---|
| `tb_width` | `lt_width` | [x] | [x] | Reads cached `lt__g.width` |
| `tb_height` | `lt_height` | [x] | [x] | Reads cached `lt__g.height` |
| `tb_clear` | `lt_clear` | [x] | [x] | Clears back buffer with current clear attrs |
| `tb_set_clear_attrs` | `lt_set_clear_attrs` | [x] | [x] | Returns `LT_ERR_NOT_INIT` before `lt_init` (consistent with `lt_clear`/`lt_present`/`lt_set_cell`) |
| `tb_present` | `lt_present` | [x] | [x] | Shared diff loop skips cells where `back == front` (3-field equality), caches cursor position, coalesces runs, and flushes buffered output. The run/SGR emission now lives in **shared** `src/shared/sgr.c` (`lt__render_run` → `lt__emit_sgr`), so both platforms emit the same mode-aware SGR for all five output modes — normal/256/216/grayscale/truecolor — with a `LT_HI_BLACK` sentinel separating terminal-default from real black. Output bytes asserted on POSIX via pty (`tests/test_posix_sgr_output.c`); Windows uses the identical shared code (real-terminal visual confirmation pending, no Windows-native byte test yet) |
| `tb_invalidate` | `lt_invalidate` | [ ] | [ ] | Not declared |
| `tb_set_cursor` | `lt_set_cursor` | [x] | [x] | Both platforms emit `\x1b[r;cH` with hand-rolled integer formatting (no `snprintf` in render hot path) |
| `tb_hide_cursor` | `lt_hide_cursor` | [x] | [x] | |
| *(libterm addition)* | `lt_show_cursor` | [x] | [x] | Mirror of `lt_hide_cursor` |
| `tb_set_cell` | `lt_set_cell` | [x] | [x] | Bounds-checked write into back buffer |
| `tb_set_cell_ex` | `lt_set_cell_ex` | [ ] | [ ] | Not declared (multi-codepoint EGC variant) |
| `tb_extend_cell` | `lt_extend_cell` | [ ] | [ ] | Not declared |
| `tb_get_cell` | `lt_get_cell` | [x] | [x] | Shared (`src/shared/cell.c`): copies the back-buffer cell at (x, y) into a caller `struct lt_cell` (value copy, not an internal pointer like termbox2). Bounds-checked; `LT_ERR_NOT_INIT` before init. Tested in `tests/test_get_cell.c` |

### Input

| termbox2 | libterm | POSIX | Windows | Notes |
|---|---|---|---|---|
| `tb_poll_event` | `lt_poll_event` | [~] | [x] | POSIX now handles arrows, Home/End, Insert/Delete, PgUp/PgDn, F1–F12, CSI modifier forms, SGR (1006) mouse reports (button/wheel/coords/modifiers → `LT_EVENT_MOUSE`), SIGWINCH resize events, and UTF-8 assembly with `U+FFFD` fallback; parity edge-cases across terminals still remain |
| `tb_peek_event` | `lt_peek_event` | [~] | [x] | Same caveats as `lt_poll_event` |
| `tb_get_fds` | `lt_get_fds` | [ ] | [—] | Not declared |
| `tb_set_input_mode` | `lt_set_input_mode` | [x] | [x] | `lt_init` defaults to `LT_INPUT_ESC`. POSIX honors the flag for Alt-combos (`ESC <byte>`): `LT_INPUT_ALT` → one event with `LT_MOD_ALT`; `LT_INPUT_ESC` → `LT_KEY_ESC` then the byte replayed (termbox2's two-event default). Windows always reports explicit modifier state (`MOD_ALT` from `dwControlKeyState`), i.e. always `LT_INPUT_ALT`-style — the ESC-prefix distinction is a POSIX-terminal concern. `LT_INPUT_MOUSE` now emits the SGR (`?1000h`/`?1006h`) tracking-enable handshake on POSIX (disabled again on `lt_set_input_mode` clear and on `lt_shutdown`). POSIX behavior unit-tested in `tests/test_posix_input_parse.c` |

### Output mode

| termbox2 | libterm | POSIX | Windows | Notes |
|---|---|---|---|---|
| `tb_set_output_mode` | `lt_set_output_mode` | [x] | [x] | SGR emission branches on output mode through the shared `lt__emit_sgr` (`src/shared/sgr.c`), so Windows is no longer stores-only — it consumes the mode via the same path as POSIX (Windows real-terminal confirmation pending) |

### Print / send helpers

| termbox2 | libterm | POSIX | Windows | Notes |
|---|---|---|---|---|
| `tb_print` | `lt_print` | [x] | [x] | Shared (`src/shared/cell.c`): UTF-8 string into the back buffer, advancing by `lt_wcwidth` per codepoint — wide CJK/emoji take two columns, combining marks take zero (not placed; no extend-cell yet), non-printables render as `U+FFFD`. `\n` wraps to the start column of the next row, malformed UTF-8 → `U+FFFD`, off-buffer cells clipped |
| `tb_print_ex` | `lt_print_ex` | [x] | [x] | As `lt_print`, with `out_w` = widest line's column count. Tested in `tests/test_print.c` |
| `tb_printf` | `lt_printf` | [x] | [x] | `vsnprintf` into a 1 KiB buffer (truncates if longer) then `lt_print` |
| `tb_printf_ex` | `lt_printf_ex` | [ ] | [ ] | Not declared (the `out_w` + printf variant); `lt_print_ex` + `lt_printf` cover the common cases |
| `tb_send` | `lt_send` | [x] | [x] | Shared (`src/shared/output.c`): raw bytes straight to the terminal via the platform output buffer + immediate flush, length-counted (embedded NULs allowed). For escape sequences libterm doesn't model. Tested in `tests/test_send.c` |
| `tb_sendf` | `lt_sendf` | [x] | [x] | `vsnprintf` into a 1 KiB buffer (truncates if longer) then `lt_send` |
| `tb_set_func` | `lt_set_func` | [ ] | [ ] | Custom `extract_event` / `extract_pre` / `extract_post` hook |

### UTF-8 helpers

| termbox2 | libterm | POSIX | Windows | Notes |
|---|---|---|---|---|
| `tb_utf8_char_length` | `lt_utf8_char_length` | [x] | [x] | Public wrapper exported over `lt__utf8_char_length` |
| `tb_utf8_char_to_unicode` | `lt_utf8_char_to_unicode` | [x] | [x] | Public wrapper exported over strict decode path |
| `tb_utf8_unicode_to_char` | `lt_utf8_unicode_to_char` | [x] | [x] | Public wrapper exported over `lt__utf8_encode` |

### Capability / introspection

| termbox2 | libterm | POSIX | Windows | Notes |
|---|---|---|---|---|
| `tb_last_errno` | `lt_last_errno` | [ ] | [ ] | |
| `tb_strerror` | `lt_strerror` | [x] | [x] | Implemented in `src/shared/errors.c` for **all 23** return codes (distinct messages + `"unknown error"` fallback); exhaustiveness + distinctness asserted in `tests/test_api.c` |
| `tb_has_truecolor` | `lt_has_truecolor` | [ ] | [ ] | termbox2's is a compile-time flag; superseded for mode-selection by `lt_detect_color_depth` below |
| *(libterm addition)* | `lt_detect_color_depth` | [x] | [x] | Stateless runtime query: inspects `$COLORTERM` (`truecolor`/`24bit`) then `$TERM` (`*256color*` substring) and returns the terminal's color ceiling as an `LT_OUTPUT_*` mode (truecolor/256/normal). Pure standard-C `getenv` in `src/shared/output.c` — no platform code, safe to call before `lt_init`. Byte-exact logic asserted by hermetic `setenv`-driven test `tests/test_detect_color_depth.c` (harness POSIX-only; the function itself is platform-agnostic) |
| `tb_has_egc` | `lt_has_egc` | [ ] | [ ] | |
| `tb_attr_width` | `lt_attr_width` | [ ] | [ ] | |
| `tb_version` | `lt_version` | [x] | [x] | Returns `"0.1.0"` |
| `tb_iswprint` | `lt_iswprint` | [x] | [x] | Shared; non-zero when `lt_wcwidth(ch) >= 0`. Tested in `tests/test_wcwidth.c` |
| `tb_wcwidth` | `lt_wcwidth` | [x] | [x] | Shared (`src/shared/wcwidth.c`): 0 for combining/zero-width, 2 for wide CJK/emoji, 1 otherwise, -1 for control/non-printable. Markus Kuhn reference ranges (bisected combining table + wide-range tests); not locale-dependent. Consumed by `lt_print` for correct column advance |

---

## Public macros / constants

All renamed wholesale: every `TB_*` token becomes `LT_*`. The header `include/libterm/libterm.h` covers the subset libterm uses today; rows below show what is **declared and consumed** vs. what is still missing.

### Return codes (`TB_OK`, `TB_ERR*` → `LT_OK`, `LT_ERR*`)

| termbox2 | libterm | Declared | Used in code |
|---|---|---|---|
| `TB_OK` | `LT_OK` | [x] | [x] |
| `TB_ERR` | `LT_ERR` | [x] | [x] |
| `TB_ERR_NEED_MORE` | `LT_ERR_NEED_MORE` | [x] | [x] |
| `TB_ERR_INIT_ALREADY` | `LT_ERR_INIT_ALREADY` | [x] | [x] |
| `TB_ERR_INIT_OPEN` | `LT_ERR_INIT_OPEN` | [x] | [x] |
| `TB_ERR_MEM` | `LT_ERR_MEM` | [x] | [x] |
| `TB_ERR_NO_EVENT` | `LT_ERR_NO_EVENT` | [x] | [x] |
| `TB_ERR_NO_TERM` | `LT_ERR_NO_TERM` | [x] | [ ] |
| `TB_ERR_NOT_INIT` | `LT_ERR_NOT_INIT` | [x] | [x] |
| `TB_ERR_OUT_OF_BOUNDS` | `LT_ERR_OUT_OF_BOUNDS` | [x] | [x] |
| `TB_ERR_READ` | `LT_ERR_READ` | [x] | [x] |
| `TB_ERR_RESIZE_IOCTL` | `LT_ERR_RESIZE_IOCTL` | [x] | [ ] |
| `TB_ERR_RESIZE_PIPE` | `LT_ERR_RESIZE_PIPE` | [x] | [ ] |
| `TB_ERR_RESIZE_SIGACTION` | `LT_ERR_RESIZE_SIGACTION` | [x] | [ ] |
| `TB_ERR_POLL` | `LT_ERR_POLL` | [x] | [x] |
| `TB_ERR_TCGETATTR` | `LT_ERR_TCGETATTR` | [x] | [ ] |
| `TB_ERR_TCSETATTR` | `LT_ERR_TCSETATTR` | [x] | [ ] |
| `TB_ERR_UNSUPPORTED_TERM` | `LT_ERR_UNSUPPORTED_TERM` | [x] | [ ] |
| `TB_ERR_RESIZE_WRITE` | `LT_ERR_RESIZE_WRITE` | [x] | [ ] |
| `TB_ERR_RESIZE_POLL` | `LT_ERR_RESIZE_POLL` | [x] | [ ] |
| `TB_ERR_RESIZE_READ` | `LT_ERR_RESIZE_READ` | [x] | [ ] |
| `TB_ERR_RESIZE_SSCANF` | `LT_ERR_RESIZE_SSCANF` | [x] | [ ] |
| `TB_ERR_CAP_COLLISION` | `LT_ERR_CAP_COLLISION` | [x] | [ ] |

### Event types (`TB_EVENT_*` → `LT_EVENT_*`)

| termbox2 | libterm | Status |
|---|---|---|
| `TB_EVENT_KEY` | `LT_EVENT_KEY` | declared, emitted (POSIX [~], Windows [x]) |
| `TB_EVENT_RESIZE` | `LT_EVENT_RESIZE` | declared; emitted on both platforms (Windows console resize event, POSIX SIGWINCH/self-pipe path) |
| `TB_EVENT_MOUSE` | `LT_EVENT_MOUSE` | declared; emitted on POSIX (SGR 1006 reports → button in `key`, 0-based coords in `x`/`y`, modifiers in `mod`); not emitted on Windows |

### Keys (`TB_KEY_*` → `LT_KEY_*`)

Function/named keys (`F1`–`F12`, `INSERT`, `DELETE`, `HOME`, `END`, `PGUP`, `PGDN`, `ARROW_UP/DOWN/LEFT/RIGHT`, `ENTER`, `ESC`, `SPACE`, `BACKSPACE`, `BACKSPACE2`, `TAB`) are declared. Emission status:

| Key group | POSIX | Windows |
|---|---|---|
| Arrows + ESC | [x] | [x] |
| `ENTER` / `BACKSPACE` / `TAB` / `SPACE` | [x] | [x] | ENTER/TAB/BACKSPACE now report as key codes (termbox2 control-byte model); SPACE is a printable char (`ch == 0x20`) |
| F1–F12 | [x] (common xterm CSI/SS3 sequences) | [x] |
| `INSERT` / `DELETE` / `HOME` / `END` / `PGUP` / `PGDN` | [x] | [x] |
| Ctrl+letter (`LT_KEY_CTRL_A` … termbox2 set) | [x] | [x] | A standalone control byte (`0x00-0x1F`, `0x7F`) → `ev->key` = the byte, `ch == 0`, `mod == 0`, identically on both platforms. `LT_KEY_CTRL_*` declared in `libterm.h`; POSIX byte mapping unit-tested in `tests/test_posix_input_parse.c` |

### Modifiers (`TB_MOD_*` → `LT_MOD_*`)

| termbox2 | libterm | POSIX | Windows |
|---|---|---|---|
| `TB_MOD_ALT` | `LT_MOD_ALT` | [ ] | [x] |
| `TB_MOD_CTRL` | `LT_MOD_CTRL` | [ ] | [x] |
| `TB_MOD_SHIFT` | `LT_MOD_SHIFT` | [ ] | [x] |
| `TB_MOD_MOTION` | `LT_MOD_MOTION` | [x] | [ ] |

POSIX mouse reports set `LT_MOD_SHIFT`/`LT_MOD_CTRL`/`LT_MOD_ALT`/`LT_MOD_MOTION` from the SGR button-code modifier bits (4/8/16/32); `LT_MOD_MOTION` (drag) is producible only via mouse, hence newly `[x]` on POSIX. The other three remain `[ ]` above pending full keyboard-modifier parity validation.

### Colors (`TB_DEFAULT/BLACK/RED/…` → `LT_DEFAULT/BLACK/RED/…`)

Declared: `LT_DEFAULT`, `LT_BLACK`, `LT_RED`, `LT_GREEN`, `LT_YELLOW`, `LT_BLUE`, `LT_MAGENTA`, `LT_CYAN`, `LT_WHITE`, plus `LT_RGB(r,g,b)` (24-bit pack) and the `LT_HI_BLACK` sentinel. Emitted on **both platforms** via mode-aware SGR in the shared `lt__emit_sgr` (`src/shared/sgr.c`) — named (NORMAL), 8-bit palette index (256/216/grayscale), and 24-bit RGB (TRUECOLOR), with `LT_HI_BLACK` distinguishing the terminal default from real black. Emitted bytes asserted on POSIX in `tests/test_posix_sgr_output.c`; Windows runs the same shared code (real-terminal/byte verification pending).

### Attributes (`TB_BOLD/UNDERLINE/…` → `LT_BOLD/UNDERLINE/…`)

Declared: `LT_BOLD`, `LT_UNDERLINE`, `LT_REVERSE`, `LT_ITALIC`, `LT_BLINK`, `LT_DIM`, `LT_STRIKE` (bits 24-30, above the 24-bit color field). Emitted on **both platforms** via the shared SGR path (`lt__emit_sgr`, `src/shared/sgr.c`); POSIX byte-tested, Windows confirmation pending.

### Input modes (`TB_INPUT_*` → `LT_INPUT_*`)

Declared: `LT_INPUT_CURRENT`, `LT_INPUT_ESC`, `LT_INPUT_ALT`, `LT_INPUT_MOUSE`. Consumed: `lt_init` defaults to `LT_INPUT_ESC`; the POSIX input path branches on `LT_INPUT_ESC` vs `LT_INPUT_ALT` for Alt-combos (Windows uses explicit modifier state, always `LT_INPUT_ALT`-style). `LT_INPUT_MOUSE` is consumed on POSIX: setting it emits the SGR mouse tracking-enable handshake, and the input parser turns the resulting reports into `LT_EVENT_MOUSE` events (Windows mouse pending).

### Output modes (`TB_OUTPUT_*` → `LT_OUTPUT_*`)

Declared: `LT_OUTPUT_CURRENT`, `LT_OUTPUT_NORMAL`, `LT_OUTPUT_256`, `LT_OUTPUT_216`, `LT_OUTPUT_GRAYSCALE`, `LT_OUTPUT_TRUECOLOR`. All emit correct SGR through the shared `lt__emit_sgr` (`src/shared/sgr.c`) — 24-bit truecolor and 8-bit palette indices — consumed identically on both platforms; byte-tested on POSIX in `tests/test_posix_sgr_output.c`, Windows confirmation pending.

### Function-hook ids (`TB_FUNC_*` → `LT_FUNC_*`)

Not declared. Dependent on `lt_set_func`.

### Version macros

`LT_VERSION_MAJOR` (0), `LT_VERSION_MINOR` (1), `LT_VERSION_PATCH` (0) are declared and returned by `lt_version`.

---

## Public types

| termbox2 | libterm | Status |
|---|---|---|
| `uintattr_t` | `lt_attr` (`uint32_t`) | declared and used everywhere; layout: color bits 0-23, attribute bits 24-30, `LT_HI_BLACK` bit 31 |
| *(termbox2 uses `uint32_t` directly)* | `lt_uchar` (`uint32_t`) | libterm-internal alias for codepoints |
| `struct tb_cell` | `struct lt_cell` (`ch`, `fg`, `bg`) | declared; termbox2's optional `ech`/`nech`/`cech` (EGC) fields not present |
| `struct tb_event` | `struct lt_event` (`type`, `mod`, `key`, `ch`, `w`, `h`, `x`, `y`) | declared; matches termbox2 layout |

---

## What is verified end-to-end today

A feature is listed here only if it has been observed working on a real terminal and is not blocked by a known correctness bug.

| Capability | POSIX | Windows |
|---|---|---|
| `lt_init` / `lt_shutdown` round-trip without leaking handles | [x] raw mode + alt-screen + SIGWINCH self-pipe lifecycle; auto-tested headlessly via `lt_init_fd` + `openpty` (`tests/test_pty_lifecycle.c`) | [x] saves and restores both console modes |
| Console size queried from kernel (not env) | [x] `ioctl(TIOCGWINSZ)` | [x] `csbi.srWindow`-based viewport size |
| `lt_clear` zeroes the back buffer | [x] | [x] |
| `lt_set_cell` writes a codepoint into the back buffer | [x] | [x] |
| `lt_present` actually paints the terminal | [x] buffered ANSI output | [x] cursor jump (only on discontinuity) + UTF-8 emit per changed cell, buffered `WriteFile` |
| ASCII char keys via `lt_poll_event` / `lt_peek_event` | [~] UTF-8 assembly path now active with `U+FFFD` fallback; still needs broader parity validation | [x] disambiguated via `KEY_EVENT_RECORD` |
| Named keys (F1–F12, arrows, Home/End/PgUp/PgDn, Ins/Del) | [x] common xterm CSI/SS3 coverage (terminal-dependent beyond that set) | [x] full set via `plat_keys.c` |
| Modifier bits in `ev->mod` | [~] CSI modifier suffixes mapped for POSIX escape-key families; coverage is not yet universal | [x] from `dwControlKeyState` |
| UTF-8 input round-trip (BMP + supplementary) | [x] multi-byte assembly + strict decode with `U+FFFD` fallback; malformed sequences now **resync** (a non-continuation byte after a bad lead is replayed as its own event, not swallowed). End-to-end pty test `tests/test_posix_input_utf8.c` + decode boundary tests in `tests/test_utf8.c` | [x] surrogate pairs combined before emit; latch cleared on every non-completing return path |
| UTF-8 output round-trip in render path | [x] `lt__utf8_encode` path active | [x] `lt__utf8_encode` writes 1–4 bytes |
| `LT_EVENT_RESIZE` delivered exactly once per visible-size change | [~] SIGWINCH/self-pipe enabled; semantics still under parity validation | [x] `WINDOW_BUFFER_SIZE_EVENT` filtered for spurious events |
| Diff-based `lt_present` (skip unchanged cells) | [x] shared path | [x] shared path |
| Cursor-position cache (skip jump on natural advance) | [x] shared path | [x] shared path |
| Alt-screen UX (prior scrollback preserved on exit) | [x] `\x1b[?1049h` / `\x1b[?1049l` | [x] `\x1b[?1049h` on init / `\x1b[?1049l` on shutdown |
| Hand-rolled int-to-decimal in render path (no `snprintf`) | [x] | [x] `lt__plat_move_cursor` writes digits directly |
| Bench harness (`bench/bench_present.c`) | [ ] | [x] three scenarios (no-change / one-cell / full-repaint) timed via QPC |
| SGR / color emission | [x] byte-tested via pty (`tests/test_posix_sgr_output.c`) | [x] shared `lt__emit_sgr` (`src/shared/sgr.c`); colors visually confirmed in Windows Terminal via the bench SGR workloads (`bench/bench_present.c`, real `WriteFile` output) + automated byte test (`tests/test_win_sgr_output.c`) |
| Runtime color-depth detection (`lt_detect_color_depth`) | [x] `$COLORTERM`/`$TERM` → `LT_OUTPUT_*` ceiling; hermetic `setenv` test (`tests/test_detect_color_depth.c`), wired into `examples/truecolor.c` | [x] same standard-C path (test harness POSIX-only) |
| Mouse events | [~] SGR (1006) reports parsed into `LT_EVENT_MOUSE` (button/wheel/release, 0-based coords, shift/ctrl/alt/motion mods); tracking enabled via `lt_set_input_mode(LT_INPUT_MOUSE)`, disabled on shutdown. Parser unit-tested incl. malformed/overflow rejection (`tests/test_posix_input_parse.c`); live-terminal click verification still pending | [ ] pending |

---

## Known blockers

These are the things that, if fixed, would move the largest number of `[~]` rows above to `[x]`.

1. ~~**Windows SGR/color emission.**~~ **Resolved.** SGR emission was extracted into the shared `src/shared/sgr.c` (`lt__emit_sgr` / `lt__render_run`); both platforms now run it. Colors were visually confirmed in Windows Terminal (bench SGR workloads, real `WriteFile` output) and are guarded by an automated Windows byte test (`tests/test_win_sgr_output.c`) alongside the POSIX pty test.
2. **POSIX modifier semantics are partial.** CSI modifier suffixes are now mapped for escape-key families, but behavior is not yet universal across all key paths and terminals.
3. ~~**POSIX UTF-8 input semantics need parity hardening.**~~ **Resolved.** Multi-byte assembly + strict decode with `U+FFFD` fallback now resync correctly (no byte-swallowing on malformed input), covered end-to-end by a pty test (`tests/test_posix_input_utf8.c`) and decode boundary tests. Remaining input work is general cross-terminal escape-sequence coverage (poll/peek), not UTF-8.
4. **Public API surface still trails termbox2.** `lt_init_file` / `lt_init_rwfd`, `lt_get_fds`, print/send helpers, and several introspection helpers remain undeclared. (`lt_init_fd` is now declared and implemented on POSIX.)
5. **Output-mode parity now shares one code path.** Both platforms consume `lt_set_output_mode` through the shared `lt__emit_sgr`, so the previous POSIX-only / Windows-stores-only skew is gone; remaining work is the Windows verification noted in blocker #1.

---

## Out of scope for libterm (intentional divergence from termbox2)

- Header-only distribution. libterm ships as a compiled library by design; `compat/termbox2.h` (planned) will provide the drop-in macro layer.
- POSIX shim on Windows. Windows uses `ReadConsoleInputW` and Win32 console handles directly; no cygwin/msys assumptions.
- GPU-accelerated render path. refterm informs the *escape-emission* hot path (run coalescing, glyph→bytes cache); a Direct2D/Vulkan atlas is explicitly not a goal.

---

## Definition of "complete" (v1.0)

1. Every row in the public-functions tables above is `[x]` on both platforms.
2. Every declared `LT_*` macro is consumed by the implementation (no dead constants).
3. POSIX and Windows produce identical `lt_event` sequences for the same physical key, mouse, and resize input.
4. `lt_present` is diff-based and emits zero bytes when no cell has changed.
5. UTF-8 input and output round-trip correctly for 1–4-byte sequences and grapheme clusters.
6. All four output color modes render correctly on Windows Terminal, xterm, tmux, macOS Terminal, and the Linux console. SGR emission is implemented as one shared code path (`src/shared/sgr.c`) consumed by both platforms, with POSIX byte-correctness tested (`tests/test_posix_sgr_output.c`); cross-terminal visual confirmation and an automated Windows byte test remain.
7. Sanitizer CI is green (Linux gcc + clang, Windows MSVC); fuzzer runs an hour without crashes; valgrind reports no leaks.
8. `compat/termbox2.h` drop-in layer exists and a non-trivial example (small text editor) builds against it.

# libterm architecture

One page, file-path anchored. For per-symbol status see `ROADMAP.md`; for
user-facing docs see the [wiki](https://github.com/rizukirr/libterm/wiki).

## Layering

```
include/libterm/libterm.h     public API (lt_*, LT_*)
        │
src/shared/                   platform-agnostic core — NO platform #ifdefs
  api.c        init/shutdown orchestration, lt__g global state
  buffer.c     cell buffers, resize handling (lt__handle_resize)
  output.c     lt_present diff loop, cursor semantics, lt_invalidate
  sgr.c        mode-aware SGR emission (all 5 output modes)
  keymap.c     pure input decoder (bytes → events)
  cell.c/egc.c grapheme clusters (interned cluster ids)
  colorq.c     OSC color-query parsing (lt_query_color)
        │  calls through
src/internal.h                the lt__plat_* interface + lt__g definition
        │  implemented by exactly one of
src/platform/posix/           /dev/tty, termios, SIGWINCH self-pipe, OSC round-trip
src/platform/windows/         Win32 console API + VT output
        +
src/intrinsics/               SIMD cell scans (diff/equal/fill)
  scalar.c avx2.c avx512.c neon.c sve.c rvv.c   one contract, six backends
  dispatch.c                  runtime selection (see below)
```

The discipline that keeps ports honest: `src/shared/` must never branch on
platform. If shared code needs something platform-specific, it grows a
`lt__plat_*` function instead.

## The cell model

`struct lt_cell` is a fixed **16-byte POD**: `ch` (codepoint), `fg`, `bg`
(attrs), `_reserved`. Grapheme clusters (combining marks, ZWJ emoji) do NOT
make cells variable-size: trailing codepoints are interned into a
content-deduped table (`src/shared/egc.c`) and `_reserved` holds the cluster
id. Equal id ⇔ equal cluster, so **byte-equality is a valid diff** — which
is what makes the SIMD scans correct.

## Present pipeline (`src/shared/output.c`)

dirty rows → SIMD first-differ/first-equal scans (`src/intrinsics/diff.h`)
→ run coalescing → one cursor move + SGR per run (`lt__render_run` →
`lt__emit_sgr`) → wrapped in a synchronized-update bracket (DEC 2026) → if a
user cursor is set, repark it (termbox2 parity: cursor hidden at init,
`lt_set_cursor` implies show) → flush. A resize forces the next present to
repaint every cell (the terminal's reflowed content no longer matches the
blanked buffers).

## Input pipeline

Platform layer reads raw bytes (POSIX: select on tty + SIGWINCH self-pipe;
Windows: `ReadConsoleInputW`) → the **pure shared decoder**
(`src/shared/keymap.c`): fixed sequence table (xterm/SS3/rxvt/linux-console)
+ parametric CSI (tilde keys, modifiers) + kitty CSI-u (press/repeat/release)
+ SGR mouse + OSC consumption + UTF-8 assembly. Purity is the testability
trick: the decoder is exercised byte-by-byte without a terminal.

## Runtime SIMD dispatch (`src/intrinsics/dispatch.c`)

`LIBTERM_SIMD=auto` compiles every backend the target arch supports — each
TU with its three symbols renamed via CMake defines — and a constructor
picks at startup (x86_64: cpuid scalar→AVX2→AVX-512; Linux aarch64: hwcap
NEON→SVE; Linux riscv64: scalar→RVV). One binary, fast everywhere it can
be, no illegal instructions where it can't. Explicit `LIBTERM_SIMD=<backend>`
builds a single static backend.

## Test taxonomy (`tests/`)

| Kind | Example | What it proves |
|---|---|---|
| Unit (link the lib) | `test_egc.c` | API contracts |
| White-box (#include a TU) | `test_posix_input_parse.c` | internal state machines, byte-exact parsing |
| pty byte-capture | `test_posix_sgr_output.c`, `test_cursor_visibility.c` | exact emitted escape bytes, end to end |
| Emitted-ISA assert | `assert_simd_emitted.sh` | each SIMD backend really compiled to its ISA |
| Export surface | `test_exports` | only `lt_*` symbols are public |

## Intentionally untracked

`docs/` (private design/verification pipeline artifacts) and `external/`
(reference clones: termbox2 for API parity, refterm for perf ideas) are
gitignored on purpose — don't add them to commits.

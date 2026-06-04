# Changelog

All notable changes to libterm are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project
adheres to [Semantic Versioning](https://semver.org/) (pre-1.0: minor versions
may contain breaking API changes; patch versions never do).

## [Unreleased]

### Added
- Color querying: `lt_query_color` (default fg/bg + palette 0..255, packed
  `0x00RRGGBB`) and `lt_is_dark_background` (BT.709 light/dark verdict).
  POSIX: real OSC 10/11/4 round-trip with timeout; `rgb:` and URxvt `rgba:`
  reply forms; input typed during the query window is preserved. Windows:
  native `GetConsoleScreenBufferInfoEx` color table (palette >15 returns
  `LT_ERR_UNSUPPORTED_TERM`).
- `examples/theme.c` — light/dark-themed panel demonstrating the query API and
  the assume-dark fallback; quits on `q`.
- Install/export packaging: `install(TARGETS)` + `LibtermTargets.cmake`, so
  `find_package(Libterm)` yields working `Libterm::libterm` / `Libterm::static`
  / `Libterm::shared` targets; smoke-tested by `tests/smoke_install.sh`.
- macOS: portable test subset + the pty lifecycle canary run in CI (`<util.h>`/no-libutil shims; full pty-test parity tracked in ROADMAP Known blockers).
- Native Windows CI job (`windows-mingw-native`: MinGW build + ctest on
  `windows-latest`, running the portable + Windows test suites); `LT_VERSION_*`
  header constants synced with the project version (0.2.0).

### Changed
- Key decoder consumes and discards stray OSC replies instead of emitting
  garbage key events.
- README introduction rewritten (fast / native / modern protocols).

## [0.1.0] - 2026-06-03

Retroactive baseline — no git tag exists for 0.1.0; v0.2.0 will be the first tag. Initial public surface:

- termbox2-compatible API (`lt_`/`LT_` prefix) on POSIX and native Win32
  Console; three documented intentional divergences.
- Double-buffered diff renderer with SIMD cell scans (AVX2/AVX-512/NEON/SVE/
  RVV, scalar fallback) inside synchronized-update brackets.
- Modern input model: kitty keyboard protocol negotiation with legacy
  fallback; press/repeat/release actions; bare-modifier keys; SGR mouse;
  `LT_INPUT_COMPAT` for termbox2 semantics.
- Output modes: normal/256/216/grayscale/truecolor; UTF-8, wcwidth, grapheme
  clusters.

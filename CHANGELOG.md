# Changelog

All notable changes to libterm are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project
adheres to [Semantic Versioning](https://semver.org/) (pre-1.0: minor versions
may contain breaking API changes; patch versions never do).

## [Unreleased]

### Added
- Prebuilt release binaries: every published GitHub release now automatically
  gets install-tree archives for 6 targets (linux x86_64/aarch64/riscv64,
  windows x86_64 MinGW, macos arm64/x86_64) plus a `SHA256SUMS` file, built in
  Release mode with runtime-dispatched SIMD and gated by a tag-vs-CMake-version
  guard (`.github/workflows/release.yml`).
- CI `fetchcontent-smoke` job (ubuntu / windows / macos): builds a minimal
  `FetchContent` consumer against the tree and asserts the subproject
  contract — build+link, no leaked example/test/bench targets, an
  uninstrumented library (also with `LIBTERM_BUILD_BENCH=ON`), and no
  install pollution (`tests/smoke_fetchcontent.sh`).

### Changed
- **Behavior change (termbox2 parity):** the terminal cursor is now **hidden
  by default** after `lt_init` — termbox2 has always done this, and visibility
  is now implied: `lt_set_cursor` shows the cursor (and `lt_present` keeps it
  parked at the set position across frames); `lt_hide_cursor` hides it again.
  Apps that relied on the old visible-by-default behavior must call
  `lt_set_cursor` (or `lt_show_cursor`). Explicit `lt_hide_cursor()` calls
  right after init are now redundant (and harmless).
- Subproject builds are clean by default: `LIBTERM_BUILD_SHARED`,
  `LIBTERM_BUILD_EXAMPLES`, `LIBTERM_BUILD_BENCH`, `LIBTERM_BUILD_TESTS`, and
  the new `LIBTERM_INSTALL` default to ON only when libterm is the top-level
  project. A `FetchContent` / `add_subdirectory` consumer gets exactly one
  target (the static library) and no install rules; everything stays opt-in.
  Top-level builds are unchanged.
- Bench instrumentation no longer leaks into the shipped library: benches
  link a dedicated `libterm_bench` copy carrying `LIBTERM_ENABLE_RENDER_STATS`
  and the grid dims; `libterm_static` / `libterm_shared` are always clean
  (previously a default build's library was compiled with render stats
  because `bench/` mutated `libterm_static` whenever `LIBTERM_BUILD_BENCH`
  was ON — its default). `LIBTERM_BENCH_HEADLESS` now affects only the bench
  copy, so it no longer breaks examples/tests.
- `find_package(Libterm)` version compatibility is `SameMinorVersion` while
  pre-1.0 (matching this file's semver policy — pre-1.0 minors may break
  API); switches to `SameMajorVersion` automatically at 1.0.

### Fixed
- Resize no longer leaves stale terminal content on screen: a size change now
  forces the next `lt_present` to repaint every cell (the terminal's
  scrolled/reflowed content no longer matches libterm's blanked buffers, so
  the diff previously skipped blank-equal cells — most visible after height
  shrinks). Both platform resize paths now share `lt__handle_resize`. Resize
  delivery semantics (exactly-once, burst coalescing, no-op suppression) are
  now validated end-to-end.
- The terminal cursor no longer blinks at the end of the last painted row in
  apps that never asked for a cursor (user-reported on `examples/resize`).
- `FetchContent` / `add_subdirectory` consumption was broken: the include
  interface and header install rule used `CMAKE_SOURCE_DIR`, which is the
  *consumer's* root in subproject builds, so libterm's own TUs failed with
  `libterm/libterm.h: No such file or directory`. Both now use
  `PROJECT_SOURCE_DIR` (same fix applied throughout `tests/CMakeLists.txt`).

## [0.1.0] - 2026-06-05

First release.

### Added
- termbox2-compatible API (`lt_`/`LT_` prefix) on POSIX and native Win32
  Console; three documented intentional divergences.
- Double-buffered diff renderer with SIMD cell scans (AVX2/AVX-512/NEON/SVE/
  RVV, scalar fallback) inside synchronized-update brackets.
- Modern input model: kitty keyboard protocol negotiation with legacy
  fallback; press/repeat/release actions; bare-modifier keys; SGR mouse;
  `LT_INPUT_COMPAT` for termbox2 semantics. Stray OSC replies are consumed
  by the decoder, never shredded into key events.
- Output modes: normal/256/216/grayscale/truecolor; UTF-8, wcwidth, grapheme
  clusters.
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
  / `Libterm::shared` targets; smoke-tested by `tests/smoke_install.sh` in
  default and shared-only configurations.
- Runtime SIMD dispatch: `LIBTERM_SIMD=auto` now compiles every backend the
  target architecture supports and selects at startup from CPU capabilities
  (x86_64: scalar→AVX2→AVX-512 via cpuid; aarch64: NEON→SVE via hwcap;
  riscv64: scalar→RVV via hwcap) — one binary, fast on capable CPUs, no
  illegal-instruction risk on older ones. Explicit `LIBTERM_SIMD=<backend>`
  still builds a single static backend.
- CI: Linux (4 compiler/SIMD configs), macOS (portable subset + pty lifecycle
  canary; full pty-test parity tracked in ROADMAP Known blockers), native
  Windows (`windows-mingw-native`: MinGW build + ctest), MinGW cross-build,
  install smoke, sanitizers, cross-arch emulation (aarch64 NEON/SVE,
  riscv64 RVV).

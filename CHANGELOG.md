# Changelog

All notable changes to libterm are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project
adheres to [Semantic Versioning](https://semver.org/) (pre-1.0: minor versions
may contain breaking API changes; patch versions never do).

## [Unreleased]

## [0.1.0] - 2026-06-06

First release. (Re-tagged 2026-06-06 to fold in the packaging and
consumption work that landed immediately after the original 2026-06-05 tag;
the prebuilt archives were rebuilt from the re-tagged commit.)

### Added
- Prebuilt release binaries: every published GitHub release automatically
  gets install-tree archives for 6 targets (linux x86_64/aarch64/riscv64,
  windows x86_64 MinGW, macos arm64/x86_64) plus a `SHA256SUMS` file, built in
  Release mode with runtime-dispatched SIMD and gated by a tag-vs-CMake-version
  guard (`.github/workflows/release.yml`).
- Consumable as a subproject: `FetchContent` / `add_subdirectory` builds get
  exactly one target (the static library) — `LIBTERM_BUILD_SHARED`,
  `LIBTERM_BUILD_EXAMPLES`, `LIBTERM_BUILD_BENCH`, `LIBTERM_BUILD_TESTS`, and
  `LIBTERM_INSTALL` default to ON only when libterm is the top-level project,
  and everything stays opt-in. Guarded by the CI `fetchcontent-smoke` job
  (ubuntu / windows / macos): build+link, no leaked targets, an
  uninstrumented library (also with `LIBTERM_BUILD_BENCH=ON`), no install
  pollution (`tests/smoke_fetchcontent.sh`).
- pkg-config support: installed trees ship `lib/pkgconfig/libterm.pc`,
  relocatable (`${pcfiledir}`-relative prefix) so it works wherever a release
  tarball is extracted. CI-guarded in `tests/smoke_install.sh`, including a
  moved-prefix pass. Artifacts follow Unix naming convention — `libterm.a` /
  `libterm.so` / `libterm.dll`, link flag `-lterm`. `find_package(Libterm)`
  version compatibility is `SameMinorVersion` while pre-1.0 (switches to
  `SameMajorVersion` automatically at 1.0).
- "Without CMake" README section: pkg-config consumption plus the manual
  `cc`+`ar` scalar build, the latter CI-executed verbatim by
  `tests/smoke_manual_build.sh` so the documentation cannot rot.
- termbox2 cursor parity: the terminal cursor is hidden by default after
  `lt_init`; `lt_set_cursor` shows it (and `lt_present` keeps it parked at
  the set position across frames), `lt_hide_cursor` hides it again.
- Resize correctness end-to-end: a size change forces the next `lt_present`
  to repaint every cell (no stale terminal content after shrinks); both
  platform resize paths share `lt__handle_resize`, with exactly-once
  delivery, burst coalescing, and no-op suppression validated by tests.
- Bench instrumentation is isolated: benches link a dedicated
  `libterm_bench` copy carrying `LIBTERM_ENABLE_RENDER_STATS`; the shipped
  `libterm_static` / `libterm_shared` are always clean.
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

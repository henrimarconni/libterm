# Lesson: posix special-key parsing
Mode: concept
Refs: termbox2 (external/termbox2/termbox2.h:2612, external/termbox2/termbox2.h:3449, external/termbox2/termbox2.h:3513)
Started: 2026-04-27

## Curriculum
- [x] 1. Clarify libterm event model (`type` vs `key` vs `ch`) with current POSIX limitations
- [x] 2. Add minimal ESC-sequence state handling shape in POSIX input path
- [x] 3. Parse arrow key escape sequences (`ESC [ A/B/C/D`) into `LT_KEY_ARROW_*`
- [x] 4. Keep printable-byte path intact (`ev->ch`) when sequence is not special-key input
- [x] 5. Handle bare `ESC` consistently (timeout or direct mapping policy)
- [x] 6. Add focused tests for POSIX key mapping behavior
- [x] 7. Validate via example program and inspect returned `type/key/ch`

## Notes
- Keep platform-specific behavior inside `src/platform/posix/`.
- Preserve public API semantics: `type` is category, `key/ch` carry identity.
- Prefer termbox2-compatible behavior where practical.

# Lesson: libterm-benchmark-api-value
Mode: concept
Refs: refterm (production angle), then termbox2 (compatibility angle)
Started: 2026-05-07

## Curriculum
- [x] 1. Benchmark value model: what to measure and why (`lt_present` cost per frame)
- [x] 2. CMake bench target wiring (build + run bench binary cleanly)
- [x] 3. Minimal runnable benchmark harness (single workload slot, fixed frame loop)
- [x] 4. Workload A: sparse updates (best-case diff path)
- [x] 5. Workload B: full redraw (worst-case path)
- [x] 6. Measurement hygiene (warmup, sink, timer semantics, stable output)
- [x] 7. Production-angle report line (throughput + p50/p95 frame time)
- [x] 8. Compatibility workloads map from termbox2 semantics
- [x] 9. Add one termbox2-shaped workload and compare profile
- [x] 10. Final benchmark matrix + how to use it for regressions

## Notes
- Revision 1: inserted CMake wiring before benchmark implementation (user request).
- Start with production angle from refterm-style render concerns.
- After production report layer, pivot to compatibility angle (termbox2-shaped workloads).

# Lesson: post-utf8-wrapup
Mode: concept
Refs: termbox2 parity goals + current libterm POSIX input path
Started: 2026-05-15

## Curriculum
- [x] 1. POSIX UTF-8 input assembly path added to `plat_input.c` (`lt__utf8_char_length` + tail read + `lt__utf8_decode`)
- [x] 2. Invalid/incomplete UTF-8 policy set to replacement char (`U+FFFD`)
- [x] 3. ESC parser refactor started (helper extraction for CSI/SS3 final keys)
- [x] 4. POSIX CSI modifier parsing added for `...~` and letter-final forms (`A/B/C/D/H/F`)
- [x] 5. New parser test target added: `tests/test_posix_input_parse.c`
- [x] 6. `ROADMAP.md` synced to current POSIX input status
- [ ] 7. Broaden parser tests (negative cases + more modifier combinations)
- [ ] 8. Decide next parity block (input-mode semantics vs color/SGR path)

## Notes
- `examples/keyboard.c` debug output is event-based by design (`key/ch/mod`), not direct character echo.
- stderr logging while alt-screen is active can look visually offset/messy; expected during debug-mode example use.
- Current working tree includes in-progress changes in POSIX input and roadmap; stabilize before handoff commit planning.

# Lesson: refterm-optimization-study
Mode: concept
Refs: refterm (`external/refterm/README.md`, `external/refterm/faq.md`) + current `lt_present` path
Started: 2026-05-15

## Curriculum
- [x] 1. Audit existing `bench/bench_present.c` and decide what is real-terminal vs headless measurement
- [x] 2. Add a headless/mock-output seam for benchmark builds without changing public `lt_*` behavior
- [x] 3. Make benchmark workload selection explicit (`baseline`, `sparse`, `full`, `box_redraw`, `all`)
- [x] 4. Report stable timing metrics (`us/frame`, `fps`, `p50`, `p95`) without bogus pass/fail logic
- [x] 5. Add render-shape counters if timing alone cannot explain the bottleneck
- [ ] 6. Use the benchmark evidence to choose the first `lt_present` optimization

## Notes
- Refterm lesson to borrow: large writes, simple data flow, avoid repeated expensive work; do not copy refterm architecture.
- The project already has `bench/CMakeLists.txt` and `bench/bench_present.c`; start by fixing/evolving those, not creating a second benchmark.
- Prefer headless/mock-output first so libterm render-path cost is separated from terminal emulator cost.

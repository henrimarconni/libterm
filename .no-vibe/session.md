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

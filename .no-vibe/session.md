# Lesson: libterm-benchmark-api-value
Mode: concept
Refs: refterm (production angle), then termbox2 (compatibility angle)
Started: 2026-05-07

## Curriculum
- [ ] 1. Benchmark value model: what to measure and why (`lt_present` cost per frame)
- [ ] 2. CMake bench target wiring (build + run bench binary cleanly)
- [ ] 3. Minimal runnable benchmark harness (single workload slot, fixed frame loop)
- [ ] 4. Workload A: sparse updates (best-case diff path)
- [ ] 5. Workload B: full redraw (worst-case path)
- [ ] 6. Measurement hygiene (warmup, sink, timer semantics, stable output)
- [ ] 7. Production-angle report line (throughput + p50/p95 frame time)
- [ ] 8. Compatibility workloads map from termbox2 semantics
- [ ] 9. Add one termbox2-shaped workload and compare profile
- [ ] 10. Final benchmark matrix + how to use it for regressions

## Notes
- Revision 1: inserted CMake wiring before benchmark implementation (user request).
- Start with production angle from refterm-style render concerns.
- After production report layer, pivot to compatibility angle (termbox2-shaped workloads).

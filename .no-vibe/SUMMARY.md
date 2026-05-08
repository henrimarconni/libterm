# SUMMARY — this project's learning journey

## Current Focus
- Benchmark matrix for `lt_present` workloads completed (session: libterm-benchmark-api-value, layer 10/10).
- Next practical step: turn matrix into a repeatable regression gate.

## Accomplishments
- 2026-05-08 layer 10/10 Clear: collected baseline/sparse/full/box_redraw metrics table with p50/p95.
- 2026-05-08 layer 9/10 Clear: added termbox2-shaped `box_redraw` workload and wired workload map selection.
- 2026-05-08 layer 8/10 Clear: workload map abstraction introduced (`k_workloads` + active selector).

## Open Questions
- Should the benchmark choose workload via CLI arg (instead of source edit) for CI ergonomics?
- What exact threshold policy should gate regressions per workload (warn vs fail)?

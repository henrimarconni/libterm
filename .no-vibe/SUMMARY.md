# SUMMARY — this project's learning journey

## Current Focus
- Post-UTF-8 POSIX input wrap-up is in progress (session: post-utf8-wrapup, layer 6/8 completed).
- Next practical step: broaden parser tests with negative cases and more modifier combinations.

## Accomplishments
- 2026-05-15 layer 6/8 Clear: synced `ROADMAP.md` to current POSIX input status after UTF-8 and CSI parser work.
- 2026-05-15 layer 5/8 Clear: added focused POSIX parser test target `tests/test_posix_input_parse.c`.
- 2026-05-15 layer 4/8 Clear: added POSIX CSI modifier parsing for `...~` and letter-final keys (`A/B/C/D/H/F`).
- 2026-05-08 layer 10/10 Clear: collected baseline/sparse/full/box_redraw benchmark matrix with p50/p95.

## Open Questions
- Broaden parser tests: which negative cases and modifier combinations should be locked before handoff?
- Decide next parity block after input parser hardening: input-mode semantics vs color/SGR path.
- Should the benchmark choose workload via CLI arg (instead of source edit) for CI ergonomics?
- What exact threshold policy should gate regressions per workload (warn vs fail)?

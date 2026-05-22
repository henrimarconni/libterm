# SUMMARY — this project's learning journey

## Current Focus
- `post-utf8-wrapup` session **completed** 2026-05-23 (8/8 layers). Render path now emits SGR honestly for `LT_OUTPUT_NORMAL`.
- Next session candidates: layer 9 (256/216/grayscale/truecolor output modes), input-mode runtime semantics (mouse parsing, ESC vs ALT timeout), or fixing the two bugs uncovered during layer-8 verify (peek_event error-code surface, Linux shared-lib symbol visibility).

## Accomplishments
- 2026-05-23 layer 8/8 Clear: wired SGR emission for `LT_OUTPUT_NORMAL` across `lt__plat_render_run` — persistent `(cur_fg, cur_bg, cur_attrs)` cache in `lt__g` with `0xFFFFFFFF` sentinel, stateless `lt__plat_emit_sgr` helper (`\x1b[0;3X;4Y;...m` reset emission), shutdown reset. Visual demo at `examples/colors.c`. fg/bg + bold/dim/italic/underline/blink/reverse/strike all emit correctly. Attr-packing contract (attrs into fg, bg high bits ignored) documented in public header.
- 2026-05-23 chunk fix: `LIBTERM_BENCH_HEADLESS_OUTPUT` was silently applied to `libterm_static`, no-op'ing all tty writes for every consumer including examples. Now an opt-in CMake option (`-DLIBTERM_BENCH_HEADLESS=ON`), default OFF.
- 2026-05-20 layer 7/8 Clear: broadened `tests/test_posix_input_parse.c` to a 42-case letter-final modifier matrix + 9 negative/pinned-behavior cases (delegated to AI via `/no-vibe-btw`).
- 2026-05-15 layer 6/8 Clear: synced `ROADMAP.md` to current POSIX input status after UTF-8 and CSI parser work.
- 2026-05-15 layer 5/8 Clear: added focused POSIX parser test target `tests/test_posix_input_parse.c`.
- 2026-05-15 layer 4/8 Clear: added POSIX CSI modifier parsing for `...~` and letter-final keys (`A/B/C/D/H/F`).
- 2026-05-08 layer 10/10 Clear: collected baseline/sparse/full/box_redraw benchmark matrix with p50/p95.

## Open Questions
- **B1 (visual verify pending):** user has not yet run `./build/examples/colors` to confirm SGR renders visibly. Code review + build green is high-confidence but not the same as eyes on output.
- **B2 (shared lib export):** `LT_API` only expands to `__declspec` on Windows; on Linux+`-fvisibility=hidden` (set in `src/CMakeLists.txt:99,115`), the shared lib exports zero public symbols. Fix: extend `LT_API` to emit `__attribute__((visibility("default")))` on GCC/Clang.
- **peek_event error surface:** `lt_peek_event` returns codes outside `{LT_OK, LT_ERR_NO_EVENT}` in some envs (e.g. ctest with redirected stdin). `tests/test_api.c:65` asserts narrowly; either widen the assert or normalize unexpected reads to `LT_ERR_NO_EVENT` inside `lt__plat_read_event`.
- **Bench headless architecture:** the compile-time `LIBTERM_BENCH_HEADLESS_OUTPUT` only works by poisoning the whole library. A clean fix is a runtime flag in `lt__g` or a per-bench object library that gets its own `plat_output.c` build.
- Empty-modifier slot (`ESC[1;A`) currently degrades to `mod=0` with key still resolving — pinned by test, but is that the *desired* contract or just current behavior?
- Should the benchmark choose workload via CLI arg (instead of source edit) for CI ergonomics?
- What exact threshold policy should gate regressions per workload (warn vs fail)?

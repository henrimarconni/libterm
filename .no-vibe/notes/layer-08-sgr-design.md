# Layer 8 — Wire SGR emission in render-run (LT_OUTPUT_NORMAL)

Session: post-utf8-wrapup
Date: 2026-05-21
Decisions locked in Phase 1b: **8-b / G / reset**
  - 8-b: colors + attrs together (not colors-only)
  - G: persistent SGR cache in `lt__g` (across presents), sentinel-on-init, reset-on-shutdown
  - reset: stateless emitter — on any change, emit `ESC[0;<fg>;<bg>;<attrs>m`

## Goal
`lt_set_cell(x, y, ch, fg, bg)` must produce a terminal that actually shows `fg` and `bg`. Today `lt__plat_render_run` strips color/attrs; the cell's `fg`/`bg` fields participate in diff comparison but are not emitted. Layer 8 makes the emission honest for `LT_OUTPUT_NORMAL`.

## Scope

In scope:
- 8 base colors (`LT_BLACK..LT_WHITE`) → SGR `30..37` (fg) / `40..47` (bg)
- `LT_DEFAULT` → SGR `39` (fg) / `49` (bg)
- Attr bits: `LT_BOLD 1`, `LT_DIM 2`, `LT_ITALIC 3`, `LT_UNDERLINE 4`, `LT_BLINK 5`, `LT_REVERSE 7`, `LT_STRIKE 9`
- SGR cache (`cur_fg`, `cur_bg`, `cur_attrs`) in `lt__g`, persistent across presents
- Reset on shutdown: emit `ESC[0m`, set sentinel

Out of scope (later layers):
- `LT_OUTPUT_256` / `LT_OUTPUT_216` / `LT_OUTPUT_GRAYSCALE` / `LT_OUTPUT_TRUECOLOR`
- Partial-diff emission strategy
- Windows console SGR translation (only POSIX render path this layer)

## API impact
**None.** Public surface is already there:
- `lt_set_cell(x, y, ch, fg, bg)` — already stores fg/bg
- `lt_set_clear_attrs(fg, bg)` — already stores clear attrs
- `lt_set_output_mode(mode)` — already stores mode; layer 8 begins to honor `LT_OUTPUT_NORMAL`

## Data shape

Add to `struct lt_global` (in `src/internal.h`, near `cur_x`/`cur_y`):

```c
lt_attr cur_fg;
lt_attr cur_bg;
lt_attr cur_attrs;   /* only the high attr bits, not the color low byte */
```

Sentinel value: `0xFFFFFFFF` (impossible `lt_attr` because real values fit in 16 bits today). Sentinel set:
- on `lt_init` (after the existing cur_x/cur_y init)
- on `lt_shutdown` (after we emit `ESC[0m`)
- on the existing `cur_x/cur_y = -1` invalidation paths? **No** — see "Cursor invalidation" below.

## Cursor invalidation
`ESC[r;cH` does NOT touch terminal SGR. The SGR cache stays valid across cursor jumps. Do not invalidate `cur_fg/cur_bg/cur_attrs` when `cur_x/cur_y` are invalidated. The two caches are independent.

## Emission contract

New static helper in `src/platform/posix/plat_output.c`:

```c
static int lt__plat_emit_sgr(lt_attr fg, lt_attr bg, lt_attr attrs);
```

Stateless. Always emits `ESC[0;...m` containing the full target rendition. Format:

```
ESC[0
  ;3<n>   if fg is LT_BLACK..LT_WHITE
  ;39     if fg is LT_DEFAULT
  ;4<n>   if bg is LT_BLACK..LT_WHITE
  ;49     if bg is LT_DEFAULT
  ;1      if attrs & LT_BOLD
  ;2      if attrs & LT_DIM
  ;3      if attrs & LT_ITALIC
  ;4      if attrs & LT_UNDERLINE
  ;5      if attrs & LT_BLINK
  ;7      if attrs & LT_REVERSE
  ;9      if attrs & LT_STRIKE
m
```

Color macros use `1`-based values (`LT_BLACK = 0x0001..LT_WHITE = 0x0008`), so the SGR digit is `30 + (color - 1)` for `LT_BLACK..LT_WHITE`, and `39` (fg) / `49` (bg) for `LT_DEFAULT`.

## Where it gets called

In `lt__plat_render_run`, before emitting each cell run:

1. Extract `run_fg = cells[0].fg & 0x00FF` (color byte)
2. Extract `run_bg = cells[0].bg & 0x00FF`
3. Extract `run_attrs = cells[0].fg & 0xFF00` — attrs piggyback on fg per termbox convention; libterm follows the same.

   *Open question to verify against current code while typing: does libterm pack attrs into fg only, or into both fg and bg? Read `LT_BOLD..LT_STRIKE` usage to confirm before writing the mask.*

4. If `(run_fg, run_bg, run_attrs)` differs from `(lt__g.cur_fg, lt__g.cur_bg, lt__g.cur_attrs)`:
   - call `lt__plat_emit_sgr(run_fg, run_bg, run_attrs)`
   - update the cache

5. Emit the glyph bytes (existing code path).

Edge case: within a run, the SIMD diff guarantees all cells are equal, so they share the same fg/bg/attrs. No per-cell SGR check needed inside the run.

## Shutdown

In `lt_shutdown` (POSIX path), before tty restore:
```
ESC[0m
```
Then set cache sentinel to `0xFFFFFFFF` so the next `lt_init` cycle starts clean.

## Test plan

Add `tests/test_render_sgr.c` (new target in `tests/CMakeLists.txt`). Strategy: capture `lt__plat_write` output via a test seam — there's already a headless/mock-output seam from the refterm benchmark work (`bench/bench_present.c`); reuse the same lever.

Cases:
1. **First present after init** with one RED cell → emitted bytes contain `\x1b[0;31;49m` (or equivalent) before the glyph.
2. **Two consecutive runs same color** → SGR emitted only once.
3. **Run-to-run color change** → second run emits a fresh `\x1b[0;...m`.
4. **Run-to-run attr-only change** (RED → RED+BOLD) → fresh `\x1b[0;31;49;1m`.
5. **LT_DEFAULT cells** → emits `39`/`49`.
6. **All attrs at once** → `\x1b[0;31;49;1;2;3;4;5;7;9m`.
7. **No-dirty present** → no SGR emitted at all (fast-path stays free).
8. **Shutdown** → final bytes include `\x1b[0m`.

Don't test partial-diff (we chose reset emission); don't test cursor-move invalidation (we chose not to invalidate).

## Order to type it (in no-vibe)

1. Verify attr-packing convention by reading `lt_set_cell`/`buffer_clear` callers in `examples/` and `tests/`. Decide mask shape.
2. Add three fields to `struct lt_global` in `src/internal.h`; init in `lt_init`.
3. Write `lt__plat_emit_sgr` in `plat_output.c`. Compose into a small buffer (`lt__plat_reserve(24)`).
4. Insert the compare-and-emit block in `lt__plat_render_run` before the existing glyph loop.
5. Update `lt_shutdown` POSIX path: emit `ESC[0m`, set sentinel.
6. Wire the new test target in `tests/CMakeLists.txt`.
7. Write the 8 test cases.
8. Build, run all tests, verify a colored `examples/keyboard.c` actually shows color.

## Acceptance criteria

- All existing tests still pass.
- New `test_render_sgr` passes.
- Manual: run an example that calls `lt_set_cell(x, y, 'X', LT_RED, LT_DEFAULT)` and visually confirm `X` is red.
- Shell after `lt_shutdown` is uncolored (the `ESC[0m` actually fired).

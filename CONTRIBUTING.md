# Contributing to libterm

Thanks for considering a contribution! This page is short but every rule on
it exists because something broke without it.

## Quickstart

```sh
git clone https://github.com/rizukirr/libterm.git
cd libterm
cmake -B build
cmake --build build
ctest --test-dir build          # full suite, a few seconds
./build/examples/kbd            # poke it interactively
```

## Toolchain requirements

| Tool | Version | Why it's strict |
|---|---|---|
| clang-format | **exactly 22.x** | the repo's `.clang-format` uses `Language: C`, which older versions (19/20/21) reject outright; CI pins 22 and the tree is formatted to it |
| CMake | ≥ 3.32 | `cmake_minimum_required` |
| Compiler | GCC or Clang (C11) | MSVC is not currently validated |

Run `clang-format -i` (v22) on touched C/H files before pushing — the
`clang-format` CI job is a hard gate.

## Workflow

- Branch from `main`, one logical change per PR.
- **Conventional commits**: `fix(scope): ...`, `feat: ...`, `test: ...`,
  `docs: ...`, `ci: ...` — look at `git log --oneline` for the house style.
- New behavior needs a test that **fails without your change** (we practice
  TDD; reviewers will ask for the pre-fix failure mode).
- If a change alters user-visible behavior, update `ROADMAP.md`'s relevant
  row and add a `CHANGELOG.md` entry under `[Unreleased]`.

## The hard-won rules

Each of these comes from a real incident. Please read them before touching
the corresponding area.

1. **Shipped-contract guard.** Before changing any public-API behavior,
   `grep -rn` the `tests/` directory for assertions encoding the *current*
   contract — they are the spec. (A cursor-semantics change once silently
   broke the shipped `lt_set_cursor` bounds contract; a pre-existing test
   caught it.)
2. **White-box tests `#include` platform TUs.** Tests like
   `tests/test_posix_input_parse.c` compile `src/platform/posix/plat_input.c`
   directly into the test. Consequence: feature-test macros
   (`_DEFAULT_SOURCE`, `_GNU_SOURCE`) must be defined before ANY system
   header, and code in those TUs can't rely on being compiled in isolation.
3. **pty byte-tests drain only after a flush point.** Output APIs buffer;
   `lt_present()` (and init/shutdown) flush. Draining the pty master right
   after e.g. `lt_set_cursor` reads nothing. pty tests are Apple-gated
   (macOS pty buffers are tiny — see ROADMAP "Known blockers") and return
   77 (CTest SKIP) when no pty is available.
4. **Tests keep their asserts in every build type.** `tests/CMakeLists.txt`
   compiles test targets with `-UNDEBUG`, because Release's `-DNDEBUG` once
   turned the whole suite into vacuous no-ops. Never write a test that
   relies on an `assert(...)` side effect being optional.
5. **Windows = MinGW cross first.** Verify Windows-touching changes locally
   with `cmake -B build-mingw -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake
   -DLIBTERM_BUILD_TESTS=OFF -DLIBTERM_BUILD_EXAMPLES=OFF -DLIBTERM_BUILD_BENCH=OFF`
   + build; the `windows-mingw-native` CI job then runs the suite on a real
   Windows runner.
6. **No platform `#ifdef`s in `src/shared/`.** Platform differences live
   behind the `lt__plat_*` interface (see `ARCHITECTURE.md`).

## Finding work

- `ROADMAP.md` — every `[~]` (partial) or `[ ]` (missing) row is a task,
  with notes explaining the current state.
- Issues labeled `good first issue` and `help wanted`.

## What CI runs on your PR

| Job | What it gates |
|---|---|
| `clang-format` | formatting, pinned v22 |
| `build-test` matrix (linux gcc/clang × scalar/avx2, avx512 compile-only, dispatch, macOS) | build + full test suite per config |
| `windows-mingw-cross` | the Windows code path compiles from POSIX |
| `windows-mingw-native` | the suite executes on a real Windows runner |
| `install-smoke` | `find_package(Libterm)` consumption (default + shared-only) |
| `asan-ubsan` | memory/UB sanitizers over the suite |
| `cross-simd` (aarch64 neon/sve, riscv64 rvv) | non-x86 backends build + test under QEMU |
| `clang-tidy` | static analysis |

All green = reviewable. Releases additionally re-run everything in Release
mode and ship prebuilt binaries automatically (`.github/workflows/release.yml`).

## Code style

clang-format owns layout. Beyond that: match the file you're in — comment
density, naming (`lt_` public, `lt__` internal, `LT_`/`LT__` macros), and
error-handling shape (`LT_ERR_*` returns, `lt_last_errno` for syscall
causes).

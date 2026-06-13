# Fuzzing

libFuzzer harnesses for libterm's pure parsers (no globals, no I/O):

| Target        | Exercises                                  |
|---------------|--------------------------------------------|
| `fuzz_keymap` | `lt__key_decode` — input escape sequences  |
| `fuzz_colorq` | `lt__color_parse_osc_reply` — OSC replies  |
| `fuzz_utf8`   | `lt__utf8_decode` — UTF-8 byte sequences   |

These are **clang-only** (`-fsanitize=fuzzer`) and off by default.

## Build

```bash
CC=clang cmake -S . -B build-fuzz -DLIBTERM_BUILD_FUZZERS=ON
cmake --build build-fuzz
```

Binaries land in `build-fuzz/tests/fuzz/`.

## Run

Replay the checked-in seed corpus (CI smoke does this):

```bash
./build-fuzz/tests/fuzz/fuzz_keymap -runs=0 tests/fuzz/corpus/keymap
```

Explore for new inputs (writes new corpus entries it discovers):

```bash
./build-fuzz/tests/fuzz/fuzz_keymap -max_total_time=60 tests/fuzz/corpus/keymap
```

## Reproducing a crash

On a finding, libFuzzer writes `crash-<sha1>` in the cwd and prints an
AddressSanitizer/UBSan report. Re-run the single input to reproduce:

```bash
./build-fuzz/tests/fuzz/fuzz_keymap crash-<sha1>
```

The nightly workflow (`.github/workflows/fuzz-nightly.yml`) uploads any
`crash-*` file as a build artifact.

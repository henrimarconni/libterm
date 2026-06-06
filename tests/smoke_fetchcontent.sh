#!/usr/bin/env bash
#
# smoke_fetchcontent.sh <libterm_source_dir>
#
# Configure + build the minimal FetchContent consumer fixture
# (tests/fixtures/fetchcontent-consumer) against the working tree and assert
# the subproject contract:
#   1. the consumer configures, builds, and links Libterm::libterm;
#   2. no example/test/bench/shared-lib targets leak into the consumer build;
#   3. the consumed library is uninstrumented (no bench defines) — also when
#      the consumer opts into LIBTERM_BUILD_BENCH=ON (the instrumented copy
#      must be libterm_bench, never libterm_static);
#   4. `cmake --install` of the consumer installs nothing of libterm's.
#
# Uses the Ninja generator: present on all CI runners this repo uses, and it
# emits one-line-per-entry compile_commands.json commands (each line carries
# both the -o <target>.dir/ output path and the defines, which the pass-2
# greps below rely on).
set -eu
src=${1:?usage: smoke_fetchcontent.sh <libterm_source_dir>}
src=$(cd "$src" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

fail() { echo "FAIL: $*"; exit 1; }

# --- pass 1: default subproject consumption --------------------------------
build="$work/build"
cmake -S "$src/tests/fixtures/fetchcontent-consumer" -B "$build" -G Ninja \
    -DLIBTERM_SOURCE_DIR="$src" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null
cmake --build "$build" --parallel >/dev/null
[ -f "$build/consumer" ] || [ -f "$build/consumer.exe" ] \
    || fail "consumer binary missing after build"
echo "PASS: consumer configured, built, and linked"

cc_json="$build/compile_commands.json"
for leak in hello.c test_utf8.c bench_present.c 'libterm_shared\.dir'; do
    if grep -q "$leak" "$cc_json"; then
        fail "leaked into consumer build: $leak"
    fi
done
echo "PASS: no example/test/bench/shared targets in the consumer build"

if grep -q 'LIBTERM_ENABLE_RENDER_STATS\|LIBTERM_BENCH_' "$cc_json"; then
    fail "instrumented library: bench defines in consumer compile commands"
fi
echo "PASS: consumed library is uninstrumented"

cmake --install "$build" --prefix "$work/prefix" >/dev/null
polluted=$(find "$work/prefix" \( -name 'libterm*' -o -name 'Libterm*' \))
if [ -n "$polluted" ]; then
    echo "$polluted"
    fail "libterm polluted the consumer install"
fi
echo "PASS: no libterm install pollution"

# --- pass 2: consumer opts into bench — library copy must stay clean -------
build2="$work/build-bench"
cmake -S "$src/tests/fixtures/fetchcontent-consumer" -B "$build2" -G Ninja \
    -DLIBTERM_SOURCE_DIR="$src" \
    -DLIBTERM_BUILD_BENCH=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null
cc_json2="$build2/compile_commands.json"
if grep 'libterm_static\.dir' "$cc_json2" \
        | grep -q 'LIBTERM_ENABLE_RENDER_STATS'; then
    fail "libterm_static instrumented even though bench has its own copy"
fi
# positive control: the same mechanism must see the defines on the bench copy
grep 'libterm_bench\.dir' "$cc_json2" \
        | grep -q 'LIBTERM_ENABLE_RENDER_STATS' \
    || fail "positive control broken: no bench defines on libterm_bench"
echo "PASS: bench instrumentation isolated to libterm_bench"

echo "PASS [all]: FetchContent subproject contract holds"

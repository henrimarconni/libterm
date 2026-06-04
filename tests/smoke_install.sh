#!/usr/bin/env bash
#
# smoke_install.sh <libterm_source_dir>
#
# Stage-install libterm into a temp prefix, then configure, build, and run a
# minimal find_package(Libterm) consumer against it. Guards the install/export
# wiring end-to-end (install(TARGETS) + LibtermTargets.cmake + the umbrella
# Libterm::libterm shim in LibtermConfig.cmake).
set -eu
src=${1:?usage: smoke_install.sh <libterm_source_dir>}
src=$(cd "$src" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cmake -S "$src" -B "$work/build" \
    -DLIBTERM_BUILD_EXAMPLES=OFF -DLIBTERM_BUILD_BENCH=OFF \
    -DLIBTERM_BUILD_TESTS=OFF >/dev/null
cmake --build "$work/build" --parallel >/dev/null
cmake --install "$work/build" --prefix "$work/prefix" >/dev/null

mkdir "$work/consumer"
cat > "$work/consumer/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.16)
project(smoke C)
find_package(Libterm REQUIRED)
add_executable(smoke main.c)
target_link_libraries(smoke PRIVATE Libterm::libterm)
EOF
cat > "$work/consumer/main.c" <<'EOF'
#include "libterm/libterm.h"
#include <stdio.h>
int main(void) {
  puts(lt_strerror(LT_ERR_NOT_INIT));
  return 0;
}
EOF
cmake -S "$work/consumer" -B "$work/consumer/build" \
    -DCMAKE_PREFIX_PATH="$work/prefix" >/dev/null
cmake --build "$work/consumer/build" >/dev/null
"$work/consumer/build/smoke"
echo "PASS: find_package(Libterm) consumer built and ran"

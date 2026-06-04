#!/usr/bin/env bash
#
# smoke_install.sh <libterm_source_dir>
#
# Stage-install libterm into a temp prefix, then configure, build, and run a
# minimal find_package(Libterm) consumer against it. Guards the install/export
# wiring end-to-end (install(TARGETS) + LibtermTargets.cmake + the umbrella
# Libterm::libterm shim in LibtermConfig.cmake).
# Runs twice: default (static+shared; umbrella -> static) and shared-only (umbrella -> shared).
set -eu
src=${1:?usage: smoke_install.sh <libterm_source_dir>}
src=$(cd "$src" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

smoke_pass() {
    local label=$1; shift
    local stage="$work/$label"
    mkdir -p "$stage"
    cmake -S "$src" -B "$stage/build" \
        -DLIBTERM_BUILD_EXAMPLES=OFF -DLIBTERM_BUILD_BENCH=OFF \
        -DLIBTERM_BUILD_TESTS=OFF "$@" >/dev/null
    cmake --build "$stage/build" --parallel >/dev/null
    cmake --install "$stage/build" --prefix "$stage/prefix" >/dev/null

    mkdir "$stage/consumer"
    cat > "$stage/consumer/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.16)
project(smoke C)
find_package(Libterm REQUIRED)
add_executable(smoke main.c)
target_link_libraries(smoke PRIVATE Libterm::libterm)
EOF
    cat > "$stage/consumer/main.c" <<'EOF'
#include "libterm/libterm.h"
#include <stdio.h>
int main(void) {
  puts(lt_strerror(LT_ERR_NOT_INIT));
  return 0;
}
EOF
    cmake -S "$stage/consumer" -B "$stage/consumer/build" \
        -DCMAKE_PREFIX_PATH="$stage/prefix" >/dev/null
    cmake --build "$stage/consumer/build" >/dev/null
    "$stage/consumer/build/smoke"
    echo "PASS [$label]: find_package(Libterm) consumer built and ran"
}

smoke_pass default
smoke_pass shared-only -DLIBTERM_BUILD_STATIC=OFF

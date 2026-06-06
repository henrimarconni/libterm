#!/usr/bin/env bash
#
# smoke_manual_build.sh <libterm_source_dir>
#
# Run the README's "Without CMake" recipe and prove the result links and
# runs. The cc/ar lines below are the README's, with "$src"/ prefixed onto
# the repo-relative paths (the script builds in a temp dir) — keep them in
# lockstep with README.md.
set -eu
src=${1:?usage: smoke_manual_build.sh <libterm_source_dir>}
src=$(cd "$src" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$work"

cc -std=c11 -c "$src"/src/shared/*.c "$src"/src/platform/posix/*.c \
    "$src"/src/intrinsics/scalar.c -I"$src"/include -I"$src"/src
ar rcs libterm.a *.o

cat > main.c <<'EOF'
#include "libterm/libterm.h"
#include <stdio.h>
int main(void) {
  puts(lt_strerror(LT_ERR_NOT_INIT));
  return 0;
}
EOF
cc main.c libterm.a -I"$src"/include -o consumer
./consumer
echo "PASS [manual-build]: README cc+ar scalar build linked and ran"

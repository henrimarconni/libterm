#!/usr/bin/env bash
#
# assert_exports.sh <public_header> <shared_lib>
#
# Fail unless every public LT_API function in <public_header> is an exported,
# defined symbol in <shared_lib>. Guards against the visibility regression where
# the shared library exports zero public symbols (LT_API empty under
# -fvisibility=hidden).
set -u
header=${1:?usage: assert_exports.sh <public_header> <shared_lib>}
lib=${2:?usage: assert_exports.sh <public_header> <shared_lib>}

# Public function names: every line mentioning LT_API, take the lt_* token
# immediately before its '('.
mapfile -t syms < <(grep 'LT_API' "$header" | grep -oE 'lt_[a-z0-9_]+\(' | tr -d '(' | sort -u)
if [ "${#syms[@]}" -eq 0 ]; then
  echo "FAIL: no LT_API symbols found in $header" >&2
  exit 1
fi

exported=$(nm -D --defined-only "$lib" 2>/dev/null | awk '{print $NF}')
missing=0
for s in "${syms[@]}"; do
  if ! grep -qx "$s" <<<"$exported"; then
    echo "FAIL: $s is not exported from $(basename "$lib")" >&2
    missing=1
  fi
done

if [ "$missing" -ne 0 ]; then
  exit 1
fi
echo "PASS: all ${#syms[@]} public lt_* symbols exported from $(basename "$lib")"
exit 0

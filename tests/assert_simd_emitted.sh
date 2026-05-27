#!/usr/bin/env bash
#
# assert_simd_emitted.sh <backend> <build_dir>
#
# Guard against a SIMD backend silently compiling to scalar code (which would
# still pass the correctness contract test). Disassembles the per-backend
# translation unit object DIRECTLY — not the whole library — so that
# auto-vectorization in unrelated TUs (NEON is baseline on aarch64) cannot mask
# a fallback. Asserts the selected backend's object actually contains that
# ISA's instructions; for scalar, asserts the opposite (no SIMD at all).
set -u

backend=${1:?usage: assert_simd_emitted.sh <backend> <build_dir>}
build_dir=${2:?usage: assert_simd_emitted.sh <backend> <build_dir>}

case "$backend" in
  scalar) objdump_bin=objdump;                   sig='vpcmpeqb|%ymm|%zmm|whilelt|vsetvli|cmeq|vsse32'; expect=absent ;;
  avx2)   objdump_bin=objdump;                   sig='vpcmpeqb|%ymm';                                  expect=present ;;
  avx512) objdump_bin=objdump;                   sig='%zmm|vmovdqu64|kmov';                            expect=present ;;
  neon)   objdump_bin=aarch64-linux-gnu-objdump; sig='cmeq|shrn|\.16b';                                expect=present ;;
  sve)    objdump_bin=aarch64-linux-gnu-objdump; sig='whilelt|ld1b|cmpne|ptest';                       expect=present ;;
  rvv)    objdump_bin=riscv64-linux-gnu-objdump; sig='vsetvli|vle8|vsse32|vmsne|vfirst';               expect=present ;;
  *) echo "FAIL: unknown backend '$backend'" >&2; exit 2 ;;
esac

if ! command -v "$objdump_bin" >/dev/null 2>&1; then
  echo "SKIP: $objdump_bin not on PATH; cannot disassemble '$backend'" >&2
  exit 0
fi

obj=$(find "$build_dir" -name "${backend}.c.o" -path "*intrinsics*" 2>/dev/null | head -1)
if [ -z "$obj" ]; then
  echo "FAIL: SIMD object for backend '$backend' not found under $build_dir" >&2
  exit 1
fi

n=$("$objdump_bin" -d "$obj" 2>/dev/null | grep -icE "$sig")

if [ "$expect" = present ]; then
  if [ "$n" -gt 0 ]; then
    echo "PASS: '$backend' emits SIMD instructions ($n matches for /$sig/ in $(basename "$obj"))"
    exit 0
  fi
  echo "FAIL: '$backend' object has NO SIMD instructions (/$sig/) — silent scalar fallback?" >&2
  exit 1
else
  if [ "$n" -eq 0 ]; then
    echo "PASS: '$backend' object contains no SIMD instructions, as expected"
    exit 0
  fi
  echo "FAIL: scalar object unexpectedly contains SIMD instructions ($n matches for /$sig/)" >&2
  exit 1
fi

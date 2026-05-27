#!/usr/bin/env bash
#
# simd_vlen_sweep.sh
#
# Exercise the scalable SIMD backends (sve, rvv) across multiple emulated vector
# lengths. The default ctest run only covers one width (qemu -cpu max); a
# VLEN-specific tail/predication bug would slip past it. This sweep builds each
# backend via its cross toolchain and runs the contract test under qemu at
# several VLENs. Run from anywhere; exits non-zero if any width fails.
#
# Requires: aarch64-linux-gnu-gcc, riscv64-linux-gnu-gcc, qemu-aarch64,
# qemu-riscv64, and the matching sysroots under /usr/<triple>.
set -u
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 2
fail=0
common="-DLIBTERM_BUILD_TESTS=ON -DLIBTERM_BUILD_EXAMPLES=OFF -DLIBTERM_BUILD_BENCH=OFF -DLIBTERM_BUILD_SHARED=OFF"

echo "== build + sweep sve (qemu-aarch64, sve-max-vq) =="
if cmake -S . -B build-sve -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-qemu.cmake \
      -DLIBTERM_SIMD=sve $common >/dev/null 2>&1 \
   && cmake --build build-sve --target test_simd_diff >/dev/null 2>&1; then
  for vq in 1 2 4 8 16; do
    qemu-aarch64 -cpu "max,sve-max-vq=$vq" -L /usr/aarch64-linux-gnu \
      ./build-sve/tests/test_simd_diff
    rc=$?; bits=$((vq * 128))
    if [ $rc -eq 0 ]; then echo "  sve ${bits}-bit (vq=$vq): PASS"
    else echo "  sve ${bits}-bit (vq=$vq): FAIL (exit $rc)"; fail=1; fi
  done
else
  echo "  sve build FAILED"; fail=1
fi

echo "== build + sweep rvv (qemu-riscv64, vlen) =="
if cmake -S . -B build-rvv -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/riscv64-qemu.cmake \
      -DLIBTERM_SIMD=rvv $common >/dev/null 2>&1 \
   && cmake --build build-rvv --target test_simd_diff >/dev/null 2>&1; then
  for vl in 128 256 512 1024; do
    qemu-riscv64 -cpu "rv64,v=true,vlen=$vl,vext_spec=v1.0" -L /usr/riscv64-linux-gnu \
      ./build-rvv/tests/test_simd_diff
    rc=$?
    if [ $rc -eq 0 ]; then echo "  rvv vlen=$vl: PASS"
    else echo "  rvv vlen=$vl: FAIL (exit $rc)"; fail=1; fi
  done
else
  echo "  rvv build FAILED"; fail=1
fi

rm -f "$root"/*.core 2>/dev/null
if [ $fail -eq 0 ]; then echo "VLEN sweep: ALL PASS"; else echo "VLEN sweep: FAILURES"; fi
exit $fail

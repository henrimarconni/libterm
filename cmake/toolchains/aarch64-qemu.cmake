# Cross-compile for AArch64 and run tests under qemu-aarch64.
# Used for both the neon and sve SIMD backends:
#   cmake -S . -B build-neon -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-qemu.cmake -DLIBTERM_SIMD=neon
#   cmake -S . -B build-sve  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-qemu.cmake -DLIBTERM_SIMD=sve
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
# -cpu max enables SVE (and NEON is baseline); -L points qemu at the sysroot.
set(CMAKE_CROSSCOMPILING_EMULATOR "/usr/bin/qemu-aarch64;-cpu;max;-L;/usr/aarch64-linux-gnu")

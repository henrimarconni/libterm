# Cross-compile for RISC-V 64 and run tests under qemu-riscv64.
# Used for the rvv SIMD backend:
#   cmake -S . -B build-rvv -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/riscv64-qemu.cmake -DLIBTERM_SIMD=rvv
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR riscv64)
set(CMAKE_C_COMPILER riscv64-linux-gnu-gcc)
# -cpu max enables the V (vector) extension; -L points qemu at the sysroot.
set(CMAKE_CROSSCOMPILING_EMULATOR "/usr/bin/qemu-riscv64;-cpu;max;-L;/usr/riscv64-linux-gnu")

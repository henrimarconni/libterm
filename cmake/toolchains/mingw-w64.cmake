# Cross-compile for Windows (x86_64) with the MinGW-w64 GCC toolchain.
# Compile-check the Windows platform layer from a POSIX host:
#   cmake -S . -B build-mingw -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake \
#         -DLIBTERM_BUILD_TESTS=OFF -DLIBTERM_BUILD_EXAMPLES=OFF -DLIBTERM_BUILD_BENCH=OFF
#   cmake --build build-mingw --target libterm_static
# Runs no tests (needs wine to execute Windows binaries) — this is a build/compile
# verification of the Windows code path (src/platform/windows/*).
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

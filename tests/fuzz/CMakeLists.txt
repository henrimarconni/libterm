# libFuzzer targets for the pure parsers. Built only when
# LIBTERM_BUILD_FUZZERS=ON; never registered as ctest tests (the CI jobs
# invoke the binaries directly). clang-only: -fsanitize=fuzzer is unavailable
# under gcc/MSVC.
if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR
        "LIBTERM_BUILD_FUZZERS requires clang (-fsanitize=fuzzer); "
        "current compiler is ${CMAKE_C_COMPILER_ID}. Re-run with CC=clang.")
endif()

set(LIBTERM_FUZZ_FLAGS -fsanitize=fuzzer,address,undefined
                       -fno-omit-frame-pointer -g)

function(libterm_add_fuzzer name)
    add_executable(${name} ${name}.c)
    target_link_libraries(${name} PRIVATE ${LIBTERM_LINK_TARGET})
    target_include_directories(${name} PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/include)
    target_compile_options(${name} PRIVATE ${LIBTERM_FUZZ_FLAGS})
    target_link_options(${name} PRIVATE ${LIBTERM_FUZZ_FLAGS})
endfunction()

libterm_add_fuzzer(fuzz_keymap)

set(LIBTERM_LINK_TARGET libterm_static)
if(NOT LIBTERM_BUILD_STATIC)
    set(LIBTERM_LINK_TARGET libterm_shared)
endif()

function(libterm_add_test name)
    add_executable(${name} ${name}.c)
    target_link_libraries(${name} PRIVATE ${LIBTERM_LINK_TARGET})
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

libterm_add_test(test_utf8)
libterm_add_test(test_buffer)
libterm_add_test(test_api)
libterm_add_test(test_simd_diff)
if(NOT WIN32)
    libterm_add_test(test_posix_input_parse)
    add_executable(test_posix_input_sgr_run_split test_posix_input_sgr_run_split.c)
    target_include_directories(test_posix_input_sgr_run_split PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    add_test(NAME test_posix_input_sgr_run_split COMMAND test_posix_input_sgr_run_split)
    add_executable(test_pty_lifecycle test_pty_lifecycle.c)
    target_link_libraries(test_pty_lifecycle PRIVATE ${LIBTERM_LINK_TARGET} util)
    target_include_directories(test_pty_lifecycle PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME test_pty_lifecycle COMMAND test_pty_lifecycle)
    set_tests_properties(test_pty_lifecycle PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(test_posix_sgr_output test_posix_sgr_output.c)
    target_link_libraries(test_posix_sgr_output PRIVATE ${LIBTERM_LINK_TARGET} util)
    target_include_directories(test_posix_sgr_output PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME test_posix_sgr_output COMMAND test_posix_sgr_output)
    set_tests_properties(test_posix_sgr_output PROPERTIES SKIP_RETURN_CODE 77)
endif()

# Assert the selected SIMD backend actually emitted its ISA instructions, so a
# silent scalar fallback cannot pass the correctness contract test. Runs on the
# host (bash + objdump on the per-backend object); not under the cross emulator.
if(DEFINED LIBTERM_SIMD_RESOLVED)
    add_test(
        NAME test_simd_emitted
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/assert_simd_emitted.sh
                ${LIBTERM_SIMD_RESOLVED} ${CMAKE_BINARY_DIR})
endif()

# Assert the shared library actually exports its public API (guards B2: the
# visibility regression where LT_API was empty under -fvisibility=hidden).
if(NOT WIN32 AND TARGET libterm_shared)
    add_test(
        NAME test_exports
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/assert_exports.sh
                ${CMAKE_SOURCE_DIR}/include/libterm/libterm.h
                $<TARGET_FILE:libterm_shared>)
endif()

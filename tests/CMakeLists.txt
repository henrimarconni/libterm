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
libterm_add_test(test_wcwidth)
libterm_add_test(test_buffer)
libterm_add_test(test_api)
libterm_add_test(test_simd_diff)
libterm_add_test(test_detect_color_depth)
if(WIN32)
    libterm_add_test(test_win_sgr_output)
endif()
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
    add_executable(test_posix_input_utf8 test_posix_input_utf8.c)
    target_link_libraries(test_posix_input_utf8 PRIVATE ${LIBTERM_LINK_TARGET} util)
    target_include_directories(test_posix_input_utf8 PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME test_posix_input_utf8 COMMAND test_posix_input_utf8)
    set_tests_properties(test_posix_input_utf8 PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(test_posix_input_frag test_posix_input_frag.c)
    target_link_libraries(test_posix_input_frag PRIVATE ${LIBTERM_LINK_TARGET} util)
    target_include_directories(test_posix_input_frag PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME test_posix_input_frag COMMAND test_posix_input_frag)
    set_tests_properties(test_posix_input_frag PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(test_print test_print.c)
    target_link_libraries(test_print PRIVATE ${LIBTERM_LINK_TARGET} util)
    target_include_directories(test_print PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME test_print COMMAND test_print)
    set_tests_properties(test_print PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(test_get_cell test_get_cell.c)
    target_link_libraries(test_get_cell PRIVATE ${LIBTERM_LINK_TARGET} util)
    target_include_directories(test_get_cell PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME test_get_cell COMMAND test_get_cell)
    set_tests_properties(test_get_cell PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(test_send test_send.c)
    target_link_libraries(test_send PRIVATE ${LIBTERM_LINK_TARGET} util)
    target_include_directories(test_send PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME test_send COMMAND test_send)
    set_tests_properties(test_send PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(test_invalidate test_invalidate.c)
    target_link_libraries(test_invalidate PRIVATE ${LIBTERM_LINK_TARGET} util)
    target_include_directories(test_invalidate PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME test_invalidate COMMAND test_invalidate)
    set_tests_properties(test_invalidate PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(test_last_errno test_last_errno.c)
    target_link_libraries(test_last_errno PRIVATE ${LIBTERM_LINK_TARGET})
    target_include_directories(test_last_errno PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME test_last_errno COMMAND test_last_errno)
    set_tests_properties(test_last_errno PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(test_get_fds test_get_fds.c)
    target_link_libraries(test_get_fds PRIVATE ${LIBTERM_LINK_TARGET} util)
    target_include_directories(test_get_fds PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME test_get_fds COMMAND test_get_fds)
    set_tests_properties(test_get_fds PROPERTIES SKIP_RETURN_CODE 77)
endif()

# Assert the selected SIMD backend actually emitted its ISA instructions, so a
# silent scalar fallback cannot pass the correctness contract test. Runs on the
# host (bash + objdump on the per-backend object); not under the cross emulator.
#
# Host-only: the script assumes a Unix toolchain — POSIX `bash`, `objdump`, and
# `<backend>.c.o` object naming. On Windows ctest resolves `bash` to the WSL
# shim (which can't open the C:/ script path) and MinGW emits `<backend>.c.obj`,
# so the assertion is neither runnable nor meaningful there. CI runs it on Linux.
if(NOT WIN32 AND DEFINED LIBTERM_SIMD_RESOLVED)
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

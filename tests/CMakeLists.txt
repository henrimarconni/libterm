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
if(NOT WIN32)
    libterm_add_test(test_posix_input_parse)
    add_executable(test_posix_input_sgr_run_split test_posix_input_sgr_run_split.c)
    target_include_directories(test_posix_input_sgr_run_split PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    add_test(NAME test_posix_input_sgr_run_split COMMAND test_posix_input_sgr_run_split)
endif()

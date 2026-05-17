set(LIBTERM_BENCH_LINK_TARGET libterm_static)
if(NOT LIBTERM_BUILD_STATIC)
    set(LIBTERM_BENCH_LINK_TARGET libterm_shared)
endif()

if(LIBTERM_BUILD_STATIC)
    target_compile_definitions(libterm_static PRIVATE
        LIBTERM_BENCH_HEADLESS_OUTPUT
        LIBTERM_ENABLE_RENDER_STATS
        LIBTERM_BENCH_WIDTH=40
        LIBTERM_BENCH_HEIGHT=12)
endif()

function(libterm_add_bench name)
    add_executable(${name} ${name}.c)
    target_compile_definitions(${name} PRIVATE LIBTERM_ENABLE_RENDER_STATS)
    target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(${name} PRIVATE ${LIBTERM_BENCH_LINK_TARGET})
endfunction()

libterm_add_bench(bench_present)

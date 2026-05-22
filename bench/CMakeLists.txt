set(LIBTERM_BENCH_LINK_TARGET libterm_static)
if(NOT LIBTERM_BUILD_STATIC)
    set(LIBTERM_BENCH_LINK_TARGET libterm_shared)
endif()

option(LIBTERM_BENCH_HEADLESS
    "Make libterm_static no-op all tty writes (for bench measurement; breaks examples/tests)"
    OFF)

if(LIBTERM_BUILD_STATIC)
    target_compile_definitions(libterm_static PRIVATE
        LIBTERM_ENABLE_RENDER_STATS
        LIBTERM_BENCH_WIDTH=40
        LIBTERM_BENCH_HEIGHT=12)
    if(LIBTERM_BENCH_HEADLESS)
        target_compile_definitions(libterm_static PRIVATE
            LIBTERM_BENCH_HEADLESS_OUTPUT)
    endif()
endif()

function(libterm_add_bench name)
    add_executable(${name} ${name}.c)
    target_compile_definitions(${name} PRIVATE LIBTERM_ENABLE_RENDER_STATS)
    target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(${name} PRIVATE ${LIBTERM_BENCH_LINK_TARGET})
endfunction()

libterm_add_bench(bench_present)

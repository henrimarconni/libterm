# Bench executables link libterm_bench — the instrumented copy built in src/
# (render stats + grid dims; see the libterm_bench block there). The real
# libterm_static / libterm_shared never carry bench defines.
function(libterm_add_bench name)
    add_executable(${name} ${name}.c)
    target_compile_definitions(${name} PRIVATE
        LIBTERM_ENABLE_RENDER_STATS
        LIBTERM_BENCH_WIDTH=${LIBTERM_BENCH_WIDTH}
        LIBTERM_BENCH_HEIGHT=${LIBTERM_BENCH_HEIGHT})
    target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(${name} PRIVATE libterm_bench)
endfunction()

libterm_add_bench(bench_present)

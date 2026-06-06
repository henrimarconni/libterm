set(LIBTERM_SHARED_SOURCES
    shared/api.c
    shared/buffer.c
    shared/cell.c
    shared/colorq.c
    shared/egc.c
    shared/errors.c
    shared/input.c
    shared/keymap.c
    shared/output.c
    shared/sgr.c
    shared/utf8.c
    shared/wcwidth.c
)

if(WIN32)
    set(LIBTERM_PLATFORM_SOURCES
        platform/windows/plat_init.c
        platform/windows/plat_input.c
        platform/windows/plat_keys.c
        platform/windows/plat_output.c
        platform/windows/plat_resize.c
    )
else()
    set(LIBTERM_PLATFORM_SOURCES
        platform/posix/plat_init.c
        platform/posix/plat_input.c
        platform/posix/plat_output.c
        platform/posix/plat_resize.c
        platform/posix/plat_select.c
    )
endif()

# --- SIMD ISA selection ---
# auto = runtime dispatch: compile every backend the target arch supports and
# pick at startup from CPU capabilities (dispatch.c). An explicit backend
# value builds that single TU statically, exactly as before. When auto
# resolves to a single candidate (e.g. macOS arm64 -> neon), it degrades to
# the static build — dispatch machinery only exists where there is a choice.
set(LIBTERM_SIMD "auto" CACHE STRING
    "SIMD ISA: auto (runtime dispatch)|scalar|avx2|avx512|neon|sve|rvv")
set_property(CACHE LIBTERM_SIMD PROPERTY STRINGS
    auto scalar avx2 avx512 neon sve rvv)

# Per-backend compile flags — single source of truth for both modes.
function(libterm_simd_flags backend outvar)
    set(flags "")
    if(backend STREQUAL "avx2")
        if(MSVC)
            set(flags "/arch:AVX2")
        else()
            set(flags "-mavx2")
        endif()
    elseif(backend STREQUAL "avx512")
        if(MSVC)
            set(flags "/arch:AVX512")
        else()
            set(flags "-mavx512f;-mavx512bw")
        endif()
    elseif(backend STREQUAL "sve")
        set(flags "-march=armv8-a+sve")
    elseif(backend STREQUAL "rvv")
        set(flags "-march=rv64gcv")
    endif()
    set(${outvar} "${flags}" PARENT_SCOPE)
endfunction()

set(LIBTERM_SIMD_BACKENDS "")
if(LIBTERM_SIMD STREQUAL "auto")
    include(CheckCCompilerFlag)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
        if(MSVC)
            # MSVC lacks __attribute__((constructor)); keep the legacy static
            # pick (MSVC is not built in CI — see ci.yml).
            check_c_compiler_flag("/arch:AVX2" LIBTERM_SIMD_AVX2)
            if(LIBTERM_SIMD_AVX2)
                set(LIBTERM_SIMD_RESOLVED "avx2")
            else()
                set(LIBTERM_SIMD_RESOLVED "scalar")
            endif()
        else()
            set(LIBTERM_SIMD_BACKENDS scalar)
            check_c_compiler_flag("-mavx2" LIBTERM_CC_HAS_AVX2)
            if(LIBTERM_CC_HAS_AVX2)
                list(APPEND LIBTERM_SIMD_BACKENDS avx2)
            endif()
            check_c_compiler_flag("-mavx512bw" LIBTERM_CC_HAS_AVX512)
            if(LIBTERM_CC_HAS_AVX512)
                list(APPEND LIBTERM_SIMD_BACKENDS avx512)
            endif()
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(LIBTERM_SIMD_BACKENDS neon)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            check_c_compiler_flag("-march=armv8-a+sve" LIBTERM_CC_HAS_SVE)
            if(LIBTERM_CC_HAS_SVE)
                list(APPEND LIBTERM_SIMD_BACKENDS sve)
            endif()
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
        set(LIBTERM_SIMD_BACKENDS scalar)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            check_c_compiler_flag("-march=rv64gcv" LIBTERM_CC_HAS_RVV)
            if(LIBTERM_CC_HAS_RVV)
                list(APPEND LIBTERM_SIMD_BACKENDS rvv)
            endif()
        endif()
    else()
        set(LIBTERM_SIMD_RESOLVED "scalar")
    endif()

    list(LENGTH LIBTERM_SIMD_BACKENDS LIBTERM_SIMD_NBACKENDS)
    if(LIBTERM_SIMD_NBACKENDS GREATER 1)
        set(LIBTERM_SIMD_RESOLVED "dispatch")
    elseif(LIBTERM_SIMD_NBACKENDS EQUAL 1)
        list(GET LIBTERM_SIMD_BACKENDS 0 LIBTERM_SIMD_RESOLVED)
        set(LIBTERM_SIMD_BACKENDS "")
    endif()
else()
    set(LIBTERM_SIMD_RESOLVED ${LIBTERM_SIMD})
endif()

if(LIBTERM_SIMD_RESOLVED STREQUAL "dispatch")
    set(LIBTERM_SIMD_TUS intrinsics/dispatch.c)
    set(LIBTERM_SIMD_HAVE_DEFS "LT_SIMD_DISPATCH")
    foreach(backend IN LISTS LIBTERM_SIMD_BACKENDS)
        list(APPEND LIBTERM_SIMD_TUS intrinsics/${backend}.c)
        string(TOUPPER ${backend} LIBTERM_SIMD_BU)
        list(APPEND LIBTERM_SIMD_HAVE_DEFS "LT_SIMD_HAVE_${LIBTERM_SIMD_BU}")
    endforeach()
    set(LIBTERM_SIMD_FLAGS "")
    message(STATUS
        "libterm SIMD: runtime dispatch (backends: ${LIBTERM_SIMD_BACKENDS})")
else()
    set(LIBTERM_SIMD_TUS "intrinsics/${LIBTERM_SIMD_RESOLVED}.c")
    libterm_simd_flags(${LIBTERM_SIMD_RESOLVED} LIBTERM_SIMD_FLAGS)
    message(STATUS
        "libterm SIMD: ${LIBTERM_SIMD_RESOLVED} (flags: ${LIBTERM_SIMD_FLAGS})")
endif()

# Expose to sibling subdirectories (tests/ keys its SIMD assertions off these).
set(LIBTERM_SIMD_RESOLVED "${LIBTERM_SIMD_RESOLVED}" PARENT_SCOPE)
set(LIBTERM_SIMD_BACKENDS "${LIBTERM_SIMD_BACKENDS}" PARENT_SCOPE)

set(LIBTERM_ALL_SOURCES
    ${LIBTERM_SHARED_SOURCES}
    ${LIBTERM_PLATFORM_SOURCES}
    ${LIBTERM_SIMD_TUS}
)

set(LIBTERM_PRIVATE_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(LIBTERM_PUBLIC_INCLUDES
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# --- static ---
if(LIBTERM_BUILD_STATIC)
    add_library(libterm_static STATIC ${LIBTERM_ALL_SOURCES})
    set_target_properties(libterm_static PROPERTIES
        OUTPUT_NAME libterm
        C_VISIBILITY_PRESET hidden
    )
    target_compile_definitions(libterm_static PRIVATE LIBTERM_BUILDING)
    target_include_directories(libterm_static
        PUBLIC  ${LIBTERM_PUBLIC_INCLUDES}
        PRIVATE ${LIBTERM_PRIVATE_INCLUDES}
    )
    libterm_set_warnings(libterm_static)
    add_library(Libterm::static ALIAS libterm_static)
endif()

# --- shared ---
if(LIBTERM_BUILD_SHARED)
    add_library(libterm_shared SHARED ${LIBTERM_ALL_SOURCES})
    set_target_properties(libterm_shared PROPERTIES
        OUTPUT_NAME libterm
        C_VISIBILITY_PRESET hidden
        VERSION   ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    target_compile_definitions(libterm_shared
        PRIVATE LIBTERM_BUILDING
        PUBLIC  LIBTERM_SHARED
    )
    target_include_directories(libterm_shared
        PUBLIC  ${LIBTERM_PUBLIC_INCLUDES}
        PRIVATE ${LIBTERM_PRIVATE_INCLUDES}
    )
    libterm_set_warnings(libterm_shared)
    add_library(Libterm::shared ALIAS libterm_shared)
endif()

# default alias: prefer static if built, else shared
if(LIBTERM_BUILD_STATIC)
    add_library(Libterm::libterm ALIAS libterm_static)
elseif(LIBTERM_BUILD_SHARED)
    add_library(Libterm::libterm ALIAS libterm_shared)
endif()

# Apply per-TU compile flags (and, in dispatch mode, the per-backend symbol
# renames + the backend-presence defines for dispatch.c).
if(LIBTERM_SIMD_RESOLVED STREQUAL "dispatch")
    foreach(backend IN LISTS LIBTERM_SIMD_BACKENDS)
        libterm_simd_flags(${backend} LIBTERM_SIMD_BFLAGS)
        set(LIBTERM_SIMD_BDEFS
            "lt__simd_diff_first_differ_cell=lt__simd_diff_first_differ_cell_${backend}"
            "lt__simd_diff_first_equal_cell=lt__simd_diff_first_equal_cell_${backend}"
            "lt__simd_fill_cells=lt__simd_fill_cells_${backend}")
        if(LIBTERM_SIMD_BFLAGS)
            set_source_files_properties(intrinsics/${backend}.c PROPERTIES
                COMPILE_OPTIONS "${LIBTERM_SIMD_BFLAGS}"
                COMPILE_DEFINITIONS "${LIBTERM_SIMD_BDEFS}")
        else()
            set_source_files_properties(intrinsics/${backend}.c PROPERTIES
                COMPILE_DEFINITIONS "${LIBTERM_SIMD_BDEFS}")
        endif()
    endforeach()
    set_source_files_properties(intrinsics/dispatch.c PROPERTIES
        COMPILE_DEFINITIONS "${LIBTERM_SIMD_HAVE_DEFS}")
elseif(LIBTERM_SIMD_FLAGS)
    set_source_files_properties(${LIBTERM_SIMD_TUS} PROPERTIES
        COMPILE_OPTIONS "${LIBTERM_SIMD_FLAGS}")
endif()

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/libterm
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# --- install: compiled libraries + CMake export ------------------------------
# The package config (cmake/LibtermConfig.cmake.in) includes LibtermTargets.cmake,
# generated here. EXPORT_NAME makes the installed names match the in-tree
# aliases (Libterm::static / Libterm::shared); the umbrella Libterm::libterm is
# recreated by the config file (ALIAS targets cannot be exported).
set(LIBTERM_INSTALL_TARGETS "")
if(TARGET libterm_static)
    set_target_properties(libterm_static PROPERTIES EXPORT_NAME static)
    list(APPEND LIBTERM_INSTALL_TARGETS libterm_static)
endif()
if(TARGET libterm_shared)
    set_target_properties(libterm_shared PROPERTIES EXPORT_NAME shared)
    list(APPEND LIBTERM_INSTALL_TARGETS libterm_shared)
endif()
install(TARGETS ${LIBTERM_INSTALL_TARGETS}
    EXPORT LibtermTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(EXPORT LibtermTargets
    NAMESPACE Libterm::
    DESTINATION lib/cmake/Libterm
)

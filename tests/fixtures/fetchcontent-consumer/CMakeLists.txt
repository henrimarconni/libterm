cmake_minimum_required(VERSION 3.32)
project(consumer C)

if(NOT DEFINED LIBTERM_SOURCE_DIR)
    message(FATAL_ERROR "pass -DLIBTERM_SOURCE_DIR=<libterm checkout>")
endif()

include(FetchContent)
FetchContent_Declare(libterm SOURCE_DIR "${LIBTERM_SOURCE_DIR}")
FetchContent_MakeAvailable(libterm)

add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE Libterm::libterm)

# Gives `cmake --install` something legitimate to do, so the
# no-libterm-pollution assertion exercises a real install.
install(TARGETS consumer RUNTIME DESTINATION bin)

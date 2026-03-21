cmake_minimum_required(VERSION 3.16)
project(seam VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(SEAM_BUILD_TESTS    "Build test suite"       ON)
option(SEAM_BUILD_EXAMPLES "Build examples"         OFF)

# ── libseam (host-side analysis library) ─────────────────────────────────────
add_library(seam STATIC
    src/seam_analyze.c
    src/seam_rules.c
    src/seam_print.c
)
target_include_directories(seam PUBLIC include)
target_compile_options(seam PRIVATE -Wall -Wextra -Wpedantic)

# ── tests ─────────────────────────────────────────────────────────────────────
if(SEAM_BUILD_TESTS)
    enable_testing()
    add_executable(seam_test test/test_analyze.c)
    target_link_libraries(seam_test seam)
    target_include_directories(seam_test PRIVATE include)
    add_test(NAME seam_test COMMAND seam_test)
endif()

# ── install ───────────────────────────────────────────────────────────────────
install(TARGETS seam ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/seam
    FILES_MATCHING PATTERN "*.h")

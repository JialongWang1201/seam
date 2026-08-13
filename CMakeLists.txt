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

    # In-memory unit tests (no disk I/O)
    add_executable(seam_test test/test_analyze.c)
    target_link_libraries(seam_test seam)
    target_include_directories(seam_test PRIVATE include)
    add_test(NAME seam_test COMMAND seam_test)

    add_executable(seam_cobs_test test/test_cobs.c)
    target_include_directories(seam_cobs_test PRIVATE include)
    add_test(NAME seam_cobs_test COMMAND seam_cobs_test)

    # CLI binary used for fixture-based round-trip tests
    add_executable(seam_cli tools/seam_cli.c)
    target_link_libraries(seam_cli seam)
    target_include_directories(seam_cli PRIVATE include)

    # Fixture round-trip tests — exercises disk read + raw bundle parse + analysis
    set(FIXTURES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures)

    # Valid bundles: expect exit 0
    foreach(name normal_kdi_cascade partial_fill wraparound)
        add_test(NAME fixture_${name}
                 COMMAND seam_cli ${FIXTURES_DIR}/${name}.cfl)
        set_tests_properties(fixture_${name} PROPERTIES
            PASS_REGULAR_EXPRESSION "VERDICT:"
        )
    endforeach()

    # Invalid bundles: expect exit 1
    foreach(name bad_magic version_mismatch truncated)
        add_test(NAME fixture_${name}
                 COMMAND seam_cli ${FIXTURES_DIR}/${name}.cfl)
        set_tests_properties(fixture_${name} PROPERTIES
            WILL_FAIL TRUE
        )
    endforeach()
endif()

# ── install ───────────────────────────────────────────────────────────────────
install(TARGETS seam ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/seam
    FILES_MATCHING PATTERN "*.h")
